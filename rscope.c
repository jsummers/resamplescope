// ResampleScope
// Copyright (C) 2011-2017 Jason Summers

//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.

#ifdef _WIN32
#define RS_WINDOWS
#endif

#ifdef RS_WINDOWS

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#endif

#ifndef _UNICODE
#define _UNICODE
#endif

#define BGDWIN32 1 // For gd
#define NONDLL 1 // For gd

#endif

#ifdef RS_WINDOWS
#include <windows.h>
#endif

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <gd.h>
#include <gdfonts.h>


#ifdef RS_WINDOWS
#define my_snprintf my_snprintf_win
#else
#define my_snprintf snprintf
#endif

#define RS_VERSION   "1.1"

// Ideally, DOTIMG_SRC_WIDTH should be a prime number, 2 larger than an easily-typed number.
#define DOTIMG_SRC_WIDTH    557

// DOTIMG_HPIXELSPAN should be an odd number. Larger numbers make the source image larger,
// but allow for larger downscaling factors, and filters with larger radii.
#define DOTIMG_HPIXELSPAN   25
#define DOTIMG_NUMSTRIPS    DOTIMG_HPIXELSPAN
#define DOTIMG_HCENTER      ((DOTIMG_HPIXELSPAN-1)/2)

// DOTIMG_STRIPHEIGHT should be an odd number, at least 9 or 11. Larger numbers make the
// source image larger.
#define DOTIMG_STRIPHEIGHT  11
#define DOTIMG_VCENTER      ((DOTIMG_STRIPHEIGHT-1)/2)
#define DOTIMG_SRC_HEIGHT   (DOTIMG_NUMSTRIPS*DOTIMG_STRIPHEIGHT)

#define DOTIMG_DST_WIDTH    (DOTIMG_SRC_WIDTH-2)
#define DOTIMG_DST_HEIGHT   DOTIMG_SRC_HEIGHT


// LINEIMG_SRC_WIDTH should be an odd number, at least 9 or 11. Larger numbers allow for analysis
// of filters with larger radii, but may require you to upscale to larger sizes.
#define LINEIMG_SRC_WIDTH   15
// LINEIMG_SRC_HEIGHT should be an odd number, big enough to keep the middle rows safe from
// the effects of the top and bottom edges of the image.
#define LINEIMG_SRC_HEIGHT  15

// LINEIMG_DST_WIDTH is purely a recommendation to the user, and has no effect on the program.
// For the smoothest graphs, it should be an odd multiple of LINEIMG_SRC_WIDTH. Ideally, it
// should be easy to type.
#define LINEIMG_DST_WIDTH   555
#define LINEIMG_DST_HEIGHT  LINEIMG_SRC_HEIGHT


#define PATTERN_LINEIMG 1
#define PATTERN_DOTIMG 2

struct infile_info {
	const char *fn;
	const char *name;
	double scale_factor_req;  // The scale factor requested by the user.
	int scale_factor_req_set;
	double scale_fudge_factor_req; // Multiply the default scale factor by this fudge factor.
	int scale_fudge_factor_req_set;
	int thicklines;
	int color_r, color_g, color_b;
};

struct context {
	int rotated;

	// Size of the input image.
	int w, h;

	// The scale factor (of the image features, not necessarily of the image itself)
	// that we believe was used when the input image was created.
	double scale_factor;

	// The scale factor based on the number of pixels in the images.
	double natural_scale_factor;

	gdImagePtr im_in;
	FILE *im_in_fp;

	// Preferred color to use to for information about the current input image.
	int curr_color;

	int border_color;

	// Persistent information about each input file.
	struct infile_info inf[2];

	// Information about the output image coordinates.
	int gr_width, gr_height;
	double gr_zero_x, gr_zero_y;
	double gr_unit_x, gr_unit_y;

	const char *outfn;
	gdImagePtr im_out;

	// Tracks how many graphs we've plotted on this output image.
	int graph_count;

	int include_logo;
	int expandrange;

#define CCMETHOD_LINEAR 0
#define CCMETHOD_SRGB   2
	int color_correction_method;
	double srgb50_as_lin1, srgb_250_as_lin1; // Cached calculated values

	// Temporary space for the samples being analyzed.
	// (Currently only used when with lineimg.)
	double *samples;

	// Used by the line drawing function
	int lastpos_set;
	int lastpos_x, lastpos_y;
	double lastpos_x_dbl, lastpos_y_dbl;
};

#ifdef RS_WINDOWS

static void my_vsnprintf_win(char *buf, size_t buflen, const char *fmt, va_list ap)
{
	_vsnprintf_s(buf, buflen, _TRUNCATE, fmt, ap);
}
static void my_snprintf_win(char *buf, size_t buflen, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	my_vsnprintf_win(buf, buflen, fmt, ap);
	va_end(ap);
}

#endif

static FILE *my_fopen(const char *file, const char *mode)
{
	// TODO: Special processing for Windows
	return fopen(file, mode);
}

static void my_gdImageString(gdImagePtr im, gdFontPtr f, int x, int y,
	unsigned char *s, int color)
{
	// TODO: Character set translation
	gdImageString(im, f, x, y, s, color);
}


static void gr_init(struct context *c)
{
	c->gr_width = 600;
	c->gr_zero_x = 230.0;
	c->gr_unit_x = 90.0;

	c->gr_height = 300;
	if(c->expandrange==2) {
		c->gr_zero_y = 260.0;
		c->gr_unit_y = -90.0;
	}
	else if(c->expandrange==1) {
		c->gr_zero_y = 240.0;
		c->gr_unit_y = -150.0;
	}
	else {
		c->gr_zero_y = 220.0;
		c->gr_unit_y = -200.0;
	}

	c->im_out =  gdImageCreate(c->gr_width,c->gr_height);
	gdImageFilledRectangle(c->im_out,0,0,c->gr_width-1,c->gr_height-1,
	 gdImageColorResolve(c->im_out,255,255,255));
}

static void gr_done(struct context *c)
{
	FILE *w;
	w = my_fopen(c->outfn,"wb");
	if(!w) return;

	gdImagePng(c->im_out,w);
	fclose(w);
	gdImageDestroy(c->im_out);
}

static int point_is_visible(struct context *c, int x, int y)
{
	if(x<0 || y<0) return 0;
	if(x>=c->gr_width || y>=c->gr_height) return 0;
	return 1;
}

// Convert from logical coordinates to output-image coordinates.
static int xcoord(struct context *c, double ix)
{
	double physx;
	physx = c->gr_zero_x + (ix*c->gr_unit_x);
	return (int)(0.5+physx);
}
static int ycoord(struct context *c, double iy)
{
	double physy;
	physy = c->gr_zero_y + (iy*c->gr_unit_y);
	return (int)(0.5+physy);
}

static void gr_draw_grid(struct context *c)
{
	int clr;
	int i;
	char tbuf[20];

	clr = gdImageColorResolve(c->im_out,192,192,192);
	for(i= -10; i<=10; i++) {
		// Draw lines for half-integers
		gdImageDashedLine(c->im_out,xcoord(c,0.5+(double)i),0,xcoord(c,0.5+(double)i),c->gr_height,clr);
		gdImageDashedLine(c->im_out,0,ycoord(c,0.5+(double)i),c->gr_width,ycoord(c,0.5+(double)i),clr);
	}

	// Draw lines for integers
	clr = gdImageColorResolve(c->im_out,192,192,192);
	for(i= -10; i<=10; i++) {
		gdImageLine(c->im_out,xcoord(c,i),0,xcoord(c,i),c->gr_height,clr);
		gdImageLine(c->im_out,0,ycoord(c,i),c->gr_width,ycoord(c,i),clr);

	}

	// Draw x- and y- axes
	clr = gdImageColorResolve(c->im_out,0,0,0);
	gdImageLine(c->im_out,xcoord(c,0.0),0,xcoord(c,0.0),c->gr_height,clr);
	gdImageLine(c->im_out,0,ycoord(c,0.0),c->gr_width,ycoord(c,0.0),clr);

	// Draw labels
	clr = gdImageColorResolve(c->im_out,0,128,0);
	for(i= 0; i<=1; i++) {
		my_snprintf(tbuf, sizeof(tbuf), "%d", i);
		my_gdImageString(c->im_out,gdFontSmall,xcoord(c,i)-6,c->gr_height-14,
			(unsigned char*)tbuf,clr);
		my_gdImageString(c->im_out,gdFontSmall,3,ycoord(c,i)-12,
			(unsigned char*)tbuf,clr);
	}

	// Draw border around the whole image
	gdImageRectangle(c->im_out,0,0,c->gr_width-1,c->gr_height-1,c->border_color);
}

static void gr_lineto(struct context *c, double xpos1, double ypos1, int clr)
{
	int xpos, ypos;

	xpos = xcoord(c,xpos1);
	ypos = ycoord(c,ypos1);

	if(c->lastpos_set) {
		gdImageLine(c->im_out,c->lastpos_x,c->lastpos_y,xpos,ypos,clr);
	}

	c->lastpos_x = xpos;
	c->lastpos_y = ypos;
	c->lastpos_x_dbl = xpos1;
	c->lastpos_y_dbl = ypos1;
	c->lastpos_set=1;
}

// Heuristically figure out a friendly name to use for the graph.
static void gr_get_name_from_fn(const char *fn, char *buf, size_t buflen)
{
	char *r;
	r = strrchr(fn,'/');
#ifdef RS_WINDOWS
	if(!r) r = strrchr(fn,'\\');
#endif

	if(r)
		my_snprintf(buf, buflen, "%s", r+1);
	else
		my_snprintf(buf, buflen, "%s", fn);

	r = strrchr(buf,'.');
	if(r) {
		*r = '\0';
	}
}

static void gr_draw_graph_name(struct context *c, struct infile_info *inf, int sf_flag)
{
	int ypos;
	char buf[100];
	char s[100];

	ypos = c->gr_height-19-14*c->graph_count;

	if(inf->name) {
		my_snprintf(buf, sizeof(buf), "%s", inf->name);
	}
	else {
		gr_get_name_from_fn(inf->fn,buf,100);
	}
	if(inf->thicklines) gdImageSetThickness(c->im_out,3);
	gdImageLine(c->im_out,5,ypos+7,13,ypos+7,c->curr_color);
	gdImageSetThickness(c->im_out,1);

	my_snprintf(s, sizeof(s), "%s", buf);
	if(sf_flag) {
		double ff;
		ff = c->scale_factor / c->natural_scale_factor;
		if(ff<0.99999999 || ff>1.00000001) {
			my_snprintf(s, sizeof(s), "%s (factor=%.8f)", buf, ff);
		}
	}

	s[sizeof(s)-1]='\0';
	my_gdImageString(c->im_out,gdFontSmall,17,ypos,(unsigned char*)s,c->curr_color);
}

static void gr_draw_logo(struct context *c)
{
	if(!c->include_logo) return;

	gdImageFilledRectangle(c->im_out,c->gr_width-81,c->gr_height-15,
	 c->gr_width-1,c->gr_height-1,c->border_color);

	my_gdImageString(c->im_out,gdFontSmall,c->gr_width-79,c->gr_height-15,
		(unsigned char*)"ResampleScope",
		gdImageColorResolve(c->im_out,255,255,255));
}

/////////////////////////////////////////////////

// Wrappers for gd functions, which swap the x and y coordinates if -r was used,
// and may do colorspace transformation.

static void rs_gdImageSetPixel(struct context *c, gdImagePtr im, int x, int y, int color)
{
	if(c->rotated)
		gdImageSetPixel(im,y,x,color);
	else
		gdImageSetPixel(im,x,y,color);
}

static double srgb_to_linear(double v_srgb)
{
	if(v_srgb<=0.04045) {
		return v_srgb/12.92;
	}
	else {
		return pow( (v_srgb+0.055)/(1.055) , 2.4);
	}
}

// Returns a value typically in the range 0..255,
// where 50 and 250 are our special "dark" and "light" colors.
static double rs_gdImageGetPixel(struct context *c, gdImagePtr im, int x, int y)
{
	int colorref;
	double val;

	if(c->rotated)
		colorref = gdImageGetPixel(im,y,x);
	else
		colorref = gdImageGetPixel(im,x,y);

	val = (double)gdImageGreen(im, colorref);

	if(c->color_correction_method==CCMETHOD_SRGB) {
		double v1;
		// We're assuming the app being tested behaved as follows:
		// (1) converted the original colors from sRGB to linear;
		// (2) resized the image in a linear colorspace;
		// (3) converted back to sRGB.

		// First, undo (3) by converting from sRGB[0..255] to linear[0..1].
		v1 = srgb_to_linear((double)val/255.0);

		// The catch is that the color is now in that app's linear colorspace,
		// not ours.
		// If we were to simply multiply the value by 255, our dark color would
		// correspond to ~8.1, and our light color to ~243.7.
		// That's not what we want. We want 50 and 250.
		// So, we have to be careful to scale and translate it to the correct
		// range.
		val = (v1-c->srgb50_as_lin1) *
			((250.0-50.0)/(c->srgb_250_as_lin1-c->srgb50_as_lin1)) + 50.0;
	}

	return val;
}

static int rs_gdImageSX(struct context *c, gdImagePtr im)
{
	return c->rotated ? gdImageSY(im) : gdImageSX(im);
}

static int rs_gdImageSY(struct context *c, gdImagePtr im)
{
	return c->rotated ? gdImageSX(im) : gdImageSY(im);
}

/////////////////////////////////////////////////

// Opens and reads the image, if that hasn't already been done.
static int open_file_for_reading(struct context *c, const char *fn)
{
	// The file may have already been opened, to detect the image type.
	// If not, open it now.
	if(!c->im_in_fp) {
		c->im_in_fp = my_fopen(fn,"rb");
		if(!c->im_in_fp) {
			fprintf(stderr,"* Error: Failed to read %s\n",fn);
			return 0;
		}
	}

	if(!c->im_in) {
		c->im_in = gdImageCreateFromPng(c->im_in_fp);
		if(!c->im_in) {
			fprintf(stderr,"gd creation failed\n");
			return 0;
		}
	}

	return 1;
}

static void close_file_for_reading(struct context *c)
{
	if(c->im_in) { gdImageDestroy(c->im_in); c->im_in = NULL; }
	if(c->im_in_fp) { fclose(c->im_in_fp); c->im_in_fp = NULL; }
}

//////////////////// DOTIMG ////////////////////

// Plot values for pixels the given "strip".
// The first DOTIMG_STRIPHEIGHT scalines are strip 0,
//    the next DOTIMG_STRIPHEIGHT are strip 1, etc.
static int plot_strip(struct context *c, struct infile_info *inf, int stripnum)
{
	double v;
	int dstpos,k;
	double tot;
	double value;
	int xc,yc;
	double zp;
	double tmp_offset;
	double offset; // Relative position of the nearest "0" point, in target image coords.

	for(dstpos=0;dstpos<c->w;dstpos++) {

		// Scan through the possible "0" points in this strip, and find the nearest one.
		offset = 10000.0; // (too far)

		// There are "0" points at ({5,16,27,38...}+stripnum) * scale_factor

		for(k=(DOTIMG_HCENTER+stripnum); k<(DOTIMG_SRC_WIDTH-DOTIMG_HCENTER); k+=DOTIMG_HPIXELSPAN) {
			// Convert to target image coordinates.

			// I don't really remember why this formula works, but it seems to.
			zp = (c->scale_factor)*(((double)k) + 0.5 - ((double)DOTIMG_SRC_WIDTH)/2.0) + (c->w/2.0)  - 0.5;

			// The directed distance to this 0 point.
			tmp_offset = ((double)dstpos)-zp;

			// Keep track of the smallest (in absolute value) distance.
			if(fabs(tmp_offset)<fabs(offset)) {
				offset = tmp_offset;
			}
		}

		// Make sure the offset is small enough to be meaningful
		// TODO: This might only be correct when downscaling.
		if(fabs(offset)>(c->scale_factor*DOTIMG_HCENTER)) continue;

		// Add up the DOTIMG_STRIPHEIGHT pixels vertically that are in this strip.
		// Ideally, all but the middle one will be empty, but in reality most
		// filters get applied vertically as well as horizontally, which can 
		// cause vertical blurring depending on the filter. This is how we
		// undo that.
		tot = 0;
		for(k=0;k<DOTIMG_STRIPHEIGHT;k++) {
			v = rs_gdImageGetPixel(c,c->im_in,dstpos,DOTIMG_STRIPHEIGHT*stripnum+k);
			tot += (v-50.0);
		}

		// Convert (0 to 200) to (0 to 1).
		value = tot/200.0;

		if(c->scale_factor < 1.0) {
			// Compensate for the fact that we're shrinking the image, which
			// reduces the size of a pixel, making it dimmer.
			// This factor is normally about 0.996, so this will only make
			// a small difference.
			value /= c->scale_factor;
		}
		else {
			offset /= c->scale_factor;
		}

		// Plot the point.
		xc = xcoord(c,offset);
		yc = ycoord(c,value);
		if(point_is_visible(c,xc,yc)) {
			gdImageSetPixel(c->im_out,xc,yc,c->curr_color);
			if(inf->thicklines) {
				gdImageSetPixel(c->im_out,xc-1,yc,c->curr_color);
				gdImageSetPixel(c->im_out,xc+1,yc,c->curr_color);
				gdImageSetPixel(c->im_out,xc,yc-1,c->curr_color);
				gdImageSetPixel(c->im_out,xc,yc+1,c->curr_color);
			}
		}

	}
	return 1;
}

static void decide_scale_factor(struct context *c, struct infile_info *inf, int src_width)
{
	// Start with the default scale factor:
	c->scale_factor = ((double)c->w) / src_width;
	c->natural_scale_factor = c->scale_factor;

	if(inf->scale_factor_req_set) {
		// scale factor overridden by user
		c->scale_factor = inf->scale_factor_req;
	}

	if(inf->scale_fudge_factor_req_set) {
		// adjust the scale factor as requested
		c->scale_factor *= inf->scale_fudge_factor_req;
	}
}

static int run_dotimg_1file(struct context *c, struct infile_info *inf)
{
	int retval=0;
	int i;

	fprintf(stderr," Reading %s\n",inf->fn);

	if(!open_file_for_reading(c,inf->fn)) goto done;

	c->w = rs_gdImageSX(c,c->im_in);
	c->h = rs_gdImageSY(c,c->im_in);
	if(c->h != DOTIMG_SRC_HEIGHT) {
		fprintf(stderr,"* Error: Image is wrong height (is %d, should be %d)\n",c->h,DOTIMG_SRC_HEIGHT);
		goto done;
	}
	if(c->w<50) {
		fprintf(stderr,"* Error: Image is wrong width (is %d, must be at least 50)\n",c->w);
		goto done;
	}

	decide_scale_factor(c,inf,DOTIMG_SRC_WIDTH);

	gr_draw_graph_name(c,inf,1);

	for(i=0;i<DOTIMG_NUMSTRIPS;i++) {
		if(!plot_strip(c,inf,i)) goto done;
	}

	retval=1;
done:
	close_file_for_reading(c);
	c->graph_count++;
	return retval;
}

static int run_ds(struct context *c)
{
	int ret;

	fprintf(stderr,"Writing %s [dot pattern]\n",c->outfn);

	gr_init(c);
	c->border_color = gdImageColorResolve(c->im_out,144,192,144);
	gr_draw_grid(c);
	gr_draw_logo(c);

	if(c->inf[1].fn) {
		c->curr_color = gdImageColorResolve(c->im_out,
		  c->inf[1].color_r,c->inf[1].color_g,c->inf[1].color_b);
		ret = run_dotimg_1file(c,&c->inf[1]);
		c->lastpos_set = 0;
	}

	c->curr_color = gdImageColorResolve(c->im_out,
	  c->inf[0].color_r,c->inf[0].color_g,c->inf[0].color_b);
	ret = run_dotimg_1file(c,&c->inf[0]);

	gr_done(c);
	return ret;
}

////////////////////////////////////////////////


//////////////////// LINEIMG ///////////////////

static void gr_lineimg_graph_main(struct context *c, struct infile_info *inf)
{
	int i;
	double v;
	int clr;
	double xp, yp;
	double tot = 0.0;
	double area;

	clr = c->curr_color;

	if(inf->thicklines)
		gdImageSetThickness(c->im_out,3);

	for(i=0;i<c->w;i++) {
		v = c->samples[i];
		yp = (v-50.0)/200.0;
		tot += yp;
		xp = 0.5+(double)i-(((double)c->w)/2.0);

		if(c->scale_factor < 1.0) {
			yp /= c->scale_factor;
		}
		else {
			xp /= c->scale_factor;
		}

		gr_lineto(c,xp,yp,clr);
	}

	gdImageSetThickness(c->im_out,1);

	area = tot/c->scale_factor;
	fprintf(stderr,"  Area = %.6f",area);
	fprintf(stderr,"\n");
}

static int run_lineimg_1file(struct context *c, struct infile_info *inf)
{
	int retval=0;
	int i;
	int scanline; // The (middle) scanline we'll analyze

	fprintf(stderr," Reading %s\n",inf->fn);

	if(!open_file_for_reading(c,inf->fn)) {
		goto done;
	}

	c->w = rs_gdImageSX(c,c->im_in);
	c->h = rs_gdImageSY(c,c->im_in);

	if(c->h < 3) {
		fprintf(stderr,"Image height (%d) too small\n",c->h);
		goto done;
	}

	decide_scale_factor(c,inf,LINEIMG_SRC_WIDTH);

	gr_draw_graph_name(c,inf,1);

	scanline = c->h / 2;

	// Copy the samples we're analyzing
	c->samples = malloc(sizeof(double)*c->w);
	for(i=0;i<c->w;i++) {
		// Read from three different scanlines, to give us a chance of
		// detecting weird issues where the scanlines aren't identical.
		c->samples[i] = rs_gdImageGetPixel(c,c->im_in,i, scanline+(i%3)-1);
	}

	gr_lineimg_graph_main(c,inf);
	retval=1;
done:
	close_file_for_reading(c);
	if(c->samples) { free(c->samples); c->samples=NULL; }
	c->graph_count++;
	return retval;
}

static int run_us(struct context *c)
{
	int ret = 0;

	fprintf(stderr,"Writing %s [line pattern]\n",c->outfn);

	gr_init(c);
	c->border_color = gdImageColorResolve(c->im_out,204,136,204);
	gr_draw_grid(c);
	gr_draw_logo(c);

	if(c->inf[1].fn) {
		c->curr_color = gdImageColorResolve(c->im_out,
		  c->inf[1].color_r,c->inf[1].color_g,c->inf[1].color_b);
		ret = run_lineimg_1file(c,&c->inf[1]);
		c->lastpos_set = 0;
	}

	c->curr_color = gdImageColorResolve(c->im_out,
	  c->inf[0].color_r,c->inf[0].color_g,c->inf[0].color_b);
	ret = run_lineimg_1file(c,&c->inf[0]);

	gr_done(c);
	return ret;
}

///////////////////////////////////////////////


/////////////// FILE GENERATION ///////////////

static int gen_dotimg_image(struct context *c)
{
	int i,j;
	gdImagePtr im = NULL;
	FILE *w = NULL;
	int clr_gray, clr_white;
	int clr;
	int retval=0;
	const char *fn;

	fn = c->rotated ? "pdr.png" : "pd.png";

	w = my_fopen(fn,"wb");
	if(!w) {
		fprintf(stderr,"Can't write %s\n",fn);
		goto done;
	}

	if(c->rotated)
		im = gdImageCreateTrueColor(DOTIMG_SRC_HEIGHT,DOTIMG_SRC_WIDTH);
	else
		im = gdImageCreateTrueColor(DOTIMG_SRC_WIDTH,DOTIMG_SRC_HEIGHT);

	clr_gray = gdImageColorResolve(im,50,50,50);
	clr_white = gdImageColorResolve(im,250,250,250);

	for(j=0;j<DOTIMG_SRC_HEIGHT;j++) {
		for(i=0;i<DOTIMG_SRC_WIDTH;i++) {
			clr = clr_gray;
			if((j%DOTIMG_STRIPHEIGHT==DOTIMG_VCENTER) && (i>=DOTIMG_HCENTER) && (i<DOTIMG_SRC_WIDTH-DOTIMG_HCENTER)) {
				if((i-j/DOTIMG_STRIPHEIGHT)%DOTIMG_HPIXELSPAN == DOTIMG_HCENTER) {
					clr = clr_white;
				}
			}
			rs_gdImageSetPixel(c,im,i,j,clr);
		}
	}

	gdImagePng(im,w);
	if(c->rotated) {
		fprintf(stderr,"Wrote %s (%dx%d - resize to %dx%d)\n",fn,
		  DOTIMG_SRC_HEIGHT,DOTIMG_SRC_WIDTH,
		  DOTIMG_DST_HEIGHT,DOTIMG_DST_WIDTH);
	}
	else {
		fprintf(stderr,"Wrote %s (%dx%d - resize to %dx%d)\n",fn,
		  DOTIMG_SRC_WIDTH,DOTIMG_SRC_HEIGHT,
		  DOTIMG_DST_WIDTH,DOTIMG_DST_HEIGHT);
	}

	retval=1;
done:
	if(im) gdImageDestroy(im);
	if(w) fclose(w);
	return retval;
}

static int gen_lineimg_image(struct context *c)
{
	int i,j;
	gdImagePtr im = NULL;
	FILE *w = NULL;
	int clr_black, clr_white;
	int clr;
	int retval=0;
	int middle;
	const char *fn;

	fn = c->rotated ? "plr.png" : "pl.png";

	w = my_fopen(fn,"wb");
	if(!w) {
		fprintf(stderr,"Can't write %s\n",fn);
		goto done;
	}

	if(c->rotated)
		im = gdImageCreateTrueColor(LINEIMG_SRC_HEIGHT,LINEIMG_SRC_WIDTH);
	else
		im = gdImageCreateTrueColor(LINEIMG_SRC_WIDTH,LINEIMG_SRC_HEIGHT);

	clr_black = gdImageColorResolve(im,50,50,50);
	clr_white = gdImageColorResolve(im,250,250,250);

	middle = LINEIMG_SRC_WIDTH / 2;

	for(j=0;j<LINEIMG_SRC_HEIGHT;j++) {
		for(i=0;i<LINEIMG_SRC_WIDTH;i++) {
			if(i==middle) clr = clr_white;
			else clr = clr_black;
			rs_gdImageSetPixel(c,im,i,j,clr);
		}
	}

	gdImagePng(im,w);

	if(c->rotated) {
		fprintf(stderr,"Wrote %s (%dx%d - resize to %dx%d)\n",fn,
		  LINEIMG_SRC_HEIGHT,LINEIMG_SRC_WIDTH,
		  LINEIMG_DST_HEIGHT,LINEIMG_DST_WIDTH);
	}
	else {
		fprintf(stderr,"Wrote %s (%dx%d - resize to %dx%d)\n",fn,
		  LINEIMG_SRC_WIDTH,LINEIMG_SRC_HEIGHT,
		  LINEIMG_DST_WIDTH,LINEIMG_DST_HEIGHT);
	}

	retval=1;
done:
	if(im) gdImageDestroy(im);
	if(w) fclose(w);
	return retval;
}

static void gen_html(struct context *c)
{
	FILE *w = NULL;
	char *fn;

	fn = c->rotated ? "rscoper.html" : "rscope.html";

	w = my_fopen(fn,"w");
	if(!w) return;

	fprintf(w,"<html>\n");
	fprintf(w,"<head>\n");
	fprintf(w,"<title>ResampleScope browser test page</title>\n");
	fprintf(w,"<style>\n");
	fprintf(w,"IMG { -ms-interpolation-mode:bicubic }\n");
	fprintf(w,"TD.t { text-align:right }\n");
	fprintf(w,"TD.c { text-align:center }\n");
	fprintf(w,"</style>\n");
	fprintf(w,"</head>\n");
	fprintf(w,"<body bgcolor=\"#bbbbcc\">\n");
	fprintf(w,"<table>\n");

	if(c->rotated) {
		fprintf(w,"<tr><td colspan=2></td><td class=c>Downscale:</td><td class=c>Downscale350,200:</td></tr>\n");
		fprintf(w,"<tr><td class=t rowspan=2>Upscale:</td><td rowspan=2><img src=plr.png width=%d height=%d></td>\n",
		  LINEIMG_DST_HEIGHT,LINEIMG_DST_WIDTH);
		fprintf(w,"<td rowspan=2><img src=pdr.png width=%d height=%d></td>\n",
		  DOTIMG_DST_HEIGHT,DOTIMG_DST_WIDTH);
		fprintf(w,"<td><img src=pdr.png width=%d height=%d></td></tr>\n",
		  DOTIMG_DST_HEIGHT,350);
		fprintf(w,"<tr><td><img src=pdr.png width=%d height=%d></td></tr>\n",
		  DOTIMG_DST_HEIGHT,200);
	}
	else {
		fprintf(w,"<tr><td class=t>Upscale:</td><td colspan=2><img src=pl.png width=%d height=%d></td></tr>\n",
		  LINEIMG_DST_WIDTH,LINEIMG_DST_HEIGHT);
		fprintf(w,"<tr><td class=t>Downscale:</td><td colspan=2><img src=pd.png width=%d height=%d></td></tr>\n",
		  DOTIMG_DST_WIDTH,DOTIMG_DST_HEIGHT);
		fprintf(w,"<tr><td class=t>Downscale350,200:</td><td><img src=pd.png width=%d height=%d></td>\n",
		  350,DOTIMG_DST_HEIGHT);
		fprintf(w,"<td><img src=pd.png width=%d height=%d></td></tr>\n",
		  200,DOTIMG_DST_HEIGHT);
	}

	fprintf(w,"</table>\n");
	fprintf(w,"</body>\n</html>\n");

	fclose(w);
	fprintf(stderr,"Wrote %s\n",fn);
}

static void gen_source_images(struct context *c)
{
	gen_lineimg_image(c);
	gen_dotimg_image(c);
	gen_html(c);
}

///////////////////////////////////////////////

// Returns PATTERN_*.
// On error, prints an error message and returns 0.
static int detect_image_type(struct context *c, const char *fn)
{
	int w;
	int i;

	if(!open_file_for_reading(c,fn)) return 0;
	//fprintf(stderr,"Autodetecting %s\n",fn);

	w = rs_gdImageSX(c,c->im_in);

	// Look at the top row. If it contains any bright pixels, assume PATTERN_LINEIMG.
	// Otherwise, assume PATTERN_DOTIMG
	for(i=0;i<w;i++) {
		if(rs_gdImageGetPixel(c,c->im_in,i,0)>=99.9) return PATTERN_LINEIMG;
	}

	return PATTERN_DOTIMG;
}

///////////////////////////////////////////////

static void usage(const char *prg)
{
	FILE *f = stderr;
	fprintf(f,"ResampleScope v%s, ",RS_VERSION);
	fprintf(f,"Copyright (C) 2011 Jason Summers\n");
	fprintf(f,"Usage:\n");
	fprintf(f,"  %s [-r] -gen\n",prg);
	fprintf(f,"     Generate the source image files\n");
	fprintf(f,"  %s [options] <image-file.png> [<secondary-image-file.png>] <output-file.png>\n",prg);
	fprintf(f,"     Analyze a resized image file\n");
	fprintf(f," Options:\n");
	fprintf(f,"  -pd             - Assume the \"dots pattern\" source image was used\n");
	fprintf(f,"  -pl             - Assume the \"lines pattern\" source image was used\n");
	fprintf(f,"  -sf <factor>    - Assume image-file.png's features were scaled by this factor\n");
	fprintf(f,"  -ff <factor>    - Multiply image-file.png's assumed scale factor by this factor\n");
	fprintf(f,"  -srgb           - Assume an sRGB-colorspace-aware resize was performed\n");
	fprintf(f,"  -r              - Swap the x and y dimensions, to test the vertical direction\n");
	fprintf(f,"  -range[2]       - Shrink the graph, to increase the visible vertical range\n");
	fprintf(f,"  -thick1         - Graph image-file.png using thicker lines\n");
	fprintf(f,"  -thick          - Graph secondary-image-file.png using thicker lines\n");
	fprintf(f,"  -nologo         - Don't include the program name in output-file.png\n");
	fprintf(f,"  -name <name>    - Friendly name for image-file.png\n");
	fprintf(f,"  -name2 <name>   - Friendly name for secondary-image-file.png\n");
}

static void init_ctx(struct context *c)
{
	memset(c,0,sizeof(struct context));
	c->include_logo = 1;

	c->inf[0].color_r = 0;
	c->inf[0].color_g = 0;
	c->inf[0].color_b = 255;
	c->inf[1].color_r = 224;
	c->inf[1].color_g = 64;
	c->inf[1].color_b = 64;

	c->srgb50_as_lin1 = srgb_to_linear(50.0/255.0);
	c->srgb_250_as_lin1 = srgb_to_linear(250.0/255.0);
}

static int main2(int argc, char **argv)
{
	struct context c;
	const char *param1 = NULL;
	const char *param2 = NULL;
	const char *param3 = NULL;
	int i;
	int paramcount;
	const char *prg;
#define OP_GEN      1
#define OP_ANALYZE  2
	int op = 0;
	int pattern = 0;

	prg = (argc>=1)?argv[0]:"rscope";

	init_ctx(&c);

	paramcount=0;

	i=1;
	while(i<argc) {
		if(argv[i][0]=='-') {
			if(!strcmp(argv[i],"-gen")) {
				op = OP_GEN;
			}
			else if(!strcmp(argv[i],"-pd")) {
				op = OP_ANALYZE;
				pattern = PATTERN_DOTIMG;
			}
			else if(!strcmp(argv[i],"-pl")) {
				op = OP_ANALYZE;
				pattern = PATTERN_LINEIMG;
			}
			else if(!strcmp(argv[i],"-r")) {
				c.rotated = 1;
			}
			else if((i<argc-1) && !strcmp(argv[i],"-name")) {
				c.inf[0].name = argv[i+1];
				i++;
			}
			else if((i<argc-1) && !strcmp(argv[i],"-name2")) {
				c.inf[1].name = argv[i+1];
				i++;
			}
			else if((i<argc-1) && !strcmp(argv[i],"-sf")) {
				c.inf[0].scale_factor_req = atof(argv[i+1]);
				c.inf[0].scale_factor_req_set = 1;
				i++;
			}
			else if((i<argc-1) && !strcmp(argv[i],"-ff")) {
				c.inf[0].scale_fudge_factor_req = atof(argv[i+1]);
				c.inf[0].scale_fudge_factor_req_set = 1;
				i++;
			}
			else if(!strcmp(argv[i],"-srgb")) {
				c.color_correction_method = CCMETHOD_SRGB;
			}
			else if(!strcmp(argv[i],"-nologo")) {
				c.include_logo=0;
			}
			else if(!strcmp(argv[i],"-range")) {
				c.expandrange=1;
			}
			else if(!strcmp(argv[i],"-range2")) {
				c.expandrange=2;
			}
			else if(!strcmp(argv[i],"-thick1")) {
				c.inf[0].thicklines = 1;
			}
			else if(!strcmp(argv[i],"-thick")) {
				c.inf[1].thicklines = 1;
				// Use a lighter color for thick lines.
				c.inf[1].color_r=255; c.inf[1].color_g=128; c.inf[1].color_b=128;
			}
			else {
				fprintf(stderr,"Unknown option: %s\n",argv[i]);
				return 1;
			}
		}
		else {
			// a non-option parameter
			switch(paramcount) {
			case 0: param1=argv[i]; break;
			case 1: param2=argv[i]; break;
			case 2: param3=argv[i]; break;
			}
			paramcount++;
		}
		i++;
	}

	// Detect the image type (either LINEIMG or DOTIMG).
	// This will base its detection on whichever file gets read first, which
	// will be the secondary file if there is one.
	// The file will be left in an open state, and must be closed by calling
	// close_file_for_reading.
	// If we leave the file open, the next (first) image analysis we do will use
	// it, instead of opening a new file. So we have to be sure we leave the
	// correct file open. (Yes, this is ugly.)
	if(op==0 && (paramcount==2 || paramcount==3)) {
		op = OP_ANALYZE;
		pattern = detect_image_type(&c,(paramcount==2)?param1:param2);

		if(pattern!=PATTERN_DOTIMG && pattern!=PATTERN_LINEIMG) {
			close_file_for_reading(&c);
			return 1;
		}
	}

	if(op==OP_GEN) {
		gen_source_images(&c);
		return 0;
	}
	else if(op==OP_ANALYZE && pattern==PATTERN_DOTIMG) {
		if(paramcount==2) {
			// DOTIMG, 1 input file
			c.inf[0].fn = param1;
			c.inf[1].fn = NULL;
			c.outfn = param2;
			run_ds(&c);
		}
		else if(paramcount==3) {
			// DOTIMG, 2 input files
			c.inf[0].fn = param1;
			c.inf[1].fn = param2;
			c.outfn = param3;
			run_ds(&c);
		}
		else {
			usage(prg);
			return 1;
		}
	}
	else if(op==OP_ANALYZE && pattern==PATTERN_LINEIMG) {
		if(paramcount==2) {
			// LINEIMG, 1 input file
			c.inf[0].fn = param1;
			c.inf[1].fn = NULL;
			c.outfn = param2;
			run_us(&c);
		}
		else if(paramcount==3) {
			// LINEIMG, 2 input files
			c.inf[0].fn = param1;
			c.inf[1].fn = param2;
			c.outfn = param3;
			run_us(&c);
		}
		else {
			usage(prg);
			return 1;
		}
	}
	else {
		usage(prg);
		return 1;
	}

	return 0;
}

#ifdef RS_WINDOWS

static char *utf16_to_utf8_strdup(const wchar_t *src)
{
	char *dst;
	int dstlen;
	int ret;

	// Calculate the size required by the target string.
	ret = WideCharToMultiByte(CP_UTF8, 0, src, -1, NULL, 0, NULL, NULL);
	if(ret<1) return NULL;

	dstlen = ret;
	dst = malloc(dstlen);

	ret = WideCharToMultiByte(CP_UTF8, 0, src, -1, dst, dstlen, NULL, NULL);
	if(ret<1) {
		free(dst);
		return NULL;
	}
	return dst;
}

static char **convert_args_to_utf8(int argc, wchar_t **argvW)
{
	int i;
	char **argvUTF8;

	argvUTF8 = (char**)malloc(argc*sizeof(char*));

	// Convert parameters to UTF-8
	for(i=0;i<argc;i++) {
		argvUTF8[i] = utf16_to_utf8_strdup(argvW[i]);
	}

	return argvUTF8;
}

static void free_utf8_args(int argc, char **argv)
{
	int i;

	for(i=0;i<argc;i++) {
		free(argv[i]);
	}
	free(argv);
}

int wmain(int argc, wchar_t **argvW)
{
	int ret;
	char **argv;

	argv = convert_args_to_utf8(argc, argvW);
	ret = main2(argc, argv);
	free_utf8_args(argc, argv);
	return ret;
}

#else

int main(int argc, char **argv)
{
	return main2(argc, argv);
}

#endif

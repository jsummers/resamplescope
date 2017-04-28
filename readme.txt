ResampleScope
Version 1.1
Copyright (C) 2011 Jason Summers

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.


ResampleScope is a utility to help analyze the image scaling algorithms used
by other applications.

You'll have to know what various resampling filters look like -- it will show
you a picture of the filter, but it won't try to name it.


How to build
------------

libgd (with PNG support) is required.

Linux (etc.): Try running "make".

Windows: There are project files in the "proj" subdirectory that may help to
compile ResampleScope as a Windows console application. However, this is
primarily intended for maintainer use. You will have to build compatible
copies of libgd, libpng, and zlib, and no help is provided for this. Another
option is to use Cygwin.


How to use
----------

First, run "rscope -gen" to generate the source images. It will suggest the
sizes to resize the images to, but in some cases you can use other sizes.

Then, load them into an application, resize them, and save the resized
images in PNG format. (Take a screenshot if you're testing a web browser or
other application that doesn't let you save resized images.)

Then, run "rscope <resized-image.png> <output-image.png>" on the resized
images. It will write an image file containing a picture of the resampling
filter used.

For a list of options, run rscope with no parameters.


Notes
-----

If the application does any gamma correction, you may have to find a way to
turn off that feature. If you don't, the resulting graphs will be very
obviously warped vertically. Your other option is to try the -srgb option,
which will work in some limited cases (though even then, some graphs will be
clipped at the bottom).

ResampleScope only works with simple 1-dimensional ("separable") scaling
algorithms. All of the common algorithms (Lanczos, Mitchell, any kind of
"cubic" algorithm, etc.) are of this type.

ResampleScope only works with "nonadaptive" algorithms. If, for example, an
application runs a sharpening filter on the image after scaling it,
ResampleScope won't produce meaningful results.

By default, ResampleScope analyzes the algorithm that was used to scale the
image in the horizontal direction. To analyze the vertical dimension instead,
generate new pattern files by running "rscope -r -gen", and include the -r
option when you analyze the images.

Downscaling is harder to analyze than upscaling. ResampleScope works best if
you reduce the size as little as possible (but by at least 2 pixels). But
unfortunately, you can't trust that an application uses the same algorithm for
all scale factors, so sometimes you have to try some larger factors. Also,
some algorithms can only be distingished at larger scale factors.

If, for the "dots" pattern (pd.png), you get a picture that is very broken up
horizontally, you probably need to use the "-sf" or "-ff" option. This is
needed when the application scales the image *features* at a factor that is
not exactly the same as the scale factor of the image itself. The "dots"
pattern is extremely sensitive to this, and it requires the scale factor to be
known very precisely.

When using the "line" pattern (pl.png), ResampleScope prints an "area".
Normally, this should be very close to 1.0, but there are a number of (good
and bad) reasons that it might not be. The most common reason is that the
application scaled the image features at an unexpected scaling factor, so you
should use "-sf" or "-ff", as described in the preceding paragraph.

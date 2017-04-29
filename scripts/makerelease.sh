#!/bin/bash

VER="1.2"

if [ ! -f rscope.c ]
then
    echo "Run this script from the main directory"
    exit 1
fi

if [ -f rscope-${VER}.zip ]
then
	echo "Delete or move rscope-${VER}.zip first"
	exit 1
fi

rm -rf .build-tmp
mkdir .build-tmp

D=".build-tmp/rscope-${VER}"

mkdir $D

mkdir $D/x86
cp -p Release/rscope.exe $D/x86/

cp -p rscope.c readme.txt COPYING.txt Makefile $D/

mkdir $D/proj
mkdir $D/proj/vs2008
cp -p proj/vs2008/*.sln proj/vs2008/*.vcproj $D/proj/vs2008/

mkdir $D/scripts
cp -p scripts/* $D/scripts/


cp -p rscope.c readme.txt COPYING.txt Makefile $D/


#zip -9 rscope-${VER}.zip rscope.c readme.txt COPYING.txt Makefile
#zip -9 -j rscope-${VER}-win32.zip scripts/Release/rscope.exe

cd .build-tmp
zip -r ../rscope-${VER}.zip rscope-${VER}
cd ..

rm -rf .build-tmp


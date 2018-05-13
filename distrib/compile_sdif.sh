#!/bin/bash

# Compile Easdif from submodule
# and install everything in external/easdif directory.

if [ $# -lt 2 ]; then
    # Usage: compile_sdif.sh <SOURCEDIR> <DESTDIR>
    exit 1
fi

# Directory of the source code
SOURCEDIR="$1"
# Destination directory for build and install bath
DESTDIR="$2"

mkdir -p "$DESTDIR"
cd "$DESTDIR"

# Install path
mkdir -p easdif

if [ "$3" = "--osx" ]; then
  # In order to use libc++ and not libstdc++
  OSXOPTS=-DUSE_LLVM_STD:BOOL=ON
fi

# Build path
mkdir -p build
cd build
echo "SDIF: START CMAKE"
cmake $OSXOPTS -DSDIF_BUILD_STATIC:BOOL=ON -DEASDIF_BUILD_STATIC:BOOL=ON -DCMAKE_INSTALL_PREFIX_DEFAULTS_INIT:BOOL=ON -DCMAKE_INSTALL_PREFIX:STRING=$PWD/../easdif $SOURCEDIR
echo "SDIF: START MAKE"
make VERBOSE=1
#make
echo "SDIF: START MAKE INSTALL"
make install
ls -l $PWD/../easdif/*

touch easdif_buildfile

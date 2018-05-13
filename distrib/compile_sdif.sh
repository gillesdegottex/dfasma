#!/bin/bash

# Compile Easdif from submodule
# and install everything in external/easdif directory.

rm -fr external/sdif/easdif
cd external/sdif
# Install path
mkdir -p easdif

if [ "$1" = "--osx" ]; then
  # In order to use libc++ and not libstdc++
  OSXOPTS=-DUSE_LLVM_STD:BOOL=ON
fi

# Build path
mkdir build
cd build
echo $PWD/../easdif
cmake $OSXOPTS -DSDIF_BUILD_STATIC:BOOL=ON -DEASDIF_BUILD_STATIC:BOOL=ON -DCMAKE_INSTALL_PREFIX_DEFAULTS_INIT:BOOL=ON -DCMAKE_INSTALL_PREFIX:STRING=$PWD/../easdif ../EASDIF_SDIF
# make VERBOSE=1
make
make install
ls -l $PWD/../easdif/*

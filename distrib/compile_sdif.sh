#!/bin/bash

# Compile Easdif from the sourceforge sources
# and put everything in the external directory.

rm -fr external/sdif
mkdir -p external/sdif
cd external/sdif
# Install path
mkdir easdif

# Sources
#cvs -d:pserver:anonymous@sdif.cvs.sourceforge.net:/cvsroot/sdif login
cvs -z3 -d:pserver:anonymous@sdif.cvs.sourceforge.net:/cvsroot/sdif co -P EASDIF_SDIF

if [ "$1" = "--static" ]; then
  # Apply patch for installing static libraries
  cd EASDIF_SDIF
  patch -p0 < ../../../distrib/compile_sdif_install_static.diff
  cd ..
  SDIFSTATICOPT=-DEASDIF_BUILD_STATIC:BOOL=ON
fi

# Build path
mkdir build
cd build
echo $PWD/../easdif
cmake -DSDIF_BUILD_STATIC:BOOL=ON $SDIFSTATICOPT -DCMAKE_INSTALL_PREFIX_DEFAULTS_INIT:BOOL=ON -DCMAKE_INSTALL_PREFIX:STRING=$PWD/../easdif ../EASDIF_SDIF
make VERBOSE=1
make install
ls -l $PWD/../easdif

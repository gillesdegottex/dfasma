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

# Build path
mkdir build
cd build
echo $PWD/../easdif
cmake -DSDIF_BUILD_STATIC:BOOL=ON -DEASDIF_BUILD_STATIC:BOOL=ON -DCMAKE_INSTALL_PREFIX:STRING=$PWD/../easdif ../EASDIF_SDIF
make
make install
ls $PWD/../easdif

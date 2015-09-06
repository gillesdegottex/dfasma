#!/bin/bash

# This script downloads and compiles DFasma directly from GitHub.
# 
# It assumes that the development packages for Qt5 are installed on the system.
# It also assumes that the development packages of the requested libs are
# installed (e.g. fftw3, libsndfile, etc.)
# 
# Usage for downloading the very latest updates (risks of unstability!):
#   $ bash dfasma-compile-from-github.sh
# 
# Usage for downloading a specific version (using its tag name):
#   $ bash dfasma-compile-from-github.sh v1.0.0
# The tags beeing listed here: https://github.com/gillesdegottex/dfasma/tags
# 

# You might want to update this depending on your Qt installation
QMAKENAME=qmake

# For SDIF support (deactivate if it doesn't work for some reasons)
OPTIONS="CONFIG+=file_sdif"
# For SDIF support using Ircam's SDIF installation
#OPTIONS="CONFIG+=file_sdif INCLUDEPATH+=/u/formes/share/include LIBS+=-L/u/formes/share/lib/x86_64-Linux-rh65"

# ------------------------------------------------------------------------------

printf "\033[0;31mCompiling DFasma from GitHub\033[0m\n"

printf "\033[0;31mUsing: $QMAKENAME\033[0m\n"

if [ -n "$OPTIONS" ]; then
    printf "\033[0;31mOPTIONS=$OPTIONS\033[0m\n"
fi

# Setup the working directory
DFASMACOMPDIR=dfasma_from_github
if [ -n "$1" ]; then
    DFASMACOMPDIR=$DFASMACOMPDIR-$1
fi

# Clean everything
rm -fr $DFASMACOMPDIR

# Pull the repo
git clone git://github.com/gillesdegottex/dfasma $DFASMACOMPDIR
cd $DFASMACOMPDIR

if [ $OPTIONS == *"file_sdif"* ]; then
    if [[ $OPTIONS != *"/u/formes/share"* ]]; then
        bash distrib/compile_sdif.sh
        OPTIONS="$OPTIONS FILE_SDIF_LIBDIR=external/sdif"
    fi
fi

# If asked, move to the requested tag
if [ -n "$1" ]; then
    printf "\033[0;31mReseting at $1\033[0m\n"
    git reset --hard $1
fi

# Pull the submodules
git submodule update --init --recursive

# Ensure that the cache system is not bothering us
touch cache.tmp
# Generate the Makefile
$QMAKENAME -makefile -cache cache.tmp $OPTIONS dfasma.pro

# Compile...
make

# ./dfasma

#!/bin/bash

DFASMA_VERSION=`git describe --tags --always`

echo "// This file is generated by versions.h" > versions.h
echo "#define DFASMA_VERSION \"$DFASMA_VERSION\"" >> versions.h

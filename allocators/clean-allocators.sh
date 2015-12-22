#!/bin/bash

MAKE_OPTIONS="--quiet"

# Hoard
cd hoard-20151222/src
make clean ${MAKE_OPTIONS} 2>&1 > /dev/null
cd ../..

# TCMalloc
cd gperftools-20151222
make clean ${MAKE_OPTIONS} 2>&1 > /dev/null
make distclean ${MAKE_OPTIONS} 2>&1 > /dev/null
rm -rf build
cd ..

# TBBMalloc
cd tbb44_20151115oss
make clean ${MAKE_OPTIONS} 2>&1 > /dev/null
cd ..

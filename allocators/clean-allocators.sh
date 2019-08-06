#!/bin/bash

MAKE_OPTIONS="--quiet"

# IBMmalloc
cd IBMmalloc
make clean ${MAKE_OPTIONS} 2>&1 > /dev/null
cd ..

# Hoard
cd hoard/src
make clean ${MAKE_OPTIONS} 2>&1 > /dev/null
cd ../..

# TCMalloc
cd gperftools
make clean ${MAKE_OPTIONS} 2>&1 > /dev/null
make mostlyclean ${MAKE_OPTIONS} 2>&1 > /dev/null
make distclean ${MAKE_OPTIONS} 2>&1 > /dev/null
rm -rf build $(cat .gitignore | sed 's|^|./|g')
git checkout src/windows/gperftools/tcmalloc.h
cd ..

# TBBMalloc
cd tbb44_20151115oss
make clean ${MAKE_OPTIONS} 2>&1 > /dev/null
cd ..

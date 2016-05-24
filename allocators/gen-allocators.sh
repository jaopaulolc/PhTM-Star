#!/bin/bash

MAKE_OPTIONS="--quiet"

./clean-allocators.sh

# IBMmalloc
echo "Compiling IBMmalloc..."
cd IBMmalloc
make ${MAKE_OPTIONS} 2>&1 > /dev/null
if [ $? -eq 0 ]; then
	echo "IBMmalloc compilation succeded!"
else
	echo "IBMmalloc compilation failed!"
fi
cd ..


# Hoard
echo "Compiling Hoard..."
cd hoard-20151222/src
make Linux-gcc-unknown $MAKE_OPTIONS 2>&1 > /dev/null
if [ $? -eq 0 ]; then
	echo "Hoard compilation succeded!"
else
	echo "Hoard compilation failed!"
fi
cd ../..


# TCMalloc
echo "Compiling TCMalloc..."
cd gperftools
./autogen.sh 2>&1 > /dev/null
./configure --prefix=$PWD/build --enable-minimal \
						--enable-shared --enable-libunwind 2>&1 > /dev/null
make install $MAKE_OPTIONS 2>&1 > /dev/null
if [ $? -eq 0 ]; then
	echo "TCMalloc compilation succeded!"
else
	echo "TCMalloc compilation failed!"
fi
cd ..


# TBBMalloc
echo "Compiling TBBMalloc..."
cd tbb44_20151115oss
make tbbmalloc $MAKE_OPTIONS 2>&1 > /dev/null
if [ $? -eq 0 ]; then
	cp build/linux_*_gcc_*_release/libtbbmalloc*.so.2 .
	echo "TBBMalloc compilation succeded!"
else
	echo "TBBMalloc compilation failed!"
fi
cd ..

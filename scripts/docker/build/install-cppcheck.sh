#!/bin/sh
set -e

export CPPCHECK_VERSION=$1
export PATCHED_VERSION=$2

# Compile cppcheck:
cd /tmp
wget https://github.com/danmar/cppcheck/archive/$CPPCHECK_VERSION.tar.gz 
tar xzf $CPPCHECK_VERSION.tar.gz 
cd cppcheck-$CPPCHECK_VERSION/ 
sed -i "s/\"$CPPCHECK_VERSION\"/\"$PATCHED_VERSION\"/" cmake/versions.cmake  
cmake . 
make -j4 FILESDIR=/usr install 

cd /tmp
wget https://bootstrap.pypa.io/get-pip.py
python3 get-pip.py
pip install cppcheck-junit
	    
cd /tmp
rm -rf $CPPCHECK_VERSION.tar.gz cppcheck-$CPPCHECK_VERSION get-pip.py


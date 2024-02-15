#!/bin/bash

# Set links for GCC
update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-${COMPILER_VERSION} 10 \
	--slave /usr/bin/gcc-ar gcc-ar /usr/bin/gcc-ar-${COMPILER_VERSION} \
  --slave /usr/bin/gcc-nm gcc-nm /usr/bin/gcc-nm-${COMPILER_VERSION} \
  --slave /usr/bin/gcc-ranlib gcc-ranlib /usr/bin/gcc-ranlib-${COMPILER_VERSION} \
  --slave /usr/bin/gcov gcov /usr/bin/gcov-${COMPILER_VERSION}

update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-${COMPILER_VERSION} 10

update-alternatives --install /usr/bin/cc cc /usr/bin/gcc 30
update-alternatives --set cc /usr/bin/gcc

update-alternatives --install /usr/bin/c++ c++ /usr/bin/g++ 30
update-alternatives --set c++ /usr/bin/g++

# Compile openssl library:
export OPENSSLBRANCH=$1
export OPENSSLPATCH=$2
export OPENSSLVERSION="${OPENSSLBRANCH}.${OPENSSLPATCH}"

if [ "$OPENSSLBRANCH" != "3.2" ]; then
  OLD="old/${OPENSSLBRANCH}/"
fi;

export OPENSSLPATH=`echo $OPENSSLVERSION | sed 's/\.[0-9]*$//g'`
[ "$ARCH" = "x86_64" -a ${OPENSSLPATH:0:1} = "3" ] && X86_64_SUFFIX=64

cd /tmp
curl -O https://www.openssl.org/source/openssl-$OPENSSLVERSION.tar.gz
tar xzf openssl-$OPENSSLVERSION.tar.gz
cd openssl-$OPENSSLVERSION
./config --prefix=/opt no-async no-dso
make -j64
make install_dev
cd /tmp
rm -rf openssl-$OPENSSLVERSION.tar.gz openssl-$OPENSSLVERSION

# Clean up any strange cores
rm -rf /core.*

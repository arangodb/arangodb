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

if [ "$OPENSSLBRANCH" != "3.3" ]; then
  OLD="old/${OPENSSLBRANCH}/"
fi;

export OPENSSLPATH=`echo $OPENSSLVERSION | sed 's/\.[0-9]*$//g'`
[ "$ARCH" = "x86_64" -a ${OPENSSLPATH:0:1} = "3" ] && X86_64_SUFFIX=64

cd /tmp
TARBALL=openssl-$OPENSSLVERSION.tar.gz
curl -L --output $TARBALL https://www.openssl.org/source/$TARBALL
tar xzf $TARBALL
cd openssl-$OPENSSLVERSION
./config --prefix=/opt no-async no-dso
make -j$(nproc)  || exit 1
make install_dev
cd /tmp
rm -rf openssl-$OPENSSLVERSION.tar.gz openssl-$OPENSSLVERSION

# Compile openldap library:
export OPENLDAPVERSION=2.6.10
cd /tmp
curl -O ftp://ftp.openldap.org/pub/OpenLDAP/openldap-release/openldap-$OPENLDAPVERSION.tgz
tar xzf openldap-$OPENLDAPVERSION.tgz
cd openldap-$OPENLDAPVERSION
CPPFLAGS=-I/opt/include \
LDFLAGS="-L/opt/lib$X86_64_SUFFIX" \
./configure -prefix=/opt --enable-static
make depend && make -j$(nproc) || exit 1
make install
cd /tmp
rm -rf openldap-$OPENLDAPVERSION.tgz openldap-$OPENLDAPVERSION

# Clean up any strange cores
rm -rf /core.*

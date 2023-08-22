#!/bin/sh
set -e

export OPENSSLVERSION=$1
test -n "$OPENSSLVERSION"
export OPENSSLPATH=`echo $OPENSSLVERSION | sed 's/\([a-zA-Z]$\|\.[0-9]$\)//g'`

set -- /opt/openssl-$OPENSSLPATH/lib*
export OPENSSLLIBPATH=$1

# Compile openldap library:
export OPENLDAPVERSION=2.6.6
cd /tmp
curl -O ftp://ftp.openldap.org/pub/OpenLDAP/openldap-release/openldap-$OPENLDAPVERSION.tgz
tar xzf openldap-$OPENLDAPVERSION.tgz
cd openldap-$OPENLDAPVERSION
# cp -a /tools/config.* ./build
CPPFLAGS=-I/opt/openssl-$OPENSSLPATH/include \
LDFLAGS=-L$OPENSSLLIBPATH \
  ./configure --prefix=/opt/openssl-$OPENSSLPATH --with-threads --with-tls=openssl --enable-static --disable-shared
make depend && make -j `nproc`
make install
cd /tmp
rm -rf openldap-$OPENLDAPVERSION.tgz openldap-$OPENLDAPVERSION


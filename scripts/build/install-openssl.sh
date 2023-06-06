#!/bin/sh
set -e

# Compile openssl library:
export OPENSSLBRANCH=$1
export OPENSSLREVISION=$2
export OPENSSLVERSION="${OPENSSLBRANCH}${OPENSSLREVISION}"

echo $OPENSSLBRANCH


if [ "$OPENSSLBRANCH" != "1.1.1" -a "$OPENSSLBRANCH" != "3.0" -a "$OPENSSLBRANCH" != "3.1" ]; then
  OLD="old/${OPENSSLBRANCH}/"
fi;

echo "https://www.openssl.org/source/${OLD}openssl-$OPENSSLVERSION.tar.gz"

test -n "$OPENSSLVERSION"
export OPENSSLPATH=`echo $OPENSSLVERSION | sed 's/\([a-zA-Z]$\|\.[0-9]$\)//g'`
cd /tmp
curl -O https://www.openssl.org/source/${OLD}openssl-$OPENSSLVERSION.tar.gz
tar xzvf openssl-$OPENSSLVERSION.tar.gz
cd openssl-$OPENSSLVERSION
./config --prefix=/opt/openssl-$OPENSSLPATH no-async no-shared
make build_libs -j `nrpoc`
make install_dev -j `nrpoc`
cd /tmp
rm -rf openssl-$OPENSSLVERSION.tar.gz openssl-$OPENSSLVERSION

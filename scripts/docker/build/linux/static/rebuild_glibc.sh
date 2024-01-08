#!/bin/sh
cd /root
apt-get source glibc
cd glibc-*
patch -u debian/rules.d/build.mk /static/enable_static_nss.pat
export EMAIL="Max Neunhoeffer <max@arangodb.com>"
debchange -n "Switched on --enable-static-nss=yes"
debchange -r ignored
export DEB_BUILD_OPTIONS=nocheck
debian/rules -j 64 binary
cd /root
mkdir debs
mv libc*.deb debs
mv locales*.deb debs
mv nscd*.deb debs

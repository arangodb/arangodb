#!/bin/sh
set -e
cd /root
apt-get source glibc
cd glibc-*
patch -u debian/rules.d/build.mk /static/enable_static_nss.pat
export EMAIL="Max Neunhoeffer <max@arango.ai>"
debchange -n "Switched on --enable-static-nss=yes"
debchange -r ignored
# No -j for debian/rules itself: its top-level targets are not parallel-safe
# (26.04: build_locales-all depends on "$(stamp)/build_libc", which no rule
# matches). parallel= runs each glibc build pass with that many jobs instead.
export DEB_BUILD_OPTIONS="nocheck parallel=$(nproc)"
debian/rules binary
cd /root
mkdir debs
mv libc*.deb debs
mv locales*.deb debs
mv nscd*.deb debs

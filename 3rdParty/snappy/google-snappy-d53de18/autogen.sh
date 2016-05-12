#! /bin/sh -e
rm -rf autom4te.cache
aclocal -I m4
autoheader
if glibtoolize --version >/dev/null 2>/dev/null; then
  LIBTOOLIZE=${LIBTOOLIZE:-glibtoolize}
else
  LIBTOOLIZE=${LIBTOOLIZE:-libtoolize}
fi
$LIBTOOLIZE --copy
automake --add-missing --copy
autoconf

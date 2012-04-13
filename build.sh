#!/bin/bash

echo "########################################################"
echo "#                     build.sh                         #"
echo "########################################################"

. config/detect_distro.sh

OPTIONS="--disable-dependency-tracking --disable-relative"
PREFIX="--prefix=/usr --sysconfdir=/etc"
RESULTS="avocado avocsh"

export CPPFLAGS=""
export LDFLAGS=""
export MAKEJ=2
export LDD_INFO="no"
export VALGRIND_TEST="no"

echo
echo "########################################################"
echo "Building on $TRI_OS_LONG"
echo "########################################################"
echo

case $TRI_OS_LONG in

  Linux-openSUSE-11.4*)
    echo "Using configuration for openSuSE 11.4"
    OPTIONS="$OPTIONS --disable-all-in-one --with-boost-test"
    LDD_INFO="yes"
    ;;

  Linux-openSUSE-11*)
    echo "Using configuration for openSuSE 11"
    OPTIONS="$OPTIONS --enable-all-in-one"
    VALGRIND_TEST="yes"
    LDD_INFO="yes"
    ;;

  Linux-Debian-6*)
    echo "Using configuration for Debian"
    OPTIONS="$OPTIONS --enable-all-in-one"
    LDD_INFO="yes"
    ;;

  Linux-Debian*)
    echo "Using configuration for Debian"
    OPTIONS="$OPTIONS --enable-all-in-one"
    LDD_INFO="yes"
    ;;

  Linux-CentOS-*)
    echo "Using configuration for Centos"
    LDD_INFO="yes"
    ;;

  Linux-Ubuntu-11.10*)
    echo "Using configuration for Ubuntu"
    OPTIONS="$OPTIONS --enable-all-in-one"
    LDD_INFO="yes"
    ;;

  Linux-Ubuntu-*)
    echo "Using configuration for Ubuntu"
    OPTIONS="$OPTIONS --enable-all-in-one"
    LDD_INFO="yes"
    ;;

  Darwin*)
    echo "Using configuration for DARWIN"
    CPPFLAGS='-isystem /usr/include -isystem /opt/local/include -Wno-deprecated-declarations'
    LDFLAGS='-L/usr/lib -L/opt/local/lib' # need to use OpenSSL from system
    OPTIONS="$OPTIONS --enable-all-in-one"
    ;;

  *)
    echo "Using default configuration"
    OPTIONS="$OPTIONS --enable-error-on-warning"
    ;;

esac

echo
echo "########################################################"
echo "CPPFLAGS: $CPPFLAGS"
echo "LDFLAGS: $LDFLAGS"
echo "OPTIONS: $OPTIONS"
echo "########################################################"
echo
echo "########################################################"
echo "init system:"
echo "   make setup"
echo "########################################################"
echo

make setup || exit 1

echo
echo "########################################################"
echo "configure:"
echo "   ./configure $PREFIX $OPTIONS"
echo "########################################################"
echo

./configure $PREFIX $OPTIONS || exit 1

echo
echo "########################################################"
echo "compile:"
echo "    make $TARGETS"
echo "########################################################"
echo

make -j $MAKEJ || exit 1

for result in $RESULTS;  do
  echo
  echo "########################################################"
  echo "$result"
  echo "########################################################"
  echo

  ident $result

  if test "x$LDD_INFO" = "xyes";  then
    echo
    ldd $result
    echo
  fi
done

echo

echo
echo "########################################################"
echo "unittests:"
echo "    make unittests"
echo "########################################################"
echo

make unittests-boost || exit 1
make unittests-shell-server || exit 1
make unittests-http-server || exit 1
make unittests-shell-client || exit 1

if test "x$VALGRIND_TEST" = "xyes";  then
  echo
  echo "########################################################"
  echo "unittests with VALGRIND:"
  echo "    make unittests VALGRIND=valgrind --leak-check=full"
  echo "########################################################"
  echo

  make unittests VALGRIND="valgrind --leack-check=full" || exit 1
fi

#!/bin/bash

echo "########################################################"
echo "#                     build.sh                         #"
echo "########################################################"
echo

. config/detect_distro.sh

OPTIONS="--disable-dependency-tracking --disable-relative"
PREFIX="--prefix=/usr --sysconfdir=/etc"
RESULTS="arangod arangosh arangoimp"

export CPPFLAGS=""
export LDFLAGS=""
export MAKEJ=2
export LDD_INFO="no"

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
    RESULTS="$RESULTS arangoirb"
    ;;

  Linux-openSUSE-11*)
    echo "Using configuration for openSuSE 11"
    OPTIONS="$OPTIONS --enable-all-in-one"
    LDD_INFO="yes"
    RESULTS="$RESULTS arangoirb"
    ;;

  Linux-Debian-6*)
    echo "Using configuration for Debian"
    OPTIONS="$OPTIONS --enable-all-in-one"
    LDD_INFO="yes"
    RESULTS="$RESULTS arangoirb"
    ;;

  Linux-Debian*)
    echo "Using configuration for Debian"
    OPTIONS="$OPTIONS --enable-all-in-one --disable-mruby"
    LDD_INFO="yes"
    ;;

  Linux-CentOS-*)
    echo "Using configuration for Centos"
    OPTIONS="$OPTIONS --enable-all-in-one --disable-mruby"
    LDD_INFO="yes"
    ;;

  Linux-Ubuntu-11.10*)
    echo "Using configuration for Ubuntu"
    OPTIONS="$OPTIONS --enable-all-in-one"
    LDD_INFO="yes"
    RESULTS="$RESULTS arangoirb"
    ;;

  Linux-Ubuntu-*)
    echo "Using configuration for Ubuntu"
    OPTIONS="$OPTIONS --enable-all-in-one"
    LDD_INFO="yes"
    RESULTS="$RESULTS arangoirb"
    ;;

  Darwin*)
    echo "Using configuration for DARWIN"
    CPPFLAGS='-isystem /usr/include -isystem /opt/local/include -Wno-deprecated-declarations'
    LDFLAGS='-L/usr/lib -L/opt/local/lib' # need to use OpenSSL from system
    OPTIONS="$OPTIONS --enable-all-in-one"
    RESULTS="$RESULTS arangoirb"
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

#!/bin/bash

echo "########################################################"
echo "#                     build.sh                         #"
echo "########################################################"

. config/detect_distro.sh

ARCH=""
OPTIONS=""
PREFIX="--prefix=/usr --sysconfdir=/etc"
RESULTS="avocado"

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
    STATIC=""
    LDD_INFO="yes"
    ;;

  Linux-openSUSE-11*)
    echo "Using configuration for openSuSE 11"
    STATIC="--enable-static-libev"
    LDD_INFO="yes"
    ;;

  Linux-Debian-6*)
    echo "Using configuration for Debian"
    STATIC="--enable-static-libev --enable-static-boost --enable-static-mysql"
    LDD_INFO="yes"
    ;;

  Linux-Debian*)
    echo "Using configuration for Debian"
    STATIC="--enable-static-libev --enable-static-boost --enable-static-mysql"
    LDD_INFO="yes"
    ;;

  Linux-CentOS-*)
    echo "Using configuration for Centos"
    LDD_INFO="yes"
    ;;

  Linux-Ubuntu-11.10*)
    echo "Using configuration for Ubuntu"
    STATIC="--enable-static-libev --enable-static-boost --enable-static-mysql"
    LDD_INFO="yes"
    ;;

  Linux-Ubuntu-*)
    echo "Using configuration for Ubuntu"
    STATIC="--enable-static-libev --enable-static-boost --enable-static-mysql"
    LDD_INFO="yes"
    ;;

  Darwin*)
    echo "Using configuration for DARWIN"
    ARCH="32 64"
    CPPFLAGS='-isystem /usr/include -isystem /opt/local/include -Wno-deprecated-declarations'
    LDFLAGS='-L/usr/lib -L/opt/local/lib' # need to use OpenSSL from system
    STATIC="--enable-static-boost --enable-static-mysql"
    ;;

  *)
    echo "Using default configuration"
    OPTIONS="--enable-error-on-warning"
    ;;

esac

echo
echo "########################################################"
echo "ARCH: $ARCH"
echo "CPPFLAGS: $CPPFLAGS"
echo "LDFLAGS: $LDFLAGS"
echo "STATIC: $STATIC"
echo "OPTIONS: $OPTIONS"
echo "########################################################"
echo
echo "########################################################"
echo "init system:"
echo "   make setup"
echo "########################################################"
echo

make setup || exit 1

TARGETS=""

if test "x$ARCH" = "x";  then

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

  make -j $MAKEJ $TARGETS || exit 1

else
  for arch in $ARCH;  do
    CONF_FLAGS=""

    if test "x$arch" = x32;  then
      CONF_FLAGS="--enable-32bit"
    fi

    echo
    echo "########################################################"
    echo "configure architekture $arch"
    echo "   rm -rf ARCH.$arch ; mkdir ARCH.$arch ; cd ARCH.$arch"
    echo "   ../configure $PREFIX $OPTIONS $CONF_FLAGS\""
    echo "########################################################"
    echo

    rm -rf ARCH.$arch
    mkdir ARCH.$arch

    cd ARCH.$arch 

    ../configure \
        $PREFIX \
        $OPTIONS \
        $CONF_FLAGS || exit 1

    echo
    echo "########################################################"
    echo "compile: "
    echo "    make -j $MAKEJ $TARGETS "
    echo "########################################################"
    echo

    make -j $MAKEJ $TARGETS || exit 1

    cd ..
  done

  for result in $RESULTS;  do
    lipo=""

    for arch in $ARCH;  do
      lipo="$lipo ARCH.$arch/$result"
    done

    lipo $lipo -create -output $result || exit 1
  done
fi

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

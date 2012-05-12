#!/bin/bash

echo "########################################################"
echo "#                     build.sh                         #"
echo "########################################################"

. config/detect_distro.sh

OPTIONS="--disable-dependency-tracking --disable-relative --enable-gcov"
PREFIX="--prefix=/usr --sysconfdir=/etc"
RESULTS="arango avocsh"

export CPPFLAGS=""
export LDFLAGS=""
export MAKEJ="-j 2"
export LDD_INFO="no"

echo
echo "########################################################"
echo "Building on $TRI_OS_LONG"
echo "########################################################"
echo

case $TRI_OS_LONG in

  Linux-openSUSE-11.4*)
    echo "Using configuration for openSuSE 11.4"
    OPTIONS="$OPTIONS --enable-all-in-one"
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
echo "    make $MAKEJ"
echo "########################################################"
echo

make $MAKEJ || exit 1

echo
echo "########################################################"
echo "unittests:"
echo "    make unittests"
echo "########################################################"
echo

make unittests FORCE=1 || exit 1

echo
echo "########################################################"
echo "lcov:"
echo "########################################################"
echo

rm -rf COVERAGE
mkdir COVERAGE
mkdir COVERAGE/html

current=`pwd`
vprj_folder=`basename $current`
lcov_folder="${vprj_folder}/COVERAGE/html"
c_info_file="${vprj_folder}/COVERAGE/coverage.lcov"
r_info_file="${vprj_folder}/COVERAGE/reduced.lcov"

(
  cd ..
  lcov -b ${vprj_folder} -c -d ${vprj_folder} -o ${c_info_file}.1 
  lcov --remove ${c_info_file}.1 "*3rdParty*" -o ${c_info_file}.2
  rm ${c_info_file}.1
  lcov --remove ${c_info_file}.2 "*UnitTests*" -o ${c_info_file}
  rm ${c_info_file}.2
  lcov --extract ${c_info_file} "*${vprj_folder}*" -o ${r_info_file}
  genhtml -o ${lcov_folder} ${r_info_file}
)

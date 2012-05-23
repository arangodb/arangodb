#!/bin/bash

echo "########################################################"
echo "#                     build.sh                         #"
echo "########################################################"
echo

. config/detect_distro.sh

export VALGRIND_TEST="no"
export RSPEC_AVAILABLE="yes"

echo
echo "########################################################"
echo "Building on $TRI_OS_LONG"
echo "########################################################"
echo

case $TRI_OS_LONG in

  Linux-openSUSE-11.4*)
    echo "Using configuration for openSuSE 11.4"
    ;;

  Linux-openSUSE-11*)
    echo "Using configuration for openSuSE 11"
    VALGRIND_TEST="yes"
    ;;

  Linux-Debian-6*)
    echo "Using configuration for Debian"
    ;;

  Linux-Debian*)
    echo "Using configuration for Debian"
    RSPEC_AVAILABLE="no"
    ;;

  Linux-CentOS-*)
    echo "Using configuration for Centos"
    RSPEC_AVAILABLE="no"
    ;;

  Linux-Ubuntu-11.10*)
    echo "Using configuration for Ubuntu"
    ;;

  Linux-Ubuntu-*)
    echo "Using configuration for Ubuntu"
    ;;

  Darwin*)
    echo "Using configuration for DARWIN"
    ;;

  *)
    echo "Using default configuration"
    ;;

esac

while [ $? -ne 0 ]; do
  if [ "$1" == "--valgrind" ];  then
    VALGRIND_TEST="yes"
  elif [ "$1" == "--no-valgrind" ];  then
    VALGRIND_TEST="no"
  elif [ "$1" == "--resc" ];  then
    RSPEC_AVAILABLE="yes"
  elif [ "$1" == "--no-resc" ];  then
    RSPEC_AVAILABLE="no"
  fi

  shift
done

echo
echo "########################################################"
echo "unittests:"
echo "    make unittests"
echo "########################################################"
echo

echo "VALGRIND: $VALGRIND_TEST"
echo "RSPEC: $RSPEC_AVAILABLE"

make unittests-boost || exit 1
make unittests-shell-server || exit 1
make unittests-shell-server-ahuacatl || exit 1

if test "x$RSPEC_AVAILABLE" = "xyes";  then
  make unittests-http-server || exit 1
fi

make unittests-shell-client || exit 1

if test "x$VALGRIND_TEST" = "xyes";  then
  echo
  echo "########################################################"
  echo "unittests with VALGRIND:"
  echo "    make unittests VALGRIND=valgrind --leak-check=full"
  echo "########################################################"
  echo

  make unittests VALGRIND="valgrind --suppressions=RestServer/arango.supp --leak-check=full" || exit 1
fi

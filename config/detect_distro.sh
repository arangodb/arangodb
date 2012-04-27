#!/bin/sh
# Detects which OS and if it is Linux then it will detect which Linux Distribution.

echo "........................................................"
echo ".            Detect OS and Distribution                ."
echo "........................................................"
echo

OS=`uname -s`
REV=`uname -r`
MACH=`uname -m`

if [ "${OS}" = "Darwin" ] ; then
  DIST="MacOSX"
  RELEASE=$(sw_vers | grep "ProductVersion" | awk '{ print $2}' )
  KERNEL=$(uname -v | awk '{ print $11}')
  CODENAME="Darwin"

elif [ "${OS}" = "Linux" ] ; then
  KERNEL=`uname -r`
  
  # use "lsb_release"
  DIST=$(lsb_release -d 2>/dev/null| awk '{ print $2 }')
  RELEASE=$(lsb_release -r 2>/dev/null | awk '{ print $2 }')
  CODENAME=$(lsb_release -c 2>/dev/null | awk '{ print $2 }')

  if [ "x${DIST}" = "x" ] ; then
    # use special files:

    if [ -f /etc/redhat-release ] ; then
      DIST=$( cat /etc/redhat-release | awk '{ print $1}' )
      RELEASE=`cat /etc/redhat-release | sed s/.*release\ // | sed s/\ .*//`
      CODENAME=`cat /etc/redhat-release | sed s/.*\(// | sed s/\)//`

    elif [ -f /etc/SuSE-release ] ; then
      DIST=$(cat /etc/SuSE-release | tr "\n" ' '| awk '{ print $1}')
      RELEASE=$(cat /etc/SuSE-release | tr "\n" ' ' | sed s/.*VERSION\ =\ // | awk '{ print $1}')
      CODENAME=$(cat /etc/SuSE-release | tr "\n" ' ' | sed s/.*CODENAME\ =\ // | awk '{ print $1}')

    elif [ -f /etc/mandrake-release ] ; then
      DIST='Mandrake'
      RELEASE=`cat /etc/mandrake-release | sed s/.*release\ // | sed s/\ .*//`
      CODENAME=`cat /etc/mandrake-release | sed s/.*\(// | sed s/\)//`

    elif [ -f /etc/debian_version ] ; then
      DIST="Debian"
      RELEASE=`cat /etc/debian_version`
    fi
  fi

  if [ "x${CODENAME}" = "x" ] ; then
    CODENAME="unknown"
  fi

fi

OSSTR="${OS} ${DIST} ${RELEASE} ${MACH} (${CODENAME} ${KERNEL})"

echo ${OSSTR}
echo "........................................................"
echo "TRI_OS=\"${OS}\""
echo "TRI_DIST=\"${DIST}\""
echo "TRI_RELEASE=\"${RELEASE}\""
echo "TRI_MACH=\"${MACH}\""
echo "TRI_CODENAME=\"${CODENAME}\""
echo "TRI_KERNEL=\"${KERNEL}\""
echo "TRI_OS_LONG=\"${OS}-${DIST}-${RELEASE}-${MACH}\""

TRI_OS=${OS}
TRI_DIST=${DIST}
TRI_RELEASE=${RELEASE}
TRI_MACH=${MACH}
TRI_CODENAME=${CODENAME}
TRI_KERNEL=${KERNEL}
TRI_OSSTR=${OSSTR}
TRI_OS_LONG="${OS}-${DIST}-${RELEASE}-${MACH}"

echo "........................................................"

#!/bin/bash

NSIS_PATH="/cygdrive/c/Program Files (x86)/NSIS"
#shell parameter:
#1 the bits (64 or 32)
#2 the parent directory which contains the Build64 or Build32 directory
BUILD=$2
bits=$1

# Yo windows we like year backslash paths.
WD=`pwd`
SRCPATH=`cygpath -w ${WD}|sed "s;\\\\\;\\\\\\\\\\\\\;g"`

INSTALLERNAME=`grep CPACK_PACKAGE_FILE_NAME ${BUILD}/CPackConfig.cmake | grep -o '".*"' | awk -F\" '{print $2}'`

if [ ! -f ${BUILD}/$INSTALLERNAME-internal.exe ]; then
  cp ${BUILD}/$INSTALLERNAME.exe ${BUILD}/$INSTALLERNAME-internal.exe 
fi
cat Installation/Windows/Templates/arango-packer-template.nsi | \
    sed -e "s;@BITS@;$bits;g" \
        -e  "s;@INSTALLERNAME@;${INSTALLERNAME};" \
        -e "s;@SRCDIR@;${SRCPATH};g" > ${BUILD}/$INSTALLERNAME.nsi

"$NSIS_PATH"/makensis.exe ${BUILD}\\$INSTALLERNAME.nsi


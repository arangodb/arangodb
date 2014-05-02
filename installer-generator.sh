#!/bin/bash

NSIS_PATH="/cygdrive/c/Program Files (x86)/NSIS"

bits=$1
INSTALLERNAME=`grep CPACK_PACKAGE_FILE_NAME Build$bits/CPackConfig.cmake | grep -o '".*"' | awk -F\" '{print $2}'`
if [ ! -f Build$bits/$INSTALLERNAME-internal.exe ]; then
  cp Build$bits/$INSTALLERNAME.exe Build$bits/$INSTALLERNAME-internal.exe 
fi
cat Installation/Windows/Templates/arango-packer-template.nsi | sed -e "s/@BITS@/$bits/g" | sed -e  "s/@INSTALLERNAME@/${INSTALLERNAME}/g" > Build$bits/$INSTALLERNAME.nsi

"$NSIS_PATH"/makensis.exe   Build$bits/$INSTALLERNAME.nsi


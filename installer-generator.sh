#!/bin/bash

NSIS_PATH="/cygdrive/c/Program Files (x86)/NSIS"

bits=$1
INSTALLERNAME=`grep CPACK_PACKAGE_FILE_NAME Build$bits/CPackConfig.cmake | grep -o '".*"' | awk -F\" '{print $2}'`
cp Build$bits/$INSTALLERNAME.exe Build$bits/$INSTALLERNAME-internal.exe 
cat Installation/Windows/Templates/arango-packer-template.nsi | sed -e "s/@BITS@/$bits/g" | sed -e  "s/@INSTALLERNAME@/${INSTALLERNAME}/g" > Build$bits/$INSTALLERNAME.nsi

"$NSIS_PATH"/bin/makensis.exe   Build$bits/$INSTALLERNAME.nsi


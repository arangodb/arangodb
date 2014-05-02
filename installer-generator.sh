#!/bin/bash

NSIS_PATH="/cygdrive/c/Program Files (x86)/NSIS"

bits=$1
INSTALLERNAME=`grep CPACK_PACKAGE_FILE_NAME Build$bits/CPackConfig.cmake | grep -o '".*"' | awk -F\" '{print $2}'`
cat Installation/Windows/Templates/arango-packer-template.nsi | sed -e "s/@BITS@/$bits/g" | sed -e  "s/@INSTALLERNAME@/${INSTALLERNAME}/g" > Build$bits/arango-packer-$bits.nsi

"$NSIS_PATH"/bin/makensis.exe   Build$bits/arango-packer-$bits.nsi


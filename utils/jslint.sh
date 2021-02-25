#!/bin/sh
# Limitation - This script can not work with filenames containing spaces

WD=$(pwd)
if [ -z "$*" ] ; then
  JAVASCRIPT_JSLINT="\
    $(find "${WD}/js/actions" -name "*.js") \
    $(find "${WD}/js/common/bootstrap" -name "*.js") \
    $(find "${WD}/js/client/bootstrap" -name "*.js") \
    $(find "${WD}/js/server/bootstrap" -name "*.js") \
    \
    $(find "${WD}/js/common/modules/@arangodb" -name "*.js") \
    $(find "${WD}/js/client/modules/@arangodb" -name "*.js") \
    $(find "${WD}/js/server/modules/@arangodb" -name "*.js") \
    $(find "${WD}/tests/js/server" -name "*.js" | grep -v "ranges-combined") \
    $(find "${WD}/tests/js/common" -name "*.js" | grep -v "test-data") \
    $(find "${WD}/tests/js/client" -name "*.js") \
    $(find "${WD}/UnitTests" -name "*.js") \
    \
    $(find "${WD}/js/apps/system/_admin/aardvark/APP/frontend/js/models" -name "*.js") \
    $(find "${WD}/js/apps/system/_admin/aardvark/APP/frontend/js/views" -name "*.js") \
    $(find "${WD}/js/apps/system/_admin/aardvark/APP/frontend/js/collections" -name "*.js") \
    $(find "${WD}/js/apps/system/_admin/aardvark/APP/frontend/js/routers" -name "*.js") \
    $(find "${WD}/js/apps/system/_admin/aardvark/APP/frontend/js/arango" -name "*.js") \
    \
    $(find "${WD}/scripts" -name "*.js") \
    \
    ${WD}/js/common/modules/jsunity.js \
    ${WD}/js/client/client.js \
    ${WD}/js/client/inspector.js \
    ${WD}/js/server/server.js \
    ${WD}/js/server/initialize.js \
    \
  "
  if [ -d "${WD}/enterprise" ] ; then
    echo Considering enterprise files...
    JAVASCRIPT_JSLINT="$JAVASCRIPT_JSLINT \
      $(find "${WD}/enterprise/js" -name "*.js") \
      $(find "${WD}/enterprise/tests/js" -name "*.js") \
      "
  fi
else
  JAVASCRIPT_JSLINT="$*"
fi

FILELIST=""
for file in ${JAVASCRIPT_JSLINT}; do
  FILELIST="${FILELIST} --jslint ${file}";
done

JSDIR="./js"
if [ -z "${ARANGOSH}" ];  then
  if [ -x build/bin/arangosh ];  then
    ARANGOSH=build/bin/arangosh
  elif [ -x bin/arangosh ];  then
    ARANGOSH=bin/arangosh
  else
    echo "$0: cannot locate arangosh"
  fi
else
    if [ "${ARANGOSH}" = "/usr/bin/arangosh" ]; then
        # system arangosh? give it its own javascript dir, install jslint to it.
        JSDIR=$(grep startup-directory /etc/arangodb3/arangosh.conf  |sed "s;.*= *;;")
        mkdir -p "${JSDIR}/node/node_modules/"
        cp -a js/node/node_modules/eslint "${JSDIR}/node/node_modules/"
     fi
fi

# wordspitting is intentional here - no arrays in POSIX shells
# shellcheck disable=2086
exec $ARANGOSH \
    -c none \
    --log.level error \
    --log.file - \
    --server.password "" \
    --javascript.startup-directory "${JSDIR}" \
    ${FILELIST}

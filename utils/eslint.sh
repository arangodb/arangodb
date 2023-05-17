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
    $(find "${WD}/tests/js/server" -name "*.js" -o -name "*.inc" | grep -v "ranges-combined") \
    $(find "${WD}/tests/js/common" -name "*.js" -o -name "*.inc" | grep -v "test-data") \
    $(find "${WD}/tests/js/client" -name "*.js" -o -name "*.inc") \
    $(find "${WD}/3rdParty/rta-makedata/test_data" -name "*.js" -o -name "*.inc") \
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

# wordspitting is intentional here - no arrays in POSIX shells
# shellcheck disable=2086
exec eslint \
  -c js/.eslintrc \
  --quiet \
  ${JAVASCRIPT_JSLINT}

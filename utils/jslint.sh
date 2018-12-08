#!/bin/bash

if [ "x$@" == "x" ] ; then
  JAVASCRIPT_JSLINT="\
    `find ./js/actions -name "*.js"` \
    `find ./js/common/bootstrap -name "*.js"` \
    `find ./js/client/bootstrap -name "*.js"` \
    `find ./js/server/bootstrap -name "*.js"` \
    \
    `find ./js/common/modules/@arangodb -name "*.js"` \
    `find ./js/client/modules/@arangodb -name "*.js"` \
    `find ./js/server/modules/@arangodb -name "*.js"` \
    `find ./tests/js/server -name "*.js" | grep -v "ranges-combined"` \
    `find ./tests/js/common -name "*.js" | grep -v "test-data"` \
    `find ./tests/js/client -name "*.js"` \
    `find ./UnitTests -name "*.js"` \
    \
    `find ./js/apps/system/_admin/aardvark/APP/frontend/js/models -name "*.js"` \
    `find ./js/apps/system/_admin/aardvark/APP/frontend/js/views -name "*.js"` \
    `find ./js/apps/system/_admin/aardvark/APP/frontend/js/collections -name "*.js"` \
    `find ./js/apps/system/_admin/aardvark/APP/frontend/js/routers -name "*.js"` \
    `find ./js/apps/system/_admin/aardvark/APP/frontend/js/arango -name "*.js"` \
    \
    `find ./scripts -name "*.js"` \
    \
    ./js/common/modules/jsunity.js \
    ./js/client/client.js \
    ./js/server/server.js \
    \
  "
  if [ -d ./enterprise ] ; then
    echo Considering enterprise files...
    JAVASCRIPT_JSLINT="$JAVASCRIPT_JSLINT \
      `find ./enterprise/js -name "*.js"` \
      `find ./enterprise/tests/js -name "*.js"` \
      "
  fi
else
  JAVASCRIPT_JSLINT="$@"
fi

FILELIST=""

for file in ${JAVASCRIPT_JSLINT}; do
  FILELIST="${FILELIST} --jslint ${file}";
done

if [ -z "${ARANGOSH}" ];  then
  if [ -x build/bin/arangosh ];  then
    ARANGOSH=build/bin/arangosh
  elif [ -x bin/arangosh ];  then
    ARANGOSH=bin/arangosh
  else
    echo "$0: cannot locate arangosh"
  fi
fi

exec $ARANGOSH \
    -c none \
    --log.level error \
    --log.file - \
    --server.password "" \
    --javascript.startup-directory ./js \
    ${FILELIST}

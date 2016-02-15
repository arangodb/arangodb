#!/bin/bash

JAVASCRIPT_JSLINT="\
  `find ./js/actions -name "*.js"` \
  `find ./js/common/bootstrap -name "*.js"` \
  `find ./js/client/bootstrap -name "*.js"` \
  `find ./js/server/bootstrap -name "*.js"` \
  \
  `find ./js/common/modules/@arangodb -name "*.js"` \
  `find ./js/client/modules/@arangodb -name "*.js"` \
  `find ./js/server/modules/@arangodb -name "*.js"` \
  `find ./js/server/tests -name "*.js" | grep -v "ranges-combined"` \
  `find ./js/common/tests -name "*.js"` \
  `find ./js/client/tests -name "*.js"` \
  \
  `find ./js/apps/system/_system/cerberus/APP -name "*.js"` \
  `find ./js/apps/system/_api/gharial/APP -name "*.js"` \
  `find ./js/apps/system/_system/sessions/APP -name "*.js"` \
  `find ./js/apps/system/_system/simple-auth/APP -name "*.js"` \
  `find ./js/apps/system/_system/users/APP -name "*.js"` \
  \
  `find ./js/apps/system/_admin/aardvark/APP/frontend/js/models -name "*.js"` \
  `find ./js/apps/system/_admin/aardvark/APP/frontend/js/views -name "*.js"` \
  `find ./js/apps/system/_admin/aardvark/APP/frontend/js/collections -name "*.js"` \
  `find ./js/apps/system/_admin/aardvark/APP/frontend/js/routers -name "*.js"` \
  `find ./js/apps/system/_admin/aardvark/APP/frontend/js/arango -name "*.js"` \
  `find ./js/apps/system/_admin/aardvark/APP/frontend/js/shell -name "*.js"` \
  `find ./js/apps/system/_admin/aardvark/APP/test/specs -name "*.js"` \
  `find ./js/apps/system/_admin/aardvark/APP/test/clusterSpecs -name "*.js"` \
  \
  `find ./scripts -name "*.js"` \
  \
  ./js/common/modules/jsunity.js \
  ./js/client/client.js \
  ./js/server/server.js \
  ./js/server/upgrade-database.js \
  \
  ./js/apps/system/_admin/aardvark/APP/frontend/js/shell/browser.js \
"

FILELIST=""

for file in ${JAVASCRIPT_JSLINT}; do
    FILELIST="${FILELIST} --jslint ${file}";
done

exec ./build/bin/arangosh \
    -c none \
    --log.level error \
    --server.password "" \
    --javascript.startup-directory ./js \
    ${FILELIST}

#!/bin/bash
set -e

SRCDIR=$1

if test ! -f "$SRCDIR/.file-list-js";  then
(
  cd ${SRCDIR}

  find \
       js/actions \
       js/apps/system/_admin \
       js/apps/system/_api \
       js/apps/system/_system \
       js/client \
       js/common \
       js/node \
       js/server \
       \
       -type f -print
) \
| egrep -v "^js/common/tests/" \
| egrep -v "^js/common/test-data/" \
| egrep -v "^js/client/tests/" \
| egrep -v "^js/client/test-data/" \
| egrep -v "^js/server/tests/" \
| egrep -v "^js/server/test-data/" \
| egrep -v "^js/apps/system/_admin/.*/node_modules/" \
| egrep -v "^js/apps/system/.*/test/" \
| egrep -v "^js/apps/system/.*/test_data/" \
| egrep -v "^js/apps/system/.*/coverage/" \
| egrep -v "fileset.*tests.*fixtures.*an.*odd.*filename" > "$SRCDIR/.file-list-js"
fi

cat "$SRCDIR/.file-list-js"

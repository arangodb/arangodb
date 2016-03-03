#!/bin/bash

JAVASCRIPT_BROWSER="\
  modules/client/@arangodb/aql/queries.js \
  modules/client/@arangodb/arango-collection.js \
  modules/client/@arangodb/arango-database.js \
  modules/client/@arangodb/arango-query-cursor.js \
  modules/client/@arangodb/arango-statement.js \
  modules/client/@arangodb/arangosh.js \
  modules/client/@arangodb/graph-blueprint.js \
  modules/client/@arangodb/index.js \
  modules/client/@arangodb/replication.js \
  modules/client/@arangodb/simple-query.js \
  modules/client/@arangodb/tutorial.js \
  modules/common/@arangodb/aql/explainer.js \
  modules/common/@arangodb/aql/functions.js \
  modules/common/@arangodb/arango-collection-common.js \
  modules/common/@arangodb/arango-statement-common.js \
  modules/common/@arangodb/common.js \
  modules/common/@arangodb/general-graph.js \
  modules/common/@arangodb/graph-common.js \
  modules/common/@arangodb/graph.js \
  modules/common/@arangodb/graph/traversal.js \
  modules/common/@arangodb/is.js \
  modules/common/@arangodb/mimetypes.js \
  modules/common/@arangodb/simple-query-common.js \
  \
  bootstrap/modules/internal.js \
  bootstrap/modules/console.js \
  bootstrap/errors.js \
  bootstrap/monkeypatches.js \
  \
  client/client.js \
  client/bootstrap/modules/internal.js \
"

DIRECTORIES_BROWSER=""

for file in $JAVASCRIPT_BROWSER; do
  DIRECTORIES_BROWSER="$DIRECTORIES_BROWSER js/apps/system/_admin/aardvark/APP/frontend/js/`dirname $file`"
done

DIRECTORIES_BROWSER=`echo $DIRECTORIES_BROWSER | tr " " "\n" | sort | uniq`

for dir in $DIRECTORIES_BROWSER; do
  test -d $dir || mkdir -p $dir
done

for file in $JAVASCRIPT_BROWSER; do
  dst="js/apps/system/_admin/aardvark/APP/frontend/js/$file"

  src=`echo $file | sed \
    -e 's:^bootstrap/client:js/client/bootstrap/:' \
    -e 's:^bootstrap/:js/common/bootstrap/:' \
    -e 's:^client/:js/client/:' \
    -e 's:^modules/common/:js/common/modules/:' \
    -e 's:^modules/client/:js/client/modules/:'`
  
  test -f "$src" | exit 1
  test -f "$dst" | exit 1

  if test $src -nt $dst; then
    module=`echo $file | sed -e 's:.*@:@:' -e 's:\.js$::'`

    case $file in
      bootstrap/*|client/bootstrap/*|client/*)
        echo "generating module bootstrap $file"

        cp $src $dst
        ;;

      modules/common/*|modules/client/*)
        echo "generating module $module"

        (echo "module.define(\"$module\", function(exports, module) {" \
          && cat $src \
          && echo "});") > $dst
        ;;

      *)
        echo "$0: cannot handle $file"
        exit 1
    esac
  fi
done



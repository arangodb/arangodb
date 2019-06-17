#!/usr/bin/env bash

if test "`git status --short | grep '^\(.[MAU]\|[MAU].\) .*js$' | wc -l`" -eq 0; then
  exit 0;
fi

if [ -z "${ARANGOSH}" ];  then
  if [ -x build/bin/arangosh ];  then
    ARANGOSH=build/bin/arangosh
  elif [ -x bin/arangosh ];  then
    ARANGOSH=bin/arangosh
  else
    echo "$0: cannot locate arangosh"
  fi
fi

for file in ` git status --short | grep '^\(.[MAU]\|[MAU].\) .*js$' | cut -d " " -f 3`; do
  ${ARANGOSH} -c etc/relative/arangosh.conf --log.level error --jslint $file || exit 1
done

#!/usr/bin/env bash

if test "`git status --short | grep '^\(.[MAU]\|[MAU].\) .*js$' | wc -l`" -eq 0; then
  exit 0;
fi

for file in ` git status --short | grep '^\(.[MAU]\|[MAU].\) .*js$' | cut -d " " -f 3`; do
  eslint -c js/.eslintrc $file || exit 1
done

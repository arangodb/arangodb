#!/bin/bash
NAME=`basename $1 .js`

cat $1 \
  | sed -e 's:\(["\]\):\\\0:g' \
  | awk 'BEGIN {print "static string JS_'$NAME' = " } { print "  \"" $0 "\\n\"" } END { print ";"}'

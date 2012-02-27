#!/bin/bash
NAME=`echo $1 | sed -e 's:^\(.*/\)*js/\(.*\)\.js$:\2:' | tr "/-" "__"`

cat $1 \
  | sed -e 's:\(["\]\):\\\1:g' \
  | awk 'BEGIN {print "static string JS_'$NAME' = " } { print "  \"" $0 "\\n\"" } END { print ";"}'

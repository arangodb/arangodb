#!/bin/bash

INPUT="$1"
OUTPUT="Doxygen/wiki/`basename $INPUT`"

pandoc -f markdown -t markdown -o $OUTPUT.tmp $INPUT || exit 1

cat $OUTPUT.tmp \
  | sed -e 's:\(GET\|PUT\|DELETE\|POST\) /\\_:\1 /_:g' \
  | sed -e 's:^/\\_:/_:g' \
  | sed -e 's:<tt>\*:<tt>_:g' \
  > $OUTPUT || exit 1
rm -f $OUTPUT.tmp

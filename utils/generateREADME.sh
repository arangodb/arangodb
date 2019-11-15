#!/usr/bin/env bash
set -e

IN=${1:-README.md}
OUT=${2:-README}

fgrep -v "[Build Status]" $IN \
  | fgrep -v "ArangoDB-Logo" \
  | fgrep -v "[Build Status]" \
  | sed -e 's:&gt;:>:g' \
  | awk 'BEGIN { s = 0; } /^\*\*\*\*/ {if (s > 0) print ""} {if (length($0) == 0) {if (s != 0) print $0;} else {s = 1; print $0; }} /^\*\*\*\*/ {print ""}' \
  > $OUT

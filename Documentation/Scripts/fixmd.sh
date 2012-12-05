#!/bin/bash

INPUT="$1"
BASENAME="`basename $INPUT`"
OUTPUT="Doxygen/wiki/$BASENAME"

if test "$BASENAME" = "Home.md";  then
  sed -e 's:a href=":a href="wiki/:g' < $INPUT > $OUTPUT
else
  cp $INPUT $OUTPUT
fi

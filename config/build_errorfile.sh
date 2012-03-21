#!/bin/bash
SCRIPT="$1"
SOURCE="$2"
DEST="$3"

python "$SCRIPT" "$SOURCE" "$DEST.tmp"
if cmp -s $DEST ${DEST}.tmp;  then
  rm ${DEST}.tmp
else
  mv ${DEST}.tmp $DEST
fi

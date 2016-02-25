#!/bin/bash

SOURCE="$1"
DEST="$2"

SCRIPT="`dirname $0`/generateMimetypes.py"

python "$SCRIPT" "$SOURCE" "$DEST.tmp"

if cmp -s $DEST ${DEST}.tmp;  then
  rm ${DEST}.tmp
else
  mv ${DEST}.tmp $DEST
fi

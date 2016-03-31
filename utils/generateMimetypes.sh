#!/bin/bash

SOURCE="$1"
DEST="$2"

PYTHON_EXECUTABLE=${PYTHON_EXECUTABLE:-python}

SCRIPT="`dirname $0`/generateMimetypes.py"

${PYTHON_EXECUTABLE} "$SCRIPT" "$SOURCE" "$DEST.tmp"

if cmp -s $DEST ${DEST}.tmp;  then
  rm ${DEST}.tmp
else
  mv ${DEST}.tmp $DEST
fi

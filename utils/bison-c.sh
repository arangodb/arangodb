#!/bin/sh

BISON="$1"
OUTPUT="$2"
INPUT="$3"

if test "x$BISON" = x -o "x$OUTPUT" = x -o "x$INPUT" = x;  then
  echo "usage: $0 <bison> <output> <input>"
  exit 1
fi

#############################################################################
## bison
#############################################################################

${BISON} -d -ra -o ${OUTPUT} ${INPUT}

#############################################################################
## sanity checks
#############################################################################

PREFIX=`echo ${OUTPUT} | sed -e 's:\.c$::'`

test -f ${PREFIX}.h || exit 1
test -f ${PREFIX}.c || exit 1


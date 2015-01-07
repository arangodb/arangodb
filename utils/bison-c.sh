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

${BISON} -d -ra --warnings="deprecated,other,error=conflicts-sr,error=conflicts-rr" -o ${OUTPUT} ${INPUT}

#############################################################################
## sanity checks
#############################################################################

PREFIX=`echo ${OUTPUT} | sed -e 's:\.cpp$::'`

test -f ${PREFIX}.hpp || exit 1
test -f ${PREFIX}.cpp || exit 1

cp ${PREFIX}.hpp ${PREFIX}.h

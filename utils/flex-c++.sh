#!/bin/sh

FLEX="$1"
OUTPUT="$2"
INPUT="$3"

if test "x$FLEX" = "x" -o "x$OUTPUT" = "x" -o "x$INPUT" = "x";  then
  echo "usage: $0 <flex> <output> <input>"
  exit 1
fi

#############################################################################
## flex
#############################################################################

${FLEX} -L -o${OUTPUT} ${INPUT}

#############################################################################
## sanity checks
#############################################################################

test -f ${OUTPUT} || exit 1

#############################################################################
## fix casts
#############################################################################

cat ${OUTPUT} \
  | sed -e 's:( i = 0; i < _yybytes_len;:( i = 0; i < (int) _yybytes_len;:' \
  | sed -e 's:( yyl = 0; yyl < yyleng; ++yyl ):( yyl = 0; yyl < (int) yyleng; ++yyl ):' \
  | sed -e 's:yy_n_chars, (size_t) num_to_read );:yy_n_chars, (int) num_to_read );:' \
  | sed -e 's:register ::g' \
  > ${OUTPUT}.tmp

# give some information
diff -u ${OUTPUT} ${OUTPUT}.tmp

# and move the files to the final destination
mv ${OUTPUT}.tmp ${OUTPUT} || exit 1

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
  | sed -e 's:yy_n_chars, (size_t) num_to_read );:yy_n_chars, (int) num_to_read );:' \
  | sed -e 's:int yyl;:size_t yyl;:' \
  | sed -e 's:(int) (yyg->yy_n_chars + number_to_move):(yy_size_t) (yyg->yy_n_chars + number_to_move):' \
  | sed -e 's:register ::g' \
  > ${OUTPUT}.tmp

# give some information
# diff -u ${OUTPUT} ${OUTPUT}.tmp

# and move the files to the final destination
mv ${OUTPUT}.tmp ${OUTPUT} || exit 1

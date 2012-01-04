#!/bin/sh

FLEX="$1"
DIR="$2"
FILE="$3"

if test "x$FLEX" = "x" -o "x$DIR" = "x" -o "x$FILE" = "x" -o ! -d "$DIR";  then
  echo "usage: $0 <flex> <directory> <file-prefix>"
  exit 1
fi

if test ! -d "$DIR";  then
  echo "$0: expecting '$DIR' to be a directory"
  exit 1
fi

#############################################################################
## flex
#############################################################################

${FLEX} -L -o${DIR}/${FILE}.c ${DIR}/${FILE}.l

#############################################################################
## sanity checks
#############################################################################

test -f ${DIR}/${FILE}.c || exit 1

#############################################################################
## fix casts
#############################################################################

cat ${DIR}/${FILE}.c \
  | sed -e 's:yy_n_chars, (size_t) num_to_read );:yy_n_chars, (int) num_to_read );:' \
  | sed -e 's:extern int isatty (int );:#ifndef _WIN32\
\0\
#endif:' \
  > ${DIR}/${FILE}.c.tmp

# give some information
diff -u ${DIR}/${FILE}.c ${DIR}/${FILE}.c.tmp

# and move the files to the final destination
mv ${DIR}/${FILE}.c.tmp ${DIR}/${FILE}.c || exit 1

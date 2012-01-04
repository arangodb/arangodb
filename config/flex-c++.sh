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

${FLEX} -L -o${DIR}/${FILE}.cpp ${DIR}/${FILE}.ll

#############################################################################
## sanity checks
#############################################################################

test -f ${DIR}/${FILE}.cpp || exit 1

#############################################################################
## fix casts
#############################################################################

cat ${DIR}/${FILE}.cpp \
  | sed -e 's:(yy_n_chars), (size_t) num_to_read );:(yy_n_chars), (int) num_to_read );:' \
  > ${DIR}/${FILE}.cpp.tmp

# give some information
diff -u ${DIR}/${FILE}.cpp ${DIR}/${FILE}.cpp.tmp

# and move the files to the final destination
mv ${DIR}/${FILE}.cpp.tmp ${DIR}/${FILE}.cpp || exit 1

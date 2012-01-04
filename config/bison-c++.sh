#!/bin/sh

BISON="$1"
SRC="$2"
DST="$2"
FILE="$3"

if test "x$BISON" = x -o "x$SRC" = x -o "x$DST" = x -o "x$FILE" = x;  then
  echo "usage: $0 <bison> <directory> <file-prefix>"
  exit 1
fi

if test ! -d "$SRC";  then
  echo "$0: expecting '$SRC' to be a directory"
  exit 1
fi

if test ! -d "$DST";  then
  echo "$0: expecting '$DST' to be a directory"
  exit 1
fi

#############################################################################
## bison
#############################################################################

${BISON} -l -d -ra -S lalr1.cc -o "${DST}/${FILE}.cpp" "${SRC}/${FILE}.yy"

#############################################################################
## sanity checks
#############################################################################

test -f ${SRC}/${FILE}.hpp || exit 1
test -f ${SRC}/${FILE}.cpp || exit 1
test -f ${SRC}/position.hh || exit 1

#############################################################################
## rename file
#############################################################################

mv ${SRC}/${FILE}.hpp ${SRC}/${FILE}.h || exit 1

#############################################################################
## fix header file name in source, fix defines
#############################################################################

sed -e 's:\.hpp:.h:' < ${SRC}/${FILE}.cpp \
  | sed -e 's:# if YYENABLE_NLS:# if defined(YYENABLE_NLS) \&\& YYENABLE_NLS:' \
  > ${SRC}/${FILE}.cpp.tmp

# give some information
diff -u ${SRC}/${FILE}.cpp ${SRC}/${FILE}.cpp.tmp

# and move the files to the final destination
mv ${SRC}/${FILE}.cpp.tmp ${SRC}/${FILE}.cpp || exit 1


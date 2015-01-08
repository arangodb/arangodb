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

${BISON} -l -d -ra -S lalr1.cc --warnings="deprecated,other,error=conflicts-sr,error=conflicts-rr" -o ${OUTPUT} ${INPUT}

#############################################################################
## sanity checks
#############################################################################

PREFIX=`echo ${OUTPUT} | sed -e 's:\.cpp$::'`

test -f ${PREFIX}.hpp || exit 1
test -f ${PREFIX}.cpp || exit 1


#############################################################################
# the following command will insert extra parentheses around a sub expression
# to suppress compiler warnings. it will copy the file to a new location 1st
#############################################################################

DIR=`dirname ${OUTPUT}`

mv ${DIR}/position.hh ${DIR}/position.hh.tmp

sed -e 's/\(pos1.filename && pos2.filename && .pos1.filename == .pos2.filename\)/(&)/g' \
    ${DIR}/position.hh.tmp > ${DIR}/position.hh 

# give some information
diff -u ${DIR}/position.hh.tmp ${DIR}/position.hh 

# cleanup
rm -f ${DIR}/position.hh.tmp


#############################################################################
## rename file
#############################################################################

mv ${PREFIX}.hpp ${PREFIX}.h || exit 1

#############################################################################
## fix header file name in source, fix defines
#############################################################################

sed -e 's:\.hpp:.h:' < ${OUTPUT} \
  | sed -e 's:# if YYENABLE_NLS:# if defined(YYENABLE_NLS) \&\& YYENABLE_NLS:' \
  | sed -e 's:__attribute__((__unused__)):' \
  > ${OUTPUT}.tmp

# give some information
diff -u ${OUTPUT} ${OUTPUT}.tmp

# and move the files to the final destination
mv ${OUTPUT}.tmp ${OUTPUT} || exit 1


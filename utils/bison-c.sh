#!/bin/sh

BISON="$1"
OUTPUT="$2"
INPUT="$3"

if test "x$BISON" = x -o "x$OUTPUT" = x -o "x$INPUT" = x;  then
  echo "usage: $0 <bison> <output> <input>"
  exit 1
fi

PREFIX=`echo ${OUTPUT} | sed -e 's:\.cpp$::'`

# clean up after ourselves
trap "rm -f ${PREFIX}.tmp" EXIT TERM HUP INT

BISON_MAJOR_VER=`${BISON} --version |grep bison|sed -e "s;.* ;;" -e "s;\..*;;"`

# invoke bison
if test "${BISON_MAJOR_VER}" -ge "3"; then 
  BISON_OPTS="--warnings=deprecated,other,error=conflicts-sr,error=conflicts-rr"
  ${BISON} -d -ra "${BISON_OPTS}" -o "${OUTPUT}" "${INPUT}"
else
  # do not call bison 2.x with empty BISON_OPTS (extra operand error) and avoid
  # error: %define variable 'parse.error' is not used
  sed -e "s:%define parse.error verbose:%error-verbose:" "${INPUT}" > "grammar.y~"
  ${BISON} -d -ra -o "${OUTPUT}" "grammar.y~"
fi

# smoke test
test -f "${PREFIX}.hpp" || exit 1
test -f "${PREFIX}.cpp" || exit 1

# prepend "clang-format off" 
echo "/* clang-format off */" | cat - "${PREFIX}.hpp" > "${PREFIX}.tmp"
cp "${PREFIX}.tmp" "${PREFIX}.hpp"
echo "/* clang-format off */" | cat - "${PREFIX}.cpp" > "${PREFIX}.tmp"
cp "${PREFIX}.tmp" "${PREFIX}.cpp"
rm -f 

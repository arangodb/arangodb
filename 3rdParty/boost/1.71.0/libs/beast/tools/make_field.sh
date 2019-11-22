#!/bin/sh

export LC_COLLATE=C

echo "// string constants"
echo '            "<unknown-field>",'
cat $1 | sort -f | uniq | sed 's/^/            \"/; s/$/\",/'
echo

echo "enum class field : unsigned short"
echo "{"
echo "    unknown = 0,"
echo
#cat $1 | uniq | sort -f | sed 's/./\L&/g; s/^/\t/; s/$/,/'
cat $1 | sort -f | uniq | sed 's/\(.*\)/    \L\1,/; s/-/_/g'
echo "};"
echo

echo "// pairs"
#cat $1 | uniq | sort -f | sed 's/\(.*\)/\tmatch\(field::\L\1, \"\E\1\"\);/; s/-/_/'
cat $1 | sort -f | uniq | perl -nE 'chomp; $a=lc($_); $a=~s/-/_/g; say "        match(field::$a, \"$_\");";' | tr -d "\015"


#!/bin/bash
INFO="$1"
FILE="$2"

if test ! -f "$INFO";  then
  echo "$0: cannot open info file $INFO"
  exit 1
fi

if test -d .svn;  then
  revision=`svnversion`
else
  if test ! -f "$FILE";  then
    echo "$0: cannot open build file $FILE"
    exit 1
  fi

  revision=`grep 'Revision:' $INFO | awk '{print $2}'`
fi

if test -z "$revision";  then
  echo "$0: cannot read revision from file $INFO"
  exit 1
fi

version=`sed -e 's:.*"\([^[]*[^ []\).*":\1:' $FILE`

if test -z "$version";  then
  echo "$0: cannot read vision from file $FILE"
  exit 1
fi

echo "#define TRIAGENS_VERSION \"$version [$revision]\"" > ${FILE}.tmp

if cmp $FILE ${FILE}.tmp;  then
  rm ${FILE}.tmp
else
  mv ${FILE}.tmp $FILE
fi




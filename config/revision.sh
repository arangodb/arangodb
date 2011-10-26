#!/bin/bash

if [ $# -lt 4 ];  then
  echo "usage: $0 <name> <guard-name> <build.h> <header1> ..."
  exit 1
fi

NAME=$1
shift

name=`echo $NAME | awk '{print tolower($1)}'`

GUARD=$1
shift

BUILD=$1
shift

echo "TRIAGENS_$GUARD" | tr "a-z" "A-Z" | awk '{print "#ifndef " $1}'
echo "TRIAGENS_$GUARD" | tr "a-z" "A-Z" | awk '{print "#define " $1 " 1"}'
echo ""
echo "$*" | tr " " "\n" | sort | awk '$1 != "" {print "#include <" $1 ">"}'
echo ""
echo "namespace triagens {"
echo "  namespace $name {"
echo "    inline string version () {"
echo "      return " | tr -d "\n"
cat $BUILD | sed -e 's:^.*"\(.*\)":"\1";:'
echo "    }"
echo ""
echo "    inline string revision () {"
echo "      return \"\$Revision: $NAME " | tr -d "\n"
echo `cat $BUILD | sed -e 's:.*"\(.*\)".*:\1:'` | tr -d "\n"
echo ' (c) triAGENS GmbH $";'
echo "    }"
echo "  }"
echo "}"
echo ""
echo "#endif"

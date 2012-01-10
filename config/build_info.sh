#!/bin/bash
INFO="$1"

echo "volatile char * BUILD_INFO ="

if test -f "$INFO";  then
  grep 'Revision:' $INFO | awk '{print "  \"$Revision: " $2 " $\""}'
  grep 'Repository UUID:' $INFO | awk '{print "  \"$Source: " $3 " $\""}'
  grep 'Last Changed Date:' $INFO | awk '{print "  \"$Date: " $4 " " $5 " " $6 " $\""}'
else
  echo '  "$Revision$"'
  echo '  "$Source$"'
  echo '  "$Date$"'
fi

echo ";"

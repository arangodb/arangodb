#!/usr/bin/env bash
# Copyright (c) Facebook, Inc. and its affiliates. All Rights Reserved.
#
# Check for some simple mistakes that should prevent commit or push

BAD=""

git grep 'namespace rocksdb' -- '*.[ch]*'
if [ "$?" != "1" ]; then
  echo "^^^^^ Do not hardcode namespace rocksdb. Use ROCKSDB_NAMESPACE"
  BAD=1
fi

git grep -i 'nocommit' -- ':!build_tools/check-sources.sh'
if [ "$?" != "1" ]; then
  echo "^^^^^ Code was not intended to be committed"
  BAD=1
fi

git grep '<rocksdb/' -- ':!build_tools/check-sources.sh'
if [ "$?" != "1" ]; then
  echo '^^^^^ Use double-quotes as in #include "rocksdb/something.h"'
  BAD=1
fi

if [ "$BAD" ]; then
  exit 1
fi

#!/bin/bash

echo
echo '$0: setup make-system'

make setup || exit 1

echo
echo "$0: configuring ArangoDB"
# V8 needs lib realtime:
./configure \
	--enable-relative \
	|| exit 1

echo
echo "$0: compiling ArangoDB"

make -j2 || exit 1

echo
echo "$0: done"

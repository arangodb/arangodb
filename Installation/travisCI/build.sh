#!/bin/bash

echo
echo '$0: setup make-system'

make setup || exit 1

echo
echo "$0: configuring ArangoDB"
# V8 needs lib realtime:
./configure \
	--enable-relative \
	--enable-all-in-one-libev \
	--enable-all-in-one-v8 \
	--enable-all-in-one-icu \
	|| exit 1

echo
echo "$0: compiling ArangoDB"

make -j2 || exit 1

echo
echo "$0: done"

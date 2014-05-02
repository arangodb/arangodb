#! /bin/sh

cpp -P ${cppargs} ${1:-./src/libbson.symbols} | sed -e '/^$/d' -e 's/ G_GNUC.*$//' -e 's/ PRIVATE//' -e 's/ DATA//' | sort > expected-abi

nm -D -g --defined-only .libs/libbson-1.0.so | cut -d ' ' -f 3 | egrep -v '^(__bss_start|_edata|_end)' | sort > actual-abi
diff -u expected-abi actual-abi && rm -f expected-abi actual-abi

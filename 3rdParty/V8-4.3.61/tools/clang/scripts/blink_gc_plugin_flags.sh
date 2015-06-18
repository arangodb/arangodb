#!/usr/bin/env bash
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script returns the flags that should be passed to clang.

SRC_DIR=$(cd $(dirname $0)/../../.. && echo $PWD)
CLANG_LIB_PATH=$SRC_DIR/third_party/llvm-build/Release+Asserts/lib

if uname -s | grep -q Darwin; then
  LIBSUFFIX=dylib
else
  LIBSUFFIX=so
fi

FLAGS=""
PREFIX="-Xclang -plugin-arg-blink-gc-plugin -Xclang"
for arg in "$@"; do
  if [[ "$arg" = "enable-oilpan=1" ]]; then
    FLAGS="$FLAGS $PREFIX enable-oilpan"
  elif [[ "$arg" = "dump-graph=1" ]]; then
    FLAGS="$FLAGS $PREFIX dump-graph"
  elif [[ "$arg" = "warn-raw-ptr=1" ]]; then
    FLAGS="$FLAGS $PREFIX warn-raw-ptr"
  elif [[ "$arg" = "warn-unneeded-finalizer=1" ]]; then
    FLAGS="$FLAGS $PREFIX warn-unneeded-finalizer"
  elif [[ "$arg" = custom_clang_lib_path=* ]]; then
    CLANG_LIB_PATH="$(cd "${arg#*=}" && echo $PWD)"
  fi
done

echo -Xclang -load -Xclang \"$CLANG_LIB_PATH/libBlinkGCPlugin.$LIBSUFFIX\" \
  -Xclang -add-plugin -Xclang blink-gc-plugin $FLAGS

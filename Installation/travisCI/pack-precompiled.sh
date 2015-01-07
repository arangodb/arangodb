#!/bin/bash

tar cvf travis-precompiled.tar \
  .v8-build-64 \
  3rdParty/V8-3.29.59/include \
  3rdParty/V8-3.29.59/out/x64.release/obj.target/tools/gyp \
  3rdParty/V8-3.29.59/third_party/icu/source/common \
  3rdParty/V8-3.29.59/third_party/icu/source/i18n \
  3rdParty/V8-3.29.59/third_party/icu/source/io \
  3rdParty/V8-3.29.59/out/x64.release/obj.target/third_party/icu

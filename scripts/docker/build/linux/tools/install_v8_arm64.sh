#!/bin/bash
mkdir -p /opt/v8/lib
mkdir -p /opt/v8/include
mkdir -p /opt/v8/third_party/icu/source

cd /opt/v8/v8

cp -a out.gn/arm64.release.sample/obj/*.a /opt/v8/lib
cp -a out.gn/arm64.release.sample/obj/v8_libbase /opt/v8/lib/v8_libbase
cp -a out.gn/arm64.release.sample/obj/v8_libplatform /opt/v8/lib/v8_libplatform

cp -a out.gn/arm64.release.sample/obj/third_party/icu/*.a /opt/v8/lib
cp -a out.gn/arm64.release.sample/obj/third_party/icu/icui18n /opt/v8/lib/icui18n
cp -a out.gn/arm64.release.sample/obj/third_party/icu/icuuc_private /opt/v8/lib/icuuc_private
cp -a third_party/icu/source/common /opt/v8/third_party/icu/source
cp -a third_party/icu/source/i18n /opt/v8/third_party/icu/source
cp -a third_party/icu/source/io /opt/v8/third_party/icu/source
cp -a third_party/icu/common /opt/v8/third_party/icu

cp -r include/* /opt/v8/include
# rm -rf /opt/v8/v8

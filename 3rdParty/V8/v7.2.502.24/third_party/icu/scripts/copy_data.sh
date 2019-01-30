#!/bin/bash
# Copyright (c) 2015 The Chromium Authors. All rights reserved.
# # Use of this source code is governed by a BSD-style license that can be
# # found in the LICENSE file.
#
# This script is tested ONLY on Linux. It may not work correctly on
# Mac OS X.
#

if [ $# -lt 1 ];
then
  echo "Usage: "$0" (common|android|ios)" >&2
  exit 1
fi

TOPSRC="$(dirname "$0")/.."
source "${TOPSRC}/scripts/data_common.sh"


function copy_common {
  DATA_PREFIX="data/out/tmp/icudt${VERSION}"

  echo "Generating the big endian data bundle"
  LD_LIBRARY_PATH=lib bin/icupkg -tb "${DATA_PREFIX}l.dat" "${DATA_PREFIX}b.dat"

  echo "Copying icudtl.dat and icudtlb.dat"
  for endian in l b
  do
    cp "${DATA_PREFIX}${endian}.dat" "${TOPSRC}/common/icudt${endian}.dat"
  done

  echo "Done with copying pre-built ICU data files."
}

function copy_android_ios {
  echo "Copying icudtl.dat for $1"

  cp "data/out/tmp/icudt${VERSION}l.dat" "${TOPSRC}/$2/icudtl.dat"

  echo "Done with copying pre-built ICU data file for $1."
}

function copy_cast {
  echo "Copying icudtl.dat for $1"

  cp "data/out/tmp/icudt${VERSION}l.dat" "${TOPSRC}/$2/icudt${VERSION}l.dat"

  LD_LIBRARY_PATH=lib/ bin/icupkg -r \
    "${TOPSRC}/$2/cast-removed-resources.txt" \
    "${TOPSRC}/$2/icudt${VERSION}l.dat"

  mv "${TOPSRC}/$2/icudt${VERSION}l.dat" "${TOPSRC}/$2/icudtl.dat"

  echo "Done with copying pre-built ICU data file for $1."
}

function copy_flutter {
  echo "Copying icudtl.dat for Flutter"

  cp "data/out/tmp/icudt${VERSION}l.dat" "${TOPSRC}/flutter/icudt${VERSION}l.dat"

  echo "Removing unused resources from icudtl.dat for Flutter"

  LD_LIBRARY_PATH=lib/ bin/icupkg -r \
    "${TOPSRC}/flutter/flutter-removed-resources.txt" \
    "${TOPSRC}/flutter/icudt${VERSION}l.dat"
  mv "${TOPSRC}/flutter/icudt${VERSION}l.dat" "${TOPSRC}/flutter/icudtl.dat"

  echo "Done with copying pre-built ICU data file for Flutter."
}

case "$1" in
  "common")
    copy_common
    ;;
  "android")
    copy_android_ios Android android
    ;;
  "ios")
    copy_android_ios iOS ios
    ;;
  "cast")
    copy_cast Cast cast
    ;;
  "flutter")
    copy_flutter
    ;;
esac

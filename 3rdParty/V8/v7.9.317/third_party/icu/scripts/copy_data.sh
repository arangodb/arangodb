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
  echo "Usage: "$0" (android|android_extra|android_small|cast|chromeos|common|flutter|ios)" >&2
  exit 1
fi

TOPSRC="$(dirname "$0")/.."
source "${TOPSRC}/scripts/data_common.sh"


function copy_common {
  DATA_PREFIX="data/out/tmp/icudt${VERSION}"
  TZRES_PREFIX="data/out/build/icudt${VERSION}l"

  echo "Generating the big endian data bundle"
  LD_LIBRARY_PATH=lib bin/icupkg -tb "${DATA_PREFIX}l.dat" "${DATA_PREFIX}b.dat"

  echo "Copying icudtl.dat and icudtlb.dat"
  for endian in l b
  do
    cp "${DATA_PREFIX}${endian}.dat" "${TOPSRC}/common/icudt${endian}.dat"
  done

  echo "Copying metaZones.res, timezoneTypes.res, zoneinfo64.res"
  for tzfile in metaZones timezoneTypes zoneinfo64
  do
    cp "${TZRES_PREFIX}/${tzfile}.res" "${TOPSRC}/tzres/${tzfile}.res"
  done

  echo "Done with copying pre-built ICU data files."
}

function copy_data {
  echo "Copying icudtl.dat for $1"

  cp "data/out/tmp/icudt${VERSION}l.dat" "${TOPSRC}/$2/icudtl.dat"

  echo "Done with copying pre-built ICU data file for $1."
}

function copy_android_extra {
  echo "Copying icudtl_extra.dat for AndroidExtra"

  LD_LIBRARY_PATH=lib/ bin/icupkg -r \
    "${TOPSRC}/filters/android-extra-removed-resources.txt" \
    "data/out/tmp/icudt${VERSION}l.dat"

  echo "AFTER strip out the content is"
  LD_LIBRARY_PATH=lib/ bin/icupkg -l \
    "data/out/tmp/icudt${VERSION}l.dat"

  cp "data/out/tmp/icudt${VERSION}l.dat" "${TOPSRC}/android_small/icudtl_extra.dat"

  echo "Done with copying pre-built ICU data file for AndroidExtra."
}


BACKUP_DIR="dataout/$1"
function backup_outdir {
  rm -rf "${BACKUP_DIR}"
  mkdir "${BACKUP_DIR}"
  find "data/out" | cpio -pdmv "${BACKUP_DIR}"
}

case "$1" in
  "chromeos")
    copy_data ChromeOS $1
    backup_outdir $1
    ;;
  "common")
    copy_common
    backup_outdir $1
    ;;
  "android")
    copy_data Android $1
    backup_outdir $1
    ;;
  "android_small")
    copy_data AndroidSmall $1
    backup_outdir $1
    ;;
  "android_extra")
    copy_android_extra
    backup_outdir $1
    ;;
  "ios")
    copy_data iOS $1
    backup_outdir $1
    ;;
  "cast")
    copy_data Cast $1
    backup_outdir $1
    ;;
  "flutter")
    copy_data Flutter $1
    backup_outdir $1
    ;;
esac

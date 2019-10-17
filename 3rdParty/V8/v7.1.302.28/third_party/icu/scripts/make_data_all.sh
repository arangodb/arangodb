#!/bin/bash

set -x

ICUROOT="$(dirname "$0")/.."

function config_data {
  if [ $# -lt 1 ];
  then
    echo "config target missing." >&2
    echo "Should be (android|android_extra|android_small|cast|chromeos|common|flutter|ios)" >&2
    exit 1
  fi

  ICU_DATA_FILTER_FILE="${ICUROOT}/filters/$1.json" \
  "${ICUROOT}/source/runConfigureICU" --enable-debug --disable-release \
    Linux/gcc --disable-tests  --disable-layoutex || \
    { echo "failed to configure data for $1" >&2; exit 1; }
}

echo "Build the necessary tools"
"${ICUROOT}/source/runConfigureICU" --enable-debug --disable-release \
    Linux/gcc  --disable-tests --disable-layoutex
make -j 120

echo "Build the filtered data for common"
(cd data && make clean)
config_data common
make -j 120
$ICUROOT/scripts/copy_data.sh common

echo "Build the filtered data for chromeos"
(cd data && make clean)
config_data chromeos
make -j 120
$ICUROOT/scripts/copy_data.sh chromeos

echo "Build the filtered data for Cast"
(cd data && make clean)
config_data cast
$ICUROOT/cast/patch_locale.sh && make -j 120
$ICUROOT/scripts/copy_data.sh cast

echo "Build the filtered data for Android"
(cd data && make clean)
config_data android
make -j 120
$ICUROOT/scripts/copy_data.sh android

echo "Build the filtered data for AndroidSmall"
(cd data && make clean)
config_data android_small
make -j 120
$ICUROOT/scripts/copy_data.sh android_small

echo "Build the filtered data for AndroidExtra"
(cd data && make clean)
config_data android_extra
make -j 120
$ICUROOT/scripts/copy_data.sh android_extra

echo "Build the filtered data for iOS"
(cd data && make clean)
config_data ios
make -j 120
$ICUROOT/scripts/copy_data.sh ios

echo "Build the filtered data for Flutter"
(cd data && make clean)
config_data flutter
$ICUROOT/flutter/patch_brkitr.sh && make -j 120
$ICUROOT/scripts/copy_data.sh flutter

echo "Clean up the git"
$ICUROOT/scripts/clean_up_data_source.sh

#!/bin/bash

set -x

ICUROOT="$(dirname "$0")/.."

$ICUROOT/scripts/trim_data.sh
$ICUROOT/scripts/make_data.sh
$ICUROOT/scripts/copy_data.sh common
$ICUROOT/cast/patch_locale.sh
$ICUROOT/scripts/make_data.sh
$ICUROOT/scripts/copy_data.sh cast
$ICUROOT/android/patch_locale.sh
$ICUROOT/scripts/make_data.sh
$ICUROOT/scripts/copy_data.sh android
$ICUROOT/ios/patch_locale.sh
$ICUROOT/scripts/make_data.sh
$ICUROOT/scripts/copy_data.sh ios
$ICUROOT/flutter/patch_brkitr.sh
$ICUROOT/scripts/make_data.sh
$ICUROOT/scripts/copy_data.sh flutter
$ICUROOT/scripts/clean_up_data_source.sh

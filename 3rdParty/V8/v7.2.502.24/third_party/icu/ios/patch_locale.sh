#!/bin/sh
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

treeroot="$(dirname "$0")/.."
cd "${treeroot}"

cd source/data

# Keep only two common calendars. Add locale-specific calendars to Thai.
COMMON_CALENDARS="gregorian|generic"
for i in locales/*.txt; do
  CALENDARS="${COMMON_CALENDARS}"
  case $(basename $i .txt | sed 's/_.*$//') in
    th)
      EXTRA_CAL='buddhist'
      ;;
    *)
      EXTRA_CAL=''
      ;;
  esac

  CAL_PATTERN="(${COMMON_CALENDARS}${EXTRA_CAL:+|${EXTRA_CAL}})"
  echo $CAL_PATTERN

  echo Overwriting $i...
  sed -r '/^    calendar\{$/,/^    \}$/ {
            /^    calendar\{$/p
            /^        default\{".*"\}$/p
            /^        '${CAL_PATTERN}'\{$/, /^        \}$/p
            /^    \}$/p
            d
          }' -i $i
done

echo DONE.

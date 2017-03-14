#!/bin/bash
# Copyright (c) 2015 The Chromium Authors. All rights reserved.
# # Use of this source code is governed by a BSD-style license that can be
# # found in the LICENSE file.
#
# This script is tested ONLY on Linux. It may not work correctly on
# Mac OS X.
#
TOPSRC="$(dirname "$0")/.."
source "${TOPSRC}/scripts/data_common.sh"

DATA_PREFIX="data/out/tmp/icudt${VERSION}"

echo "Generating the big endian data bundle"
LD_LIBRARY_PATH=lib bin/icupkg -tb "${DATA_PREFIX}l.dat" "${DATA_PREFIX}b.dat"

echo "Copying icudtl.dat and icudtlb.dat"
for endian in l b
do
  cp "${DATA_PREFIX}${endian}.dat" "${TOPSRC}/common/icudt${endian}.dat"
done

echo "Done with copying pre-built ICU data files."

#!/bin/bash
# Copyright (c) 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This is shared among scripts to build and copy ICU data.

[ -r icudefs.mk ] && \
    egrep '^## Top-level Makefile.in for ICU' Makefile > /dev/null || \
    { echo "cd to the ICU build directory before running "$0"." >&2; exit 1; }

TOPSRC2=$(egrep '^top_srcdir =' Makefile | cut -d ' ' -f 3)

if [[ ! "${TOPSRC}/source" -ef "${TOPSRC2}" ]]; then
  echo "ICU was built from the source in ${TOPSRC2}, but";
  echo "this script is in ${TOPSRC}.";
  exit 2
fi

echo "Working on ICU built from ${TOPSRC2}"

VERSION=$(egrep '^SO_TARGET.*MAJOR' icudefs.mk | cut -d ' ' -f 3)
echo "The major version of ICU is $VERSION"

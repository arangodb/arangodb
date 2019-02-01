#!/bin/bash
# Copyright (c) 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script is tested ONLY on Linux. It may not work correctly on
# Mac OS X.

TOPSRC="$(dirname "$0")/.."
source "${TOPSRC}/scripts/data_common.sh"

function check_build {
  if [[ ! -x bin/icupkg || ! -x bin/pkgdata || ! -r lib/libicuuc.so ]]; then
    echo "ICU has not been built. Configure and build ICU first by running" >&2
    echo "${TOPSRC}/source/runConfigureICU Linux --disable-layout && make" >&2
    exit 3
  fi
}

# Rewrite data/Makefile to regenerate string pool bundles (pool.res)
# optimized for our trimmed data (after running trim_data.sh or
# android/patch_locale.sh)
# See http://site.icu-project.org/processes/release/tasks/build
#     http://bugs.icu-project.org/trac/ticket/8101
#     http://bugs.icu-project.org/trac/ticket/12069
function rewrite_data_makefile {
  echo "Creating Makefile to regenerate pool bundles..."
  sed -i.orig -e \
  ' /usePoolBundle/ {
    s/usePoolBundle/writePoolBundle/
    s/\(\$(UNITSRCDIR.*\)\$(<F)$/\1$(UNIT_SRC)/
    s/\(\$(REGIONSRCDIR.*\)\$(<F)$/\1$(REGION_SRC)/
    s/\(\$(CURRSRCDIR.*\)\$(<F)$/\1$(CURR_SRC)/
    s/\(\$(LANGSRCDIR.*\)\$(<F)$/\1$(LANG_SRC)/
    s/\(\$(ZONESRCDIR.*\)\$(<F)$/\1$(ZONE_SRC)/
    s/\(\$(LOCSRCDIR.*\)\$(<F)$/\1$(RES_SRC)/
  }' Makefile
}

# Rebuild the ICU data. 'make' has to be run twice because of
# http://bugs.icu-project.org/trac/ticket/10570
function build_data {
  make clean
  make
  sed -i 's/css3transform.res/root.res/' out/tmp/icudata.lst
  make
}

# Copy a single pool.res to the source tree and emit the sizes
# before and after.
function copy_pool_bundle {
  echo "copying pool.res for $3"
  echo "original: $(ls -l $2/pool.res | awk '{print $5;}')"
  echo "optimized: $(ls -l $1 | awk '{print $5;}')"
  cp "$1" "$2"
}

function copy_pool_bundles {
  echo "Copying optimized pool bundles to the ${TOPSRC}/source/data ..."
  POOLROOT="out/build/icudt${VERSION}l"
  SRCDATA="${TOPSRC}/source/data"

  copy_pool_bundle "${POOLROOT}/pool.res" "${SRCDATA}/locales" locales

  for c in lang region zone curr unit; do
    copy_pool_bundle "${POOLROOT}/${c}/pool.res" "${SRCDATA}/${c}" "${c}"
  done
}

function clean_up_src {
  echo "Cleaning up the source tree. Removing optimize pool bundles ..."
  cd "${TOPSRC}/source/data"
  for c in locales lang region zone curr unit; do
    git checkout -- "${c}/pool.res"
  done
}

function main() {
  check_build
  cd data
  rewrite_data_makefile
  echo "PASS1: Building the ICU data to generate custom pool bundles ..."
  build_data

  copy_pool_bundles

  echo "Restoring the original Makefile"
  mv Makefile.orig Makefile

  echo "PASS2: Building the final ICU data with custom pool bundles"
  build_data
  ls -l out/tmp/icudt*l.dat

  clean_up_src

  echo "Done."
}

main "$@"

#!/bin/bash
# Copyright (c) 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

ICUROOT="$(dirname "$0")/.."
LINUX_SOURCE="${ICUROOT}/linux/icudtl_dat.S"
MAC_SOURCE="${ICUROOT}/mac/icudtl_dat.S"

echo "Generating ${MAC_SOURCE} from ${LINUX_SOURCE}"

# Linux uses 'icudt${MAJOR VERSION}_dat' while Mac has "_" prepended to it.
ICUDATA_SYMBOL="_$(head -1 ${LINUX_SOURCE} | cut -d ' ' -f 2)"

cat > ${MAC_SOURCE} <<PREAMBLE
.globl ${ICUDATA_SYMBOL}
#ifdef U_HIDE_DATA_SYMBOL
       .private_extern ${ICUDATA_SYMBOL}
#endif
       .data
       .const
       .align 4
${ICUDATA_SYMBOL}:
PREAMBLE

PREAMBLE_LENGTH=$(($(egrep -n '^icudt' ${LINUX_SOURCE} | cut -d : -f 1) + 1))
tail -n +${PREAMBLE_LENGTH} ${LINUX_SOURCE} >> ${MAC_SOURCE}
echo "Done with generating ${MAC_SOURCE}"

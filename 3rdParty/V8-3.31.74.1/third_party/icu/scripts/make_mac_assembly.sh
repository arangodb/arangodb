#!/bin/bash
# Copyright (c) 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

ICUROOT="$(dirname $0)/.."
LINUX_SOURCE="${ICUROOT}/linux/icudtl_dat.S"
MAC_SOURCE="${ICUROOT}/mac/icudtl_dat.S"

cat > ${MAC_SOURCE} <<PREAMBLE
.globl _icudt52_dat
#ifdef U_HIDE_DATA_SYMBOL
       .private_extern _icudt52_dat
#endif
       .data
       .const
       .align 4
_icudt52_dat:
PREAMBLE

PREAMBLE_LENGTH=$(($(egrep -n '^icudt' ${LINUX_SOURCE} | cut -d : -f 1) + 1))
tail -n +${PREAMBLE_LENGTH} ${LINUX_SOURCE} >> ${MAC_SOURCE}

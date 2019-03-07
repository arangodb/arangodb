#!/bin/sh
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Reference:
#   http://encoding.spec.whatwg.org/#single-byte-decoder

# Download the following file, run it in source/data/mappings directory
# and save the result to ibm-866_html5-2012.ucm
#   http://encoding.spec.whatwg.org/index-ibm866.txt )

cat <<PREAMBLE
# ***************************************************************************
# *
# *   Generated from index-ibm866.txt (
# *   http://encoding.spec.whatwg.org/index-ibm866.txt )
# *   following the algorithm for the single byte legacy encoding
# *   described at http://encoding.spec.whatwg.org/#single-byte-decoder
# *
# ***************************************************************************
<code_set_name>               "ibm-866_html5-2012"
<char_name_mask>              "AXXXX"
<mb_cur_max>                  1
<mb_cur_min>                  1
<uconv_class>                 "SBCS"
<subchar>                     \x7F
<icu:charsetFamily>           "ASCII"

CHARMAP
PREAMBLE




awk 'BEGIN { for (i=0; i < 0x80; ++i) { printf("<U%04X> \\x%02X |0\n", i, i);}}
!/^#/ && !/^$/ { printf ("<U%4s> \\x%02X |0\n", substr($2, 3), $1 + 0x80);}' \
index-ibm866.txt | sort
echo 'END CHARMAP'


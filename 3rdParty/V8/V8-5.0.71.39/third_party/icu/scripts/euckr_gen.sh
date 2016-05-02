#!/bin/sh
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# References:
#   https://encoding.spec.whatwg.org/#euc-kr

# This script downloads the following file.
#   https://encoding.spec.whatwg.org/index-euc-kr.txt

function preamble {
cat <<PREAMBLE
# ***************************************************************************
# *
# *   Copyright (C) 1995-2015, International Business Machines
# *   Corporation and others.  All Rights Reserved.
# *
# *   Generated per the algorithm for EUC-KR
# *   described at http://encoding.spec.whatwg.org/#euc-kr
# *
# ***************************************************************************
<code_set_name>               "euc-kr-html"
<mb_cur_max>                  2
<mb_cur_min>                  1
<uconv_class>                 "MBCS"
<subchar>                     \x3F
<icu:charsetFamily>           "ASCII"

# 81-fe in states 2 and 3 can be tigher and a1-fe, but
# to be compliant to HTML5 spec, it should be 81-fe.
<icu:state>                  0-7f, 81-c5:1, c6:2, c7-fe:3
<icu:state>                  41-5a, 61-7a, 81-fe
<icu:state>                  41-52, 81-fe
<icu:state>                  81-fe

CHARMAP
PREAMBLE
}

function ascii {
  for i in $(seq 0 127)
  do
    printf '<U%04X> \\x%02X |0\n' $i $i
  done
}


# HKSCS characters are not supported in encoding ( |lead < 0xA1| )
function euckr {
  awk '!/^#/ && !/^$/ \
       { pointer = $1; \
         ucs = substr($2, 3); \
         lead = pointer / 190 + 0x81; \
         trail = $1 % 190 + 0x41; \
         tag = 0; \
         printf ("<U%4s> \\x%02X\\x%02X |%d\n", ucs,\
                 lead,  trail, tag);\
       }' \
  index-euc-kr.txt
}

function unsorted_table {
  euckr
}

wget -N -r -nd https://encoding.spec.whatwg.org/index-euc-kr.txt
preamble
ascii
unsorted_table | sort -k1  | uniq
echo 'END CHARMAP'

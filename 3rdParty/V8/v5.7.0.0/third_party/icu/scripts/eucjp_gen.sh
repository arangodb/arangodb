#!/bin/sh
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# References:
#   https://encoding.spec.whatwg.org/#euc-jp
#   https://legacy-encoding.sourceforge.jp/wiki/index.php?cp51932
#   https://www.iana.org/assignments/charset-reg/CP51932
#   Table 3-64 in CJKV Information Processing 2/e.

# Download the following two files, run it in source/data/mappings directory
# and save the result to euc-jp-html5.ucm
#   https://encoding.spec.whatwg.org/index-jis0208.txt
#   https://encoding.spec.whatwg.org/index-jis0212.txt

function preamble {
cat <<PREAMBLE
# ***************************************************************************
# *
# *   Copyright (C) 1995-2014, International Business Machines
# *   Corporation and others.  All Rights Reserved.
# *
# *   Generated per the algorithm for EUC-JP
# *   described at https://encoding.spec.whatwg.org/#euc-jp.
# *
# ***************************************************************************
<code_set_name>               "euc-jp-html"
<char_name_mask>              "AXXXX"
<mb_cur_max>                  3
<mb_cur_min>                  1
<uconv_class>                 "MBCS"
<subchar>                     \x3F
<icu:charsetFamily>           "ASCII"

<icu:state>                   0-7f, 8e:2, 8f:3, a1-fe:1
<icu:state>                   a1-fe
<icu:state>                   a1-df
<icu:state>                   a1-fe:1, a1:4, a3-a5:4, a8:4, ac-af:4, ee-f2:4, f4-fe:4
<icu:state>                   a1-fe.u

CHARMAP
PREAMBLE
}

#<U0000> \x00 |0
function ascii {
  for i in $(seq 0 127)
  do
    printf '<U%04X> \\x%02X |0\n' $i $i
  done
}


# Map 0x8E 0x[A1-DF] to U+FF61 to U+FF9F
function half_width_kana {
  for i in $(seq 0xA1 0xDF)
  do
    # 65377 = 0xFF61, 161 = 0xA1
    printf '<U%04X> \\x8E\\x%02X |0\n' $(($i + 65377 - 161))  $i
  done
}


# index-jis0208.txt has index pointers larger than the size of
# the encoding space available in 2-byte Graphic plane of ISO-2022-based
# encoding (94 x 94 = 8836). We have to exclude them because they're for
# Shift-JIS.
# In addition, index-jis0208.txt has 10 pairs of duplicate mapping entries.
# All the bi-directional mapping entries come *before* the uni-directional
# (EUC-JP to Unicode) entries so that we put '|3' if we have seen
# the same Unicode code point earlier in the list. According to the definition
# of 'index pointer' in the W3C encoding spec, it's the first entry in the
# file for a given Unicode code point.

function jis208 {
  awk '!/^#/ && !/^$/ && $1 <= 8836  \
       { printf ("<U%4s> \\x%02X\\x%02X |%d\n", substr($2, 3),\
                 $1 / 94 + 0xA1, $1 % 94 + 0xA1,\
                 ($2 in uset) ? 3 : 0); \
         uset[$2] = 1;
       }' \
  index-jis0208.txt
}

# JIS X 212 is for decoding only (use '|3' to denote that).

function jis212 {
  awk '!/^#/ && !/^$/ \
       { printf ("<U%4s> \\x8F\\x%02X\\x%02X |3\n", substr($2, 3),\
                 $1 / 94 + 0xA1, $1 % 94 + 0xA1);}' \
  index-jis0212.txt
}

function unsorted_table {
  ascii
  half_width_kana
  jis208
  jis212
  echo '<U00A5> \x5C |1'
  echo '<U203E> \x7E |1'
  echo '<U2212> \xA1\xDD |1'
}

wget -N -r -nd https://encoding.spec.whatwg.org/index-jis0208.txt
wget -N -r -nd https://encoding.spec.whatwg.org/index-jis0212.txt
preamble
unsorted_table | sort  | uniq
echo 'END CHARMAP'

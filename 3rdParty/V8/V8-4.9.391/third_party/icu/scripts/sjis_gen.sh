#!/bin/sh
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# References:
#   https://encoding.spec.whatwg.org/#shift_jis

# Download the following file, run it in source/data/mappings directory
# and save the result to euc-jp-html5.ucm
#   https://encoding.spec.whatwg.org/index-jis0208.txt

function preamble {
cat <<PREAMBLE
# ***************************************************************************
# *
# *   Copyright (C) 1995-2014, International Business Machines
# *   Corporation and others.  All Rights Reserved.
# *
# *   Generated per the algorithm for Shift_JIS
# *   described at https://encoding.spec.whatwg.org/#shift_jis
# *
# ***************************************************************************
<code_set_name>               "shift_jis-html5"
<char_name_mask>              "AXXXX"
<mb_cur_max>                  2
<mb_cur_min>                  1
<uconv_class>                 "MBCS"
<subchar>                     \x3F
<icu:charsetFamily>           "ASCII"

<icu:state>                   0-80, 81-9f:1, a1-df, e0-fc:1, 82:3, 84:4, 85-86:2, 87:5, 88:2, 98:6, eb-ec:2, ef:2, f9:2, fc:7

<icu:state>                   40-7e, 80-fc
<icu:state>                   80-fc
<icu:state>                   4f-7e, 80-fc, 59-5f.i, 7a-7e.i
<icu:state>                   40-7e, 80-fc, 61-6f.i
<icu:state>                   40-7e, 80-fc, 76-7d.i
<icu:state>                   40-7e, 80-fc, 73-7e.i
<icu:state>                   40-4b, 80-fc


CHARMAP
PREAMBLE
}

# The encoding spec for Shift_JIS says U+0080 has to be round-tripped with
# 0x80. So, this is one character more than ASCII up to 128 (0x80).
function ascii {
  for i in $(seq 0 128)
  do
    printf '<U%04X> \\x%02X |0\n' $i $i
  done
}


# Map 0x[A1-DF] to U+FF61 to U+FF9F
function half_width_kana {
  for i in $(seq 0xA1 0xDF)
  do
    # 65377 = 0xFF61, 161 = 0xA1
    printf '<U%04X> \\x%02X |0\n' $(($i + 65377 - 161))  $i
  done
}


# From https://encoding.spec.whatwg.org/#index-shift_jis-pointer
# The index shift_jis pointer for code point is the return value of
# these steps for the round-trip code points (tag = 0)
#
#   Let index be index jis0208 excluding all pointers in the range 8272 to 8835.
#   Return the index pointer for code point in index.
# For index ($1) outside the above range, it's for decoding only and tag
# is set to '3'.
# Besides, there are 24 more characters with multiple SJIS representations.
# Only the first of multiple is tagged with '0' (bi-directional mapping)
# while the rest is tagged with '3'.

function jis208 {
  awk '!/^#/ && !/^$/ \
       { lead = $1 / 188; \
         lead_offset = lead < 0x1F ? 0x81 : 0xC1; \
         trail = $1 % 188; \
         trail_offset = trail < 0x3F ? 0x40 : 0x41; \
         is_in_range = ($1 < 8272 || $1 > 8835); \
         tag = (is_in_range && has_seen[$2] == 0) ? 0 : 3; \
         printf ("<U%4s> \\x%02X\\x%02X |%d\n", substr($2, 3),\
                 lead + lead_offset, trail + trail_offset, tag);\
         if (is_in_range) has_seen[$2] = 1; \
       }' \
  index-jis0208.txt
}

# EUDC (End User Defined Characters)  is for decoding only
# (use '|3' to denote that).
# See https://encoding.spec.whatwg.org/#shift_jis-decoder - step 5
# This function is called twice with {0x40, 0x7E, 0x40} and {0x80, 0xFC, 0x41}
# to implement it.

function eudc {
  # The upper bound for the lead byte is 0xF8 because each lead can
  # have 188 characters and the total # of characters in the EUDC
  # is 1692 = 188 * (0xF9 - 0xF0) = 10528 - 8836 (see Shift_JIS decoder
  # step 3.5 in the encoding spec.)
  for lead in $(seq 0xF0 0xF8)
  do
    for byte in $(seq $1 $2)
    do
      offset=$3
      pointer=$((($lead - 0xC1) * 188 + $byte - $offset))
      unicode=$(($pointer - 8836 + 0xE000))
      printf "<U%4X> \\\x%02X\\\x%02X |3\n" $unicode $lead $byte
    done
  done
}

function unsorted_table {
  ascii
  half_width_kana
  jis208
  eudc "0x40" "0x7E" "0x40"
  eudc "0x80" "0xFC" "0x41"
  echo '<U00A5> \x5C |1'
  echo '<U203E> \x7E |1'
  echo '<U2212> \x81\x7C |1'
}

wget -N -r -nd https://encoding.spec.whatwg.org/index-jis0208.txt
preamble
unsorted_table | sort  | uniq
echo 'END CHARMAP'

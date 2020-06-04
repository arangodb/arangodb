#!/bin/sh
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# References:
#   https://encoding.spec.whatwg.org/#big5

# This script downloads the following file.
#   https://encoding.spec.whatwg.org/index-big5.txt

function preamble {
cat <<PREAMBLE
# ***************************************************************************
# *
# *   Copyright (C) 1995-2014, International Business Machines
# *   Corporation and others.  All Rights Reserved.
# *
# *   Generated per the algorithm for Big5
# *   described at http://encoding.spec.whatwg.org/#big5
# *
# ***************************************************************************
<code_set_name>               "big5-html"
<char_name_mask>              "AXXXX"
<mb_cur_max>                  2
<mb_cur_min>                  1
<uconv_class>                 "MBCS"
<subchar>                     \x3F
<icu:charsetFamily>           "ASCII"

# 'p' is for the range that may produce non-BMP code points.
# 'i' is to make the code range illegal.
# Big5 has a lot of small holes in the 2nd byte. If it's in the ASCII range,
# the 2nd byte has to be added back to the stream to be compliant to the
# encoding spec. Each state adds 1kB in the data size.
# See http://userguide.icu-project.org/conversion/data.
<icu:state>                   0-7f, a1-fe:1, 87-a0:2, c8:2, fa-fe:2, 87:3, 89:4, 8a:5, 8b:6, 8d:7, 9b:8, 9f:9, a0:a
<icu:state>                   40-7e, a1-fe
<icu:state>                   40-7e.p, a1-fe.p
<icu:state>                   40-7e.p, a1-fe.p, 66.i
<icu:state>                   40-7e.p, a1-fe.p, 42.i, 44.i, 45.i, 4a-4b.i
<icu:state>                   40-7e.p, a1-fe.p, 42.i, 63.i, 75.i
<icu:state>                   40-7e.p, a1-fe.p, 54.i
<icu:state>                   40-7e.p, a1-fe.p, 41.i
<icu:state>                   40-7e.p, a1-fe.p, 61.i
<icu:state>                   40-7e.p, a1-fe.p, 4e.i
<icu:state>                   40-7e.p, a1-fe.p, 54.i, 57.i, 5a.i, 62.i, 72.i

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
# Entries with pointer=528[79] and 5247 ~ 5250 have to be decoding-only
# even though they come before the other entry with the same Unicode
# character. The corresponding Unicode characters are U+255[0E],
# U+256[1A], and U+534[15].
# See https://www.w3.org/Bugs/Public/show_bug.cgi?id=27878
function big5 {
  awk '!/^#/ && !/^$/ \
       { pointer = $1; \
         ucs = substr($2, 3); \
         sortkey = (length(ucs) < 5) ? ("0" ucs) : ucs;
         lead = pointer / 157 + 0x81; \
         is_decoding_only = lead < 0xA1 || seen_before[ucs] || \
             pointer == 5287 || pointer == 5289 || \
             (5247 <= pointer && pointer <= 5250);
         trail = $1 % 157; \
         trail_offset = trail < 0x3F ? 0x40 : 0x62; \
         tag = (is_decoding_only ? 3 : 0); \
         printf ("<U%4s> \\x%02X\\x%02X |%d %s\n", ucs,\
                 lead,  trail + trail_offset, tag, sortkey);\
         seen_before[ucs] = is_decoding_only ? 0 : 1; \
       }' \
  index-big5.txt
}

function two_char_seq {
cat <<EOF
<U00CA><U0304> \x88\x62 |3 000CA
<U00CA><U030C> \x88\x64 |3 000CA
<U00EA><U0304> \x88\xA3 |3 000EA
<U00EA><U030C> \x88\xA5 |3 000EA
EOF
}

function unsorted_table {
  two_char_seq
  big5
}

wget -N -r -nd https://encoding.spec.whatwg.org/index-big5.txt
preamble
ascii
unsorted_table | sort -k4  | uniq | cut -f 1-3 -d ' '
echo 'END CHARMAP'

#!/bin/bash
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

function preamble {

encoding="$1"
cat <<PREAMBLE
# ***************************************************************************
# *
# *   Generated from index-$encoding.txt (
# *   https://encoding.spec.whatwg.org/index-${encoding}.txt )
# *   following the algorithm for the single byte legacy encoding
# *   described at http://encoding.spec.whatwg.org/#single-byte-decoder
# *
# ***************************************************************************
<code_set_name>               "${encoding}-html"
<char_name_mask>              "AXXXX"
<mb_cur_max>                  1
<mb_cur_min>                  1
<uconv_class>                 "SBCS"
<subchar>                     \x3F
<icu:charsetFamily>           "ASCII"

CHARMAP
PREAMBLE

}

# The list of html5 encodings. Note that iso-8859-8-i is not listed here
# because its mapping table is exactly the same as iso-8859-8. The difference
# is BiDi handling (logical vs visual).
encodings="ibm866 iso-8859-2 iso-8859-3 iso-8859-4 iso-8859-5 iso-8859-6\
           iso-8859-7 iso-8859-8 iso-8859-10 iso-8859-13 iso-8859-14\
           iso-8859-15 iso-8859-16 koi8-r koi8-u macintosh\
           windows-874 windows-1250 windows-1251 windows-1252 windows-1253\
           windows-1254 windows-1255 windows-1256 windows-1257 windows-1258\
           x-mac-cyrillic"

ENCODING_DIR="$(dirname "$0")/../source/data/mappings"
for e in ${encodings}
do
  output="${ENCODING_DIR}/${e}-html.ucm"
  index="index-${e}.txt"
  indexurl="https://encoding.spec.whatwg.org/index-${e}.txt"
  curl -o ${index} "${indexurl}"
  preamble ${e} > ${output}
  awk 'BEGIN \
       { \
         for (i=0; i < 0x80; ++i) \
         { \
           printf("<U%04X> \\x%02X |0\n", i, i);} \
         } \
       !/^#/ && !/^$/ \
       {
         printf ("<U%4s> \\x%02X |0\n", substr($2, 3), $1 + 0x80); \
       }' ${index} | sort >> ${output}
  echo 'END CHARMAP' >> ${output}
  rm ${index}
done


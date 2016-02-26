Mapping files for Japanese encodings

1998 12/25

Fuji Xerox Information Systems
MURATA Makoto

1.  Overview

This version of XML::Parser and XML::Encoding does not come with map files for
the charset "Shift_JIS" and the charset "euc-jp".  Unfortunately, each of these 
charsets has more than one mapping.  None of these mappings are
considered as authoritative.

Therefore, we have come to believe that it is dangerous to provide map files
for these charsets.  Rather, we introduce several private charsets and map
files for these private charsets.  If IANA, Unicode Consoritum, and JIS
eventually reach a consensus, we will be able to provide map files for
"Shift_JIS" and "euc-jp".

2. Different mappings from existing charsets to Unicode

1) Different mappings in JIS X0221 and Unicode

The mapping between JIS X0208:1990 and Unicode 1.1 and the mapping
between JIS X0212:1990 and Unicode 1.1 are published from Unicode
consortium.  They are available at 
ftp://ftp.unicode.org/Public/MAPPINGS/EASTASIA/JIS/JIS0208.TXT and 
ftp://ftp.unicode.org/Public/MAPPINGS/EASTASIA/JIS/JIS0212.TXT, 
respectively.)  These mapping files have a note as below:

#   The kanji mappings are a normative part of ISO/IEC 10646.  The
#       non-kanji mappings are provisional, pending definition of
#       official mappings by Japanese standards bodies.

Unfortunately, the non-kanji mappings in the Japanese standard for ISO 10646/1,
namely JIS X 0221:1995, is different from the Unicode Consortium mapping since
0x213D of JIS X 0208 is mapped to U+2014 (em dash) rather than U+2015
(horizontal bar).  Furthermore, JIS X 0221 clearly says that the mapping is
informational and non-normative.  As a result, some companies (e.g., Microsoft and
Apple) have introduced slightly different mappings.  Therefore, neither the
Unicode consortium mapping nor the JIS X 0221 mapping are considered as
authoritative.

2) Shift-JIS

This charset is especially problematic, since its definition has been unclear
since its inception.

The current registration of the charset "Shift_JIS" is as below:

>Name: Shift_JIS  (preferred MIME name)
>MIBenum: 17
>Source: A Microsoft code that extends csHalfWidthKatakana to include 
>       kanji by adding a second byte when the value of the first 
>       byte is in the ranges 81-9F or E0-EF.
>Alias: MS_Kanji 
>Alias: csShiftJIS

First, this does not reference to the mapping "Shift-JIS to Unicode" 
published by the Unicode consortium (available at
ftp://ftp.unicode.org/Public/MAPPINGS/EASTASIA/JIS/SHIFTJIS.TXT).

Second, "kanji" in this registration can be interepreted in different ways.
Does this "kanji" reference to JIS X0208:1978, JIS X0208:1983, or JIS
X0208:1990(== JIS X0208:1997)?  These three standards are *incompatible* with
each other.  Moreover, we can even argue that "kanji" refers to JIS X0212 or
ideographic characters in other countries.

Third, each company has extended Shift JIS. For example, Microsoft introduced
OEM extensions (NEC extensionsand IBM extensions).

Forth, Shift JIS uses JIS X0201, which is almost upper-compatible with US-ASCII
but is not quite.  5C and 7E of JIS X 0201 are different from backslash and 
tilde, respectively.  However, many programming languages (e.g., Java) 
ignore this difference and assumes that 5C and 7E of Shift JIS are backslash 
and tilde.


3.  Proposed charsets and mappings

As a tentative solution, we introduce two private charsets for EUC-JP and four
priviate charsets for Shift JIS.

1) EUC-JP

We have two charsets, namely "x-eucjp-unicode" and "x-eucjp-jisx0221".  Their 
difference is only one code point.  The mapping for the former is based 
on the Unicode Consortium mapping, while the latter is based on the JIS X0221 
mapping.

2) Shift JIS

We have four charsets, namely x-sjis-unicode, x-sjis-jisx0221, 
x-sjis-jdk117, and x-sjis-cp932.

The mapping for the charset x-sjis-unicode is the one published by the Unicode
consortium.  The mapping for x-sjis-jisx0221 is almost equivalent to
x-sjis-unicode, but 0x213D of JIS X 0208 is mapped to U+2014 (em dash) rather
than U+2015.  The charset x-sjis-jdk117 is again almost equivalent to
x-sjis-unicode, but 0x5C and 0x7E of JIS X0201 are mapped to backslash and
tilde.

The charset x-sjis-cp932 is used by Microsoft Windows, and its mapping is
published from the Unicode Consortium (available at:
ftp://ftp.unicode.org/Public/MAPPINGS/VENDORS/MICSFT/WINDOWS/CP932.txt).  The
coded character set for this charset includes NEC-extensions and
IBM-extensions.  0x5C and 0x7E of JIS X0201 are mapped to backslash and tilde;
0x213D is mapped to U+2015; and 0x2140, 0x2141, 0x2142, and 0x215E of JIS X
0208 are mapped to compatibility characters.

Makoto
 
Fuji Xerox Information Systems
 
Tel: +81-44-812-7230   Fax: +81-44-812-7231
E-mail: murata@apsdc.ksp.fujixerox.co.jp

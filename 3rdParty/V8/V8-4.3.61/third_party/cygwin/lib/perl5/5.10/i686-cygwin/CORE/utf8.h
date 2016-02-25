/*    utf8.h
 *
 *    Copyright (C) 2000, 2001, 2002, 2005, 2006, 2007, by Larry Wall and others
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

/* Use UTF-8 as the default script encoding?
 * Turning this on will break scripts having non-UTF-8 binary
 * data (such as Latin-1) in string literals. */
#ifdef USE_UTF8_SCRIPTS
#    define USE_UTF8_IN_NAMES (!IN_BYTES)
#else
#    define USE_UTF8_IN_NAMES (PL_hints & HINT_UTF8)
#endif

/* Source backward compatibility. */
#define uvuni_to_utf8(d, uv)		uvuni_to_utf8_flags(d, uv, 0)
#define is_utf8_string_loc(s, len, ep)	is_utf8_string_loclen(s, len, ep, 0)

#ifdef EBCDIC
/* The equivalent of these macros but implementing UTF-EBCDIC
   are in the following header file:
 */

#include "utfebcdic.h"

#else
START_EXTERN_C

#ifdef DOINIT
EXTCONST unsigned char PL_utf8skip[] = {
1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, /* ascii */
1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, /* ascii */
1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, /* ascii */
1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, /* ascii */
1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, /* bogus */
1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, /* bogus */
2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2, /* scripts */
3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,4,4,4,4,4,4,4,4,5,5,5,5,6,6,	 /* cjk etc. */
7,13, /* Perl extended (not UTF-8).  Up to 72bit allowed (64-bit + reserved). */
};
#else
EXTCONST unsigned char PL_utf8skip[];
#endif

END_EXTERN_C
#define UTF8SKIP(s) PL_utf8skip[*(const U8*)(s)]

/* Native character to iso-8859-1 */
#define NATIVE_TO_ASCII(ch)      (ch)
#define ASCII_TO_NATIVE(ch)      (ch)
/* Transform after encoding */
#define NATIVE_TO_UTF(ch)        (ch)
#define UTF_TO_NATIVE(ch)        (ch)
/* Transforms in wide UV chars */
#define UNI_TO_NATIVE(ch)        (ch)
#define NATIVE_TO_UNI(ch)        (ch)
/* Transforms in invariant space */
#define NATIVE_TO_NEED(enc,ch)   (ch)
#define ASCII_TO_NEED(enc,ch)    (ch)

/* As there are no translations avoid the function wrapper */
#define utf8n_to_uvchr utf8n_to_uvuni
#define uvchr_to_utf8  uvuni_to_utf8

/*

 The following table is from Unicode 3.2.

 Code Points		1st Byte  2nd Byte  3rd Byte  4th Byte

   U+0000..U+007F	00..7F
   U+0080..U+07FF	C2..DF    80..BF
   U+0800..U+0FFF	E0        A0..BF    80..BF
   U+1000..U+CFFF       E1..EC    80..BF    80..BF
   U+D000..U+D7FF       ED        80..9F    80..BF
   U+D800..U+DFFF       ******* ill-formed *******
   U+E000..U+FFFF       EE..EF    80..BF    80..BF
  U+10000..U+3FFFF	F0        90..BF    80..BF    80..BF
  U+40000..U+FFFFF	F1..F3    80..BF    80..BF    80..BF
 U+100000..U+10FFFF	F4        80..8F    80..BF    80..BF

Note the A0..BF in U+0800..U+0FFF, the 80..9F in U+D000...U+D7FF,
the 90..BF in U+10000..U+3FFFF, and the 80...8F in U+100000..U+10FFFF.
The "gaps" are caused by legal UTF-8 avoiding non-shortest encodings:
it is technically possible to UTF-8-encode a single code point in different
ways, but that is explicitly forbidden, and the shortest possible encoding
should always be used (and that is what Perl does).

 */

/*
 Another way to look at it, as bits:

 Code Points                    1st Byte   2nd Byte  3rd Byte  4th Byte

                    0aaaaaaa     0aaaaaaa
            00000bbbbbaaaaaa     110bbbbb  10aaaaaa
            ccccbbbbbbaaaaaa     1110cccc  10bbbbbb  10aaaaaa
  00000dddccccccbbbbbbaaaaaa     11110ddd  10cccccc  10bbbbbb  10aaaaaa

As you can see, the continuation bytes all begin with C<10>, and the
leading bits of the start byte tell how many bytes the are in the
encoded character.

*/


#define UNI_IS_INVARIANT(c)		(((UV)c) <  0x80)
#define UTF8_IS_INVARIANT(c)		UNI_IS_INVARIANT(NATIVE_TO_UTF(c))
#define NATIVE_IS_INVARIANT(c)		UNI_IS_INVARIANT(NATIVE_TO_ASCII(c))
#define UTF8_IS_START(c)		(((U8)c) >= 0xc0 && (((U8)c) <= 0xfd))
#define UTF8_IS_CONTINUATION(c)		(((U8)c) >= 0x80 && (((U8)c) <= 0xbf))
#define UTF8_IS_CONTINUED(c) 		(((U8)c) &  0x80)
#define UTF8_IS_DOWNGRADEABLE_START(c)	(((U8)c & 0xfc) == 0xc0)

#define UTF_START_MARK(len) ((len >  7) ? 0xFF : (0xFE << (7-len)))
#define UTF_START_MASK(len) ((len >= 7) ? 0x00 : (0x1F >> (len-2)))

#define UTF_CONTINUATION_MARK		0x80
#define UTF_ACCUMULATION_SHIFT		6
#define UTF_CONTINUATION_MASK		((U8)0x3f)
#define UTF8_ACCUMULATE(old, new)	(((old) << UTF_ACCUMULATION_SHIFT) | (((U8)new) & UTF_CONTINUATION_MASK))

#define UTF8_EIGHT_BIT_HI(c)	((((U8)(c))>>UTF_ACCUMULATION_SHIFT)|UTF_START_MARK(2))
#define UTF8_EIGHT_BIT_LO(c)	(((((U8)(c)))&UTF_CONTINUATION_MASK)|UTF_CONTINUATION_MARK)

#ifdef HAS_QUAD
#define UNISKIP(uv) ( (uv) < 0x80           ? 1 : \
		      (uv) < 0x800          ? 2 : \
		      (uv) < 0x10000        ? 3 : \
		      (uv) < 0x200000       ? 4 : \
		      (uv) < 0x4000000      ? 5 : \
		      (uv) < 0x80000000     ? 6 : \
                      (uv) < UTF8_QUAD_MAX ? 7 : 13 )
#else
/* No, I'm not even going to *TRY* putting #ifdef inside a #define */
#define UNISKIP(uv) ( (uv) < 0x80           ? 1 : \
		      (uv) < 0x800          ? 2 : \
		      (uv) < 0x10000        ? 3 : \
		      (uv) < 0x200000       ? 4 : \
		      (uv) < 0x4000000      ? 5 : \
		      (uv) < 0x80000000     ? 6 : 7 )
#endif

/*
 * Note: we try to be careful never to call the isXXX_utf8() functions
 * unless we're pretty sure we've seen the beginning of a UTF-8 character
 * (that is, the two high bits are set).  Otherwise we risk loading in the
 * heavy-duty swash_init and swash_fetch routines unnecessarily.
 */
#define isIDFIRST_lazy_if(p,c) ((IN_BYTES || (!c || (*((const U8*)p) < 0xc0))) \
				? isIDFIRST(*(p)) \
				: isIDFIRST_utf8((const U8*)p))
#define isALNUM_lazy_if(p,c)   ((IN_BYTES || (!c || (*((const U8*)p) < 0xc0))) \
				? isALNUM(*(p)) \
				: isALNUM_utf8((const U8*)p))


#endif /* EBCDIC vs ASCII */

/* Rest of these are attributes of Unicode and perl's internals rather than the encoding */

#define isIDFIRST_lazy(p)	isIDFIRST_lazy_if(p,1)
#define isALNUM_lazy(p)		isALNUM_lazy_if(p,1)

#define UTF8_MAXBYTES 13
/* How wide can a single UTF-8 encoded character become in bytes.
 * NOTE: Strictly speaking Perl's UTF-8 should not be called UTF-8
 * since UTF-8 is an encoding of Unicode and given Unicode's current
 * upper limit only four bytes is possible.  Perl thinks of UTF-8
 * as a way to encode non-negative integers in a binary format. */
#define UTF8_MAXLEN UTF8_MAXBYTES

#define UTF8_MAXLEN_UCLC 3		/* Obsolete, do not use. */
#define UTF8_MAXLEN_UCLC_MULT 39	/* Obsolete, do not use. */
#define UTF8_MAXLEN_FOLD 3		/* Obsolete, do not use. */
#define UTF8_MAXLEN_FOLD_MULT 39	/* Obsolete, do not use. */

/* The maximum number of UTF-8 bytes a single Unicode character can
 * uppercase/lowercase/fold into; this number depends on the Unicode
 * version.  An example of maximal expansion is the U+03B0 which
 * uppercases to U+03C5 U+0308 U+0301.  The Unicode databases that
 * tell these things are UnicodeDatabase.txt, CaseFolding.txt, and
 * SpecialCasing.txt. */
#define UTF8_MAXBYTES_CASE	6

#define IN_BYTES (CopHINTS_get(PL_curcop) & HINT_BYTES)
#define DO_UTF8(sv) (SvUTF8(sv) && !IN_BYTES)

#define UTF8_ALLOW_EMPTY		0x0001
#define UTF8_ALLOW_CONTINUATION		0x0002
#define UTF8_ALLOW_NON_CONTINUATION	0x0004
#define UTF8_ALLOW_FE_FF		0x0008 /* Allow above 0x7fffFFFF */
#define UTF8_ALLOW_SHORT		0x0010
#define UTF8_ALLOW_SURROGATE		0x0020
#define UTF8_ALLOW_FFFF			0x0040 /* Allow UNICODE_ILLEGAL */
#define UTF8_ALLOW_LONG			0x0080
#define UTF8_ALLOW_ANYUV		(UTF8_ALLOW_EMPTY|UTF8_ALLOW_FE_FF|\
					 UTF8_ALLOW_SURROGATE|UTF8_ALLOW_FFFF)
#define UTF8_ALLOW_ANY			0x00FF
#define UTF8_CHECK_ONLY			0x0200
#define UTF8_ALLOW_DEFAULT		(ckWARN(WARN_UTF8) ? 0 : \
					 UTF8_ALLOW_ANYUV)

#define UNICODE_SURROGATE_FIRST		0xD800
#define UNICODE_SURROGATE_LAST		0xDFFF
#define UNICODE_REPLACEMENT		0xFFFD
#define UNICODE_BYTE_ORDER_MARK		0xFEFF
#define UNICODE_ILLEGAL			0xFFFF

/* Though our UTF-8 encoding can go beyond this,
 * let's be conservative and do as Unicode 3.2 says. */
#define PERL_UNICODE_MAX	0x10FFFF

#define UNICODE_ALLOW_SURROGATE 0x0001	/* Allow UTF-16 surrogates (EVIL) */
#define UNICODE_ALLOW_FDD0	0x0002	/* Allow the U+FDD0...U+FDEF */
#define UNICODE_ALLOW_FFFF	0x0004	/* Allow U+FFF[EF], U+1FFF[EF], ... */
#define UNICODE_ALLOW_SUPER	0x0008	/* Allow past 0x10FFFF */
#define UNICODE_ALLOW_ANY	0x000F

#define UNICODE_IS_SURROGATE(c)		((c) >= UNICODE_SURROGATE_FIRST && \
					 (c) <= UNICODE_SURROGATE_LAST)
#define UNICODE_IS_REPLACEMENT(c)	((c) == UNICODE_REPLACEMENT)
#define UNICODE_IS_BYTE_ORDER_MARK(c)	((c) == UNICODE_BYTE_ORDER_MARK)
#define UNICODE_IS_ILLEGAL(c)		((c) == UNICODE_ILLEGAL)

#ifdef HAS_QUAD
#    define UTF8_QUAD_MAX	UINT64_C(0x1000000000)
#endif

#define UTF8_IS_ASCII(c) UTF8_IS_INVARIANT(c)

#define UNICODE_LATIN_SMALL_LETTER_SHARP_S	0x00DF
#define UNICODE_GREEK_CAPITAL_LETTER_SIGMA	0x03A3
#define UNICODE_GREEK_SMALL_LETTER_FINAL_SIGMA	0x03C2
#define UNICODE_GREEK_SMALL_LETTER_SIGMA	0x03C3

#define EBCDIC_LATIN_SMALL_LETTER_SHARP_S	0x0059

#define UNI_DISPLAY_ISPRINT	0x0001
#define UNI_DISPLAY_BACKSLASH	0x0002
#define UNI_DISPLAY_QQ		(UNI_DISPLAY_ISPRINT|UNI_DISPLAY_BACKSLASH)
#define UNI_DISPLAY_REGEX	(UNI_DISPLAY_ISPRINT|UNI_DISPLAY_BACKSLASH)

#ifdef EBCDIC
#   define ANYOF_FOLD_SHARP_S(node, input, end)	\
	(ANYOF_BITMAP_TEST(node, EBCDIC_LATIN_SMALL_LETTER_SHARP_S) && \
	 (ANYOF_FLAGS(node) & ANYOF_UNICODE) && \
	 (ANYOF_FLAGS(node) & ANYOF_FOLD) && \
	 ((end) > (input) + 1) && \
	 toLOWER((input)[0]) == 's' && \
	 toLOWER((input)[1]) == 's')
#else
#   define ANYOF_FOLD_SHARP_S(node, input, end)	\
	(ANYOF_BITMAP_TEST(node, UNICODE_LATIN_SMALL_LETTER_SHARP_S) && \
	 (ANYOF_FLAGS(node) & ANYOF_UNICODE) && \
	 (ANYOF_FLAGS(node) & ANYOF_FOLD) && \
	 ((end) > (input) + 1) && \
	 toLOWER((input)[0]) == 's' && \
	 toLOWER((input)[1]) == 's')
#endif
#define SHARP_S_SKIP 2

#ifdef EBCDIC
/* IS_UTF8_CHAR() is not ported to EBCDIC */
#else
#define IS_UTF8_CHAR_1(p)	\
	((p)[0] <= 0x7F)
#define IS_UTF8_CHAR_2(p)	\
	((p)[0] >= 0xC2 && (p)[0] <= 0xDF && \
	 (p)[1] >= 0x80 && (p)[1] <= 0xBF)
#define IS_UTF8_CHAR_3a(p)	\
	((p)[0] == 0xE0 && \
	 (p)[1] >= 0xA0 && (p)[1] <= 0xBF && \
	 (p)[2] >= 0x80 && (p)[2] <= 0xBF)
#define IS_UTF8_CHAR_3b(p)	\
	((p)[0] >= 0xE1 && (p)[0] <= 0xEC && \
	 (p)[1] >= 0x80 && (p)[1] <= 0xBF && \
	 (p)[2] >= 0x80 && (p)[2] <= 0xBF)
#define IS_UTF8_CHAR_3c(p)	\
	((p)[0] == 0xED && \
	 (p)[1] >= 0x80 && (p)[1] <= 0xBF && \
	 (p)[2] >= 0x80 && (p)[2] <= 0xBF)
/* In IS_UTF8_CHAR_3c(p) one could use
 * (p)[1] >= 0x80 && (p)[1] <= 0x9F
 * if one wanted to exclude surrogates. */
#define IS_UTF8_CHAR_3d(p)	\
	((p)[0] >= 0xEE && (p)[0] <= 0xEF && \
	 (p)[1] >= 0x80 && (p)[1] <= 0xBF && \
	 (p)[2] >= 0x80 && (p)[2] <= 0xBF)
#define IS_UTF8_CHAR_4a(p)	\
	((p)[0] == 0xF0 && \
	 (p)[1] >= 0x90 && (p)[1] <= 0xBF && \
	 (p)[2] >= 0x80 && (p)[2] <= 0xBF && \
	 (p)[3] >= 0x80 && (p)[3] <= 0xBF)
#define IS_UTF8_CHAR_4b(p)	\
	((p)[0] >= 0xF1 && (p)[0] <= 0xF3 && \
	 (p)[1] >= 0x80 && (p)[1] <= 0xBF && \
	 (p)[2] >= 0x80 && (p)[2] <= 0xBF && \
	 (p)[3] >= 0x80 && (p)[3] <= 0xBF)
/* In IS_UTF8_CHAR_4c(p) one could use
 * (p)[0] == 0xF4
 * if one wanted to stop at the Unicode limit U+10FFFF.
 * The 0xF7 allows us to go to 0x1fffff (0x200000 would
 * require five bytes).  Not doing any further code points
 * since that is not needed (and that would not be strict
 * UTF-8, anyway).  The "slow path" in Perl_is_utf8_char()
 * will take care of the "extended UTF-8". */
#define IS_UTF8_CHAR_4c(p)	\
	((p)[0] == 0xF4 && (p)[0] <= 0xF7 && \
	 (p)[1] >= 0x80 && (p)[1] <= 0xBF && \
	 (p)[2] >= 0x80 && (p)[2] <= 0xBF && \
	 (p)[3] >= 0x80 && (p)[3] <= 0xBF)

#define IS_UTF8_CHAR_3(p)	\
	(IS_UTF8_CHAR_3a(p) || \
	 IS_UTF8_CHAR_3b(p) || \
	 IS_UTF8_CHAR_3c(p) || \
	 IS_UTF8_CHAR_3d(p))
#define IS_UTF8_CHAR_4(p)	\
	(IS_UTF8_CHAR_4a(p) || \
	 IS_UTF8_CHAR_4b(p) || \
	 IS_UTF8_CHAR_4c(p))

/* IS_UTF8_CHAR(p) is strictly speaking wrong (not UTF-8) because it
 * (1) allows UTF-8 encoded UTF-16 surrogates
 * (2) it allows code points past U+10FFFF.
 * The Perl_is_utf8_char() full "slow" code will handle the Perl
 * "extended UTF-8". */
#define IS_UTF8_CHAR(p, n)	\
	((n) == 1 ? IS_UTF8_CHAR_1(p) : \
 	 (n) == 2 ? IS_UTF8_CHAR_2(p) : \
	 (n) == 3 ? IS_UTF8_CHAR_3(p) : \
	 (n) == 4 ? IS_UTF8_CHAR_4(p) : 0)

#define IS_UTF8_CHAR_FAST(n) ((n) <= 4)

#endif /* IS_UTF8_CHAR() for UTF-8 */

/*
 * Local variables:
 * c-indentation-style: bsd
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 *
 * ex: set ts=8 sts=4 sw=4 noet:
 */

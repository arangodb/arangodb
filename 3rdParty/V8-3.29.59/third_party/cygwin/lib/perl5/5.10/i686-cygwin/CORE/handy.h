/*    handy.h
 *
 *    Copyright (C) 1991, 1992, 1993, 1994, 1995, 1996, 1997, 1999,
 *    2000, 2001, 2002, 2004, 2005, 2006, 2007, by Larry Wall and others
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

#if !defined(__STDC__)
#ifdef NULL
#undef NULL
#endif
#ifndef I286
#  define NULL 0
#else
#  define NULL 0L
#endif
#endif

#define Null(type) ((type)NULL)

/*
=head1 Handy Values

=for apidoc AmU||Nullch
Null character pointer.

=for apidoc AmU||Nullsv
Null SV pointer.

=cut
*/

#define Nullch Null(char*)
#define Nullfp Null(PerlIO*)
#define Nullsv Null(SV*)

#ifdef TRUE
#undef TRUE
#endif
#ifdef FALSE
#undef FALSE
#endif
#define TRUE (1)
#define FALSE (0)


/* XXX Configure ought to have a test for a boolean type, if I can
   just figure out all the headers such a test needs.
   Andy Dougherty	August 1996
*/
/* bool is built-in for g++-2.6.3 and later, which might be used
   for extensions.  <_G_config.h> defines _G_HAVE_BOOL, but we can't
   be sure _G_config.h will be included before this file.  _G_config.h
   also defines _G_HAVE_BOOL for both gcc and g++, but only g++
   actually has bool.  Hence, _G_HAVE_BOOL is pretty useless for us.
   g++ can be identified by __GNUG__.
   Andy Dougherty	February 2000
*/
#ifdef __GNUG__		/* GNU g++ has bool built-in */
#  ifndef HAS_BOOL
#    define HAS_BOOL 1
#  endif
#endif

/* The NeXT dynamic loader headers will not build with the bool macro
   So declare them now to clear confusion.
*/
#if defined(NeXT) || defined(__NeXT__)
# undef FALSE
# undef TRUE
  typedef enum bool { FALSE = 0, TRUE = 1 } bool;
# define ENUM_BOOL 1
# ifndef HAS_BOOL
#  define HAS_BOOL 1
# endif /* !HAS_BOOL */
#endif /* NeXT || __NeXT__ */

#ifndef HAS_BOOL
# if defined(UTS) || defined(VMS)
#  define bool int
# else
#  define bool char
# endif
# define HAS_BOOL 1
#endif

/* Try to figure out __func__ or __FUNCTION__ equivalent, if any.
 * XXX Should really be a Configure probe, with HAS__FUNCTION__
 *     and FUNCTION__ as results.
 * XXX Similarly, a Configure probe for __FILE__ and __LINE__ is needed. */
#if (defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L) || (defined(__SUNPRO_C)) /* C99 or close enough. */
#  define FUNCTION__ __func__
#else
#  if (defined(_MSC_VER) && _MSC_VER < 1300) || /* Pre-MSVC 7.0 has neither __func__ nor __FUNCTION and no good workarounds, either. */ \
      (defined(__DECC_VER)) /* Tru64 or VMS, and strict C89 being used, but not modern enough cc (in Tur64, -c99 not known, only -std1). */
#    define FUNCTION__ ""
#  else
#    define FUNCTION__ __FUNCTION__ /* Common extension. */
#  endif
#endif

/* XXX A note on the perl source internal type system.  The
   original intent was that I32 be *exactly* 32 bits.

   Currently, we only guarantee that I32 is *at least* 32 bits.
   Specifically, if int is 64 bits, then so is I32.  (This is the case
   for the Cray.)  This has the advantage of meshing nicely with
   standard library calls (where we pass an I32 and the library is
   expecting an int), but the disadvantage that an I32 is not 32 bits.
   Andy Dougherty	August 1996

   There is no guarantee that there is *any* integral type with
   exactly 32 bits.  It is perfectly legal for a system to have
   sizeof(short) == sizeof(int) == sizeof(long) == 8.

   Similarly, there is no guarantee that I16 and U16 have exactly 16
   bits.

   For dealing with issues that may arise from various 32/64-bit
   systems, we will ask Configure to check out

	SHORTSIZE == sizeof(short)
	INTSIZE == sizeof(int)
	LONGSIZE == sizeof(long)
	LONGLONGSIZE == sizeof(long long) (if HAS_LONG_LONG)
	PTRSIZE == sizeof(void *)
	DOUBLESIZE == sizeof(double)
	LONG_DOUBLESIZE == sizeof(long double) (if HAS_LONG_DOUBLE).

*/

#ifdef I_INTTYPES /* e.g. Linux has int64_t without <inttypes.h> */
#   include <inttypes.h>
#   ifdef INT32_MIN_BROKEN
#       undef  INT32_MIN
#       define INT32_MIN (-2147483647-1)
#   endif
#   ifdef INT64_MIN_BROKEN
#       undef  INT64_MIN
#       define INT64_MIN (-9223372036854775807LL-1)
#   endif
#endif

typedef I8TYPE I8;
typedef U8TYPE U8;
typedef I16TYPE I16;
typedef U16TYPE U16;
typedef I32TYPE I32;
typedef U32TYPE U32;
#ifdef PERL_CORE
#   ifdef HAS_QUAD
typedef I64TYPE I64;
typedef U64TYPE U64;
#   endif
#endif /* PERL_CORE */

#if defined(HAS_QUAD) && defined(USE_64_BIT_INT)
#   ifndef UINT64_C /* usually from <inttypes.h> */
#       if defined(HAS_LONG_LONG) && QUADKIND == QUAD_IS_LONG_LONG
#           define INT64_C(c)	CAT2(c,LL)
#           define UINT64_C(c)	CAT2(c,ULL)
#       else
#           if LONGSIZE == 8 && QUADKIND == QUAD_IS_LONG
#               define INT64_C(c)	CAT2(c,L)
#               define UINT64_C(c)	CAT2(c,UL)
#           else
#               define INT64_C(c)	((I64TYPE)(c))
#               define UINT64_C(c)	((U64TYPE)(c))
#           endif
#       endif
#   endif
#endif

/* HMB H.Merijn Brand - a placeholder for preparing Configure patches */
#if defined(LOCALTIME_R_NEEDS_TZSET) && defined(HAS_PSEUDOFORK) && defined(USE_DTRACE)
/* Not (yet) used at top level, but mention them for metaconfig */
#endif

/* Mention I8SIZE, U8SIZE, I16SIZE, U16SIZE, I32SIZE, U32SIZE,
   I64SIZE, and U64SIZE here so that metaconfig pulls them in. */

#if defined(UINT8_MAX) && defined(INT16_MAX) && defined(INT32_MAX)

/* I8_MAX and I8_MIN constants are not defined, as I8 is an ambiguous type.
   Please search CHAR_MAX in perl.h for further details. */
#define U8_MAX UINT8_MAX
#define U8_MIN UINT8_MIN

#define I16_MAX INT16_MAX
#define I16_MIN INT16_MIN
#define U16_MAX UINT16_MAX
#define U16_MIN UINT16_MIN

#define I32_MAX INT32_MAX
#define I32_MIN INT32_MIN
#ifndef UINT32_MAX_BROKEN /* e.g. HP-UX with gcc messes this up */
#  define U32_MAX UINT32_MAX
#else
#  define U32_MAX 4294967295U
#endif
#define U32_MIN UINT32_MIN

#else

/* I8_MAX and I8_MIN constants are not defined, as I8 is an ambiguous type.
   Please search CHAR_MAX in perl.h for further details. */
#define U8_MAX PERL_UCHAR_MAX
#define U8_MIN PERL_UCHAR_MIN

#define I16_MAX PERL_SHORT_MAX
#define I16_MIN PERL_SHORT_MIN
#define U16_MAX PERL_USHORT_MAX
#define U16_MIN PERL_USHORT_MIN

#if LONGSIZE > 4
# define I32_MAX PERL_INT_MAX
# define I32_MIN PERL_INT_MIN
# define U32_MAX PERL_UINT_MAX
# define U32_MIN PERL_UINT_MIN
#else
# define I32_MAX PERL_LONG_MAX
# define I32_MIN PERL_LONG_MIN
# define U32_MAX PERL_ULONG_MAX
# define U32_MIN PERL_ULONG_MIN
#endif

#endif

/* log(2) is pretty close to  0.30103, just in case anyone is grepping for it */
#define BIT_DIGITS(N)   (((N)*146)/485 + 1)  /* log2(10) =~ 146/485 */
#define TYPE_DIGITS(T)  BIT_DIGITS(sizeof(T) * 8)
#define TYPE_CHARS(T)   (TYPE_DIGITS(T) + 2) /* sign, NUL */

#define Ctl(ch) ((ch) & 037)

/*
=head1 SV-Body Allocation

=for apidoc Ama|SV*|newSVpvs|const char* s
Like C<newSVpvn>, but takes a literal string instead of a string/length pair.

=for apidoc Ama|SV*|newSVpvs_flags|const char* s|U32 flags
Like C<newSVpvn_flags>, but takes a literal string instead of a string/length
pair.

=for apidoc Ama|SV*|newSVpvs_share|const char* s
Like C<newSVpvn_share>, but takes a literal string instead of a string/length
pair and omits the hash parameter.

=for apidoc Am|void|sv_catpvs|SV* sv|const char* s
Like C<sv_catpvn>, but takes a literal string instead of a string/length pair.

=for apidoc Am|void|sv_setpvs|SV* sv|const char* s
Like C<sv_setpvn>, but takes a literal string instead of a string/length pair.

=head1 Memory Management

=for apidoc Ama|char*|savepvs|const char* s
Like C<savepvn>, but takes a literal string instead of a string/length pair.

=head1 GV Functions

=for apidoc Am|HV*|gv_stashpvs|const char* name|I32 create
Like C<gv_stashpvn>, but takes a literal string instead of a string/length pair.

=head1 Hash Manipulation Functions

=for apidoc Am|SV**|hv_fetchs|HV* tb|const char* key|I32 lval
Like C<hv_fetch>, but takes a literal string instead of a string/length pair.

=for apidoc Am|SV**|hv_stores|HV* tb|const char* key|NULLOK SV* val
Like C<hv_store>, but takes a literal string instead of a string/length pair
and omits the hash parameter.

=cut
*/

/* concatenating with "" ensures that only literal strings are accepted as argument */
#define STR_WITH_LEN(s)  (s ""), (sizeof(s)-1)

/* note that STR_WITH_LEN() can't be used as argument to macros or functions that
 * under some configurations might be macros, which means that it requires the full
 * Perl_xxx(aTHX_ ...) form for any API calls where it's used.
 */

/* STR_WITH_LEN() shortcuts */
#define newSVpvs(str) Perl_newSVpvn(aTHX_ STR_WITH_LEN(str))
#define newSVpvs_flags(str,flags)	\
    Perl_newSVpvn_flags(aTHX_ STR_WITH_LEN(str), flags)
#define newSVpvs_share(str) Perl_newSVpvn_share(aTHX_ STR_WITH_LEN(str), 0)
#define sv_catpvs(sv, str) Perl_sv_catpvn_flags(aTHX_ sv, STR_WITH_LEN(str), SV_GMAGIC)
#define sv_setpvs(sv, str) Perl_sv_setpvn(aTHX_ sv, STR_WITH_LEN(str))
#define savepvs(str) Perl_savepvn(aTHX_ STR_WITH_LEN(str))
#define gv_stashpvs(str, create) Perl_gv_stashpvn(aTHX_ STR_WITH_LEN(str), create)
#define gv_fetchpvs(namebeg, add, sv_type) Perl_gv_fetchpvn_flags(aTHX_ STR_WITH_LEN(namebeg), add, sv_type)
#define hv_fetchs(hv,key,lval)						\
  ((SV **)Perl_hv_common(aTHX_ (hv), NULL, STR_WITH_LEN(key), 0,	\
			 (lval) ? (HV_FETCH_JUST_SV | HV_FETCH_LVALUE)	\
			 : HV_FETCH_JUST_SV, NULL, 0))

#define hv_stores(hv,key,val)						\
  ((SV **)Perl_hv_common(aTHX_ (hv), NULL, STR_WITH_LEN(key), 0,	\
			 (HV_FETCH_ISSTORE|HV_FETCH_JUST_SV), (val), 0))


/*
=head1 Miscellaneous Functions

=for apidoc Am|bool|strNE|char* s1|char* s2
Test two strings to see if they are different.  Returns true or
false.

=for apidoc Am|bool|strEQ|char* s1|char* s2
Test two strings to see if they are equal.  Returns true or false.

=for apidoc Am|bool|strLT|char* s1|char* s2
Test two strings to see if the first, C<s1>, is less than the second,
C<s2>.  Returns true or false.

=for apidoc Am|bool|strLE|char* s1|char* s2
Test two strings to see if the first, C<s1>, is less than or equal to the
second, C<s2>.  Returns true or false.

=for apidoc Am|bool|strGT|char* s1|char* s2
Test two strings to see if the first, C<s1>, is greater than the second,
C<s2>.  Returns true or false.

=for apidoc Am|bool|strGE|char* s1|char* s2
Test two strings to see if the first, C<s1>, is greater than or equal to
the second, C<s2>.  Returns true or false.

=for apidoc Am|bool|strnNE|char* s1|char* s2|STRLEN len
Test two strings to see if they are different.  The C<len> parameter
indicates the number of bytes to compare.  Returns true or false. (A
wrapper for C<strncmp>).

=for apidoc Am|bool|strnEQ|char* s1|char* s2|STRLEN len
Test two strings to see if they are equal.  The C<len> parameter indicates
the number of bytes to compare.  Returns true or false. (A wrapper for
C<strncmp>).

=cut
*/

#define strNE(s1,s2) (strcmp(s1,s2))
#define strEQ(s1,s2) (!strcmp(s1,s2))
#define strLT(s1,s2) (strcmp(s1,s2) < 0)
#define strLE(s1,s2) (strcmp(s1,s2) <= 0)
#define strGT(s1,s2) (strcmp(s1,s2) > 0)
#define strGE(s1,s2) (strcmp(s1,s2) >= 0)
#define strnNE(s1,s2,l) (strncmp(s1,s2,l))
#define strnEQ(s1,s2,l) (!strncmp(s1,s2,l))

#ifdef HAS_MEMCMP
#  define memNE(s1,s2,l) (memcmp(s1,s2,l))
#  define memEQ(s1,s2,l) (!memcmp(s1,s2,l))
#else
#  define memNE(s1,s2,l) (bcmp(s1,s2,l))
#  define memEQ(s1,s2,l) (!bcmp(s1,s2,l))
#endif

#define memEQs(s1, l, s2) \
	(sizeof(s2)-1 == l && memEQ(s1, (s2 ""), (sizeof(s2)-1)))
#define memNEs(s1, l, s2) !memEQs(s1, l, s2)

/*
 * Character classes.
 *
 * Unfortunately, the introduction of locales means that we
 * can't trust isupper(), etc. to tell the truth.  And when
 * it comes to /\w+/ with tainting enabled, we *must* be able
 * to trust our character classes.
 *
 * Therefore, the default tests in the text of Perl will be
 * independent of locale.  Any code that wants to depend on
 * the current locale will use the tests that begin with "lc".
 */

#ifdef HAS_SETLOCALE  /* XXX Is there a better test for this? */
#  ifndef CTYPE256
#    define CTYPE256
#  endif
#endif

/*

=head1 Character classes

=for apidoc Am|bool|isALNUM|char ch
Returns a boolean indicating whether the C C<char> is an ASCII alphanumeric
character (including underscore) or digit.

=for apidoc Am|bool|isALPHA|char ch
Returns a boolean indicating whether the C C<char> is an ASCII alphabetic
character.

=for apidoc Am|bool|isSPACE|char ch
Returns a boolean indicating whether the C C<char> is whitespace.

=for apidoc Am|bool|isDIGIT|char ch
Returns a boolean indicating whether the C C<char> is an ASCII
digit.

=for apidoc Am|bool|isUPPER|char ch
Returns a boolean indicating whether the C C<char> is an uppercase
character.

=for apidoc Am|bool|isLOWER|char ch
Returns a boolean indicating whether the C C<char> is a lowercase
character.

=for apidoc Am|char|toUPPER|char ch
Converts the specified character to uppercase.

=for apidoc Am|char|toLOWER|char ch
Converts the specified character to lowercase.

=cut
*/

#define isALNUM(c)	(isALPHA(c) || isDIGIT(c) || (c) == '_')
#define isIDFIRST(c)	(isALPHA(c) || (c) == '_')
#define isALPHA(c)	(isUPPER(c) || isLOWER(c))
#define isSPACE(c) \
	((c) == ' ' || (c) == '\t' || (c) == '\n' || (c) =='\r' || (c) == '\f')
#define isPSXSPC(c)	(isSPACE(c) || (c) == '\v')
#define isBLANK(c)	((c) == ' ' || (c) == '\t')
#define isDIGIT(c)	((c) >= '0' && (c) <= '9')
#ifdef EBCDIC
    /* In EBCDIC we do not do locales: therefore() isupper() is fine. */
#   define isUPPER(c)	isupper(c)
#   define isLOWER(c)	islower(c)
#   define isALNUMC(c)	isalnum(c)
#   define isASCII(c)	isascii(c)
#   define isCNTRL(c)	iscntrl(c)
#   define isGRAPH(c)	isgraph(c)
#   define isPRINT(c)	isprint(c)
#   define isPUNCT(c)	ispunct(c)
#   define isXDIGIT(c)	isxdigit(c)
#   define toUPPER(c)	toupper(c)
#   define toLOWER(c)	tolower(c)
#else
#   define isUPPER(c)	((c) >= 'A' && (c) <= 'Z')
#   define isLOWER(c)	((c) >= 'a' && (c) <= 'z')
#   define isALNUMC(c)	(isALPHA(c) || isDIGIT(c))
#   define isASCII(c)	((c) <= 127)
#   define isCNTRL(c)	((c) < ' ' || (c) == 127)
#   define isGRAPH(c)	(isALNUM(c) || isPUNCT(c))
#   define isPRINT(c)	(((c) >= 32 && (c) < 127))
#   define isPUNCT(c)	(((c) >= 33 && (c) <= 47) || ((c) >= 58 && (c) <= 64)  || ((c) >= 91 && (c) <= 96) || ((c) >= 123 && (c) <= 126))
#   define isXDIGIT(c)  (isDIGIT(c) || ((c) >= 'a' && (c) <= 'f') || ((c) >= 'A' && (c) <= 'F'))
#   define toUPPER(c)	(isLOWER(c) ? (c) - ('a' - 'A') : (c))
#   define toLOWER(c)	(isUPPER(c) ? (c) + ('a' - 'A') : (c))
#endif

#ifdef USE_NEXT_CTYPE

#  define isALNUM_LC(c) \
	(NXIsAlNum((unsigned int)(c)) || (char)(c) == '_')
#  define isIDFIRST_LC(c) \
	(NXIsAlpha((unsigned int)(c)) || (char)(c) == '_')
#  define isALPHA_LC(c)		NXIsAlpha((unsigned int)(c))
#  define isSPACE_LC(c)		NXIsSpace((unsigned int)(c))
#  define isDIGIT_LC(c)		NXIsDigit((unsigned int)(c))
#  define isUPPER_LC(c)		NXIsUpper((unsigned int)(c))
#  define isLOWER_LC(c)		NXIsLower((unsigned int)(c))
#  define isALNUMC_LC(c)	NXIsAlNum((unsigned int)(c))
#  define isCNTRL_LC(c)		NXIsCntrl((unsigned int)(c))
#  define isGRAPH_LC(c)		NXIsGraph((unsigned int)(c))
#  define isPRINT_LC(c)		NXIsPrint((unsigned int)(c))
#  define isPUNCT_LC(c)		NXIsPunct((unsigned int)(c))
#  define toUPPER_LC(c)		NXToUpper((unsigned int)(c))
#  define toLOWER_LC(c)		NXToLower((unsigned int)(c))

#else /* !USE_NEXT_CTYPE */

#  if defined(CTYPE256) || (!defined(isascii) && !defined(HAS_ISASCII))

#    define isALNUM_LC(c)   (isalnum((unsigned char)(c)) || (char)(c) == '_')
#    define isIDFIRST_LC(c) (isalpha((unsigned char)(c)) || (char)(c) == '_')
#    define isALPHA_LC(c)	isalpha((unsigned char)(c))
#    define isSPACE_LC(c)	isspace((unsigned char)(c))
#    define isDIGIT_LC(c)	isdigit((unsigned char)(c))
#    define isUPPER_LC(c)	isupper((unsigned char)(c))
#    define isLOWER_LC(c)	islower((unsigned char)(c))
#    define isALNUMC_LC(c)	isalnum((unsigned char)(c))
#    define isCNTRL_LC(c)	iscntrl((unsigned char)(c))
#    define isGRAPH_LC(c)	isgraph((unsigned char)(c))
#    define isPRINT_LC(c)	isprint((unsigned char)(c))
#    define isPUNCT_LC(c)	ispunct((unsigned char)(c))
#    define toUPPER_LC(c)	toupper((unsigned char)(c))
#    define toLOWER_LC(c)	tolower((unsigned char)(c))

#  else

#    define isALNUM_LC(c)	(isascii(c) && (isalnum(c) || (c) == '_'))
#    define isIDFIRST_LC(c)	(isascii(c) && (isalpha(c) || (c) == '_'))
#    define isALPHA_LC(c)	(isascii(c) && isalpha(c))
#    define isSPACE_LC(c)	(isascii(c) && isspace(c))
#    define isDIGIT_LC(c)	(isascii(c) && isdigit(c))
#    define isUPPER_LC(c)	(isascii(c) && isupper(c))
#    define isLOWER_LC(c)	(isascii(c) && islower(c))
#    define isALNUMC_LC(c)	(isascii(c) && isalnum(c))
#    define isCNTRL_LC(c)	(isascii(c) && iscntrl(c))
#    define isGRAPH_LC(c)	(isascii(c) && isgraph(c))
#    define isPRINT_LC(c)	(isascii(c) && isprint(c))
#    define isPUNCT_LC(c)	(isascii(c) && ispunct(c))
#    define toUPPER_LC(c)	toupper(c)
#    define toLOWER_LC(c)	tolower(c)

#  endif
#endif /* USE_NEXT_CTYPE */

#define isPSXSPC_LC(c)		(isSPACE_LC(c) || (c) == '\v')
#define isBLANK_LC(c)		isBLANK(c) /* could be wrong */

#define isALNUM_uni(c)		is_uni_alnum(c)
#define isIDFIRST_uni(c)	is_uni_idfirst(c)
#define isALPHA_uni(c)		is_uni_alpha(c)
#define isSPACE_uni(c)		is_uni_space(c)
#define isDIGIT_uni(c)		is_uni_digit(c)
#define isUPPER_uni(c)		is_uni_upper(c)
#define isLOWER_uni(c)		is_uni_lower(c)
#define isALNUMC_uni(c)		is_uni_alnumc(c)
#define isASCII_uni(c)		is_uni_ascii(c)
#define isCNTRL_uni(c)		is_uni_cntrl(c)
#define isGRAPH_uni(c)		is_uni_graph(c)
#define isPRINT_uni(c)		is_uni_print(c)
#define isPUNCT_uni(c)		is_uni_punct(c)
#define isXDIGIT_uni(c)		is_uni_xdigit(c)
#define toUPPER_uni(c,s,l)	to_uni_upper(c,s,l)
#define toTITLE_uni(c,s,l)	to_uni_title(c,s,l)
#define toLOWER_uni(c,s,l)	to_uni_lower(c,s,l)
#define toFOLD_uni(c,s,l)	to_uni_fold(c,s,l)

#define isPSXSPC_uni(c)		(isSPACE_uni(c) ||(c) == '\f')
#define isBLANK_uni(c)		isBLANK(c) /* could be wrong */

#define isALNUM_LC_uvchr(c)	(c < 256 ? isALNUM_LC(c) : is_uni_alnum_lc(c))
#define isIDFIRST_LC_uvchr(c)	(c < 256 ? isIDFIRST_LC(c) : is_uni_idfirst_lc(c))
#define isALPHA_LC_uvchr(c)	(c < 256 ? isALPHA_LC(c) : is_uni_alpha_lc(c))
#define isSPACE_LC_uvchr(c)	(c < 256 ? isSPACE_LC(c) : is_uni_space_lc(c))
#define isDIGIT_LC_uvchr(c)	(c < 256 ? isDIGIT_LC(c) : is_uni_digit_lc(c))
#define isUPPER_LC_uvchr(c)	(c < 256 ? isUPPER_LC(c) : is_uni_upper_lc(c))
#define isLOWER_LC_uvchr(c)	(c < 256 ? isLOWER_LC(c) : is_uni_lower_lc(c))
#define isALNUMC_LC_uvchr(c)	(c < 256 ? isALNUMC_LC(c) : is_uni_alnumc_lc(c))
#define isCNTRL_LC_uvchr(c)	(c < 256 ? isCNTRL_LC(c) : is_uni_cntrl_lc(c))
#define isGRAPH_LC_uvchr(c)	(c < 256 ? isGRAPH_LC(c) : is_uni_graph_lc(c))
#define isPRINT_LC_uvchr(c)	(c < 256 ? isPRINT_LC(c) : is_uni_print_lc(c))
#define isPUNCT_LC_uvchr(c)	(c < 256 ? isPUNCT_LC(c) : is_uni_punct_lc(c))

#define isPSXSPC_LC_uni(c)	(isSPACE_LC_uni(c) ||(c) == '\f')
#define isBLANK_LC_uni(c)	isBLANK(c) /* could be wrong */

#define isALNUM_utf8(p)		is_utf8_alnum(p)
/* The ID_Start of Unicode is quite limiting: it assumes a L-class
 * character (meaning that you cannot have, say, a CJK character).
 * Instead, let's allow ID_Continue but not digits. */
#define isIDFIRST_utf8(p)	(is_utf8_idcont(p) && !is_utf8_digit(p))
#define isALPHA_utf8(p)		is_utf8_alpha(p)
#define isSPACE_utf8(p)		is_utf8_space(p)
#define isDIGIT_utf8(p)		is_utf8_digit(p)
#define isUPPER_utf8(p)		is_utf8_upper(p)
#define isLOWER_utf8(p)		is_utf8_lower(p)
#define isALNUMC_utf8(p)	is_utf8_alnumc(p)
#define isASCII_utf8(p)		is_utf8_ascii(p)
#define isCNTRL_utf8(p)		is_utf8_cntrl(p)
#define isGRAPH_utf8(p)		is_utf8_graph(p)
#define isPRINT_utf8(p)		is_utf8_print(p)
#define isPUNCT_utf8(p)		is_utf8_punct(p)
#define isXDIGIT_utf8(p)	is_utf8_xdigit(p)
#define toUPPER_utf8(p,s,l)	to_utf8_upper(p,s,l)
#define toTITLE_utf8(p,s,l)	to_utf8_title(p,s,l)
#define toLOWER_utf8(p,s,l)	to_utf8_lower(p,s,l)

#define isPSXSPC_utf8(c)	(isSPACE_utf8(c) ||(c) == '\f')
#define isBLANK_utf8(c)		isBLANK(c) /* could be wrong */

#define isALNUM_LC_utf8(p)	isALNUM_LC_uvchr(utf8_to_uvchr(p,  0))
#define isIDFIRST_LC_utf8(p)	isIDFIRST_LC_uvchr(utf8_to_uvchr(p,  0))
#define isALPHA_LC_utf8(p)	isALPHA_LC_uvchr(utf8_to_uvchr(p,  0))
#define isSPACE_LC_utf8(p)	isSPACE_LC_uvchr(utf8_to_uvchr(p,  0))
#define isDIGIT_LC_utf8(p)	isDIGIT_LC_uvchr(utf8_to_uvchr(p,  0))
#define isUPPER_LC_utf8(p)	isUPPER_LC_uvchr(utf8_to_uvchr(p,  0))
#define isLOWER_LC_utf8(p)	isLOWER_LC_uvchr(utf8_to_uvchr(p,  0))
#define isALNUMC_LC_utf8(p)	isALNUMC_LC_uvchr(utf8_to_uvchr(p,  0))
#define isCNTRL_LC_utf8(p)	isCNTRL_LC_uvchr(utf8_to_uvchr(p,  0))
#define isGRAPH_LC_utf8(p)	isGRAPH_LC_uvchr(utf8_to_uvchr(p,  0))
#define isPRINT_LC_utf8(p)	isPRINT_LC_uvchr(utf8_to_uvchr(p,  0))
#define isPUNCT_LC_utf8(p)	isPUNCT_LC_uvchr(utf8_to_uvchr(p,  0))

#define isPSXSPC_LC_utf8(c)	(isSPACE_LC_utf8(c) ||(c) == '\f')
#define isBLANK_LC_utf8(c)	isBLANK(c) /* could be wrong */

#ifdef EBCDIC
#  ifdef PERL_IMPLICIT_CONTEXT
#    define toCTRL(c)     Perl_ebcdic_control(aTHX_ c)
#  else
#    define toCTRL        Perl_ebcdic_control
#  endif
#else
  /* This conversion works both ways, strangely enough. */
#  define toCTRL(c)    (toUPPER(c) ^ 64)
#endif

/* Line numbers are unsigned, 32 bits. */
typedef U32 line_t;
#define NOLINE ((line_t) 4294967295UL)


/*
=head1 Memory Management

=for apidoc Am|void|Newx|void* ptr|int nitems|type
The XSUB-writer's interface to the C C<malloc> function.

In 5.9.3, Newx() and friends replace the older New() API, and drops
the first parameter, I<x>, a debug aid which allowed callers to identify
themselves.  This aid has been superseded by a new build option,
PERL_MEM_LOG (see L<perlhack/PERL_MEM_LOG>).  The older API is still
there for use in XS modules supporting older perls.

=for apidoc Am|void|Newxc|void* ptr|int nitems|type|cast
The XSUB-writer's interface to the C C<malloc> function, with
cast.  See also C<Newx>.

=for apidoc Am|void|Newxz|void* ptr|int nitems|type
The XSUB-writer's interface to the C C<malloc> function.  The allocated
memory is zeroed with C<memzero>.  See also C<Newx>.

=for apidoc Am|void|Renew|void* ptr|int nitems|type
The XSUB-writer's interface to the C C<realloc> function.

=for apidoc Am|void|Renewc|void* ptr|int nitems|type|cast
The XSUB-writer's interface to the C C<realloc> function, with
cast.

=for apidoc Am|void|Safefree|void* ptr
The XSUB-writer's interface to the C C<free> function.

=for apidoc Am|void|Move|void* src|void* dest|int nitems|type
The XSUB-writer's interface to the C C<memmove> function.  The C<src> is the
source, C<dest> is the destination, C<nitems> is the number of items, and C<type> is
the type.  Can do overlapping moves.  See also C<Copy>.

=for apidoc Am|void *|MoveD|void* src|void* dest|int nitems|type
Like C<Move> but returns dest. Useful for encouraging compilers to tail-call
optimise.

=for apidoc Am|void|Copy|void* src|void* dest|int nitems|type
The XSUB-writer's interface to the C C<memcpy> function.  The C<src> is the
source, C<dest> is the destination, C<nitems> is the number of items, and C<type> is
the type.  May fail on overlapping copies.  See also C<Move>.

=for apidoc Am|void *|CopyD|void* src|void* dest|int nitems|type

Like C<Copy> but returns dest. Useful for encouraging compilers to tail-call
optimise.

=for apidoc Am|void|Zero|void* dest|int nitems|type

The XSUB-writer's interface to the C C<memzero> function.  The C<dest> is the
destination, C<nitems> is the number of items, and C<type> is the type.

=for apidoc Am|void *|ZeroD|void* dest|int nitems|type

Like C<Zero> but returns dest. Useful for encouraging compilers to tail-call
optimise.

=for apidoc Am|void|StructCopy|type src|type dest|type
This is an architecture-independent macro to copy one structure to another.

=for apidoc Am|void|PoisonWith|void* dest|int nitems|type|U8 byte

Fill up memory with a byte pattern (a byte repeated over and over
again) that hopefully catches attempts to access uninitialized memory.

=for apidoc Am|void|PoisonNew|void* dest|int nitems|type

PoisonWith(0xAB) for catching access to allocated but uninitialized memory.

=for apidoc Am|void|PoisonFree|void* dest|int nitems|type

PoisonWith(0xEF) for catching access to freed memory.

=for apidoc Am|void|Poison|void* dest|int nitems|type

PoisonWith(0xEF) for catching access to freed memory.

=cut */

/* Maintained for backwards-compatibility only. Use newSV() instead. */
#ifndef PERL_CORE
#define NEWSV(x,len)	newSV(len)
#endif

#define MEM_SIZE_MAX ((MEM_SIZE)~0)

/* The +0.0 in MEM_WRAP_CHECK_ is an attempt to foil
 * overly eager compilers that will bleat about e.g.
 * (U16)n > (size_t)~0/sizeof(U16) always being false. */
#ifdef PERL_MALLOC_WRAP
#define MEM_WRAP_CHECK(n,t) MEM_WRAP_CHECK_1(n,t,PL_memory_wrap)
#define MEM_WRAP_CHECK_1(n,t,a) \
	(void)(sizeof(t) > 1 && ((MEM_SIZE)(n)+0.0) > MEM_SIZE_MAX/sizeof(t) && (Perl_croak_nocontext(a),0))
#define MEM_WRAP_CHECK_(n,t) MEM_WRAP_CHECK(n,t),

#define PERL_STRLEN_ROUNDUP(n) ((void)(((n) > MEM_SIZE_MAX - 2 * PERL_STRLEN_ROUNDUP_QUANTUM) ? (Perl_croak_nocontext(PL_memory_wrap),0):0),((n-1+PERL_STRLEN_ROUNDUP_QUANTUM)&~((MEM_SIZE)PERL_STRLEN_ROUNDUP_QUANTUM-1)))

#else

#define MEM_WRAP_CHECK(n,t)
#define MEM_WRAP_CHECK_1(n,t,a)
#define MEM_WRAP_CHECK_2(n,t,a,b)
#define MEM_WRAP_CHECK_(n,t)

#define PERL_STRLEN_ROUNDUP(n) (((n-1+PERL_STRLEN_ROUNDUP_QUANTUM)&~((MEM_SIZE)PERL_STRLEN_ROUNDUP_QUANTUM-1)))

#endif

#ifdef PERL_MEM_LOG
/*
 * If PERL_MEM_LOG is defined, all Newx()s, Renew()s, and Safefree()s
 * go through functions, which are handy for debugging breakpoints, but
 * which more importantly get the immediate calling environment (file and
 * line number, and C function name if available) passed in.  This info can
 * then be used for logging the calls, for which one gets a sample
 * implementation if PERL_MEM_LOG_STDERR is defined.
 *
 * Known problems:
 * - all memory allocs do not get logged, only those
 *   that go through Newx() and derivatives (while all
 *  Safefrees do get logged)
 * - __FILE__ and __LINE__ do not work everywhere
 * - __func__ or __FUNCTION__ even less so
 * - I think more goes on after the perlio frees but
 *   the thing is that STDERR gets closed (as do all
 *   the file descriptors)
 * - no deeper calling stack than the caller of the Newx()
 *   or the kind, but do I look like a C reflection/introspection
 *   utility to you?
 * - the function prototypes for the logging functions
 *   probably should maybe be somewhere else than handy.h
 * - one could consider inlining (macrofying) the logging
 *   for speed, but I am too lazy
 * - one could imagine recording the allocations in a hash,
 *   (keyed by the allocation address?), and maintain that
 *   through reallocs and frees, but how to do that without
 *   any News() happening...?
 */

Malloc_t Perl_mem_log_alloc(const UV n, const UV typesize, const char *typename, Malloc_t newalloc, const char *filename, const int linenumber, const char *funcname);

Malloc_t Perl_mem_log_realloc(const UV n, const UV typesize, const char *typename, Malloc_t oldalloc, Malloc_t newalloc, const char *filename, const int linenumber, const char *funcname);

Malloc_t Perl_mem_log_free(Malloc_t oldalloc, const char *filename, const int linenumber, const char *funcname);

#endif

#ifdef PERL_MEM_LOG
#define MEM_LOG_ALLOC(n,t,a)     Perl_mem_log_alloc(n,sizeof(t),STRINGIFY(t),a,__FILE__,__LINE__,FUNCTION__)
#define MEM_LOG_REALLOC(n,t,v,a) Perl_mem_log_realloc(n,sizeof(t),STRINGIFY(t),v,a,__FILE__,__LINE__,FUNCTION__)
#define MEM_LOG_FREE(a)          Perl_mem_log_free(a,__FILE__,__LINE__,FUNCTION__)
#endif

#ifndef MEM_LOG_ALLOC
#define MEM_LOG_ALLOC(n,t,a)     (a)
#endif
#ifndef MEM_LOG_REALLOC
#define MEM_LOG_REALLOC(n,t,v,a) (a)
#endif
#ifndef MEM_LOG_FREE
#define MEM_LOG_FREE(a)          (a)
#endif

#define Newx(v,n,t)	(v = (MEM_WRAP_CHECK_(n,t) MEM_LOG_ALLOC(n,t,(t*)safemalloc((MEM_SIZE)((n)*sizeof(t))))))
#define Newxc(v,n,t,c)	(v = (MEM_WRAP_CHECK_(n,t) MEM_LOG_ALLOC(n,t,(c*)safemalloc((MEM_SIZE)((n)*sizeof(t))))))
#define Newxz(v,n,t)	(v = (MEM_WRAP_CHECK_(n,t) MEM_LOG_ALLOC(n,t,(t*)safecalloc((n),sizeof(t)))))

#ifndef PERL_CORE
/* pre 5.9.x compatibility */
#define New(x,v,n,t)	Newx(v,n,t)
#define Newc(x,v,n,t,c)	Newxc(v,n,t,c)
#define Newz(x,v,n,t)	Newxz(v,n,t)
#endif

#define Renew(v,n,t) \
	  (v = (MEM_WRAP_CHECK_(n,t) MEM_LOG_REALLOC(n,t,v,(t*)saferealloc((Malloc_t)(v),(MEM_SIZE)((n)*sizeof(t))))))
#define Renewc(v,n,t,c) \
	  (v = (MEM_WRAP_CHECK_(n,t) MEM_LOG_REALLOC(n,t,v,(c*)saferealloc((Malloc_t)(v),(MEM_SIZE)((n)*sizeof(t))))))

#ifdef PERL_POISON
#define Safefree(d) \
  ((d) ? (void)(safefree(MEM_LOG_FREE((Malloc_t)(d))), Poison(&(d), 1, Malloc_t)) : (void) 0)
#else
#define Safefree(d)	safefree(MEM_LOG_FREE((Malloc_t)(d)))
#endif

#define Move(s,d,n,t)	(MEM_WRAP_CHECK_(n,t) (void)memmove((char*)(d),(const char*)(s), (n) * sizeof(t)))
#define Copy(s,d,n,t)	(MEM_WRAP_CHECK_(n,t) (void)memcpy((char*)(d),(const char*)(s), (n) * sizeof(t)))
#define Zero(d,n,t)	(MEM_WRAP_CHECK_(n,t) (void)memzero((char*)(d), (n) * sizeof(t)))

#define MoveD(s,d,n,t)	(MEM_WRAP_CHECK_(n,t) memmove((char*)(d),(const char*)(s), (n) * sizeof(t)))
#define CopyD(s,d,n,t)	(MEM_WRAP_CHECK_(n,t) memcpy((char*)(d),(const char*)(s), (n) * sizeof(t)))
#ifdef HAS_MEMSET
#define ZeroD(d,n,t)	(MEM_WRAP_CHECK_(n,t) memzero((char*)(d), (n) * sizeof(t)))
#else
/* Using bzero(), which returns void.  */
#define ZeroD(d,n,t)	(MEM_WRAP_CHECK_(n,t) memzero((char*)(d), (n) * sizeof(t)),d)
#endif

#define PoisonWith(d,n,t,b)	(MEM_WRAP_CHECK_(n,t) (void)memset((char*)(d), (U8)(b), (n) * sizeof(t)))
#define PoisonNew(d,n,t)	PoisonWith(d,n,t,0xAB)
#define PoisonFree(d,n,t)	PoisonWith(d,n,t,0xEF)
#define Poison(d,n,t)		PoisonFree(d,n,t)

#ifdef USE_STRUCT_COPY
#define StructCopy(s,d,t) (*((t*)(d)) = *((t*)(s)))
#else
#define StructCopy(s,d,t) Copy(s,d,1,t)
#endif

#define C_ARRAY_LENGTH(a)	(sizeof(a)/sizeof((a)[0]))

#ifdef NEED_VA_COPY
# ifdef va_copy
#  define Perl_va_copy(s, d) va_copy(d, s)
# else
#  if defined(__va_copy)
#   define Perl_va_copy(s, d) __va_copy(d, s)
#  else
#   define Perl_va_copy(s, d) Copy(s, d, 1, va_list)
#  endif
# endif
#endif

/* convenience debug macros */
#ifdef USE_ITHREADS
#define pTHX_FORMAT  "Perl interpreter: 0x%p"
#define pTHX__FORMAT ", Perl interpreter: 0x%p"
#define pTHX_VALUE_   (void *)my_perl,
#define pTHX_VALUE    (void *)my_perl
#define pTHX__VALUE_ ,(void *)my_perl,
#define pTHX__VALUE  ,(void *)my_perl
#else
#define pTHX_FORMAT
#define pTHX__FORMAT
#define pTHX_VALUE_
#define pTHX_VALUE
#define pTHX__VALUE_
#define pTHX__VALUE
#endif /* USE_ITHREADS */

/*
 * Local variables:
 * c-indentation-style: bsd
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 *
 * ex: set ts=8 sts=4 sw=4 noet:
 */

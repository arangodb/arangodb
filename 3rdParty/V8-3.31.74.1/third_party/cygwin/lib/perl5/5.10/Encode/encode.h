#ifndef ENCODE_H
#define ENCODE_H

#ifndef U8
/* 
   A tad devious this:
   perl normally has a #define for U8 - if that isn't present then we
   typedef it - leaving it #ifndef so we can do data parts without
   getting extern references to the code parts
*/
typedef unsigned char U8;
#endif

typedef struct encpage_s encpage_t;

struct encpage_s
{
    /* fields ordered to pack nicely on 32-bit machines */
    const U8 *const seq;   /* Packed output sequences we generate 
                  if we match */
    const encpage_t *const next;      /* Page to go to if we match */
    const U8   min;        /* Min value of octet to match this entry */
    const U8   max;        /* Max value of octet to match this entry */
    const U8   dlen;       /* destination length - 
                  size of entries in seq */
    const U8   slen;       /* source length - 
                  number of source octets needed */
};

/*
  At any point in a translation there is a page pointer which points
  at an array of the above structures.

  Basic operation :
  get octet from source stream.
  if (octet >= min && octet < max) {
    if slen is 0 then we cannot represent this character.
    if we have less than slen octets (including this one) then 
      we have a partial character.
    otherwise
      copy dlen octets from seq + dlen*(octet-min) to output
      (dlen may be zero if we don't know yet.)
      load page pointer with next to continue.
      (is slen is one this is end of a character)
      get next octet.
  }
  else {
    increment the page pointer to look at next slot in the array
  }

  arrays SHALL be constructed so there is an entry which matches
  ..0xFF at the end, and either maps it or indicates no
  representation.

  if MSB of slen is set then mapping is an approximate "FALLBACK" entry.

*/


typedef struct encode_s encode_t;
struct encode_s
{
    const encpage_t *const t_utf8;  /* Starting table for translation from 
                       the encoding to UTF-8 form */
    const encpage_t *const f_utf8;  /* Starting table for translation 
                       from UTF-8 to the encoding */
    const U8 *const rep;            /* Replacement character in this
                       encoding e.g. "?" */
    int        replen;              /* Number of octets in rep */
    U8         min_el;              /* Minimum octets to represent a
                       character */
    U8         max_el;              /* Maximum octets to represent a
                       character */
    const char *const name[2];      /* name(s) of this encoding */
};

#ifdef U8
/* See comment at top of file for deviousness */

extern int do_encode(const encpage_t *enc, const U8 *src, STRLEN *slen,
                     U8 *dst, STRLEN dlen, STRLEN *dout, int approx,
             const U8 *term, STRLEN tlen);

extern void Encode_DefineEncoding(encode_t *enc);

#endif /* U8 */

#define ENCODE_NOSPACE  1
#define ENCODE_PARTIAL  2
#define ENCODE_NOREP    3
#define ENCODE_FALLBACK 4
#define ENCODE_FOUND_TERM 5

#define FBCHAR_UTF8		"\xEF\xBF\xBD"

#define  ENCODE_DIE_ON_ERR     0x0001 /* croaks immediately */
#define  ENCODE_WARN_ON_ERR    0x0002 /* warn on error; may proceed */
#define  ENCODE_RETURN_ON_ERR  0x0004 /* immediately returns on NOREP */
#define  ENCODE_LEAVE_SRC      0x0008 /* $src updated unless set */
#define  ENCODE_PERLQQ         0x0100 /* perlqq fallback string */
#define  ENCODE_HTMLCREF       0x0200 /* HTML character ref. fb mode */
#define  ENCODE_XMLCREF        0x0400 /* XML  character ref. fb mode */
#define  ENCODE_STOP_AT_PARTIAL 0x0800 /* stop at partial explicitly */

#define  ENCODE_FB_DEFAULT     0x0000
#define  ENCODE_FB_CROAK       0x0001
#define  ENCODE_FB_QUIET       ENCODE_RETURN_ON_ERR
#define  ENCODE_FB_WARN        (ENCODE_RETURN_ON_ERR|ENCODE_WARN_ON_ERR)
#define  ENCODE_FB_PERLQQ      (ENCODE_PERLQQ|ENCODE_LEAVE_SRC)
#define  ENCODE_FB_HTMLCREF    (ENCODE_HTMLCREF|ENCODE_LEAVE_SRC)
#define  ENCODE_FB_XMLCREF     (ENCODE_XMLCREF|ENCODE_LEAVE_SRC)

#endif /* ENCODE_H */

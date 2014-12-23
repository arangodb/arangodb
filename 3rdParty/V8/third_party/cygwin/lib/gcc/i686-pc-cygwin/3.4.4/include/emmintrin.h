/* Copyright (C) 2003, 2004 Free Software Foundation, Inc.

   This file is part of GCC.

   GCC is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   GCC is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GCC; see the file COPYING.  If not, write to
   the Free Software Foundation, 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

/* As a special exception, if you include this header file into source
   files compiled by GCC, this header file does not by itself cause
   the resulting executable to be covered by the GNU General Public
   License.  This exception does not however invalidate any other
   reasons why the executable file might be covered by the GNU General
   Public License.  */

/* Implemented from the specification included in the Intel C++ Compiler
   User Guide and Reference, version 8.0.  */

#ifndef _EMMINTRIN_H_INCLUDED
#define _EMMINTRIN_H_INCLUDED

#ifdef __SSE2__
#include <xmmintrin.h>

/* SSE2 */
typedef double __v2df __attribute__ ((mode (V2DF)));
typedef int __v2di __attribute__ ((mode (V2DI)));
typedef int __v4si __attribute__ ((mode (V4SI)));
typedef int __v8hi __attribute__ ((mode (V8HI)));
typedef int __v16qi __attribute__ ((mode (V16QI)));

/* Create a selector for use with the SHUFPD instruction.  */
#define _MM_SHUFFLE2(fp1,fp0) \
 (((fp1) << 1) | (fp0))

#define __m128i __v2di
#define __m128d __v2df

/* Create a vector with element 0 as *P and the rest zero.  */
static __inline __m128d
_mm_load_sd (double const *__P)
{
  return (__m128d) __builtin_ia32_loadsd (__P);
}

/* Create a vector with all two elements equal to *P.  */
static __inline __m128d
_mm_load1_pd (double const *__P)
{
  __v2df __tmp = __builtin_ia32_loadsd (__P);
  return (__m128d) __builtin_ia32_shufpd (__tmp, __tmp, _MM_SHUFFLE2 (0,0));
}

static __inline __m128d
_mm_load_pd1 (double const *__P)
{
  return _mm_load1_pd (__P);
}

/* Load two DPFP values from P.  The address must be 16-byte aligned.  */
static __inline __m128d
_mm_load_pd (double const *__P)
{
  return (__m128d) __builtin_ia32_loadapd (__P);
}

/* Load two DPFP values from P.  The address need not be 16-byte aligned.  */
static __inline __m128d
_mm_loadu_pd (double const *__P)
{
  return (__m128d) __builtin_ia32_loadupd (__P);
}

/* Load two DPFP values in reverse order.  The address must be aligned.  */
static __inline __m128d
_mm_loadr_pd (double const *__P)
{
  __v2df __tmp = __builtin_ia32_loadapd (__P);
  return (__m128d) __builtin_ia32_shufpd (__tmp, __tmp, _MM_SHUFFLE2 (0,1));
}

/* Create a vector with element 0 as F and the rest zero.  */
static __inline __m128d
_mm_set_sd (double __F)
{
  return (__m128d) __builtin_ia32_loadsd (&__F);
}

/* Create a vector with all two elements equal to F.  */
static __inline __m128d
_mm_set1_pd (double __F)
{
  __v2df __tmp = __builtin_ia32_loadsd (&__F);
  return (__m128d) __builtin_ia32_shufpd (__tmp, __tmp, _MM_SHUFFLE2 (0,0));
}

static __inline __m128d
_mm_set_pd1 (double __F)
{
  return _mm_set1_pd (__F);
}

/* Create the vector [Z Y].  */
static __inline __m128d
_mm_set_pd (double __Z, double __Y)
{
  return (__v2df) {__Y, __Z};
}

/* Create the vector [Y Z].  */
static __inline __m128d
_mm_setr_pd (double __Z, double __Y)
{
  return _mm_set_pd (__Y, __Z);
}

/* Create a vector of zeros.  */
static __inline __m128d
_mm_setzero_pd (void)
{
  return (__m128d) __builtin_ia32_setzeropd ();
}

/* Stores the lower DPFP value.  */
static __inline void
_mm_store_sd (double *__P, __m128d __A)
{
  __builtin_ia32_storesd (__P, (__v2df)__A);
}

/* Store the lower DPFP value across two words.  */
static __inline void
_mm_store1_pd (double *__P, __m128d __A)
{
  __v2df __va = (__v2df)__A;
  __v2df __tmp = __builtin_ia32_shufpd (__va, __va, _MM_SHUFFLE2 (0,0));
  __builtin_ia32_storeapd (__P, __tmp);
}

static __inline void
_mm_store_pd1 (double *__P, __m128d __A)
{
  _mm_store1_pd (__P, __A);
}

/* Store two DPFP values.  The address must be 16-byte aligned.  */
static __inline void
_mm_store_pd (double *__P, __m128d __A)
{
  __builtin_ia32_storeapd (__P, (__v2df)__A);
}

/* Store two DPFP values.  The address need not be 16-byte aligned.  */
static __inline void
_mm_storeu_pd (double *__P, __m128d __A)
{
  __builtin_ia32_storeupd (__P, (__v2df)__A);
}

/* Store two DPFP values in reverse order.  The address must be aligned.  */
static __inline void
_mm_storer_pd (double *__P, __m128d __A)
{
  __v2df __va = (__v2df)__A;
  __v2df __tmp = __builtin_ia32_shufpd (__va, __va, _MM_SHUFFLE2 (0,1));
  __builtin_ia32_storeapd (__P, __tmp);
}

/* Sets the low DPFP value of A from the low value of B.  */
static __inline __m128d
_mm_move_sd (__m128d __A, __m128d __B)
{
  return (__m128d) __builtin_ia32_movsd ((__v2df)__A, (__v2df)__B);
}


static __inline __m128d
_mm_add_pd (__m128d __A, __m128d __B)
{
  return (__m128d)__builtin_ia32_addpd ((__v2df)__A, (__v2df)__B);
}

static __inline __m128d
_mm_add_sd (__m128d __A, __m128d __B)
{
  return (__m128d)__builtin_ia32_addsd ((__v2df)__A, (__v2df)__B);
}

static __inline __m128d
_mm_sub_pd (__m128d __A, __m128d __B)
{
  return (__m128d)__builtin_ia32_subpd ((__v2df)__A, (__v2df)__B);
}

static __inline __m128d
_mm_sub_sd (__m128d __A, __m128d __B)
{
  return (__m128d)__builtin_ia32_subsd ((__v2df)__A, (__v2df)__B);
}

static __inline __m128d
_mm_mul_pd (__m128d __A, __m128d __B)
{
  return (__m128d)__builtin_ia32_mulpd ((__v2df)__A, (__v2df)__B);
}

static __inline __m128d
_mm_mul_sd (__m128d __A, __m128d __B)
{
  return (__m128d)__builtin_ia32_mulsd ((__v2df)__A, (__v2df)__B);
}

static __inline __m128d
_mm_div_pd (__m128d __A, __m128d __B)
{
  return (__m128d)__builtin_ia32_divpd ((__v2df)__A, (__v2df)__B);
}

static __inline __m128d
_mm_div_sd (__m128d __A, __m128d __B)
{
  return (__m128d)__builtin_ia32_divsd ((__v2df)__A, (__v2df)__B);
}

static __inline __m128d
_mm_sqrt_pd (__m128d __A)
{
  return (__m128d)__builtin_ia32_sqrtpd ((__v2df)__A);
}

/* Return pair {sqrt (A[0), B[1]}.  */
static __inline __m128d
_mm_sqrt_sd (__m128d __A, __m128d __B)
{
  __v2df __tmp = __builtin_ia32_movsd ((__v2df)__A, (__v2df)__B);
  return (__m128d)__builtin_ia32_sqrtsd ((__v2df)__tmp);
}

static __inline __m128d
_mm_min_pd (__m128d __A, __m128d __B)
{
  return (__m128d)__builtin_ia32_minpd ((__v2df)__A, (__v2df)__B);
}

static __inline __m128d
_mm_min_sd (__m128d __A, __m128d __B)
{
  return (__m128d)__builtin_ia32_minsd ((__v2df)__A, (__v2df)__B);
}

static __inline __m128d
_mm_max_pd (__m128d __A, __m128d __B)
{
  return (__m128d)__builtin_ia32_maxpd ((__v2df)__A, (__v2df)__B);
}

static __inline __m128d
_mm_max_sd (__m128d __A, __m128d __B)
{
  return (__m128d)__builtin_ia32_maxsd ((__v2df)__A, (__v2df)__B);
}

static __inline __m128d
_mm_and_pd (__m128d __A, __m128d __B)
{
  return (__m128d)__builtin_ia32_andpd ((__v2df)__A, (__v2df)__B);
}

static __inline __m128d
_mm_andnot_pd (__m128d __A, __m128d __B)
{
  return (__m128d)__builtin_ia32_andnpd ((__v2df)__A, (__v2df)__B);
}

static __inline __m128d
_mm_or_pd (__m128d __A, __m128d __B)
{
  return (__m128d)__builtin_ia32_orpd ((__v2df)__A, (__v2df)__B);
}

static __inline __m128d
_mm_xor_pd (__m128d __A, __m128d __B)
{
  return (__m128d)__builtin_ia32_xorpd ((__v2df)__A, (__v2df)__B);
}

static __inline __m128d
_mm_cmpeq_pd (__m128d __A, __m128d __B)
{
  return (__m128d)__builtin_ia32_cmpeqpd ((__v2df)__A, (__v2df)__B);
}

static __inline __m128d
_mm_cmplt_pd (__m128d __A, __m128d __B)
{
  return (__m128d)__builtin_ia32_cmpltpd ((__v2df)__A, (__v2df)__B);
}

static __inline __m128d
_mm_cmple_pd (__m128d __A, __m128d __B)
{
  return (__m128d)__builtin_ia32_cmplepd ((__v2df)__A, (__v2df)__B);
}

static __inline __m128d
_mm_cmpgt_pd (__m128d __A, __m128d __B)
{
  return (__m128d)__builtin_ia32_cmpgtpd ((__v2df)__A, (__v2df)__B);
}

static __inline __m128d
_mm_cmpge_pd (__m128d __A, __m128d __B)
{
  return (__m128d)__builtin_ia32_cmpgepd ((__v2df)__A, (__v2df)__B);
}

static __inline __m128d
_mm_cmpneq_pd (__m128d __A, __m128d __B)
{
  return (__m128d)__builtin_ia32_cmpneqpd ((__v2df)__A, (__v2df)__B);
}

static __inline __m128d
_mm_cmpnlt_pd (__m128d __A, __m128d __B)
{
  return (__m128d)__builtin_ia32_cmpnltpd ((__v2df)__A, (__v2df)__B);
}

static __inline __m128d
_mm_cmpnle_pd (__m128d __A, __m128d __B)
{
  return (__m128d)__builtin_ia32_cmpnlepd ((__v2df)__A, (__v2df)__B);
}

static __inline __m128d
_mm_cmpngt_pd (__m128d __A, __m128d __B)
{
  return (__m128d)__builtin_ia32_cmpngtpd ((__v2df)__A, (__v2df)__B);
}

static __inline __m128d
_mm_cmpnge_pd (__m128d __A, __m128d __B)
{
  return (__m128d)__builtin_ia32_cmpngepd ((__v2df)__A, (__v2df)__B);
}

static __inline __m128d
_mm_cmpord_pd (__m128d __A, __m128d __B)
{
  return (__m128d)__builtin_ia32_cmpordpd ((__v2df)__A, (__v2df)__B);
}

static __inline __m128d
_mm_cmpunord_pd (__m128d __A, __m128d __B)
{
  return (__m128d)__builtin_ia32_cmpunordpd ((__v2df)__A, (__v2df)__B);
}

static __inline __m128d
_mm_cmpeq_sd (__m128d __A, __m128d __B)
{
  return (__m128d)__builtin_ia32_cmpeqsd ((__v2df)__A, (__v2df)__B);
}

static __inline __m128d
_mm_cmplt_sd (__m128d __A, __m128d __B)
{
  return (__m128d)__builtin_ia32_cmpltsd ((__v2df)__A, (__v2df)__B);
}

static __inline __m128d
_mm_cmple_sd (__m128d __A, __m128d __B)
{
  return (__m128d)__builtin_ia32_cmplesd ((__v2df)__A, (__v2df)__B);
}

static __inline __m128d
_mm_cmpgt_sd (__m128d __A, __m128d __B)
{
  return (__m128d) __builtin_ia32_movsd ((__v2df) __A,
					 (__v2df)
					 __builtin_ia32_cmpltsd ((__v2df) __B,
								 (__v2df)
								 __A));
}

static __inline __m128d
_mm_cmpge_sd (__m128d __A, __m128d __B)
{
  return (__m128d) __builtin_ia32_movsd ((__v2df) __A,
					 (__v2df)
					 __builtin_ia32_cmplesd ((__v2df) __B,
								 (__v2df)
								 __A));
}

static __inline __m128d
_mm_cmpneq_sd (__m128d __A, __m128d __B)
{
  return (__m128d)__builtin_ia32_cmpneqsd ((__v2df)__A, (__v2df)__B);
}

static __inline __m128d
_mm_cmpnlt_sd (__m128d __A, __m128d __B)
{
  return (__m128d)__builtin_ia32_cmpnltsd ((__v2df)__A, (__v2df)__B);
}

static __inline __m128d
_mm_cmpnle_sd (__m128d __A, __m128d __B)
{
  return (__m128d)__builtin_ia32_cmpnlesd ((__v2df)__A, (__v2df)__B);
}

static __inline __m128d
_mm_cmpngt_sd (__m128d __A, __m128d __B)
{
  return (__m128d) __builtin_ia32_movsd ((__v2df) __A,
					 (__v2df)
					 __builtin_ia32_cmpnltsd ((__v2df) __B,
								  (__v2df)
								  __A));
}

static __inline __m128d
_mm_cmpnge_sd (__m128d __A, __m128d __B)
{
  return (__m128d) __builtin_ia32_movsd ((__v2df) __A,
					 (__v2df)
					 __builtin_ia32_cmpnlesd ((__v2df) __B,
								  (__v2df)
								  __A));
}

static __inline __m128d
_mm_cmpord_sd (__m128d __A, __m128d __B)
{
  return (__m128d)__builtin_ia32_cmpordsd ((__v2df)__A, (__v2df)__B);
}

static __inline __m128d
_mm_cmpunord_sd (__m128d __A, __m128d __B)
{
  return (__m128d)__builtin_ia32_cmpunordsd ((__v2df)__A, (__v2df)__B);
}

static __inline int
_mm_comieq_sd (__m128d __A, __m128d __B)
{
  return __builtin_ia32_comisdeq ((__v2df)__A, (__v2df)__B);
}

static __inline int
_mm_comilt_sd (__m128d __A, __m128d __B)
{
  return __builtin_ia32_comisdlt ((__v2df)__A, (__v2df)__B);
}

static __inline int
_mm_comile_sd (__m128d __A, __m128d __B)
{
  return __builtin_ia32_comisdle ((__v2df)__A, (__v2df)__B);
}

static __inline int
_mm_comigt_sd (__m128d __A, __m128d __B)
{
  return __builtin_ia32_comisdgt ((__v2df)__A, (__v2df)__B);
}

static __inline int
_mm_comige_sd (__m128d __A, __m128d __B)
{
  return __builtin_ia32_comisdge ((__v2df)__A, (__v2df)__B);
}

static __inline int
_mm_comineq_sd (__m128d __A, __m128d __B)
{
  return __builtin_ia32_comisdneq ((__v2df)__A, (__v2df)__B);
}

static __inline int
_mm_ucomieq_sd (__m128d __A, __m128d __B)
{
  return __builtin_ia32_ucomisdeq ((__v2df)__A, (__v2df)__B);
}

static __inline int
_mm_ucomilt_sd (__m128d __A, __m128d __B)
{
  return __builtin_ia32_ucomisdlt ((__v2df)__A, (__v2df)__B);
}

static __inline int
_mm_ucomile_sd (__m128d __A, __m128d __B)
{
  return __builtin_ia32_ucomisdle ((__v2df)__A, (__v2df)__B);
}

static __inline int
_mm_ucomigt_sd (__m128d __A, __m128d __B)
{
  return __builtin_ia32_ucomisdgt ((__v2df)__A, (__v2df)__B);
}

static __inline int
_mm_ucomige_sd (__m128d __A, __m128d __B)
{
  return __builtin_ia32_ucomisdge ((__v2df)__A, (__v2df)__B);
}

static __inline int
_mm_ucomineq_sd (__m128d __A, __m128d __B)
{
  return __builtin_ia32_ucomisdneq ((__v2df)__A, (__v2df)__B);
}

/* Create a vector with element 0 as *P and the rest zero.  */

static __inline __m128i
_mm_load_si128 (__m128i const *__P)
{
  return (__m128i) __builtin_ia32_loaddqa ((char const *)__P);
}

static __inline __m128i
_mm_loadu_si128 (__m128i const *__P)
{
  return (__m128i) __builtin_ia32_loaddqu ((char const *)__P);
}

static __inline __m128i
_mm_loadl_epi64 (__m128i const *__P)
{
  return (__m128i) __builtin_ia32_movq2dq (*(unsigned long long *)__P);
}

static __inline void
_mm_store_si128 (__m128i *__P, __m128i __B)
{
  __builtin_ia32_storedqa ((char *)__P, (__v16qi)__B);
}

static __inline void
_mm_storeu_si128 (__m128i *__P, __m128i __B)
{
  __builtin_ia32_storedqu ((char *)__P, (__v16qi)__B);
}

static __inline void
_mm_storel_epi64 (__m128i *__P, __m128i __B)
{
  *(long long *)__P = __builtin_ia32_movdq2q ((__v2di)__B);
}

static __inline __m64
_mm_movepi64_pi64 (__m128i __B)
{
  return (__m64) __builtin_ia32_movdq2q ((__v2di)__B);
}

static __inline __m128i
_mm_move_epi64 (__m128i __A)
{
  return (__m128i) __builtin_ia32_movq ((__v2di)__A);
}

/* Create a vector of zeros.  */
static __inline __m128i
_mm_setzero_si128 (void)
{
  return (__m128i) __builtin_ia32_setzero128 ();
}

static __inline __m128i
_mm_set_epi64 (__m64 __A,  __m64 __B)
{
  __v2di __tmp = (__v2di)__builtin_ia32_movq2dq ((unsigned long long)__A);
  __v2di __tmp2 = (__v2di)__builtin_ia32_movq2dq ((unsigned long long)__B);
  return (__m128i)__builtin_ia32_punpcklqdq128 (__tmp2, __tmp);
}

/* Create the vector [Z Y X W].  */
static __inline __m128i
_mm_set_epi32 (int __Z, int __Y, int __X, int __W)
{
  union {
    int __a[4];
    __m128i __v;
  } __u;

  __u.__a[0] = __W;
  __u.__a[1] = __X;
  __u.__a[2] = __Y;
  __u.__a[3] = __Z;

  return __u.__v;
}

#ifdef __x86_64__
/* Create the vector [Z Y].  */
static __inline __m128i
_mm_set_epi64x (long long __Z, long long __Y)
{
  union {
    long __a[2];
    __m128i __v;
  } __u;

  __u.__a[0] = __Y;
  __u.__a[1] = __Z;

  return __u.__v;
}
#endif

/* Create the vector [S T U V Z Y X W].  */
static __inline __m128i
_mm_set_epi16 (short __Z, short __Y, short __X, short __W,
	       short __V, short __U, short __T, short __S)
{
  union {
    short __a[8];
    __m128i __v;
  } __u;

  __u.__a[0] = __S;
  __u.__a[1] = __T;
  __u.__a[2] = __U;
  __u.__a[3] = __V;
  __u.__a[4] = __W;
  __u.__a[5] = __X;
  __u.__a[6] = __Y;
  __u.__a[7] = __Z;

  return __u.__v;
}

/* Create the vector [S T U V Z Y X W].  */
static __inline __m128i
_mm_set_epi8 (char __Z, char __Y, char __X, char __W,
	      char __V, char __U, char __T, char __S,
	      char __Z1, char __Y1, char __X1, char __W1,
	      char __V1, char __U1, char __T1, char __S1)
{
  union {
    char __a[16];
    __m128i __v;
  } __u;

  __u.__a[0] = __S1;
  __u.__a[1] = __T1;
  __u.__a[2] = __U1;
  __u.__a[3] = __V1;
  __u.__a[4] = __W1;
  __u.__a[5] = __X1;
  __u.__a[6] = __Y1;
  __u.__a[7] = __Z1;
  __u.__a[8] = __S;
  __u.__a[9] = __T;
  __u.__a[10] = __U;
  __u.__a[11] = __V;
  __u.__a[12] = __W;
  __u.__a[13] = __X;
  __u.__a[14] = __Y;
  __u.__a[15] = __Z;

  return __u.__v;
}

static __inline __m128i
_mm_set1_epi64 (__m64 __A)
{
  __v2di __tmp = (__v2di)__builtin_ia32_movq2dq ((unsigned long long)__A);
  return (__m128i)__builtin_ia32_punpcklqdq128 (__tmp, __tmp);
}

static __inline __m128i
_mm_set1_epi32 (int __A)
{
  __v4si __tmp = (__v4si)__builtin_ia32_loadd (&__A);
  return (__m128i) __builtin_ia32_pshufd ((__v4si)__tmp, _MM_SHUFFLE (0,0,0,0));
}

#ifdef __x86_64__
static __inline __m128i
_mm_set1_epi64x (long long __A)
{
  __v2di __tmp = (__v2di)__builtin_ia32_movq2dq ((unsigned long long)__A);
  return (__m128i) __builtin_ia32_shufpd ((__v2df)__tmp, (__v2df)__tmp, _MM_SHUFFLE2 (0,0));
}
#endif

static __inline __m128i
_mm_set1_epi16 (short __A)
{
  int __Acopy = (unsigned short)__A;
  __v4si __tmp = (__v4si)__builtin_ia32_loadd (&__Acopy);
  __tmp = (__v4si)__builtin_ia32_punpcklwd128 ((__v8hi)__tmp, (__v8hi)__tmp);
  return (__m128i) __builtin_ia32_pshufd ((__v4si)__tmp, _MM_SHUFFLE (0,0,0,0));
}

static __inline __m128i
_mm_set1_epi8 (char __A)
{
  int __Acopy = (unsigned char)__A;
  __v4si __tmp = (__v4si)__builtin_ia32_loadd (&__Acopy);
  __tmp = (__v4si)__builtin_ia32_punpcklbw128 ((__v16qi)__tmp, (__v16qi)__tmp);
  __tmp = (__v4si)__builtin_ia32_punpcklbw128 ((__v16qi)__tmp, (__v16qi)__tmp);
  return (__m128i) __builtin_ia32_pshufd ((__v4si)__tmp, _MM_SHUFFLE (0,0,0,0));
}

static __inline __m128i
_mm_setr_epi64 (__m64 __A,  __m64 __B)
{
  __v2di __tmp = (__v2di)__builtin_ia32_movq2dq ((unsigned long long)__A);
  __v2di __tmp2 = (__v2di)__builtin_ia32_movq2dq ((unsigned long long)__B);
  return (__m128i)__builtin_ia32_punpcklqdq128 (__tmp, __tmp2);
}

/* Create the vector [Z Y X W].  */
static __inline __m128i
_mm_setr_epi32 (int __W, int __X, int __Y, int __Z)
{
  union {
    int __a[4];
    __m128i __v;
  } __u;

  __u.__a[0] = __W;
  __u.__a[1] = __X;
  __u.__a[2] = __Y;
  __u.__a[3] = __Z;

  return __u.__v;
}
/* Create the vector [S T U V Z Y X W].  */
static __inline __m128i
_mm_setr_epi16 (short __S, short __T, short __U, short __V,
	        short __W, short __X, short __Y, short __Z)
{
  union {
    short __a[8];
    __m128i __v;
  } __u;

  __u.__a[0] = __S;
  __u.__a[1] = __T;
  __u.__a[2] = __U;
  __u.__a[3] = __V;
  __u.__a[4] = __W;
  __u.__a[5] = __X;
  __u.__a[6] = __Y;
  __u.__a[7] = __Z;

  return __u.__v;
}

/* Create the vector [S T U V Z Y X W].  */
static __inline __m128i
_mm_setr_epi8 (char __S1, char __T1, char __U1, char __V1,
	       char __W1, char __X1, char __Y1, char __Z1,
	       char __S, char __T, char __U, char __V,
	       char __W, char __X, char __Y, char __Z)
{
  union {
    char __a[16];
    __m128i __v;
  } __u;

  __u.__a[0] = __S1;
  __u.__a[1] = __T1;
  __u.__a[2] = __U1;
  __u.__a[3] = __V1;
  __u.__a[4] = __W1;
  __u.__a[5] = __X1;
  __u.__a[6] = __Y1;
  __u.__a[7] = __Z1;
  __u.__a[8] = __S;
  __u.__a[9] = __T;
  __u.__a[10] = __U;
  __u.__a[11] = __V;
  __u.__a[12] = __W;
  __u.__a[13] = __X;
  __u.__a[14] = __Y;
  __u.__a[15] = __Z;

  return __u.__v;
}

static __inline __m128d
_mm_cvtepi32_pd (__m128i __A)
{
  return (__m128d)__builtin_ia32_cvtdq2pd ((__v4si) __A);
}

static __inline __m128
_mm_cvtepi32_ps (__m128i __A)
{
  return (__m128)__builtin_ia32_cvtdq2ps ((__v4si) __A);
}

static __inline __m128i
_mm_cvtpd_epi32 (__m128d __A)
{
  return (__m128i)__builtin_ia32_cvtpd2dq ((__v2df) __A);
}

static __inline __m64
_mm_cvtpd_pi32 (__m128d __A)
{
  return (__m64)__builtin_ia32_cvtpd2pi ((__v2df) __A);
}

static __inline __m128
_mm_cvtpd_ps (__m128d __A)
{
  return (__m128)__builtin_ia32_cvtpd2ps ((__v2df) __A);
}

static __inline __m128i
_mm_cvttpd_epi32 (__m128d __A)
{
  return (__m128i)__builtin_ia32_cvttpd2dq ((__v2df) __A);
}

static __inline __m64
_mm_cvttpd_pi32 (__m128d __A)
{
  return (__m64)__builtin_ia32_cvttpd2pi ((__v2df) __A);
}

static __inline __m128d
_mm_cvtpi32_pd (__m64 __A)
{
  return (__m128d)__builtin_ia32_cvtpi2pd ((__v2si) __A);
}

static __inline __m128i
_mm_cvtps_epi32 (__m128 __A)
{
  return (__m128i)__builtin_ia32_cvtps2dq ((__v4sf) __A);
}

static __inline __m128i
_mm_cvttps_epi32 (__m128 __A)
{
  return (__m128i)__builtin_ia32_cvttps2dq ((__v4sf) __A);
}

static __inline __m128d
_mm_cvtps_pd (__m128 __A)
{
  return (__m128d)__builtin_ia32_cvtps2pd ((__v4sf) __A);
}

static __inline int
_mm_cvtsd_si32 (__m128d __A)
{
  return __builtin_ia32_cvtsd2si ((__v2df) __A);
}

#ifdef __x86_64__
static __inline long long
_mm_cvtsd_si64x (__m128d __A)
{
  return __builtin_ia32_cvtsd2si64 ((__v2df) __A);
}
#endif

static __inline int
_mm_cvttsd_si32 (__m128d __A)
{
  return __builtin_ia32_cvttsd2si ((__v2df) __A);
}

#ifdef __x86_64__
static __inline long long
_mm_cvttsd_si64x (__m128d __A)
{
  return __builtin_ia32_cvttsd2si64 ((__v2df) __A);
}
#endif

static __inline __m128
_mm_cvtsd_ss (__m128 __A, __m128d __B)
{
  return (__m128)__builtin_ia32_cvtsd2ss ((__v4sf) __A, (__v2df) __B);
}

static __inline __m128d
_mm_cvtsi32_sd (__m128d __A, int __B)
{
  return (__m128d)__builtin_ia32_cvtsi2sd ((__v2df) __A, __B);
}

#ifdef __x86_64__
static __inline __m128d
_mm_cvtsi64x_sd (__m128d __A, long long __B)
{
  return (__m128d)__builtin_ia32_cvtsi642sd ((__v2df) __A, __B);
}
#endif

static __inline __m128d
_mm_cvtss_sd (__m128d __A, __m128 __B)
{
  return (__m128d)__builtin_ia32_cvtss2sd ((__v2df) __A, (__v4sf)__B);
}

#define _mm_shuffle_pd(__A, __B, __C) ((__m128d)__builtin_ia32_shufpd ((__v2df)__A, (__v2df)__B, (__C)))

static __inline __m128d
_mm_unpackhi_pd (__m128d __A, __m128d __B)
{
  return (__m128d)__builtin_ia32_unpckhpd ((__v2df)__A, (__v2df)__B);
}

static __inline __m128d
_mm_unpacklo_pd (__m128d __A, __m128d __B)
{
  return (__m128d)__builtin_ia32_unpcklpd ((__v2df)__A, (__v2df)__B);
}

static __inline __m128d
_mm_loadh_pd (__m128d __A, double const *__B)
{
  return (__m128d)__builtin_ia32_loadhpd ((__v2df)__A, (__v2si *)__B);
}

static __inline void
_mm_storeh_pd (double *__A, __m128d __B)
{
  __builtin_ia32_storehpd ((__v2si *)__A, (__v2df)__B);
}

static __inline __m128d
_mm_loadl_pd (__m128d __A, double const *__B)
{
  return (__m128d)__builtin_ia32_loadlpd ((__v2df)__A, (__v2si *)__B);
}

static __inline void
_mm_storel_pd (double *__A, __m128d __B)
{
  __builtin_ia32_storelpd ((__v2si *)__A, (__v2df)__B);
}

static __inline int
_mm_movemask_pd (__m128d __A)
{
  return __builtin_ia32_movmskpd ((__v2df)__A);
}

static __inline __m128i
_mm_packs_epi16 (__m128i __A, __m128i __B)
{
  return (__m128i)__builtin_ia32_packsswb128 ((__v8hi)__A, (__v8hi)__B);
}

static __inline __m128i
_mm_packs_epi32 (__m128i __A, __m128i __B)
{
  return (__m128i)__builtin_ia32_packssdw128 ((__v4si)__A, (__v4si)__B);
}

static __inline __m128i
_mm_packus_epi16 (__m128i __A, __m128i __B)
{
  return (__m128i)__builtin_ia32_packuswb128 ((__v8hi)__A, (__v8hi)__B);
}

static __inline __m128i
_mm_unpackhi_epi8 (__m128i __A, __m128i __B)
{
  return (__m128i)__builtin_ia32_punpckhbw128 ((__v16qi)__A, (__v16qi)__B);
}

static __inline __m128i
_mm_unpackhi_epi16 (__m128i __A, __m128i __B)
{
  return (__m128i)__builtin_ia32_punpckhwd128 ((__v8hi)__A, (__v8hi)__B);
}

static __inline __m128i
_mm_unpackhi_epi32 (__m128i __A, __m128i __B)
{
  return (__m128i)__builtin_ia32_punpckhdq128 ((__v4si)__A, (__v4si)__B);
}

static __inline __m128i
_mm_unpackhi_epi64 (__m128i __A, __m128i __B)
{
  return (__m128i)__builtin_ia32_punpckhqdq128 ((__v2di)__A, (__v2di)__B);
}

static __inline __m128i
_mm_unpacklo_epi8 (__m128i __A, __m128i __B)
{
  return (__m128i)__builtin_ia32_punpcklbw128 ((__v16qi)__A, (__v16qi)__B);
}

static __inline __m128i
_mm_unpacklo_epi16 (__m128i __A, __m128i __B)
{
  return (__m128i)__builtin_ia32_punpcklwd128 ((__v8hi)__A, (__v8hi)__B);
}

static __inline __m128i
_mm_unpacklo_epi32 (__m128i __A, __m128i __B)
{
  return (__m128i)__builtin_ia32_punpckldq128 ((__v4si)__A, (__v4si)__B);
}

static __inline __m128i
_mm_unpacklo_epi64 (__m128i __A, __m128i __B)
{
  return (__m128i)__builtin_ia32_punpcklqdq128 ((__v2di)__A, (__v2di)__B);
}

static __inline __m128i
_mm_add_epi8 (__m128i __A, __m128i __B)
{
  return (__m128i)__builtin_ia32_paddb128 ((__v16qi)__A, (__v16qi)__B);
}

static __inline __m128i
_mm_add_epi16 (__m128i __A, __m128i __B)
{
  return (__m128i)__builtin_ia32_paddw128 ((__v8hi)__A, (__v8hi)__B);
}

static __inline __m128i
_mm_add_epi32 (__m128i __A, __m128i __B)
{
  return (__m128i)__builtin_ia32_paddd128 ((__v4si)__A, (__v4si)__B);
}

static __inline __m128i
_mm_add_epi64 (__m128i __A, __m128i __B)
{
  return (__m128i)__builtin_ia32_paddq128 ((__v2di)__A, (__v2di)__B);
}

static __inline __m128i
_mm_adds_epi8 (__m128i __A, __m128i __B)
{
  return (__m128i)__builtin_ia32_paddsb128 ((__v16qi)__A, (__v16qi)__B);
}

static __inline __m128i
_mm_adds_epi16 (__m128i __A, __m128i __B)
{
  return (__m128i)__builtin_ia32_paddsw128 ((__v8hi)__A, (__v8hi)__B);
}

static __inline __m128i
_mm_adds_epu8 (__m128i __A, __m128i __B)
{
  return (__m128i)__builtin_ia32_paddusb128 ((__v16qi)__A, (__v16qi)__B);
}

static __inline __m128i
_mm_adds_epu16 (__m128i __A, __m128i __B)
{
  return (__m128i)__builtin_ia32_paddusw128 ((__v8hi)__A, (__v8hi)__B);
}

static __inline __m128i
_mm_sub_epi8 (__m128i __A, __m128i __B)
{
  return (__m128i)__builtin_ia32_psubb128 ((__v16qi)__A, (__v16qi)__B);
}

static __inline __m128i
_mm_sub_epi16 (__m128i __A, __m128i __B)
{
  return (__m128i)__builtin_ia32_psubw128 ((__v8hi)__A, (__v8hi)__B);
}

static __inline __m128i
_mm_sub_epi32 (__m128i __A, __m128i __B)
{
  return (__m128i)__builtin_ia32_psubd128 ((__v4si)__A, (__v4si)__B);
}

static __inline __m128i
_mm_sub_epi64 (__m128i __A, __m128i __B)
{
  return (__m128i)__builtin_ia32_psubq128 ((__v2di)__A, (__v2di)__B);
}

static __inline __m128i
_mm_subs_epi8 (__m128i __A, __m128i __B)
{
  return (__m128i)__builtin_ia32_psubsb128 ((__v16qi)__A, (__v16qi)__B);
}

static __inline __m128i
_mm_subs_epi16 (__m128i __A, __m128i __B)
{
  return (__m128i)__builtin_ia32_psubsw128 ((__v8hi)__A, (__v8hi)__B);
}

static __inline __m128i
_mm_subs_epu8 (__m128i __A, __m128i __B)
{
  return (__m128i)__builtin_ia32_psubusb128 ((__v16qi)__A, (__v16qi)__B);
}

static __inline __m128i
_mm_subs_epu16 (__m128i __A, __m128i __B)
{
  return (__m128i)__builtin_ia32_psubusw128 ((__v8hi)__A, (__v8hi)__B);
}

static __inline __m128i
_mm_madd_epi16 (__m128i __A, __m128i __B)
{
  return (__m128i)__builtin_ia32_pmaddwd128 ((__v8hi)__A, (__v8hi)__B);
}

static __inline __m128i
_mm_mulhi_epi16 (__m128i __A, __m128i __B)
{
  return (__m128i)__builtin_ia32_pmulhw128 ((__v8hi)__A, (__v8hi)__B);
}

static __inline __m128i
_mm_mullo_epi16 (__m128i __A, __m128i __B)
{
  return (__m128i)__builtin_ia32_pmullw128 ((__v8hi)__A, (__v8hi)__B);
}

static __inline __m64
_mm_mul_su32 (__m64 __A, __m64 __B)
{
  return (__m64)__builtin_ia32_pmuludq ((__v2si)__A, (__v2si)__B);
}

static __inline __m128i
_mm_mul_epu32 (__m128i __A, __m128i __B)
{
  return (__m128i)__builtin_ia32_pmuludq128 ((__v4si)__A, (__v4si)__B);
}

static __inline __m128i
_mm_sll_epi16 (__m128i __A, __m128i __B)
{
  return (__m128i)__builtin_ia32_psllw128 ((__v8hi)__A, (__v2di)__B);
}

static __inline __m128i
_mm_sll_epi32 (__m128i __A, __m128i __B)
{
  return (__m128i)__builtin_ia32_pslld128 ((__v4si)__A, (__v2di)__B);
}

static __inline __m128i
_mm_sll_epi64 (__m128i __A, __m128i __B)
{
  return (__m128i)__builtin_ia32_psllq128 ((__v2di)__A, (__v2di)__B);
}

static __inline __m128i
_mm_sra_epi16 (__m128i __A, __m128i __B)
{
  return (__m128i)__builtin_ia32_psraw128 ((__v8hi)__A, (__v2di)__B);
}

static __inline __m128i
_mm_sra_epi32 (__m128i __A, __m128i __B)
{
  return (__m128i)__builtin_ia32_psrad128 ((__v4si)__A, (__v2di)__B);
}

static __inline __m128i
_mm_srl_epi16 (__m128i __A, __m128i __B)
{
  return (__m128i)__builtin_ia32_psrlw128 ((__v8hi)__A, (__v2di)__B);
}

static __inline __m128i
_mm_srl_epi32 (__m128i __A, __m128i __B)
{
  return (__m128i)__builtin_ia32_psrld128 ((__v4si)__A, (__v2di)__B);
}

static __inline __m128i
_mm_srl_epi64 (__m128i __A, __m128i __B)
{
  return (__m128i)__builtin_ia32_psrlq128 ((__v2di)__A, (__v2di)__B);
}

static __inline __m128i
_mm_slli_epi16 (__m128i __A, int __B)
{
  return (__m128i)__builtin_ia32_psllwi128 ((__v8hi)__A, __B);
}

static __inline __m128i
_mm_slli_epi32 (__m128i __A, int __B)
{
  return (__m128i)__builtin_ia32_pslldi128 ((__v4si)__A, __B);
}

static __inline __m128i
_mm_slli_epi64 (__m128i __A, int __B)
{
  return (__m128i)__builtin_ia32_psllqi128 ((__v2di)__A, __B);
}

static __inline __m128i
_mm_srai_epi16 (__m128i __A, int __B)
{
  return (__m128i)__builtin_ia32_psrawi128 ((__v8hi)__A, __B);
}

static __inline __m128i
_mm_srai_epi32 (__m128i __A, int __B)
{
  return (__m128i)__builtin_ia32_psradi128 ((__v4si)__A, __B);
}

#if 0
static __m128i __attribute__((__always_inline__))
_mm_srli_si128 (__m128i __A, const int __B)
{
  return ((__m128i)__builtin_ia32_psrldqi128 (__A, __B))
}

static __m128i __attribute__((__always_inline__))
_mm_srli_si128 (__m128i __A, const int __B)
{
  return ((__m128i)__builtin_ia32_pslldqi128 (__A, __B))
}
#endif
#define _mm_srli_si128(__A, __B) ((__m128i)__builtin_ia32_psrldqi128 (__A, __B))
#define _mm_slli_si128(__A, __B) ((__m128i)__builtin_ia32_pslldqi128 (__A, __B))

static __inline __m128i
_mm_srli_epi16 (__m128i __A, int __B)
{
  return (__m128i)__builtin_ia32_psrlwi128 ((__v8hi)__A, __B);
}

static __inline __m128i
_mm_srli_epi32 (__m128i __A, int __B)
{
  return (__m128i)__builtin_ia32_psrldi128 ((__v4si)__A, __B);
}

static __inline __m128i
_mm_srli_epi64 (__m128i __A, int __B)
{
  return (__m128i)__builtin_ia32_psrlqi128 ((__v2di)__A, __B);
}

static __inline __m128i
_mm_and_si128 (__m128i __A, __m128i __B)
{
  return (__m128i)__builtin_ia32_pand128 ((__v2di)__A, (__v2di)__B);
}

static __inline __m128i
_mm_andnot_si128 (__m128i __A, __m128i __B)
{
  return (__m128i)__builtin_ia32_pandn128 ((__v2di)__A, (__v2di)__B);
}

static __inline __m128i
_mm_or_si128 (__m128i __A, __m128i __B)
{
  return (__m128i)__builtin_ia32_por128 ((__v2di)__A, (__v2di)__B);
}

static __inline __m128i
_mm_xor_si128 (__m128i __A, __m128i __B)
{
  return (__m128i)__builtin_ia32_pxor128 ((__v2di)__A, (__v2di)__B);
}

static __inline __m128i
_mm_cmpeq_epi8 (__m128i __A, __m128i __B)
{
  return (__m128i)__builtin_ia32_pcmpeqb128 ((__v16qi)__A, (__v16qi)__B);
}

static __inline __m128i
_mm_cmpeq_epi16 (__m128i __A, __m128i __B)
{
  return (__m128i)__builtin_ia32_pcmpeqw128 ((__v8hi)__A, (__v8hi)__B);
}

static __inline __m128i
_mm_cmpeq_epi32 (__m128i __A, __m128i __B)
{
  return (__m128i)__builtin_ia32_pcmpeqd128 ((__v4si)__A, (__v4si)__B);
}

static __inline __m128i
_mm_cmplt_epi8 (__m128i __A, __m128i __B)
{
  return (__m128i)__builtin_ia32_pcmpgtb128 ((__v16qi)__B, (__v16qi)__A);
}

static __inline __m128i
_mm_cmplt_epi16 (__m128i __A, __m128i __B)
{
  return (__m128i)__builtin_ia32_pcmpgtw128 ((__v8hi)__B, (__v8hi)__A);
}

static __inline __m128i
_mm_cmplt_epi32 (__m128i __A, __m128i __B)
{
  return (__m128i)__builtin_ia32_pcmpgtd128 ((__v4si)__B, (__v4si)__A);
}

static __inline __m128i
_mm_cmpgt_epi8 (__m128i __A, __m128i __B)
{
  return (__m128i)__builtin_ia32_pcmpgtb128 ((__v16qi)__A, (__v16qi)__B);
}

static __inline __m128i
_mm_cmpgt_epi16 (__m128i __A, __m128i __B)
{
  return (__m128i)__builtin_ia32_pcmpgtw128 ((__v8hi)__A, (__v8hi)__B);
}

static __inline __m128i
_mm_cmpgt_epi32 (__m128i __A, __m128i __B)
{
  return (__m128i)__builtin_ia32_pcmpgtd128 ((__v4si)__A, (__v4si)__B);
}

#define _mm_extract_epi16(__A, __B) __builtin_ia32_pextrw128 ((__v8hi)__A, __B)

#define _mm_insert_epi16(__A, __B, __C) ((__m128i)__builtin_ia32_pinsrw128 ((__v8hi)__A, __B, __C))

static __inline __m128i
_mm_max_epi16 (__m128i __A, __m128i __B)
{
  return (__m128i)__builtin_ia32_pmaxsw128 ((__v8hi)__A, (__v8hi)__B);
}

static __inline __m128i
_mm_max_epu8 (__m128i __A, __m128i __B)
{
  return (__m128i)__builtin_ia32_pmaxub128 ((__v16qi)__A, (__v16qi)__B);
}

static __inline __m128i
_mm_min_epi16 (__m128i __A, __m128i __B)
{
  return (__m128i)__builtin_ia32_pminsw128 ((__v8hi)__A, (__v8hi)__B);
}

static __inline __m128i
_mm_min_epu8 (__m128i __A, __m128i __B)
{
  return (__m128i)__builtin_ia32_pminub128 ((__v16qi)__A, (__v16qi)__B);
}

static __inline int
_mm_movemask_epi8 (__m128i __A)
{
  return __builtin_ia32_pmovmskb128 ((__v16qi)__A);
}

static __inline __m128i
_mm_mulhi_epu16 (__m128i __A, __m128i __B)
{
  return (__m128i)__builtin_ia32_pmulhuw128 ((__v8hi)__A, (__v8hi)__B);
}

#define _mm_shufflehi_epi16(__A, __B) ((__m128i)__builtin_ia32_pshufhw ((__v8hi)__A, __B))
#define _mm_shufflelo_epi16(__A, __B) ((__m128i)__builtin_ia32_pshuflw ((__v8hi)__A, __B))
#define _mm_shuffle_epi32(__A, __B) ((__m128i)__builtin_ia32_pshufd ((__v4si)__A, __B))

static __inline void
_mm_maskmoveu_si128 (__m128i __A, __m128i __B, char *__C)
{
  __builtin_ia32_maskmovdqu ((__v16qi)__A, (__v16qi)__B, __C);
}

static __inline __m128i
_mm_avg_epu8 (__m128i __A, __m128i __B)
{
  return (__m128i)__builtin_ia32_pavgb128 ((__v16qi)__A, (__v16qi)__B);
}

static __inline __m128i
_mm_avg_epu16 (__m128i __A, __m128i __B)
{
  return (__m128i)__builtin_ia32_pavgw128 ((__v8hi)__A, (__v8hi)__B);
}

static __inline __m128i
_mm_sad_epu8 (__m128i __A, __m128i __B)
{
  return (__m128i)__builtin_ia32_psadbw128 ((__v16qi)__A, (__v16qi)__B);
}

static __inline void
_mm_stream_si32 (int *__A, int __B)
{
  __builtin_ia32_movnti (__A, __B);
}

static __inline void
_mm_stream_si128 (__m128i *__A, __m128i __B)
{
  __builtin_ia32_movntdq ((__v2di *)__A, (__v2di)__B);
}

static __inline void
_mm_stream_pd (double *__A, __m128d __B)
{
  __builtin_ia32_movntpd (__A, (__v2df)__B);
}

static __inline __m128i
_mm_movpi64_epi64 (__m64 __A)
{
  return (__m128i)__builtin_ia32_movq2dq ((unsigned long long)__A);
}

static __inline void
_mm_clflush (void const *__A)
{
  return __builtin_ia32_clflush (__A);
}

static __inline void
_mm_lfence (void)
{
  __builtin_ia32_lfence ();
}

static __inline void
_mm_mfence (void)
{
  __builtin_ia32_mfence ();
}

static __inline __m128i
_mm_cvtsi32_si128 (int __A)
{
  return (__m128i) __builtin_ia32_loadd (&__A);
}

#ifdef __x86_64__
static __inline __m128i
_mm_cvtsi64x_si128 (long long __A)
{
  return (__m128i) __builtin_ia32_movq2dq (__A);
}
#endif

static __inline int
_mm_cvtsi128_si32 (__m128i __A)
{
  int __tmp;
  __builtin_ia32_stored (&__tmp, (__v4si)__A);
  return __tmp;
}

#ifdef __x86_64__
static __inline long long
_mm_cvtsi128_si64x (__m128i __A)
{
  return __builtin_ia32_movdq2q ((__v2di)__A);
}
#endif

#endif /* __SSE2__  */

#endif /* _EMMINTRIN_H_INCLUDED */

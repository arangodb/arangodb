/*
 * Copyright 2013 MongoDB, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#if !defined (BSON_INSIDE) && !defined (BSON_COMPILATION)
# error "Only <bson.h> can be included directly."
#endif


#ifndef BSON_ENDIAN_H
#define BSON_ENDIAN_H


#include "bson-config.h"
#include "bson-macros.h"
#include "bson-compat.h"
#include "bson-types.h"


BSON_BEGIN_DECLS


#define BSON_BIG_ENDIAN    4321
#define BSON_LITTLE_ENDIAN 1234


/*
 *--------------------------------------------------------------------------
 *
 * __bson_uint16_swap_slow --
 *
 *       Fallback endianness conversion for 16-bit integers.
 *
 * Returns:
 *       The endian swapped version.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

static BSON_INLINE uint16_t
__bson_uint16_swap_slow (uint16_t v) /* IN */
{
   return ((v & 0xFF) << 8) | ((v & 0xFF00) >> 8);
}


/*
 *--------------------------------------------------------------------------
 *
 * __bson_uint32_swap_slow --
 *
 *       Fallback endianness conversion for 32-bit integers.
 *
 * Returns:
 *       The endian swapped version.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

static BSON_INLINE uint32_t
__bson_uint32_swap_slow (uint32_t v) /* IN */
{
   uint32_t ret;
   const char *src = (const char *)&v;
   char *dst = (char *)&ret;

   dst[0] = src[3];
   dst[1] = src[2];
   dst[2] = src[1];
   dst[3] = src[0];

   return ret;
}


/*
 *--------------------------------------------------------------------------
 *
 * __bson_uint64_swap_slow --
 *
 *       Fallback endianness conversion for 64-bit integers.
 *
 * Returns:
 *       The endian swapped version.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

static BSON_INLINE uint64_t
__bson_uint64_swap_slow (uint64_t v) /* IN */
{
   uint64_t ret;
   const char *src = (const char *)&v;
   char *dst = (char *)&ret;

   dst[0] = src[7];
   dst[1] = src[6];
   dst[2] = src[5];
   dst[3] = src[4];
   dst[4] = src[3];
   dst[5] = src[2];
   dst[6] = src[1];
   dst[7] = src[0];

   return ret;
}


/*
 *--------------------------------------------------------------------------
 *
 * __bson_double_swap_slow --
 *
 *       Fallback endianness conversion for double floating point.
 *
 * Returns:
 *       The endian swapped version.
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

static BSON_INLINE double
__bson_double_swap_slow (double v) /* IN */
{
   double ret;
   const char *src = (const char *)&v;
   char *dst = (char *)&ret;

   dst[0] = src[7];
   dst[1] = src[6];
   dst[2] = src[5];
   dst[3] = src[4];
   dst[4] = src[3];
   dst[5] = src[2];
   dst[6] = src[1];
   dst[7] = src[0];

   return ret;
}


#if defined (__GNUC__) && (__GNUC__ >= 4)
# if __GNUC__ >= 4 && defined (__GNUC_MINOR__) && __GNUC_MINOR__ >= 3
#  define BSON_UINT32_SWAP_LE_BE(v) __builtin_bswap32 ((uint32_t)v)
#  define BSON_UINT64_SWAP_LE_BE(v) __builtin_bswap64 ((uint64_t)v)
# endif
# if __GNUC__ >= 4 && defined (__GNUC_MINOR__) && __GNUC_MINOR__ >= 8
#  define BSON_UINT16_SWAP_LE_BE(v) __builtin_bswap16 ((uint32_t)v)
# endif
#endif


#ifndef BSON_UINT16_SWAP_LE_BE
# define BSON_UINT16_SWAP_LE_BE(v) __bson_uint16_swap_slow (v)
#endif


#ifndef BSON_UINT32_SWAP_LE_BE
# define BSON_UINT32_SWAP_LE_BE(v) __bson_uint32_swap_slow (v)
#endif


#ifndef BSON_UINT64_SWAP_LE_BE
# define BSON_UINT64_SWAP_LE_BE(v) __bson_uint64_swap_slow (v)
#endif


#if BSON_BYTE_ORDER == BSON_LITTLE_ENDIAN
# define BSON_UINT16_FROM_LE(v)  ((uint16_t)v)
# define BSON_UINT16_TO_LE(v)    ((uint16_t)v)
# define BSON_UINT16_FROM_BE(v)  BSON_UINT16_SWAP_LE_BE (v)
# define BSON_UINT16_TO_BE(v)    BSON_UINT16_SWAP_LE_BE (v)
# define BSON_UINT32_FROM_LE(v)  ((uint32_t)v)
# define BSON_UINT32_TO_LE(v)    ((uint32_t)v)
# define BSON_UINT32_FROM_BE(v)  BSON_UINT32_SWAP_LE_BE (v)
# define BSON_UINT32_TO_BE(v)    BSON_UINT32_SWAP_LE_BE (v)
# define BSON_UINT64_FROM_LE(v)  ((uint64_t)v)
# define BSON_UINT64_TO_LE(v)    ((uint64_t)v)
# define BSON_UINT64_FROM_BE(v)  BSON_UINT64_SWAP_LE_BE (v)
# define BSON_UINT64_TO_BE(v)    BSON_UINT64_SWAP_LE_BE (v)
# define BSON_DOUBLE_FROM_LE(v)  ((double)v)
# define BSON_DOUBLE_TO_LE(v)    ((double)v)
#elif BSON_BYTE_ORDER == BSON_BIG_ENDIAN
# define BSON_UINT16_FROM_LE(v)  BSON_UINT16_SWAP_LE_BE (v)
# define BSON_UINT16_TO_LE(v)    BSON_UINT16_SWAP_LE_BE (v)
# define BSON_UINT16_FROM_BE(v)  ((uint16_t)v)
# define BSON_UINT16_TO_BE(v)    ((uint16_t)v)
# define BSON_UINT32_FROM_LE(v)  BSON_UINT32_SWAP_LE_BE (v)
# define BSON_UINT32_TO_LE(v)    BSON_UINT32_SWAP_LE_BE (v)
# define BSON_UINT32_FROM_BE(v)  ((uint32_t)v)
# define BSON_UINT32_TO_BE(v)    ((uint32_t)v)
# define BSON_UINT64_FROM_LE(v)  BSON_UINT64_SWAP_LE_BE (v)
# define BSON_UINT64_TO_LE(v)    BSON_UINT64_SWAP_LE_BE (v)
# define BSON_UINT64_FROM_BE(v)  ((uint64_t)v)
# define BSON_UINT64_TO_BE(v)    ((uint64_t)v)
# define BSON_DOUBLE_FROM_LE(v)  (__bson_double_swap_slow (v))
# define BSON_DOUBLE_TO_LE(v)    (__bson_double_swap_slow (v))
#else
# error "The endianness of target architecture is unknown."
#endif


BSON_END_DECLS


#endif /* BSON_ENDIAN_H */

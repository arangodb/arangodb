/**
 * This code is released under a BSD License.
 */

#ifndef SIMDCOMPUTIL_H_
#define SIMDCOMPUTIL_H_

#include "portability.h"

/* SSE2 is required */
#include <emmintrin.h>

#ifdef __cplusplus
extern "C" {
#endif

/* returns the integer logarithm of v (bit width) */
SIMDCOMP_API
uint32_t bits(const uint32_t v);

/* max integer logarithm over a range of SIMDBlockSize integers (128 integer) */
SIMDCOMP_API
uint32_t maxbits(const uint32_t * begin);

/* same as maxbits, but we specify the number of integers */
SIMDCOMP_API
uint32_t maxbits_length(const uint32_t * in,uint32_t length);

enum{ SIMDBlockSize = 128};


/* computes (quickly) the minimal value of 128 values */
SIMDCOMP_API
uint32_t simdmin(const uint32_t * in);

/* computes (quickly) the minimal value of the specified number of values */
SIMDCOMP_API
uint32_t simdmin_length(const uint32_t * in, uint32_t length);

#ifdef __SSE4_1__
/* computes (quickly) the minimal and maximal value of the specified number of values */
SIMDCOMP_API
void simdmaxmin_length(const uint32_t * in, uint32_t length, uint32_t * getmin, uint32_t * getmax);

/* computes (quickly) the minimal and maximal value of the 128 values */
SIMDCOMP_API
void simdmaxmin(const uint32_t * in, uint32_t * getmin, uint32_t * getmax);

#endif

/* like maxbit over 128 integers (SIMDBlockSize) with provided initial value 
   and using differential coding */
SIMDCOMP_API
uint32_t simdmaxbitsd1(uint32_t initvalue, const uint32_t * in);

/* like simdmaxbitsd1, but calculates maxbits over |length| integers 
   with provided initial value. |length| can be any arbitrary value. */
SIMDCOMP_API
uint32_t simdmaxbitsd1_length(uint32_t initvalue, const uint32_t * in,
                uint32_t length);

#ifdef __cplusplus
}
#endif


#endif /* SIMDCOMPUTIL_H_ */

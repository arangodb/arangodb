/**
 * This code is released under a BSD License.
 */

#ifndef INCLUDE_AVX512BITPACKING_H_
#define INCLUDE_AVX512BITPACKING_H_


#ifdef __AVX512F__

#include "portability.h"


/* AVX512 is required */
#include <immintrin.h>
/* for memset */
#include <string.h>

#include "simdcomputil.h"

enum{ AVX512BlockSize = 512};

/* max integer logarithm over a range of AVX512BlockSize integers (512 integer) */
uint32_t avx512maxbits(const uint32_t * begin);

/* reads 512 values from "in", writes  "bit" 512-bit vectors to "out" */
void avx512pack(const uint32_t *  in,__m512i *  out, const uint32_t bit);

/* reads 512 values from "in", writes  "bit" 512-bit vectors to "out" */
void avx512packwithoutmask(const uint32_t *  in,__m512i *  out, const uint32_t bit);

/* reads  "bit" 512-bit vectors from "in", writes  512 values to "out" */
void avx512unpack(const __m512i *  in,uint32_t *  out, const uint32_t bit);




#endif /* __AVX512F__ */

#endif /* INCLUDE_AVX512BITPACKING_H_ */

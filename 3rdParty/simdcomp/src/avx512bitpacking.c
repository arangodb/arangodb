#include "avx512bitpacking.h"
#ifdef __AVX512F__

static uint32_t maxbitas32int(const __m256i accumulator) {
        const __m256i _tmp1 = _mm256_or_si256(_mm256_srli_si256(accumulator, 8), accumulator);
        const __m256i _tmp2 = _mm256_or_si256(_mm256_srli_si256(_tmp1, 4), _tmp1);
        uint32_t ans1 = _mm256_extract_epi32(_tmp2,0);
        uint32_t ans2 = _mm256_extract_epi32(_tmp2,4);
        uint32_t ans = ans1 > ans2 ? ans1 : ans2;
        return ans;
}

static uint32_t avx512maxbitas32int(const __m512i accumulator) {
  uint32_t ans1 = maxbitas32int(_mm512_castsi512_si256(accumulator));
  uint32_t ans2 = maxbitas32int(_mm512_extracti64x4_epi64(accumulator,1));
	uint32_t ans = ans1 > ans2 ? ans1 : ans2;
	return bits(ans);
}

uint32_t avx512maxbits(const uint32_t * begin) {
	    const __m512i* pin = (const __m512i*)(begin);
	    __m512i accumulator = _mm512_loadu_si512(pin);
	    uint32_t k = 1;
	    for(; 16*k < AVX512BlockSize; ++k) {
	    	__m512i newvec = _mm512_loadu_si512(pin+k);
	        accumulator = _mm512_or_si512(accumulator,newvec);
	    }
	    return avx512maxbitas32int(accumulator);
}


/** avx512packing **/

typedef void (*avx512packblockfnc)(const uint32_t * pin, __m512i * compressed);
typedef void (*avx512unpackblockfnc)(const __m512i * compressed, uint32_t * pout);

static void avx512packblock0(const uint32_t * pin, __m512i * compressed) {
  (void)compressed;
  (void) pin; /* we consumed 512 32-bit integers */
}


/* we are going to pack 512 1-bit values, touching 1 512-bit words, using 32 bytes */
static void avx512packblock1(const uint32_t * pin, __m512i * compressed) {
  const __m512i * in = (const __m512i *)  pin;
  /* we are going to touch  1 512-bit word */
  __m512i w0;
  w0 = _mm512_loadu_si512 (in + 0);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 1) , 1));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 2) , 2));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 3) , 3));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 4) , 4));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 5) , 5));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 6) , 6));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 7) , 7));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 8) , 8));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 9) , 9));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 10) , 10));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 11) , 11));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 12) , 12));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 13) , 13));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 14) , 14));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 15) , 15));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 16) , 16));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 17) , 17));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 18) , 18));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 19) , 19));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 20) , 20));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 21) , 21));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 22) , 22));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 23) , 23));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 24) , 24));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 25) , 25));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 26) , 26));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 27) , 27));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 28) , 28));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 29) , 29));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 30) , 30));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 31) , 31));
  _mm512_storeu_si512(compressed + 0, w0);
}


/* we are going to pack 512 2-bit values, touching 2 512-bit words, using 64 bytes */
static void avx512packblock2(const uint32_t * pin, __m512i * compressed) {
  const __m512i * in = (const __m512i *)  pin;
  /* we are going to touch  2 512-bit words */
  __m512i w0, w1;
  w0 = _mm512_loadu_si512 (in + 0);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 1) , 2));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 2) , 4));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 3) , 6));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 4) , 8));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 5) , 10));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 6) , 12));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 7) , 14));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 8) , 16));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 9) , 18));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 10) , 20));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 11) , 22));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 12) , 24));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 13) , 26));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 14) , 28));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 15) , 30));
  _mm512_storeu_si512(compressed + 0, w0);
  w1 = _mm512_loadu_si512 (in + 16);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 17) , 2));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 18) , 4));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 19) , 6));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 20) , 8));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 21) , 10));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 22) , 12));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 23) , 14));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 24) , 16));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 25) , 18));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 26) , 20));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 27) , 22));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 28) , 24));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 29) , 26));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 30) , 28));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 31) , 30));
  _mm512_storeu_si512(compressed + 1, w1);
}


/* we are going to pack 512 3-bit values, touching 3 512-bit words, using 96 bytes */
static void avx512packblock3(const uint32_t * pin, __m512i * compressed) {
  const __m512i * in = (const __m512i *)  pin;
  /* we are going to touch  3 512-bit words */
  __m512i w0, w1;
  __m512i tmp; /* used to store inputs at word boundary */
  w0 = _mm512_loadu_si512 (in + 0);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 1) , 3));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 2) , 6));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 3) , 9));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 4) , 12));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 5) , 15));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 6) , 18));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 7) , 21));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 8) , 24));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 9) , 27));
  tmp = _mm512_loadu_si512 (in + 10);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 30));
  w1 = _mm512_srli_epi32(tmp,2);
  _mm512_storeu_si512(compressed + 0, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 11) , 1));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 12) , 4));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 13) , 7));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 14) , 10));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 15) , 13));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 16) , 16));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 17) , 19));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 18) , 22));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 19) , 25));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 20) , 28));
  tmp = _mm512_loadu_si512 (in + 21);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 31));
  w0 = _mm512_srli_epi32(tmp,1);
  _mm512_storeu_si512(compressed + 1, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 22) , 2));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 23) , 5));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 24) , 8));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 25) , 11));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 26) , 14));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 27) , 17));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 28) , 20));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 29) , 23));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 30) , 26));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 31) , 29));
  _mm512_storeu_si512(compressed + 2, w0);
}


/* we are going to pack 512 4-bit values, touching 4 512-bit words, using 128 bytes */
static void avx512packblock4(const uint32_t * pin, __m512i * compressed) {
  const __m512i * in = (const __m512i *)  pin;
  /* we are going to touch  4 512-bit words */
  __m512i w0, w1;
  w0 = _mm512_loadu_si512 (in + 0);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 1) , 4));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 2) , 8));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 3) , 12));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 4) , 16));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 5) , 20));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 6) , 24));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 7) , 28));
  _mm512_storeu_si512(compressed + 0, w0);
  w1 = _mm512_loadu_si512 (in + 8);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 9) , 4));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 10) , 8));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 11) , 12));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 12) , 16));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 13) , 20));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 14) , 24));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 15) , 28));
  _mm512_storeu_si512(compressed + 1, w1);
  w0 = _mm512_loadu_si512 (in + 16);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 17) , 4));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 18) , 8));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 19) , 12));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 20) , 16));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 21) , 20));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 22) , 24));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 23) , 28));
  _mm512_storeu_si512(compressed + 2, w0);
  w1 = _mm512_loadu_si512 (in + 24);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 25) , 4));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 26) , 8));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 27) , 12));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 28) , 16));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 29) , 20));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 30) , 24));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 31) , 28));
  _mm512_storeu_si512(compressed + 3, w1);
}


/* we are going to pack 512 5-bit values, touching 5 512-bit words, using 160 bytes */
static void avx512packblock5(const uint32_t * pin, __m512i * compressed) {
  const __m512i * in = (const __m512i *)  pin;
  /* we are going to touch  5 512-bit words */
  __m512i w0, w1;
  __m512i tmp; /* used to store inputs at word boundary */
  w0 = _mm512_loadu_si512 (in + 0);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 1) , 5));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 2) , 10));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 3) , 15));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 4) , 20));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 5) , 25));
  tmp = _mm512_loadu_si512 (in + 6);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 30));
  w1 = _mm512_srli_epi32(tmp,2);
  _mm512_storeu_si512(compressed + 0, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 7) , 3));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 8) , 8));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 9) , 13));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 10) , 18));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 11) , 23));
  tmp = _mm512_loadu_si512 (in + 12);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 28));
  w0 = _mm512_srli_epi32(tmp,4);
  _mm512_storeu_si512(compressed + 1, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 13) , 1));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 14) , 6));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 15) , 11));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 16) , 16));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 17) , 21));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 18) , 26));
  tmp = _mm512_loadu_si512 (in + 19);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 31));
  w1 = _mm512_srli_epi32(tmp,1);
  _mm512_storeu_si512(compressed + 2, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 20) , 4));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 21) , 9));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 22) , 14));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 23) , 19));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 24) , 24));
  tmp = _mm512_loadu_si512 (in + 25);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 29));
  w0 = _mm512_srli_epi32(tmp,3);
  _mm512_storeu_si512(compressed + 3, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 26) , 2));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 27) , 7));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 28) , 12));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 29) , 17));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 30) , 22));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 31) , 27));
  _mm512_storeu_si512(compressed + 4, w0);
}


/* we are going to pack 512 6-bit values, touching 6 512-bit words, using 192 bytes */
static void avx512packblock6(const uint32_t * pin, __m512i * compressed) {
  const __m512i * in = (const __m512i *)  pin;
  /* we are going to touch  6 512-bit words */
  __m512i w0, w1;
  __m512i tmp; /* used to store inputs at word boundary */
  w0 = _mm512_loadu_si512 (in + 0);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 1) , 6));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 2) , 12));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 3) , 18));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 4) , 24));
  tmp = _mm512_loadu_si512 (in + 5);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 30));
  w1 = _mm512_srli_epi32(tmp,2);
  _mm512_storeu_si512(compressed + 0, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 6) , 4));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 7) , 10));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 8) , 16));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 9) , 22));
  tmp = _mm512_loadu_si512 (in + 10);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 28));
  w0 = _mm512_srli_epi32(tmp,4);
  _mm512_storeu_si512(compressed + 1, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 11) , 2));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 12) , 8));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 13) , 14));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 14) , 20));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 15) , 26));
  _mm512_storeu_si512(compressed + 2, w0);
  w1 = _mm512_loadu_si512 (in + 16);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 17) , 6));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 18) , 12));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 19) , 18));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 20) , 24));
  tmp = _mm512_loadu_si512 (in + 21);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 30));
  w0 = _mm512_srli_epi32(tmp,2);
  _mm512_storeu_si512(compressed + 3, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 22) , 4));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 23) , 10));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 24) , 16));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 25) , 22));
  tmp = _mm512_loadu_si512 (in + 26);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 28));
  w1 = _mm512_srli_epi32(tmp,4);
  _mm512_storeu_si512(compressed + 4, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 27) , 2));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 28) , 8));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 29) , 14));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 30) , 20));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 31) , 26));
  _mm512_storeu_si512(compressed + 5, w1);
}


/* we are going to pack 512 7-bit values, touching 7 512-bit words, using 224 bytes */
static void avx512packblock7(const uint32_t * pin, __m512i * compressed) {
  const __m512i * in = (const __m512i *)  pin;
  /* we are going to touch  7 512-bit words */
  __m512i w0, w1;
  __m512i tmp; /* used to store inputs at word boundary */
  w0 = _mm512_loadu_si512 (in + 0);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 1) , 7));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 2) , 14));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 3) , 21));
  tmp = _mm512_loadu_si512 (in + 4);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 28));
  w1 = _mm512_srli_epi32(tmp,4);
  _mm512_storeu_si512(compressed + 0, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 5) , 3));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 6) , 10));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 7) , 17));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 8) , 24));
  tmp = _mm512_loadu_si512 (in + 9);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 31));
  w0 = _mm512_srli_epi32(tmp,1);
  _mm512_storeu_si512(compressed + 1, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 10) , 6));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 11) , 13));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 12) , 20));
  tmp = _mm512_loadu_si512 (in + 13);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 27));
  w1 = _mm512_srli_epi32(tmp,5);
  _mm512_storeu_si512(compressed + 2, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 14) , 2));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 15) , 9));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 16) , 16));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 17) , 23));
  tmp = _mm512_loadu_si512 (in + 18);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 30));
  w0 = _mm512_srli_epi32(tmp,2);
  _mm512_storeu_si512(compressed + 3, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 19) , 5));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 20) , 12));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 21) , 19));
  tmp = _mm512_loadu_si512 (in + 22);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 26));
  w1 = _mm512_srli_epi32(tmp,6);
  _mm512_storeu_si512(compressed + 4, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 23) , 1));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 24) , 8));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 25) , 15));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 26) , 22));
  tmp = _mm512_loadu_si512 (in + 27);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 29));
  w0 = _mm512_srli_epi32(tmp,3);
  _mm512_storeu_si512(compressed + 5, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 28) , 4));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 29) , 11));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 30) , 18));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 31) , 25));
  _mm512_storeu_si512(compressed + 6, w0);
}


/* we are going to pack 512 8-bit values, touching 8 512-bit words, using 256 bytes */
static void avx512packblock8(const uint32_t * pin, __m512i * compressed) {
  const __m512i * in = (const __m512i *)  pin;
  /* we are going to touch  8 512-bit words */
  __m512i w0, w1;
  w0 = _mm512_loadu_si512 (in + 0);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 1) , 8));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 2) , 16));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 3) , 24));
  _mm512_storeu_si512(compressed + 0, w0);
  w1 = _mm512_loadu_si512 (in + 4);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 5) , 8));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 6) , 16));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 7) , 24));
  _mm512_storeu_si512(compressed + 1, w1);
  w0 = _mm512_loadu_si512 (in + 8);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 9) , 8));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 10) , 16));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 11) , 24));
  _mm512_storeu_si512(compressed + 2, w0);
  w1 = _mm512_loadu_si512 (in + 12);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 13) , 8));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 14) , 16));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 15) , 24));
  _mm512_storeu_si512(compressed + 3, w1);
  w0 = _mm512_loadu_si512 (in + 16);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 17) , 8));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 18) , 16));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 19) , 24));
  _mm512_storeu_si512(compressed + 4, w0);
  w1 = _mm512_loadu_si512 (in + 20);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 21) , 8));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 22) , 16));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 23) , 24));
  _mm512_storeu_si512(compressed + 5, w1);
  w0 = _mm512_loadu_si512 (in + 24);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 25) , 8));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 26) , 16));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 27) , 24));
  _mm512_storeu_si512(compressed + 6, w0);
  w1 = _mm512_loadu_si512 (in + 28);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 29) , 8));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 30) , 16));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 31) , 24));
  _mm512_storeu_si512(compressed + 7, w1);
}


/* we are going to pack 512 9-bit values, touching 9 512-bit words, using 288 bytes */
static void avx512packblock9(const uint32_t * pin, __m512i * compressed) {
  const __m512i * in = (const __m512i *)  pin;
  /* we are going to touch  9 512-bit words */
  __m512i w0, w1;
  __m512i tmp; /* used to store inputs at word boundary */
  w0 = _mm512_loadu_si512 (in + 0);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 1) , 9));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 2) , 18));
  tmp = _mm512_loadu_si512 (in + 3);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 27));
  w1 = _mm512_srli_epi32(tmp,5);
  _mm512_storeu_si512(compressed + 0, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 4) , 4));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 5) , 13));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 6) , 22));
  tmp = _mm512_loadu_si512 (in + 7);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 31));
  w0 = _mm512_srli_epi32(tmp,1);
  _mm512_storeu_si512(compressed + 1, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 8) , 8));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 9) , 17));
  tmp = _mm512_loadu_si512 (in + 10);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 26));
  w1 = _mm512_srli_epi32(tmp,6);
  _mm512_storeu_si512(compressed + 2, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 11) , 3));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 12) , 12));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 13) , 21));
  tmp = _mm512_loadu_si512 (in + 14);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 30));
  w0 = _mm512_srli_epi32(tmp,2);
  _mm512_storeu_si512(compressed + 3, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 15) , 7));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 16) , 16));
  tmp = _mm512_loadu_si512 (in + 17);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 25));
  w1 = _mm512_srli_epi32(tmp,7);
  _mm512_storeu_si512(compressed + 4, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 18) , 2));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 19) , 11));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 20) , 20));
  tmp = _mm512_loadu_si512 (in + 21);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 29));
  w0 = _mm512_srli_epi32(tmp,3);
  _mm512_storeu_si512(compressed + 5, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 22) , 6));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 23) , 15));
  tmp = _mm512_loadu_si512 (in + 24);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 24));
  w1 = _mm512_srli_epi32(tmp,8);
  _mm512_storeu_si512(compressed + 6, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 25) , 1));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 26) , 10));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 27) , 19));
  tmp = _mm512_loadu_si512 (in + 28);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 28));
  w0 = _mm512_srli_epi32(tmp,4);
  _mm512_storeu_si512(compressed + 7, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 29) , 5));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 30) , 14));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 31) , 23));
  _mm512_storeu_si512(compressed + 8, w0);
}


/* we are going to pack 512 10-bit values, touching 10 512-bit words, using 320 bytes */
static void avx512packblock10(const uint32_t * pin, __m512i * compressed) {
  const __m512i * in = (const __m512i *)  pin;
  /* we are going to touch  10 512-bit words */
  __m512i w0, w1;
  __m512i tmp; /* used to store inputs at word boundary */
  w0 = _mm512_loadu_si512 (in + 0);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 1) , 10));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 2) , 20));
  tmp = _mm512_loadu_si512 (in + 3);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 30));
  w1 = _mm512_srli_epi32(tmp,2);
  _mm512_storeu_si512(compressed + 0, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 4) , 8));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 5) , 18));
  tmp = _mm512_loadu_si512 (in + 6);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 28));
  w0 = _mm512_srli_epi32(tmp,4);
  _mm512_storeu_si512(compressed + 1, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 7) , 6));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 8) , 16));
  tmp = _mm512_loadu_si512 (in + 9);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 26));
  w1 = _mm512_srli_epi32(tmp,6);
  _mm512_storeu_si512(compressed + 2, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 10) , 4));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 11) , 14));
  tmp = _mm512_loadu_si512 (in + 12);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 24));
  w0 = _mm512_srli_epi32(tmp,8);
  _mm512_storeu_si512(compressed + 3, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 13) , 2));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 14) , 12));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 15) , 22));
  _mm512_storeu_si512(compressed + 4, w0);
  w1 = _mm512_loadu_si512 (in + 16);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 17) , 10));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 18) , 20));
  tmp = _mm512_loadu_si512 (in + 19);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 30));
  w0 = _mm512_srli_epi32(tmp,2);
  _mm512_storeu_si512(compressed + 5, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 20) , 8));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 21) , 18));
  tmp = _mm512_loadu_si512 (in + 22);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 28));
  w1 = _mm512_srli_epi32(tmp,4);
  _mm512_storeu_si512(compressed + 6, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 23) , 6));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 24) , 16));
  tmp = _mm512_loadu_si512 (in + 25);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 26));
  w0 = _mm512_srli_epi32(tmp,6);
  _mm512_storeu_si512(compressed + 7, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 26) , 4));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 27) , 14));
  tmp = _mm512_loadu_si512 (in + 28);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 24));
  w1 = _mm512_srli_epi32(tmp,8);
  _mm512_storeu_si512(compressed + 8, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 29) , 2));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 30) , 12));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 31) , 22));
  _mm512_storeu_si512(compressed + 9, w1);
}


/* we are going to pack 512 11-bit values, touching 11 512-bit words, using 352 bytes */
static void avx512packblock11(const uint32_t * pin, __m512i * compressed) {
  const __m512i * in = (const __m512i *)  pin;
  /* we are going to touch  11 512-bit words */
  __m512i w0, w1;
  __m512i tmp; /* used to store inputs at word boundary */
  w0 = _mm512_loadu_si512 (in + 0);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 1) , 11));
  tmp = _mm512_loadu_si512 (in + 2);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 22));
  w1 = _mm512_srli_epi32(tmp,10);
  _mm512_storeu_si512(compressed + 0, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 3) , 1));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 4) , 12));
  tmp = _mm512_loadu_si512 (in + 5);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 23));
  w0 = _mm512_srli_epi32(tmp,9);
  _mm512_storeu_si512(compressed + 1, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 6) , 2));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 7) , 13));
  tmp = _mm512_loadu_si512 (in + 8);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 24));
  w1 = _mm512_srli_epi32(tmp,8);
  _mm512_storeu_si512(compressed + 2, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 9) , 3));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 10) , 14));
  tmp = _mm512_loadu_si512 (in + 11);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 25));
  w0 = _mm512_srli_epi32(tmp,7);
  _mm512_storeu_si512(compressed + 3, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 12) , 4));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 13) , 15));
  tmp = _mm512_loadu_si512 (in + 14);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 26));
  w1 = _mm512_srli_epi32(tmp,6);
  _mm512_storeu_si512(compressed + 4, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 15) , 5));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 16) , 16));
  tmp = _mm512_loadu_si512 (in + 17);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 27));
  w0 = _mm512_srli_epi32(tmp,5);
  _mm512_storeu_si512(compressed + 5, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 18) , 6));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 19) , 17));
  tmp = _mm512_loadu_si512 (in + 20);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 28));
  w1 = _mm512_srli_epi32(tmp,4);
  _mm512_storeu_si512(compressed + 6, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 21) , 7));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 22) , 18));
  tmp = _mm512_loadu_si512 (in + 23);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 29));
  w0 = _mm512_srli_epi32(tmp,3);
  _mm512_storeu_si512(compressed + 7, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 24) , 8));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 25) , 19));
  tmp = _mm512_loadu_si512 (in + 26);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 30));
  w1 = _mm512_srli_epi32(tmp,2);
  _mm512_storeu_si512(compressed + 8, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 27) , 9));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 28) , 20));
  tmp = _mm512_loadu_si512 (in + 29);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 31));
  w0 = _mm512_srli_epi32(tmp,1);
  _mm512_storeu_si512(compressed + 9, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 30) , 10));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 31) , 21));
  _mm512_storeu_si512(compressed + 10, w0);
}


/* we are going to pack 512 12-bit values, touching 12 512-bit words, using 384 bytes */
static void avx512packblock12(const uint32_t * pin, __m512i * compressed) {
  const __m512i * in = (const __m512i *)  pin;
  /* we are going to touch  12 512-bit words */
  __m512i w0, w1;
  __m512i tmp; /* used to store inputs at word boundary */
  w0 = _mm512_loadu_si512 (in + 0);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 1) , 12));
  tmp = _mm512_loadu_si512 (in + 2);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 24));
  w1 = _mm512_srli_epi32(tmp,8);
  _mm512_storeu_si512(compressed + 0, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 3) , 4));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 4) , 16));
  tmp = _mm512_loadu_si512 (in + 5);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 28));
  w0 = _mm512_srli_epi32(tmp,4);
  _mm512_storeu_si512(compressed + 1, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 6) , 8));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 7) , 20));
  _mm512_storeu_si512(compressed + 2, w0);
  w1 = _mm512_loadu_si512 (in + 8);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 9) , 12));
  tmp = _mm512_loadu_si512 (in + 10);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 24));
  w0 = _mm512_srli_epi32(tmp,8);
  _mm512_storeu_si512(compressed + 3, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 11) , 4));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 12) , 16));
  tmp = _mm512_loadu_si512 (in + 13);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 28));
  w1 = _mm512_srli_epi32(tmp,4);
  _mm512_storeu_si512(compressed + 4, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 14) , 8));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 15) , 20));
  _mm512_storeu_si512(compressed + 5, w1);
  w0 = _mm512_loadu_si512 (in + 16);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 17) , 12));
  tmp = _mm512_loadu_si512 (in + 18);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 24));
  w1 = _mm512_srli_epi32(tmp,8);
  _mm512_storeu_si512(compressed + 6, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 19) , 4));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 20) , 16));
  tmp = _mm512_loadu_si512 (in + 21);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 28));
  w0 = _mm512_srli_epi32(tmp,4);
  _mm512_storeu_si512(compressed + 7, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 22) , 8));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 23) , 20));
  _mm512_storeu_si512(compressed + 8, w0);
  w1 = _mm512_loadu_si512 (in + 24);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 25) , 12));
  tmp = _mm512_loadu_si512 (in + 26);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 24));
  w0 = _mm512_srli_epi32(tmp,8);
  _mm512_storeu_si512(compressed + 9, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 27) , 4));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 28) , 16));
  tmp = _mm512_loadu_si512 (in + 29);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 28));
  w1 = _mm512_srli_epi32(tmp,4);
  _mm512_storeu_si512(compressed + 10, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 30) , 8));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 31) , 20));
  _mm512_storeu_si512(compressed + 11, w1);
}


/* we are going to pack 512 13-bit values, touching 13 512-bit words, using 416 bytes */
static void avx512packblock13(const uint32_t * pin, __m512i * compressed) {
  const __m512i * in = (const __m512i *)  pin;
  /* we are going to touch  13 512-bit words */
  __m512i w0, w1;
  __m512i tmp; /* used to store inputs at word boundary */
  w0 = _mm512_loadu_si512 (in + 0);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 1) , 13));
  tmp = _mm512_loadu_si512 (in + 2);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 26));
  w1 = _mm512_srli_epi32(tmp,6);
  _mm512_storeu_si512(compressed + 0, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 3) , 7));
  tmp = _mm512_loadu_si512 (in + 4);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 20));
  w0 = _mm512_srli_epi32(tmp,12);
  _mm512_storeu_si512(compressed + 1, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 5) , 1));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 6) , 14));
  tmp = _mm512_loadu_si512 (in + 7);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 27));
  w1 = _mm512_srli_epi32(tmp,5);
  _mm512_storeu_si512(compressed + 2, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 8) , 8));
  tmp = _mm512_loadu_si512 (in + 9);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 21));
  w0 = _mm512_srli_epi32(tmp,11);
  _mm512_storeu_si512(compressed + 3, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 10) , 2));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 11) , 15));
  tmp = _mm512_loadu_si512 (in + 12);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 28));
  w1 = _mm512_srli_epi32(tmp,4);
  _mm512_storeu_si512(compressed + 4, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 13) , 9));
  tmp = _mm512_loadu_si512 (in + 14);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 22));
  w0 = _mm512_srli_epi32(tmp,10);
  _mm512_storeu_si512(compressed + 5, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 15) , 3));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 16) , 16));
  tmp = _mm512_loadu_si512 (in + 17);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 29));
  w1 = _mm512_srli_epi32(tmp,3);
  _mm512_storeu_si512(compressed + 6, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 18) , 10));
  tmp = _mm512_loadu_si512 (in + 19);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 23));
  w0 = _mm512_srli_epi32(tmp,9);
  _mm512_storeu_si512(compressed + 7, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 20) , 4));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 21) , 17));
  tmp = _mm512_loadu_si512 (in + 22);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 30));
  w1 = _mm512_srli_epi32(tmp,2);
  _mm512_storeu_si512(compressed + 8, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 23) , 11));
  tmp = _mm512_loadu_si512 (in + 24);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 24));
  w0 = _mm512_srli_epi32(tmp,8);
  _mm512_storeu_si512(compressed + 9, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 25) , 5));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 26) , 18));
  tmp = _mm512_loadu_si512 (in + 27);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 31));
  w1 = _mm512_srli_epi32(tmp,1);
  _mm512_storeu_si512(compressed + 10, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 28) , 12));
  tmp = _mm512_loadu_si512 (in + 29);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 25));
  w0 = _mm512_srli_epi32(tmp,7);
  _mm512_storeu_si512(compressed + 11, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 30) , 6));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 31) , 19));
  _mm512_storeu_si512(compressed + 12, w0);
}


/* we are going to pack 512 14-bit values, touching 14 512-bit words, using 448 bytes */
static void avx512packblock14(const uint32_t * pin, __m512i * compressed) {
  const __m512i * in = (const __m512i *)  pin;
  /* we are going to touch  14 512-bit words */
  __m512i w0, w1;
  __m512i tmp; /* used to store inputs at word boundary */
  w0 = _mm512_loadu_si512 (in + 0);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 1) , 14));
  tmp = _mm512_loadu_si512 (in + 2);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 28));
  w1 = _mm512_srli_epi32(tmp,4);
  _mm512_storeu_si512(compressed + 0, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 3) , 10));
  tmp = _mm512_loadu_si512 (in + 4);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 24));
  w0 = _mm512_srli_epi32(tmp,8);
  _mm512_storeu_si512(compressed + 1, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 5) , 6));
  tmp = _mm512_loadu_si512 (in + 6);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 20));
  w1 = _mm512_srli_epi32(tmp,12);
  _mm512_storeu_si512(compressed + 2, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 7) , 2));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 8) , 16));
  tmp = _mm512_loadu_si512 (in + 9);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 30));
  w0 = _mm512_srli_epi32(tmp,2);
  _mm512_storeu_si512(compressed + 3, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 10) , 12));
  tmp = _mm512_loadu_si512 (in + 11);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 26));
  w1 = _mm512_srli_epi32(tmp,6);
  _mm512_storeu_si512(compressed + 4, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 12) , 8));
  tmp = _mm512_loadu_si512 (in + 13);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 22));
  w0 = _mm512_srli_epi32(tmp,10);
  _mm512_storeu_si512(compressed + 5, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 14) , 4));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 15) , 18));
  _mm512_storeu_si512(compressed + 6, w0);
  w1 = _mm512_loadu_si512 (in + 16);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 17) , 14));
  tmp = _mm512_loadu_si512 (in + 18);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 28));
  w0 = _mm512_srli_epi32(tmp,4);
  _mm512_storeu_si512(compressed + 7, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 19) , 10));
  tmp = _mm512_loadu_si512 (in + 20);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 24));
  w1 = _mm512_srli_epi32(tmp,8);
  _mm512_storeu_si512(compressed + 8, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 21) , 6));
  tmp = _mm512_loadu_si512 (in + 22);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 20));
  w0 = _mm512_srli_epi32(tmp,12);
  _mm512_storeu_si512(compressed + 9, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 23) , 2));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 24) , 16));
  tmp = _mm512_loadu_si512 (in + 25);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 30));
  w1 = _mm512_srli_epi32(tmp,2);
  _mm512_storeu_si512(compressed + 10, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 26) , 12));
  tmp = _mm512_loadu_si512 (in + 27);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 26));
  w0 = _mm512_srli_epi32(tmp,6);
  _mm512_storeu_si512(compressed + 11, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 28) , 8));
  tmp = _mm512_loadu_si512 (in + 29);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 22));
  w1 = _mm512_srli_epi32(tmp,10);
  _mm512_storeu_si512(compressed + 12, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 30) , 4));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 31) , 18));
  _mm512_storeu_si512(compressed + 13, w1);
}


/* we are going to pack 512 15-bit values, touching 15 512-bit words, using 480 bytes */
static void avx512packblock15(const uint32_t * pin, __m512i * compressed) {
  const __m512i * in = (const __m512i *)  pin;
  /* we are going to touch  15 512-bit words */
  __m512i w0, w1;
  __m512i tmp; /* used to store inputs at word boundary */
  w0 = _mm512_loadu_si512 (in + 0);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 1) , 15));
  tmp = _mm512_loadu_si512 (in + 2);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 30));
  w1 = _mm512_srli_epi32(tmp,2);
  _mm512_storeu_si512(compressed + 0, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 3) , 13));
  tmp = _mm512_loadu_si512 (in + 4);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 28));
  w0 = _mm512_srli_epi32(tmp,4);
  _mm512_storeu_si512(compressed + 1, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 5) , 11));
  tmp = _mm512_loadu_si512 (in + 6);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 26));
  w1 = _mm512_srli_epi32(tmp,6);
  _mm512_storeu_si512(compressed + 2, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 7) , 9));
  tmp = _mm512_loadu_si512 (in + 8);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 24));
  w0 = _mm512_srli_epi32(tmp,8);
  _mm512_storeu_si512(compressed + 3, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 9) , 7));
  tmp = _mm512_loadu_si512 (in + 10);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 22));
  w1 = _mm512_srli_epi32(tmp,10);
  _mm512_storeu_si512(compressed + 4, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 11) , 5));
  tmp = _mm512_loadu_si512 (in + 12);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 20));
  w0 = _mm512_srli_epi32(tmp,12);
  _mm512_storeu_si512(compressed + 5, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 13) , 3));
  tmp = _mm512_loadu_si512 (in + 14);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 18));
  w1 = _mm512_srli_epi32(tmp,14);
  _mm512_storeu_si512(compressed + 6, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 15) , 1));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 16) , 16));
  tmp = _mm512_loadu_si512 (in + 17);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 31));
  w0 = _mm512_srli_epi32(tmp,1);
  _mm512_storeu_si512(compressed + 7, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 18) , 14));
  tmp = _mm512_loadu_si512 (in + 19);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 29));
  w1 = _mm512_srli_epi32(tmp,3);
  _mm512_storeu_si512(compressed + 8, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 20) , 12));
  tmp = _mm512_loadu_si512 (in + 21);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 27));
  w0 = _mm512_srli_epi32(tmp,5);
  _mm512_storeu_si512(compressed + 9, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 22) , 10));
  tmp = _mm512_loadu_si512 (in + 23);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 25));
  w1 = _mm512_srli_epi32(tmp,7);
  _mm512_storeu_si512(compressed + 10, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 24) , 8));
  tmp = _mm512_loadu_si512 (in + 25);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 23));
  w0 = _mm512_srli_epi32(tmp,9);
  _mm512_storeu_si512(compressed + 11, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 26) , 6));
  tmp = _mm512_loadu_si512 (in + 27);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 21));
  w1 = _mm512_srli_epi32(tmp,11);
  _mm512_storeu_si512(compressed + 12, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 28) , 4));
  tmp = _mm512_loadu_si512 (in + 29);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 19));
  w0 = _mm512_srli_epi32(tmp,13);
  _mm512_storeu_si512(compressed + 13, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 30) , 2));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 31) , 17));
  _mm512_storeu_si512(compressed + 14, w0);
}


/* we are going to pack 512 16-bit values, touching 16 512-bit words, using 512 bytes */
static void avx512packblock16(const uint32_t * pin, __m512i * compressed) {
  const __m512i * in = (const __m512i *)  pin;
  /* we are going to touch  16 512-bit words */
  __m512i w0, w1;
  w0 = _mm512_loadu_si512 (in + 0);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 1) , 16));
  _mm512_storeu_si512(compressed + 0, w0);
  w1 = _mm512_loadu_si512 (in + 2);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 3) , 16));
  _mm512_storeu_si512(compressed + 1, w1);
  w0 = _mm512_loadu_si512 (in + 4);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 5) , 16));
  _mm512_storeu_si512(compressed + 2, w0);
  w1 = _mm512_loadu_si512 (in + 6);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 7) , 16));
  _mm512_storeu_si512(compressed + 3, w1);
  w0 = _mm512_loadu_si512 (in + 8);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 9) , 16));
  _mm512_storeu_si512(compressed + 4, w0);
  w1 = _mm512_loadu_si512 (in + 10);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 11) , 16));
  _mm512_storeu_si512(compressed + 5, w1);
  w0 = _mm512_loadu_si512 (in + 12);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 13) , 16));
  _mm512_storeu_si512(compressed + 6, w0);
  w1 = _mm512_loadu_si512 (in + 14);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 15) , 16));
  _mm512_storeu_si512(compressed + 7, w1);
  w0 = _mm512_loadu_si512 (in + 16);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 17) , 16));
  _mm512_storeu_si512(compressed + 8, w0);
  w1 = _mm512_loadu_si512 (in + 18);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 19) , 16));
  _mm512_storeu_si512(compressed + 9, w1);
  w0 = _mm512_loadu_si512 (in + 20);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 21) , 16));
  _mm512_storeu_si512(compressed + 10, w0);
  w1 = _mm512_loadu_si512 (in + 22);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 23) , 16));
  _mm512_storeu_si512(compressed + 11, w1);
  w0 = _mm512_loadu_si512 (in + 24);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 25) , 16));
  _mm512_storeu_si512(compressed + 12, w0);
  w1 = _mm512_loadu_si512 (in + 26);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 27) , 16));
  _mm512_storeu_si512(compressed + 13, w1);
  w0 = _mm512_loadu_si512 (in + 28);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 29) , 16));
  _mm512_storeu_si512(compressed + 14, w0);
  w1 = _mm512_loadu_si512 (in + 30);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 31) , 16));
  _mm512_storeu_si512(compressed + 15, w1);
}


/* we are going to pack 512 17-bit values, touching 17 512-bit words, using 544 bytes */
static void avx512packblock17(const uint32_t * pin, __m512i * compressed) {
  const __m512i * in = (const __m512i *)  pin;
  /* we are going to touch  17 512-bit words */
  __m512i w0, w1;
  __m512i tmp; /* used to store inputs at word boundary */
  w0 = _mm512_loadu_si512 (in + 0);
  tmp = _mm512_loadu_si512 (in + 1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 17));
  w1 = _mm512_srli_epi32(tmp,15);
  _mm512_storeu_si512(compressed + 0, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 2) , 2));
  tmp = _mm512_loadu_si512 (in + 3);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 19));
  w0 = _mm512_srli_epi32(tmp,13);
  _mm512_storeu_si512(compressed + 1, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 4) , 4));
  tmp = _mm512_loadu_si512 (in + 5);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 21));
  w1 = _mm512_srli_epi32(tmp,11);
  _mm512_storeu_si512(compressed + 2, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 6) , 6));
  tmp = _mm512_loadu_si512 (in + 7);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 23));
  w0 = _mm512_srli_epi32(tmp,9);
  _mm512_storeu_si512(compressed + 3, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 8) , 8));
  tmp = _mm512_loadu_si512 (in + 9);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 25));
  w1 = _mm512_srli_epi32(tmp,7);
  _mm512_storeu_si512(compressed + 4, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 10) , 10));
  tmp = _mm512_loadu_si512 (in + 11);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 27));
  w0 = _mm512_srli_epi32(tmp,5);
  _mm512_storeu_si512(compressed + 5, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 12) , 12));
  tmp = _mm512_loadu_si512 (in + 13);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 29));
  w1 = _mm512_srli_epi32(tmp,3);
  _mm512_storeu_si512(compressed + 6, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 14) , 14));
  tmp = _mm512_loadu_si512 (in + 15);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 31));
  w0 = _mm512_srli_epi32(tmp,1);
  _mm512_storeu_si512(compressed + 7, w1);
  tmp = _mm512_loadu_si512 (in + 16);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 16));
  w1 = _mm512_srli_epi32(tmp,16);
  _mm512_storeu_si512(compressed + 8, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 17) , 1));
  tmp = _mm512_loadu_si512 (in + 18);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 18));
  w0 = _mm512_srli_epi32(tmp,14);
  _mm512_storeu_si512(compressed + 9, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 19) , 3));
  tmp = _mm512_loadu_si512 (in + 20);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 20));
  w1 = _mm512_srli_epi32(tmp,12);
  _mm512_storeu_si512(compressed + 10, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 21) , 5));
  tmp = _mm512_loadu_si512 (in + 22);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 22));
  w0 = _mm512_srli_epi32(tmp,10);
  _mm512_storeu_si512(compressed + 11, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 23) , 7));
  tmp = _mm512_loadu_si512 (in + 24);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 24));
  w1 = _mm512_srli_epi32(tmp,8);
  _mm512_storeu_si512(compressed + 12, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 25) , 9));
  tmp = _mm512_loadu_si512 (in + 26);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 26));
  w0 = _mm512_srli_epi32(tmp,6);
  _mm512_storeu_si512(compressed + 13, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 27) , 11));
  tmp = _mm512_loadu_si512 (in + 28);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 28));
  w1 = _mm512_srli_epi32(tmp,4);
  _mm512_storeu_si512(compressed + 14, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 29) , 13));
  tmp = _mm512_loadu_si512 (in + 30);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 30));
  w0 = _mm512_srli_epi32(tmp,2);
  _mm512_storeu_si512(compressed + 15, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 31) , 15));
  _mm512_storeu_si512(compressed + 16, w0);
}


/* we are going to pack 512 18-bit values, touching 18 512-bit words, using 576 bytes */
static void avx512packblock18(const uint32_t * pin, __m512i * compressed) {
  const __m512i * in = (const __m512i *)  pin;
  /* we are going to touch  18 512-bit words */
  __m512i w0, w1;
  __m512i tmp; /* used to store inputs at word boundary */
  w0 = _mm512_loadu_si512 (in + 0);
  tmp = _mm512_loadu_si512 (in + 1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 18));
  w1 = _mm512_srli_epi32(tmp,14);
  _mm512_storeu_si512(compressed + 0, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 2) , 4));
  tmp = _mm512_loadu_si512 (in + 3);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 22));
  w0 = _mm512_srli_epi32(tmp,10);
  _mm512_storeu_si512(compressed + 1, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 4) , 8));
  tmp = _mm512_loadu_si512 (in + 5);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 26));
  w1 = _mm512_srli_epi32(tmp,6);
  _mm512_storeu_si512(compressed + 2, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 6) , 12));
  tmp = _mm512_loadu_si512 (in + 7);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 30));
  w0 = _mm512_srli_epi32(tmp,2);
  _mm512_storeu_si512(compressed + 3, w1);
  tmp = _mm512_loadu_si512 (in + 8);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 16));
  w1 = _mm512_srli_epi32(tmp,16);
  _mm512_storeu_si512(compressed + 4, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 9) , 2));
  tmp = _mm512_loadu_si512 (in + 10);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 20));
  w0 = _mm512_srli_epi32(tmp,12);
  _mm512_storeu_si512(compressed + 5, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 11) , 6));
  tmp = _mm512_loadu_si512 (in + 12);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 24));
  w1 = _mm512_srli_epi32(tmp,8);
  _mm512_storeu_si512(compressed + 6, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 13) , 10));
  tmp = _mm512_loadu_si512 (in + 14);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 28));
  w0 = _mm512_srli_epi32(tmp,4);
  _mm512_storeu_si512(compressed + 7, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 15) , 14));
  _mm512_storeu_si512(compressed + 8, w0);
  w1 = _mm512_loadu_si512 (in + 16);
  tmp = _mm512_loadu_si512 (in + 17);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 18));
  w0 = _mm512_srli_epi32(tmp,14);
  _mm512_storeu_si512(compressed + 9, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 18) , 4));
  tmp = _mm512_loadu_si512 (in + 19);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 22));
  w1 = _mm512_srli_epi32(tmp,10);
  _mm512_storeu_si512(compressed + 10, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 20) , 8));
  tmp = _mm512_loadu_si512 (in + 21);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 26));
  w0 = _mm512_srli_epi32(tmp,6);
  _mm512_storeu_si512(compressed + 11, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 22) , 12));
  tmp = _mm512_loadu_si512 (in + 23);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 30));
  w1 = _mm512_srli_epi32(tmp,2);
  _mm512_storeu_si512(compressed + 12, w0);
  tmp = _mm512_loadu_si512 (in + 24);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 16));
  w0 = _mm512_srli_epi32(tmp,16);
  _mm512_storeu_si512(compressed + 13, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 25) , 2));
  tmp = _mm512_loadu_si512 (in + 26);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 20));
  w1 = _mm512_srli_epi32(tmp,12);
  _mm512_storeu_si512(compressed + 14, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 27) , 6));
  tmp = _mm512_loadu_si512 (in + 28);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 24));
  w0 = _mm512_srli_epi32(tmp,8);
  _mm512_storeu_si512(compressed + 15, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 29) , 10));
  tmp = _mm512_loadu_si512 (in + 30);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 28));
  w1 = _mm512_srli_epi32(tmp,4);
  _mm512_storeu_si512(compressed + 16, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 31) , 14));
  _mm512_storeu_si512(compressed + 17, w1);
}


/* we are going to pack 512 19-bit values, touching 19 512-bit words, using 608 bytes */
static void avx512packblock19(const uint32_t * pin, __m512i * compressed) {
  const __m512i * in = (const __m512i *)  pin;
  /* we are going to touch  19 512-bit words */
  __m512i w0, w1;
  __m512i tmp; /* used to store inputs at word boundary */
  w0 = _mm512_loadu_si512 (in + 0);
  tmp = _mm512_loadu_si512 (in + 1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 19));
  w1 = _mm512_srli_epi32(tmp,13);
  _mm512_storeu_si512(compressed + 0, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 2) , 6));
  tmp = _mm512_loadu_si512 (in + 3);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 25));
  w0 = _mm512_srli_epi32(tmp,7);
  _mm512_storeu_si512(compressed + 1, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 4) , 12));
  tmp = _mm512_loadu_si512 (in + 5);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 31));
  w1 = _mm512_srli_epi32(tmp,1);
  _mm512_storeu_si512(compressed + 2, w0);
  tmp = _mm512_loadu_si512 (in + 6);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 18));
  w0 = _mm512_srli_epi32(tmp,14);
  _mm512_storeu_si512(compressed + 3, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 7) , 5));
  tmp = _mm512_loadu_si512 (in + 8);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 24));
  w1 = _mm512_srli_epi32(tmp,8);
  _mm512_storeu_si512(compressed + 4, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 9) , 11));
  tmp = _mm512_loadu_si512 (in + 10);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 30));
  w0 = _mm512_srli_epi32(tmp,2);
  _mm512_storeu_si512(compressed + 5, w1);
  tmp = _mm512_loadu_si512 (in + 11);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 17));
  w1 = _mm512_srli_epi32(tmp,15);
  _mm512_storeu_si512(compressed + 6, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 12) , 4));
  tmp = _mm512_loadu_si512 (in + 13);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 23));
  w0 = _mm512_srli_epi32(tmp,9);
  _mm512_storeu_si512(compressed + 7, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 14) , 10));
  tmp = _mm512_loadu_si512 (in + 15);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 29));
  w1 = _mm512_srli_epi32(tmp,3);
  _mm512_storeu_si512(compressed + 8, w0);
  tmp = _mm512_loadu_si512 (in + 16);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 16));
  w0 = _mm512_srli_epi32(tmp,16);
  _mm512_storeu_si512(compressed + 9, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 17) , 3));
  tmp = _mm512_loadu_si512 (in + 18);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 22));
  w1 = _mm512_srli_epi32(tmp,10);
  _mm512_storeu_si512(compressed + 10, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 19) , 9));
  tmp = _mm512_loadu_si512 (in + 20);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 28));
  w0 = _mm512_srli_epi32(tmp,4);
  _mm512_storeu_si512(compressed + 11, w1);
  tmp = _mm512_loadu_si512 (in + 21);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 15));
  w1 = _mm512_srli_epi32(tmp,17);
  _mm512_storeu_si512(compressed + 12, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 22) , 2));
  tmp = _mm512_loadu_si512 (in + 23);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 21));
  w0 = _mm512_srli_epi32(tmp,11);
  _mm512_storeu_si512(compressed + 13, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 24) , 8));
  tmp = _mm512_loadu_si512 (in + 25);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 27));
  w1 = _mm512_srli_epi32(tmp,5);
  _mm512_storeu_si512(compressed + 14, w0);
  tmp = _mm512_loadu_si512 (in + 26);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 14));
  w0 = _mm512_srli_epi32(tmp,18);
  _mm512_storeu_si512(compressed + 15, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 27) , 1));
  tmp = _mm512_loadu_si512 (in + 28);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 20));
  w1 = _mm512_srli_epi32(tmp,12);
  _mm512_storeu_si512(compressed + 16, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 29) , 7));
  tmp = _mm512_loadu_si512 (in + 30);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 26));
  w0 = _mm512_srli_epi32(tmp,6);
  _mm512_storeu_si512(compressed + 17, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 31) , 13));
  _mm512_storeu_si512(compressed + 18, w0);
}


/* we are going to pack 512 20-bit values, touching 20 512-bit words, using 640 bytes */
static void avx512packblock20(const uint32_t * pin, __m512i * compressed) {
  const __m512i * in = (const __m512i *)  pin;
  /* we are going to touch  20 512-bit words */
  __m512i w0, w1;
  __m512i tmp; /* used to store inputs at word boundary */
  w0 = _mm512_loadu_si512 (in + 0);
  tmp = _mm512_loadu_si512 (in + 1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 20));
  w1 = _mm512_srli_epi32(tmp,12);
  _mm512_storeu_si512(compressed + 0, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 2) , 8));
  tmp = _mm512_loadu_si512 (in + 3);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 28));
  w0 = _mm512_srli_epi32(tmp,4);
  _mm512_storeu_si512(compressed + 1, w1);
  tmp = _mm512_loadu_si512 (in + 4);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 16));
  w1 = _mm512_srli_epi32(tmp,16);
  _mm512_storeu_si512(compressed + 2, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 5) , 4));
  tmp = _mm512_loadu_si512 (in + 6);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 24));
  w0 = _mm512_srli_epi32(tmp,8);
  _mm512_storeu_si512(compressed + 3, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 7) , 12));
  _mm512_storeu_si512(compressed + 4, w0);
  w1 = _mm512_loadu_si512 (in + 8);
  tmp = _mm512_loadu_si512 (in + 9);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 20));
  w0 = _mm512_srli_epi32(tmp,12);
  _mm512_storeu_si512(compressed + 5, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 10) , 8));
  tmp = _mm512_loadu_si512 (in + 11);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 28));
  w1 = _mm512_srli_epi32(tmp,4);
  _mm512_storeu_si512(compressed + 6, w0);
  tmp = _mm512_loadu_si512 (in + 12);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 16));
  w0 = _mm512_srli_epi32(tmp,16);
  _mm512_storeu_si512(compressed + 7, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 13) , 4));
  tmp = _mm512_loadu_si512 (in + 14);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 24));
  w1 = _mm512_srli_epi32(tmp,8);
  _mm512_storeu_si512(compressed + 8, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 15) , 12));
  _mm512_storeu_si512(compressed + 9, w1);
  w0 = _mm512_loadu_si512 (in + 16);
  tmp = _mm512_loadu_si512 (in + 17);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 20));
  w1 = _mm512_srli_epi32(tmp,12);
  _mm512_storeu_si512(compressed + 10, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 18) , 8));
  tmp = _mm512_loadu_si512 (in + 19);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 28));
  w0 = _mm512_srli_epi32(tmp,4);
  _mm512_storeu_si512(compressed + 11, w1);
  tmp = _mm512_loadu_si512 (in + 20);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 16));
  w1 = _mm512_srli_epi32(tmp,16);
  _mm512_storeu_si512(compressed + 12, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 21) , 4));
  tmp = _mm512_loadu_si512 (in + 22);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 24));
  w0 = _mm512_srli_epi32(tmp,8);
  _mm512_storeu_si512(compressed + 13, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 23) , 12));
  _mm512_storeu_si512(compressed + 14, w0);
  w1 = _mm512_loadu_si512 (in + 24);
  tmp = _mm512_loadu_si512 (in + 25);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 20));
  w0 = _mm512_srli_epi32(tmp,12);
  _mm512_storeu_si512(compressed + 15, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 26) , 8));
  tmp = _mm512_loadu_si512 (in + 27);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 28));
  w1 = _mm512_srli_epi32(tmp,4);
  _mm512_storeu_si512(compressed + 16, w0);
  tmp = _mm512_loadu_si512 (in + 28);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 16));
  w0 = _mm512_srli_epi32(tmp,16);
  _mm512_storeu_si512(compressed + 17, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 29) , 4));
  tmp = _mm512_loadu_si512 (in + 30);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 24));
  w1 = _mm512_srli_epi32(tmp,8);
  _mm512_storeu_si512(compressed + 18, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 31) , 12));
  _mm512_storeu_si512(compressed + 19, w1);
}


/* we are going to pack 512 21-bit values, touching 21 512-bit words, using 672 bytes */
static void avx512packblock21(const uint32_t * pin, __m512i * compressed) {
  const __m512i * in = (const __m512i *)  pin;
  /* we are going to touch  21 512-bit words */
  __m512i w0, w1;
  __m512i tmp; /* used to store inputs at word boundary */
  w0 = _mm512_loadu_si512 (in + 0);
  tmp = _mm512_loadu_si512 (in + 1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 21));
  w1 = _mm512_srli_epi32(tmp,11);
  _mm512_storeu_si512(compressed + 0, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 2) , 10));
  tmp = _mm512_loadu_si512 (in + 3);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 31));
  w0 = _mm512_srli_epi32(tmp,1);
  _mm512_storeu_si512(compressed + 1, w1);
  tmp = _mm512_loadu_si512 (in + 4);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 20));
  w1 = _mm512_srli_epi32(tmp,12);
  _mm512_storeu_si512(compressed + 2, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 5) , 9));
  tmp = _mm512_loadu_si512 (in + 6);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 30));
  w0 = _mm512_srli_epi32(tmp,2);
  _mm512_storeu_si512(compressed + 3, w1);
  tmp = _mm512_loadu_si512 (in + 7);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 19));
  w1 = _mm512_srli_epi32(tmp,13);
  _mm512_storeu_si512(compressed + 4, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 8) , 8));
  tmp = _mm512_loadu_si512 (in + 9);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 29));
  w0 = _mm512_srli_epi32(tmp,3);
  _mm512_storeu_si512(compressed + 5, w1);
  tmp = _mm512_loadu_si512 (in + 10);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 18));
  w1 = _mm512_srli_epi32(tmp,14);
  _mm512_storeu_si512(compressed + 6, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 11) , 7));
  tmp = _mm512_loadu_si512 (in + 12);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 28));
  w0 = _mm512_srli_epi32(tmp,4);
  _mm512_storeu_si512(compressed + 7, w1);
  tmp = _mm512_loadu_si512 (in + 13);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 17));
  w1 = _mm512_srli_epi32(tmp,15);
  _mm512_storeu_si512(compressed + 8, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 14) , 6));
  tmp = _mm512_loadu_si512 (in + 15);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 27));
  w0 = _mm512_srli_epi32(tmp,5);
  _mm512_storeu_si512(compressed + 9, w1);
  tmp = _mm512_loadu_si512 (in + 16);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 16));
  w1 = _mm512_srli_epi32(tmp,16);
  _mm512_storeu_si512(compressed + 10, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 17) , 5));
  tmp = _mm512_loadu_si512 (in + 18);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 26));
  w0 = _mm512_srli_epi32(tmp,6);
  _mm512_storeu_si512(compressed + 11, w1);
  tmp = _mm512_loadu_si512 (in + 19);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 15));
  w1 = _mm512_srli_epi32(tmp,17);
  _mm512_storeu_si512(compressed + 12, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 20) , 4));
  tmp = _mm512_loadu_si512 (in + 21);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 25));
  w0 = _mm512_srli_epi32(tmp,7);
  _mm512_storeu_si512(compressed + 13, w1);
  tmp = _mm512_loadu_si512 (in + 22);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 14));
  w1 = _mm512_srli_epi32(tmp,18);
  _mm512_storeu_si512(compressed + 14, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 23) , 3));
  tmp = _mm512_loadu_si512 (in + 24);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 24));
  w0 = _mm512_srli_epi32(tmp,8);
  _mm512_storeu_si512(compressed + 15, w1);
  tmp = _mm512_loadu_si512 (in + 25);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 13));
  w1 = _mm512_srli_epi32(tmp,19);
  _mm512_storeu_si512(compressed + 16, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 26) , 2));
  tmp = _mm512_loadu_si512 (in + 27);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 23));
  w0 = _mm512_srli_epi32(tmp,9);
  _mm512_storeu_si512(compressed + 17, w1);
  tmp = _mm512_loadu_si512 (in + 28);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 12));
  w1 = _mm512_srli_epi32(tmp,20);
  _mm512_storeu_si512(compressed + 18, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 29) , 1));
  tmp = _mm512_loadu_si512 (in + 30);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 22));
  w0 = _mm512_srli_epi32(tmp,10);
  _mm512_storeu_si512(compressed + 19, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 31) , 11));
  _mm512_storeu_si512(compressed + 20, w0);
}


/* we are going to pack 512 22-bit values, touching 22 512-bit words, using 704 bytes */
static void avx512packblock22(const uint32_t * pin, __m512i * compressed) {
  const __m512i * in = (const __m512i *)  pin;
  /* we are going to touch  22 512-bit words */
  __m512i w0, w1;
  __m512i tmp; /* used to store inputs at word boundary */
  w0 = _mm512_loadu_si512 (in + 0);
  tmp = _mm512_loadu_si512 (in + 1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 22));
  w1 = _mm512_srli_epi32(tmp,10);
  _mm512_storeu_si512(compressed + 0, w0);
  tmp = _mm512_loadu_si512 (in + 2);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 12));
  w0 = _mm512_srli_epi32(tmp,20);
  _mm512_storeu_si512(compressed + 1, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 3) , 2));
  tmp = _mm512_loadu_si512 (in + 4);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 24));
  w1 = _mm512_srli_epi32(tmp,8);
  _mm512_storeu_si512(compressed + 2, w0);
  tmp = _mm512_loadu_si512 (in + 5);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 14));
  w0 = _mm512_srli_epi32(tmp,18);
  _mm512_storeu_si512(compressed + 3, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 6) , 4));
  tmp = _mm512_loadu_si512 (in + 7);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 26));
  w1 = _mm512_srli_epi32(tmp,6);
  _mm512_storeu_si512(compressed + 4, w0);
  tmp = _mm512_loadu_si512 (in + 8);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 16));
  w0 = _mm512_srli_epi32(tmp,16);
  _mm512_storeu_si512(compressed + 5, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 9) , 6));
  tmp = _mm512_loadu_si512 (in + 10);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 28));
  w1 = _mm512_srli_epi32(tmp,4);
  _mm512_storeu_si512(compressed + 6, w0);
  tmp = _mm512_loadu_si512 (in + 11);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 18));
  w0 = _mm512_srli_epi32(tmp,14);
  _mm512_storeu_si512(compressed + 7, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 12) , 8));
  tmp = _mm512_loadu_si512 (in + 13);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 30));
  w1 = _mm512_srli_epi32(tmp,2);
  _mm512_storeu_si512(compressed + 8, w0);
  tmp = _mm512_loadu_si512 (in + 14);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 20));
  w0 = _mm512_srli_epi32(tmp,12);
  _mm512_storeu_si512(compressed + 9, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 15) , 10));
  _mm512_storeu_si512(compressed + 10, w0);
  w1 = _mm512_loadu_si512 (in + 16);
  tmp = _mm512_loadu_si512 (in + 17);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 22));
  w0 = _mm512_srli_epi32(tmp,10);
  _mm512_storeu_si512(compressed + 11, w1);
  tmp = _mm512_loadu_si512 (in + 18);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 12));
  w1 = _mm512_srli_epi32(tmp,20);
  _mm512_storeu_si512(compressed + 12, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 19) , 2));
  tmp = _mm512_loadu_si512 (in + 20);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 24));
  w0 = _mm512_srli_epi32(tmp,8);
  _mm512_storeu_si512(compressed + 13, w1);
  tmp = _mm512_loadu_si512 (in + 21);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 14));
  w1 = _mm512_srli_epi32(tmp,18);
  _mm512_storeu_si512(compressed + 14, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 22) , 4));
  tmp = _mm512_loadu_si512 (in + 23);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 26));
  w0 = _mm512_srli_epi32(tmp,6);
  _mm512_storeu_si512(compressed + 15, w1);
  tmp = _mm512_loadu_si512 (in + 24);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 16));
  w1 = _mm512_srli_epi32(tmp,16);
  _mm512_storeu_si512(compressed + 16, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 25) , 6));
  tmp = _mm512_loadu_si512 (in + 26);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 28));
  w0 = _mm512_srli_epi32(tmp,4);
  _mm512_storeu_si512(compressed + 17, w1);
  tmp = _mm512_loadu_si512 (in + 27);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 18));
  w1 = _mm512_srli_epi32(tmp,14);
  _mm512_storeu_si512(compressed + 18, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 28) , 8));
  tmp = _mm512_loadu_si512 (in + 29);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 30));
  w0 = _mm512_srli_epi32(tmp,2);
  _mm512_storeu_si512(compressed + 19, w1);
  tmp = _mm512_loadu_si512 (in + 30);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 20));
  w1 = _mm512_srli_epi32(tmp,12);
  _mm512_storeu_si512(compressed + 20, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 31) , 10));
  _mm512_storeu_si512(compressed + 21, w1);
}


/* we are going to pack 512 23-bit values, touching 23 512-bit words, using 736 bytes */
static void avx512packblock23(const uint32_t * pin, __m512i * compressed) {
  const __m512i * in = (const __m512i *)  pin;
  /* we are going to touch  23 512-bit words */
  __m512i w0, w1;
  __m512i tmp; /* used to store inputs at word boundary */
  w0 = _mm512_loadu_si512 (in + 0);
  tmp = _mm512_loadu_si512 (in + 1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 23));
  w1 = _mm512_srli_epi32(tmp,9);
  _mm512_storeu_si512(compressed + 0, w0);
  tmp = _mm512_loadu_si512 (in + 2);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 14));
  w0 = _mm512_srli_epi32(tmp,18);
  _mm512_storeu_si512(compressed + 1, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 3) , 5));
  tmp = _mm512_loadu_si512 (in + 4);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 28));
  w1 = _mm512_srli_epi32(tmp,4);
  _mm512_storeu_si512(compressed + 2, w0);
  tmp = _mm512_loadu_si512 (in + 5);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 19));
  w0 = _mm512_srli_epi32(tmp,13);
  _mm512_storeu_si512(compressed + 3, w1);
  tmp = _mm512_loadu_si512 (in + 6);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 10));
  w1 = _mm512_srli_epi32(tmp,22);
  _mm512_storeu_si512(compressed + 4, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 7) , 1));
  tmp = _mm512_loadu_si512 (in + 8);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 24));
  w0 = _mm512_srli_epi32(tmp,8);
  _mm512_storeu_si512(compressed + 5, w1);
  tmp = _mm512_loadu_si512 (in + 9);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 15));
  w1 = _mm512_srli_epi32(tmp,17);
  _mm512_storeu_si512(compressed + 6, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 10) , 6));
  tmp = _mm512_loadu_si512 (in + 11);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 29));
  w0 = _mm512_srli_epi32(tmp,3);
  _mm512_storeu_si512(compressed + 7, w1);
  tmp = _mm512_loadu_si512 (in + 12);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 20));
  w1 = _mm512_srli_epi32(tmp,12);
  _mm512_storeu_si512(compressed + 8, w0);
  tmp = _mm512_loadu_si512 (in + 13);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 11));
  w0 = _mm512_srli_epi32(tmp,21);
  _mm512_storeu_si512(compressed + 9, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 14) , 2));
  tmp = _mm512_loadu_si512 (in + 15);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 25));
  w1 = _mm512_srli_epi32(tmp,7);
  _mm512_storeu_si512(compressed + 10, w0);
  tmp = _mm512_loadu_si512 (in + 16);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 16));
  w0 = _mm512_srli_epi32(tmp,16);
  _mm512_storeu_si512(compressed + 11, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 17) , 7));
  tmp = _mm512_loadu_si512 (in + 18);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 30));
  w1 = _mm512_srli_epi32(tmp,2);
  _mm512_storeu_si512(compressed + 12, w0);
  tmp = _mm512_loadu_si512 (in + 19);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 21));
  w0 = _mm512_srli_epi32(tmp,11);
  _mm512_storeu_si512(compressed + 13, w1);
  tmp = _mm512_loadu_si512 (in + 20);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 12));
  w1 = _mm512_srli_epi32(tmp,20);
  _mm512_storeu_si512(compressed + 14, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 21) , 3));
  tmp = _mm512_loadu_si512 (in + 22);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 26));
  w0 = _mm512_srli_epi32(tmp,6);
  _mm512_storeu_si512(compressed + 15, w1);
  tmp = _mm512_loadu_si512 (in + 23);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 17));
  w1 = _mm512_srli_epi32(tmp,15);
  _mm512_storeu_si512(compressed + 16, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 24) , 8));
  tmp = _mm512_loadu_si512 (in + 25);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 31));
  w0 = _mm512_srli_epi32(tmp,1);
  _mm512_storeu_si512(compressed + 17, w1);
  tmp = _mm512_loadu_si512 (in + 26);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 22));
  w1 = _mm512_srli_epi32(tmp,10);
  _mm512_storeu_si512(compressed + 18, w0);
  tmp = _mm512_loadu_si512 (in + 27);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 13));
  w0 = _mm512_srli_epi32(tmp,19);
  _mm512_storeu_si512(compressed + 19, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 28) , 4));
  tmp = _mm512_loadu_si512 (in + 29);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 27));
  w1 = _mm512_srli_epi32(tmp,5);
  _mm512_storeu_si512(compressed + 20, w0);
  tmp = _mm512_loadu_si512 (in + 30);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 18));
  w0 = _mm512_srli_epi32(tmp,14);
  _mm512_storeu_si512(compressed + 21, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 31) , 9));
  _mm512_storeu_si512(compressed + 22, w0);
}


/* we are going to pack 512 24-bit values, touching 24 512-bit words, using 768 bytes */
static void avx512packblock24(const uint32_t * pin, __m512i * compressed) {
  const __m512i * in = (const __m512i *)  pin;
  /* we are going to touch  24 512-bit words */
  __m512i w0, w1;
  __m512i tmp; /* used to store inputs at word boundary */
  w0 = _mm512_loadu_si512 (in + 0);
  tmp = _mm512_loadu_si512 (in + 1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 24));
  w1 = _mm512_srli_epi32(tmp,8);
  _mm512_storeu_si512(compressed + 0, w0);
  tmp = _mm512_loadu_si512 (in + 2);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 16));
  w0 = _mm512_srli_epi32(tmp,16);
  _mm512_storeu_si512(compressed + 1, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 3) , 8));
  _mm512_storeu_si512(compressed + 2, w0);
  w1 = _mm512_loadu_si512 (in + 4);
  tmp = _mm512_loadu_si512 (in + 5);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 24));
  w0 = _mm512_srli_epi32(tmp,8);
  _mm512_storeu_si512(compressed + 3, w1);
  tmp = _mm512_loadu_si512 (in + 6);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 16));
  w1 = _mm512_srli_epi32(tmp,16);
  _mm512_storeu_si512(compressed + 4, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 7) , 8));
  _mm512_storeu_si512(compressed + 5, w1);
  w0 = _mm512_loadu_si512 (in + 8);
  tmp = _mm512_loadu_si512 (in + 9);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 24));
  w1 = _mm512_srli_epi32(tmp,8);
  _mm512_storeu_si512(compressed + 6, w0);
  tmp = _mm512_loadu_si512 (in + 10);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 16));
  w0 = _mm512_srli_epi32(tmp,16);
  _mm512_storeu_si512(compressed + 7, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 11) , 8));
  _mm512_storeu_si512(compressed + 8, w0);
  w1 = _mm512_loadu_si512 (in + 12);
  tmp = _mm512_loadu_si512 (in + 13);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 24));
  w0 = _mm512_srli_epi32(tmp,8);
  _mm512_storeu_si512(compressed + 9, w1);
  tmp = _mm512_loadu_si512 (in + 14);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 16));
  w1 = _mm512_srli_epi32(tmp,16);
  _mm512_storeu_si512(compressed + 10, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 15) , 8));
  _mm512_storeu_si512(compressed + 11, w1);
  w0 = _mm512_loadu_si512 (in + 16);
  tmp = _mm512_loadu_si512 (in + 17);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 24));
  w1 = _mm512_srli_epi32(tmp,8);
  _mm512_storeu_si512(compressed + 12, w0);
  tmp = _mm512_loadu_si512 (in + 18);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 16));
  w0 = _mm512_srli_epi32(tmp,16);
  _mm512_storeu_si512(compressed + 13, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 19) , 8));
  _mm512_storeu_si512(compressed + 14, w0);
  w1 = _mm512_loadu_si512 (in + 20);
  tmp = _mm512_loadu_si512 (in + 21);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 24));
  w0 = _mm512_srli_epi32(tmp,8);
  _mm512_storeu_si512(compressed + 15, w1);
  tmp = _mm512_loadu_si512 (in + 22);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 16));
  w1 = _mm512_srli_epi32(tmp,16);
  _mm512_storeu_si512(compressed + 16, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 23) , 8));
  _mm512_storeu_si512(compressed + 17, w1);
  w0 = _mm512_loadu_si512 (in + 24);
  tmp = _mm512_loadu_si512 (in + 25);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 24));
  w1 = _mm512_srli_epi32(tmp,8);
  _mm512_storeu_si512(compressed + 18, w0);
  tmp = _mm512_loadu_si512 (in + 26);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 16));
  w0 = _mm512_srli_epi32(tmp,16);
  _mm512_storeu_si512(compressed + 19, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 27) , 8));
  _mm512_storeu_si512(compressed + 20, w0);
  w1 = _mm512_loadu_si512 (in + 28);
  tmp = _mm512_loadu_si512 (in + 29);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 24));
  w0 = _mm512_srli_epi32(tmp,8);
  _mm512_storeu_si512(compressed + 21, w1);
  tmp = _mm512_loadu_si512 (in + 30);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 16));
  w1 = _mm512_srli_epi32(tmp,16);
  _mm512_storeu_si512(compressed + 22, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 31) , 8));
  _mm512_storeu_si512(compressed + 23, w1);
}


/* we are going to pack 512 25-bit values, touching 25 512-bit words, using 800 bytes */
static void avx512packblock25(const uint32_t * pin, __m512i * compressed) {
  const __m512i * in = (const __m512i *)  pin;
  /* we are going to touch  25 512-bit words */
  __m512i w0, w1;
  __m512i tmp; /* used to store inputs at word boundary */
  w0 = _mm512_loadu_si512 (in + 0);
  tmp = _mm512_loadu_si512 (in + 1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 25));
  w1 = _mm512_srli_epi32(tmp,7);
  _mm512_storeu_si512(compressed + 0, w0);
  tmp = _mm512_loadu_si512 (in + 2);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 18));
  w0 = _mm512_srli_epi32(tmp,14);
  _mm512_storeu_si512(compressed + 1, w1);
  tmp = _mm512_loadu_si512 (in + 3);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 11));
  w1 = _mm512_srli_epi32(tmp,21);
  _mm512_storeu_si512(compressed + 2, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 4) , 4));
  tmp = _mm512_loadu_si512 (in + 5);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 29));
  w0 = _mm512_srli_epi32(tmp,3);
  _mm512_storeu_si512(compressed + 3, w1);
  tmp = _mm512_loadu_si512 (in + 6);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 22));
  w1 = _mm512_srli_epi32(tmp,10);
  _mm512_storeu_si512(compressed + 4, w0);
  tmp = _mm512_loadu_si512 (in + 7);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 15));
  w0 = _mm512_srli_epi32(tmp,17);
  _mm512_storeu_si512(compressed + 5, w1);
  tmp = _mm512_loadu_si512 (in + 8);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 8));
  w1 = _mm512_srli_epi32(tmp,24);
  _mm512_storeu_si512(compressed + 6, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 9) , 1));
  tmp = _mm512_loadu_si512 (in + 10);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 26));
  w0 = _mm512_srli_epi32(tmp,6);
  _mm512_storeu_si512(compressed + 7, w1);
  tmp = _mm512_loadu_si512 (in + 11);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 19));
  w1 = _mm512_srli_epi32(tmp,13);
  _mm512_storeu_si512(compressed + 8, w0);
  tmp = _mm512_loadu_si512 (in + 12);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 12));
  w0 = _mm512_srli_epi32(tmp,20);
  _mm512_storeu_si512(compressed + 9, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 13) , 5));
  tmp = _mm512_loadu_si512 (in + 14);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 30));
  w1 = _mm512_srli_epi32(tmp,2);
  _mm512_storeu_si512(compressed + 10, w0);
  tmp = _mm512_loadu_si512 (in + 15);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 23));
  w0 = _mm512_srli_epi32(tmp,9);
  _mm512_storeu_si512(compressed + 11, w1);
  tmp = _mm512_loadu_si512 (in + 16);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 16));
  w1 = _mm512_srli_epi32(tmp,16);
  _mm512_storeu_si512(compressed + 12, w0);
  tmp = _mm512_loadu_si512 (in + 17);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 9));
  w0 = _mm512_srli_epi32(tmp,23);
  _mm512_storeu_si512(compressed + 13, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 18) , 2));
  tmp = _mm512_loadu_si512 (in + 19);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 27));
  w1 = _mm512_srli_epi32(tmp,5);
  _mm512_storeu_si512(compressed + 14, w0);
  tmp = _mm512_loadu_si512 (in + 20);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 20));
  w0 = _mm512_srli_epi32(tmp,12);
  _mm512_storeu_si512(compressed + 15, w1);
  tmp = _mm512_loadu_si512 (in + 21);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 13));
  w1 = _mm512_srli_epi32(tmp,19);
  _mm512_storeu_si512(compressed + 16, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 22) , 6));
  tmp = _mm512_loadu_si512 (in + 23);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 31));
  w0 = _mm512_srli_epi32(tmp,1);
  _mm512_storeu_si512(compressed + 17, w1);
  tmp = _mm512_loadu_si512 (in + 24);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 24));
  w1 = _mm512_srli_epi32(tmp,8);
  _mm512_storeu_si512(compressed + 18, w0);
  tmp = _mm512_loadu_si512 (in + 25);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 17));
  w0 = _mm512_srli_epi32(tmp,15);
  _mm512_storeu_si512(compressed + 19, w1);
  tmp = _mm512_loadu_si512 (in + 26);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 10));
  w1 = _mm512_srli_epi32(tmp,22);
  _mm512_storeu_si512(compressed + 20, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 27) , 3));
  tmp = _mm512_loadu_si512 (in + 28);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 28));
  w0 = _mm512_srli_epi32(tmp,4);
  _mm512_storeu_si512(compressed + 21, w1);
  tmp = _mm512_loadu_si512 (in + 29);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 21));
  w1 = _mm512_srli_epi32(tmp,11);
  _mm512_storeu_si512(compressed + 22, w0);
  tmp = _mm512_loadu_si512 (in + 30);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 14));
  w0 = _mm512_srli_epi32(tmp,18);
  _mm512_storeu_si512(compressed + 23, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 31) , 7));
  _mm512_storeu_si512(compressed + 24, w0);
}


/* we are going to pack 512 26-bit values, touching 26 512-bit words, using 832 bytes */
static void avx512packblock26(const uint32_t * pin, __m512i * compressed) {
  const __m512i * in = (const __m512i *)  pin;
  /* we are going to touch  26 512-bit words */
  __m512i w0, w1;
  __m512i tmp; /* used to store inputs at word boundary */
  w0 = _mm512_loadu_si512 (in + 0);
  tmp = _mm512_loadu_si512 (in + 1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 26));
  w1 = _mm512_srli_epi32(tmp,6);
  _mm512_storeu_si512(compressed + 0, w0);
  tmp = _mm512_loadu_si512 (in + 2);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 20));
  w0 = _mm512_srli_epi32(tmp,12);
  _mm512_storeu_si512(compressed + 1, w1);
  tmp = _mm512_loadu_si512 (in + 3);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 14));
  w1 = _mm512_srli_epi32(tmp,18);
  _mm512_storeu_si512(compressed + 2, w0);
  tmp = _mm512_loadu_si512 (in + 4);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 8));
  w0 = _mm512_srli_epi32(tmp,24);
  _mm512_storeu_si512(compressed + 3, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 5) , 2));
  tmp = _mm512_loadu_si512 (in + 6);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 28));
  w1 = _mm512_srli_epi32(tmp,4);
  _mm512_storeu_si512(compressed + 4, w0);
  tmp = _mm512_loadu_si512 (in + 7);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 22));
  w0 = _mm512_srli_epi32(tmp,10);
  _mm512_storeu_si512(compressed + 5, w1);
  tmp = _mm512_loadu_si512 (in + 8);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 16));
  w1 = _mm512_srli_epi32(tmp,16);
  _mm512_storeu_si512(compressed + 6, w0);
  tmp = _mm512_loadu_si512 (in + 9);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 10));
  w0 = _mm512_srli_epi32(tmp,22);
  _mm512_storeu_si512(compressed + 7, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 10) , 4));
  tmp = _mm512_loadu_si512 (in + 11);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 30));
  w1 = _mm512_srli_epi32(tmp,2);
  _mm512_storeu_si512(compressed + 8, w0);
  tmp = _mm512_loadu_si512 (in + 12);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 24));
  w0 = _mm512_srli_epi32(tmp,8);
  _mm512_storeu_si512(compressed + 9, w1);
  tmp = _mm512_loadu_si512 (in + 13);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 18));
  w1 = _mm512_srli_epi32(tmp,14);
  _mm512_storeu_si512(compressed + 10, w0);
  tmp = _mm512_loadu_si512 (in + 14);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 12));
  w0 = _mm512_srli_epi32(tmp,20);
  _mm512_storeu_si512(compressed + 11, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 15) , 6));
  _mm512_storeu_si512(compressed + 12, w0);
  w1 = _mm512_loadu_si512 (in + 16);
  tmp = _mm512_loadu_si512 (in + 17);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 26));
  w0 = _mm512_srli_epi32(tmp,6);
  _mm512_storeu_si512(compressed + 13, w1);
  tmp = _mm512_loadu_si512 (in + 18);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 20));
  w1 = _mm512_srli_epi32(tmp,12);
  _mm512_storeu_si512(compressed + 14, w0);
  tmp = _mm512_loadu_si512 (in + 19);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 14));
  w0 = _mm512_srli_epi32(tmp,18);
  _mm512_storeu_si512(compressed + 15, w1);
  tmp = _mm512_loadu_si512 (in + 20);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 8));
  w1 = _mm512_srli_epi32(tmp,24);
  _mm512_storeu_si512(compressed + 16, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 21) , 2));
  tmp = _mm512_loadu_si512 (in + 22);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 28));
  w0 = _mm512_srli_epi32(tmp,4);
  _mm512_storeu_si512(compressed + 17, w1);
  tmp = _mm512_loadu_si512 (in + 23);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 22));
  w1 = _mm512_srli_epi32(tmp,10);
  _mm512_storeu_si512(compressed + 18, w0);
  tmp = _mm512_loadu_si512 (in + 24);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 16));
  w0 = _mm512_srli_epi32(tmp,16);
  _mm512_storeu_si512(compressed + 19, w1);
  tmp = _mm512_loadu_si512 (in + 25);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 10));
  w1 = _mm512_srli_epi32(tmp,22);
  _mm512_storeu_si512(compressed + 20, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 26) , 4));
  tmp = _mm512_loadu_si512 (in + 27);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 30));
  w0 = _mm512_srli_epi32(tmp,2);
  _mm512_storeu_si512(compressed + 21, w1);
  tmp = _mm512_loadu_si512 (in + 28);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 24));
  w1 = _mm512_srli_epi32(tmp,8);
  _mm512_storeu_si512(compressed + 22, w0);
  tmp = _mm512_loadu_si512 (in + 29);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 18));
  w0 = _mm512_srli_epi32(tmp,14);
  _mm512_storeu_si512(compressed + 23, w1);
  tmp = _mm512_loadu_si512 (in + 30);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 12));
  w1 = _mm512_srli_epi32(tmp,20);
  _mm512_storeu_si512(compressed + 24, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 31) , 6));
  _mm512_storeu_si512(compressed + 25, w1);
}


/* we are going to pack 512 27-bit values, touching 27 512-bit words, using 864 bytes */
static void avx512packblock27(const uint32_t * pin, __m512i * compressed) {
  const __m512i * in = (const __m512i *)  pin;
  /* we are going to touch  27 512-bit words */
  __m512i w0, w1;
  __m512i tmp; /* used to store inputs at word boundary */
  w0 = _mm512_loadu_si512 (in + 0);
  tmp = _mm512_loadu_si512 (in + 1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 27));
  w1 = _mm512_srli_epi32(tmp,5);
  _mm512_storeu_si512(compressed + 0, w0);
  tmp = _mm512_loadu_si512 (in + 2);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 22));
  w0 = _mm512_srli_epi32(tmp,10);
  _mm512_storeu_si512(compressed + 1, w1);
  tmp = _mm512_loadu_si512 (in + 3);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 17));
  w1 = _mm512_srli_epi32(tmp,15);
  _mm512_storeu_si512(compressed + 2, w0);
  tmp = _mm512_loadu_si512 (in + 4);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 12));
  w0 = _mm512_srli_epi32(tmp,20);
  _mm512_storeu_si512(compressed + 3, w1);
  tmp = _mm512_loadu_si512 (in + 5);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 7));
  w1 = _mm512_srli_epi32(tmp,25);
  _mm512_storeu_si512(compressed + 4, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 6) , 2));
  tmp = _mm512_loadu_si512 (in + 7);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 29));
  w0 = _mm512_srli_epi32(tmp,3);
  _mm512_storeu_si512(compressed + 5, w1);
  tmp = _mm512_loadu_si512 (in + 8);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 24));
  w1 = _mm512_srli_epi32(tmp,8);
  _mm512_storeu_si512(compressed + 6, w0);
  tmp = _mm512_loadu_si512 (in + 9);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 19));
  w0 = _mm512_srli_epi32(tmp,13);
  _mm512_storeu_si512(compressed + 7, w1);
  tmp = _mm512_loadu_si512 (in + 10);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 14));
  w1 = _mm512_srli_epi32(tmp,18);
  _mm512_storeu_si512(compressed + 8, w0);
  tmp = _mm512_loadu_si512 (in + 11);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 9));
  w0 = _mm512_srli_epi32(tmp,23);
  _mm512_storeu_si512(compressed + 9, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 12) , 4));
  tmp = _mm512_loadu_si512 (in + 13);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 31));
  w1 = _mm512_srli_epi32(tmp,1);
  _mm512_storeu_si512(compressed + 10, w0);
  tmp = _mm512_loadu_si512 (in + 14);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 26));
  w0 = _mm512_srli_epi32(tmp,6);
  _mm512_storeu_si512(compressed + 11, w1);
  tmp = _mm512_loadu_si512 (in + 15);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 21));
  w1 = _mm512_srli_epi32(tmp,11);
  _mm512_storeu_si512(compressed + 12, w0);
  tmp = _mm512_loadu_si512 (in + 16);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 16));
  w0 = _mm512_srli_epi32(tmp,16);
  _mm512_storeu_si512(compressed + 13, w1);
  tmp = _mm512_loadu_si512 (in + 17);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 11));
  w1 = _mm512_srli_epi32(tmp,21);
  _mm512_storeu_si512(compressed + 14, w0);
  tmp = _mm512_loadu_si512 (in + 18);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 6));
  w0 = _mm512_srli_epi32(tmp,26);
  _mm512_storeu_si512(compressed + 15, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 19) , 1));
  tmp = _mm512_loadu_si512 (in + 20);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 28));
  w1 = _mm512_srli_epi32(tmp,4);
  _mm512_storeu_si512(compressed + 16, w0);
  tmp = _mm512_loadu_si512 (in + 21);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 23));
  w0 = _mm512_srli_epi32(tmp,9);
  _mm512_storeu_si512(compressed + 17, w1);
  tmp = _mm512_loadu_si512 (in + 22);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 18));
  w1 = _mm512_srli_epi32(tmp,14);
  _mm512_storeu_si512(compressed + 18, w0);
  tmp = _mm512_loadu_si512 (in + 23);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 13));
  w0 = _mm512_srli_epi32(tmp,19);
  _mm512_storeu_si512(compressed + 19, w1);
  tmp = _mm512_loadu_si512 (in + 24);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 8));
  w1 = _mm512_srli_epi32(tmp,24);
  _mm512_storeu_si512(compressed + 20, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 25) , 3));
  tmp = _mm512_loadu_si512 (in + 26);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 30));
  w0 = _mm512_srli_epi32(tmp,2);
  _mm512_storeu_si512(compressed + 21, w1);
  tmp = _mm512_loadu_si512 (in + 27);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 25));
  w1 = _mm512_srli_epi32(tmp,7);
  _mm512_storeu_si512(compressed + 22, w0);
  tmp = _mm512_loadu_si512 (in + 28);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 20));
  w0 = _mm512_srli_epi32(tmp,12);
  _mm512_storeu_si512(compressed + 23, w1);
  tmp = _mm512_loadu_si512 (in + 29);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 15));
  w1 = _mm512_srli_epi32(tmp,17);
  _mm512_storeu_si512(compressed + 24, w0);
  tmp = _mm512_loadu_si512 (in + 30);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 10));
  w0 = _mm512_srli_epi32(tmp,22);
  _mm512_storeu_si512(compressed + 25, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 31) , 5));
  _mm512_storeu_si512(compressed + 26, w0);
}


/* we are going to pack 512 28-bit values, touching 28 512-bit words, using 896 bytes */
static void avx512packblock28(const uint32_t * pin, __m512i * compressed) {
  const __m512i * in = (const __m512i *)  pin;
  /* we are going to touch  28 512-bit words */
  __m512i w0, w1;
  __m512i tmp; /* used to store inputs at word boundary */
  w0 = _mm512_loadu_si512 (in + 0);
  tmp = _mm512_loadu_si512 (in + 1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 28));
  w1 = _mm512_srli_epi32(tmp,4);
  _mm512_storeu_si512(compressed + 0, w0);
  tmp = _mm512_loadu_si512 (in + 2);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 24));
  w0 = _mm512_srli_epi32(tmp,8);
  _mm512_storeu_si512(compressed + 1, w1);
  tmp = _mm512_loadu_si512 (in + 3);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 20));
  w1 = _mm512_srli_epi32(tmp,12);
  _mm512_storeu_si512(compressed + 2, w0);
  tmp = _mm512_loadu_si512 (in + 4);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 16));
  w0 = _mm512_srli_epi32(tmp,16);
  _mm512_storeu_si512(compressed + 3, w1);
  tmp = _mm512_loadu_si512 (in + 5);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 12));
  w1 = _mm512_srli_epi32(tmp,20);
  _mm512_storeu_si512(compressed + 4, w0);
  tmp = _mm512_loadu_si512 (in + 6);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 8));
  w0 = _mm512_srli_epi32(tmp,24);
  _mm512_storeu_si512(compressed + 5, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 7) , 4));
  _mm512_storeu_si512(compressed + 6, w0);
  w1 = _mm512_loadu_si512 (in + 8);
  tmp = _mm512_loadu_si512 (in + 9);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 28));
  w0 = _mm512_srli_epi32(tmp,4);
  _mm512_storeu_si512(compressed + 7, w1);
  tmp = _mm512_loadu_si512 (in + 10);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 24));
  w1 = _mm512_srli_epi32(tmp,8);
  _mm512_storeu_si512(compressed + 8, w0);
  tmp = _mm512_loadu_si512 (in + 11);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 20));
  w0 = _mm512_srli_epi32(tmp,12);
  _mm512_storeu_si512(compressed + 9, w1);
  tmp = _mm512_loadu_si512 (in + 12);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 16));
  w1 = _mm512_srli_epi32(tmp,16);
  _mm512_storeu_si512(compressed + 10, w0);
  tmp = _mm512_loadu_si512 (in + 13);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 12));
  w0 = _mm512_srli_epi32(tmp,20);
  _mm512_storeu_si512(compressed + 11, w1);
  tmp = _mm512_loadu_si512 (in + 14);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 8));
  w1 = _mm512_srli_epi32(tmp,24);
  _mm512_storeu_si512(compressed + 12, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 15) , 4));
  _mm512_storeu_si512(compressed + 13, w1);
  w0 = _mm512_loadu_si512 (in + 16);
  tmp = _mm512_loadu_si512 (in + 17);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 28));
  w1 = _mm512_srli_epi32(tmp,4);
  _mm512_storeu_si512(compressed + 14, w0);
  tmp = _mm512_loadu_si512 (in + 18);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 24));
  w0 = _mm512_srli_epi32(tmp,8);
  _mm512_storeu_si512(compressed + 15, w1);
  tmp = _mm512_loadu_si512 (in + 19);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 20));
  w1 = _mm512_srli_epi32(tmp,12);
  _mm512_storeu_si512(compressed + 16, w0);
  tmp = _mm512_loadu_si512 (in + 20);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 16));
  w0 = _mm512_srli_epi32(tmp,16);
  _mm512_storeu_si512(compressed + 17, w1);
  tmp = _mm512_loadu_si512 (in + 21);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 12));
  w1 = _mm512_srli_epi32(tmp,20);
  _mm512_storeu_si512(compressed + 18, w0);
  tmp = _mm512_loadu_si512 (in + 22);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 8));
  w0 = _mm512_srli_epi32(tmp,24);
  _mm512_storeu_si512(compressed + 19, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 23) , 4));
  _mm512_storeu_si512(compressed + 20, w0);
  w1 = _mm512_loadu_si512 (in + 24);
  tmp = _mm512_loadu_si512 (in + 25);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 28));
  w0 = _mm512_srli_epi32(tmp,4);
  _mm512_storeu_si512(compressed + 21, w1);
  tmp = _mm512_loadu_si512 (in + 26);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 24));
  w1 = _mm512_srli_epi32(tmp,8);
  _mm512_storeu_si512(compressed + 22, w0);
  tmp = _mm512_loadu_si512 (in + 27);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 20));
  w0 = _mm512_srli_epi32(tmp,12);
  _mm512_storeu_si512(compressed + 23, w1);
  tmp = _mm512_loadu_si512 (in + 28);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 16));
  w1 = _mm512_srli_epi32(tmp,16);
  _mm512_storeu_si512(compressed + 24, w0);
  tmp = _mm512_loadu_si512 (in + 29);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 12));
  w0 = _mm512_srli_epi32(tmp,20);
  _mm512_storeu_si512(compressed + 25, w1);
  tmp = _mm512_loadu_si512 (in + 30);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 8));
  w1 = _mm512_srli_epi32(tmp,24);
  _mm512_storeu_si512(compressed + 26, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 31) , 4));
  _mm512_storeu_si512(compressed + 27, w1);
}


/* we are going to pack 512 29-bit values, touching 29 512-bit words, using 928 bytes */
static void avx512packblock29(const uint32_t * pin, __m512i * compressed) {
  const __m512i * in = (const __m512i *)  pin;
  /* we are going to touch  29 512-bit words */
  __m512i w0, w1;
  __m512i tmp; /* used to store inputs at word boundary */
  w0 = _mm512_loadu_si512 (in + 0);
  tmp = _mm512_loadu_si512 (in + 1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 29));
  w1 = _mm512_srli_epi32(tmp,3);
  _mm512_storeu_si512(compressed + 0, w0);
  tmp = _mm512_loadu_si512 (in + 2);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 26));
  w0 = _mm512_srli_epi32(tmp,6);
  _mm512_storeu_si512(compressed + 1, w1);
  tmp = _mm512_loadu_si512 (in + 3);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 23));
  w1 = _mm512_srli_epi32(tmp,9);
  _mm512_storeu_si512(compressed + 2, w0);
  tmp = _mm512_loadu_si512 (in + 4);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 20));
  w0 = _mm512_srli_epi32(tmp,12);
  _mm512_storeu_si512(compressed + 3, w1);
  tmp = _mm512_loadu_si512 (in + 5);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 17));
  w1 = _mm512_srli_epi32(tmp,15);
  _mm512_storeu_si512(compressed + 4, w0);
  tmp = _mm512_loadu_si512 (in + 6);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 14));
  w0 = _mm512_srli_epi32(tmp,18);
  _mm512_storeu_si512(compressed + 5, w1);
  tmp = _mm512_loadu_si512 (in + 7);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 11));
  w1 = _mm512_srli_epi32(tmp,21);
  _mm512_storeu_si512(compressed + 6, w0);
  tmp = _mm512_loadu_si512 (in + 8);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 8));
  w0 = _mm512_srli_epi32(tmp,24);
  _mm512_storeu_si512(compressed + 7, w1);
  tmp = _mm512_loadu_si512 (in + 9);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 5));
  w1 = _mm512_srli_epi32(tmp,27);
  _mm512_storeu_si512(compressed + 8, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 10) , 2));
  tmp = _mm512_loadu_si512 (in + 11);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 31));
  w0 = _mm512_srli_epi32(tmp,1);
  _mm512_storeu_si512(compressed + 9, w1);
  tmp = _mm512_loadu_si512 (in + 12);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 28));
  w1 = _mm512_srli_epi32(tmp,4);
  _mm512_storeu_si512(compressed + 10, w0);
  tmp = _mm512_loadu_si512 (in + 13);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 25));
  w0 = _mm512_srli_epi32(tmp,7);
  _mm512_storeu_si512(compressed + 11, w1);
  tmp = _mm512_loadu_si512 (in + 14);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 22));
  w1 = _mm512_srli_epi32(tmp,10);
  _mm512_storeu_si512(compressed + 12, w0);
  tmp = _mm512_loadu_si512 (in + 15);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 19));
  w0 = _mm512_srli_epi32(tmp,13);
  _mm512_storeu_si512(compressed + 13, w1);
  tmp = _mm512_loadu_si512 (in + 16);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 16));
  w1 = _mm512_srli_epi32(tmp,16);
  _mm512_storeu_si512(compressed + 14, w0);
  tmp = _mm512_loadu_si512 (in + 17);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 13));
  w0 = _mm512_srli_epi32(tmp,19);
  _mm512_storeu_si512(compressed + 15, w1);
  tmp = _mm512_loadu_si512 (in + 18);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 10));
  w1 = _mm512_srli_epi32(tmp,22);
  _mm512_storeu_si512(compressed + 16, w0);
  tmp = _mm512_loadu_si512 (in + 19);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 7));
  w0 = _mm512_srli_epi32(tmp,25);
  _mm512_storeu_si512(compressed + 17, w1);
  tmp = _mm512_loadu_si512 (in + 20);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 4));
  w1 = _mm512_srli_epi32(tmp,28);
  _mm512_storeu_si512(compressed + 18, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 21) , 1));
  tmp = _mm512_loadu_si512 (in + 22);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 30));
  w0 = _mm512_srli_epi32(tmp,2);
  _mm512_storeu_si512(compressed + 19, w1);
  tmp = _mm512_loadu_si512 (in + 23);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 27));
  w1 = _mm512_srli_epi32(tmp,5);
  _mm512_storeu_si512(compressed + 20, w0);
  tmp = _mm512_loadu_si512 (in + 24);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 24));
  w0 = _mm512_srli_epi32(tmp,8);
  _mm512_storeu_si512(compressed + 21, w1);
  tmp = _mm512_loadu_si512 (in + 25);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 21));
  w1 = _mm512_srli_epi32(tmp,11);
  _mm512_storeu_si512(compressed + 22, w0);
  tmp = _mm512_loadu_si512 (in + 26);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 18));
  w0 = _mm512_srli_epi32(tmp,14);
  _mm512_storeu_si512(compressed + 23, w1);
  tmp = _mm512_loadu_si512 (in + 27);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 15));
  w1 = _mm512_srli_epi32(tmp,17);
  _mm512_storeu_si512(compressed + 24, w0);
  tmp = _mm512_loadu_si512 (in + 28);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 12));
  w0 = _mm512_srli_epi32(tmp,20);
  _mm512_storeu_si512(compressed + 25, w1);
  tmp = _mm512_loadu_si512 (in + 29);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 9));
  w1 = _mm512_srli_epi32(tmp,23);
  _mm512_storeu_si512(compressed + 26, w0);
  tmp = _mm512_loadu_si512 (in + 30);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 6));
  w0 = _mm512_srli_epi32(tmp,26);
  _mm512_storeu_si512(compressed + 27, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 31) , 3));
  _mm512_storeu_si512(compressed + 28, w0);
}


/* we are going to pack 512 30-bit values, touching 30 512-bit words, using 960 bytes */
static void avx512packblock30(const uint32_t * pin, __m512i * compressed) {
  const __m512i * in = (const __m512i *)  pin;
  /* we are going to touch  30 512-bit words */
  __m512i w0, w1;
  __m512i tmp; /* used to store inputs at word boundary */
  w0 = _mm512_loadu_si512 (in + 0);
  tmp = _mm512_loadu_si512 (in + 1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 30));
  w1 = _mm512_srli_epi32(tmp,2);
  _mm512_storeu_si512(compressed + 0, w0);
  tmp = _mm512_loadu_si512 (in + 2);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 28));
  w0 = _mm512_srli_epi32(tmp,4);
  _mm512_storeu_si512(compressed + 1, w1);
  tmp = _mm512_loadu_si512 (in + 3);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 26));
  w1 = _mm512_srli_epi32(tmp,6);
  _mm512_storeu_si512(compressed + 2, w0);
  tmp = _mm512_loadu_si512 (in + 4);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 24));
  w0 = _mm512_srli_epi32(tmp,8);
  _mm512_storeu_si512(compressed + 3, w1);
  tmp = _mm512_loadu_si512 (in + 5);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 22));
  w1 = _mm512_srli_epi32(tmp,10);
  _mm512_storeu_si512(compressed + 4, w0);
  tmp = _mm512_loadu_si512 (in + 6);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 20));
  w0 = _mm512_srli_epi32(tmp,12);
  _mm512_storeu_si512(compressed + 5, w1);
  tmp = _mm512_loadu_si512 (in + 7);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 18));
  w1 = _mm512_srli_epi32(tmp,14);
  _mm512_storeu_si512(compressed + 6, w0);
  tmp = _mm512_loadu_si512 (in + 8);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 16));
  w0 = _mm512_srli_epi32(tmp,16);
  _mm512_storeu_si512(compressed + 7, w1);
  tmp = _mm512_loadu_si512 (in + 9);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 14));
  w1 = _mm512_srli_epi32(tmp,18);
  _mm512_storeu_si512(compressed + 8, w0);
  tmp = _mm512_loadu_si512 (in + 10);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 12));
  w0 = _mm512_srli_epi32(tmp,20);
  _mm512_storeu_si512(compressed + 9, w1);
  tmp = _mm512_loadu_si512 (in + 11);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 10));
  w1 = _mm512_srli_epi32(tmp,22);
  _mm512_storeu_si512(compressed + 10, w0);
  tmp = _mm512_loadu_si512 (in + 12);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 8));
  w0 = _mm512_srli_epi32(tmp,24);
  _mm512_storeu_si512(compressed + 11, w1);
  tmp = _mm512_loadu_si512 (in + 13);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 6));
  w1 = _mm512_srli_epi32(tmp,26);
  _mm512_storeu_si512(compressed + 12, w0);
  tmp = _mm512_loadu_si512 (in + 14);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 4));
  w0 = _mm512_srli_epi32(tmp,28);
  _mm512_storeu_si512(compressed + 13, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 15) , 2));
  _mm512_storeu_si512(compressed + 14, w0);
  w1 = _mm512_loadu_si512 (in + 16);
  tmp = _mm512_loadu_si512 (in + 17);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 30));
  w0 = _mm512_srli_epi32(tmp,2);
  _mm512_storeu_si512(compressed + 15, w1);
  tmp = _mm512_loadu_si512 (in + 18);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 28));
  w1 = _mm512_srli_epi32(tmp,4);
  _mm512_storeu_si512(compressed + 16, w0);
  tmp = _mm512_loadu_si512 (in + 19);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 26));
  w0 = _mm512_srli_epi32(tmp,6);
  _mm512_storeu_si512(compressed + 17, w1);
  tmp = _mm512_loadu_si512 (in + 20);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 24));
  w1 = _mm512_srli_epi32(tmp,8);
  _mm512_storeu_si512(compressed + 18, w0);
  tmp = _mm512_loadu_si512 (in + 21);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 22));
  w0 = _mm512_srli_epi32(tmp,10);
  _mm512_storeu_si512(compressed + 19, w1);
  tmp = _mm512_loadu_si512 (in + 22);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 20));
  w1 = _mm512_srli_epi32(tmp,12);
  _mm512_storeu_si512(compressed + 20, w0);
  tmp = _mm512_loadu_si512 (in + 23);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 18));
  w0 = _mm512_srli_epi32(tmp,14);
  _mm512_storeu_si512(compressed + 21, w1);
  tmp = _mm512_loadu_si512 (in + 24);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 16));
  w1 = _mm512_srli_epi32(tmp,16);
  _mm512_storeu_si512(compressed + 22, w0);
  tmp = _mm512_loadu_si512 (in + 25);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 14));
  w0 = _mm512_srli_epi32(tmp,18);
  _mm512_storeu_si512(compressed + 23, w1);
  tmp = _mm512_loadu_si512 (in + 26);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 12));
  w1 = _mm512_srli_epi32(tmp,20);
  _mm512_storeu_si512(compressed + 24, w0);
  tmp = _mm512_loadu_si512 (in + 27);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 10));
  w0 = _mm512_srli_epi32(tmp,22);
  _mm512_storeu_si512(compressed + 25, w1);
  tmp = _mm512_loadu_si512 (in + 28);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 8));
  w1 = _mm512_srli_epi32(tmp,24);
  _mm512_storeu_si512(compressed + 26, w0);
  tmp = _mm512_loadu_si512 (in + 29);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 6));
  w0 = _mm512_srli_epi32(tmp,26);
  _mm512_storeu_si512(compressed + 27, w1);
  tmp = _mm512_loadu_si512 (in + 30);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 4));
  w1 = _mm512_srli_epi32(tmp,28);
  _mm512_storeu_si512(compressed + 28, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(_mm512_loadu_si512 (in + 31) , 2));
  _mm512_storeu_si512(compressed + 29, w1);
}


/* we are going to pack 512 31-bit values, touching 31 512-bit words, using 992 bytes */
static void avx512packblock31(const uint32_t * pin, __m512i * compressed) {
  const __m512i * in = (const __m512i *)  pin;
  /* we are going to touch  31 512-bit words */
  __m512i w0, w1;
  __m512i tmp; /* used to store inputs at word boundary */
  w0 = _mm512_loadu_si512 (in + 0);
  tmp = _mm512_loadu_si512 (in + 1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 31));
  w1 = _mm512_srli_epi32(tmp,1);
  _mm512_storeu_si512(compressed + 0, w0);
  tmp = _mm512_loadu_si512 (in + 2);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 30));
  w0 = _mm512_srli_epi32(tmp,2);
  _mm512_storeu_si512(compressed + 1, w1);
  tmp = _mm512_loadu_si512 (in + 3);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 29));
  w1 = _mm512_srli_epi32(tmp,3);
  _mm512_storeu_si512(compressed + 2, w0);
  tmp = _mm512_loadu_si512 (in + 4);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 28));
  w0 = _mm512_srli_epi32(tmp,4);
  _mm512_storeu_si512(compressed + 3, w1);
  tmp = _mm512_loadu_si512 (in + 5);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 27));
  w1 = _mm512_srli_epi32(tmp,5);
  _mm512_storeu_si512(compressed + 4, w0);
  tmp = _mm512_loadu_si512 (in + 6);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 26));
  w0 = _mm512_srli_epi32(tmp,6);
  _mm512_storeu_si512(compressed + 5, w1);
  tmp = _mm512_loadu_si512 (in + 7);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 25));
  w1 = _mm512_srli_epi32(tmp,7);
  _mm512_storeu_si512(compressed + 6, w0);
  tmp = _mm512_loadu_si512 (in + 8);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 24));
  w0 = _mm512_srli_epi32(tmp,8);
  _mm512_storeu_si512(compressed + 7, w1);
  tmp = _mm512_loadu_si512 (in + 9);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 23));
  w1 = _mm512_srli_epi32(tmp,9);
  _mm512_storeu_si512(compressed + 8, w0);
  tmp = _mm512_loadu_si512 (in + 10);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 22));
  w0 = _mm512_srli_epi32(tmp,10);
  _mm512_storeu_si512(compressed + 9, w1);
  tmp = _mm512_loadu_si512 (in + 11);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 21));
  w1 = _mm512_srli_epi32(tmp,11);
  _mm512_storeu_si512(compressed + 10, w0);
  tmp = _mm512_loadu_si512 (in + 12);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 20));
  w0 = _mm512_srli_epi32(tmp,12);
  _mm512_storeu_si512(compressed + 11, w1);
  tmp = _mm512_loadu_si512 (in + 13);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 19));
  w1 = _mm512_srli_epi32(tmp,13);
  _mm512_storeu_si512(compressed + 12, w0);
  tmp = _mm512_loadu_si512 (in + 14);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 18));
  w0 = _mm512_srli_epi32(tmp,14);
  _mm512_storeu_si512(compressed + 13, w1);
  tmp = _mm512_loadu_si512 (in + 15);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 17));
  w1 = _mm512_srli_epi32(tmp,15);
  _mm512_storeu_si512(compressed + 14, w0);
  tmp = _mm512_loadu_si512 (in + 16);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 16));
  w0 = _mm512_srli_epi32(tmp,16);
  _mm512_storeu_si512(compressed + 15, w1);
  tmp = _mm512_loadu_si512 (in + 17);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 15));
  w1 = _mm512_srli_epi32(tmp,17);
  _mm512_storeu_si512(compressed + 16, w0);
  tmp = _mm512_loadu_si512 (in + 18);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 14));
  w0 = _mm512_srli_epi32(tmp,18);
  _mm512_storeu_si512(compressed + 17, w1);
  tmp = _mm512_loadu_si512 (in + 19);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 13));
  w1 = _mm512_srli_epi32(tmp,19);
  _mm512_storeu_si512(compressed + 18, w0);
  tmp = _mm512_loadu_si512 (in + 20);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 12));
  w0 = _mm512_srli_epi32(tmp,20);
  _mm512_storeu_si512(compressed + 19, w1);
  tmp = _mm512_loadu_si512 (in + 21);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 11));
  w1 = _mm512_srli_epi32(tmp,21);
  _mm512_storeu_si512(compressed + 20, w0);
  tmp = _mm512_loadu_si512 (in + 22);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 10));
  w0 = _mm512_srli_epi32(tmp,22);
  _mm512_storeu_si512(compressed + 21, w1);
  tmp = _mm512_loadu_si512 (in + 23);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 9));
  w1 = _mm512_srli_epi32(tmp,23);
  _mm512_storeu_si512(compressed + 22, w0);
  tmp = _mm512_loadu_si512 (in + 24);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 8));
  w0 = _mm512_srli_epi32(tmp,24);
  _mm512_storeu_si512(compressed + 23, w1);
  tmp = _mm512_loadu_si512 (in + 25);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 7));
  w1 = _mm512_srli_epi32(tmp,25);
  _mm512_storeu_si512(compressed + 24, w0);
  tmp = _mm512_loadu_si512 (in + 26);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 6));
  w0 = _mm512_srli_epi32(tmp,26);
  _mm512_storeu_si512(compressed + 25, w1);
  tmp = _mm512_loadu_si512 (in + 27);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 5));
  w1 = _mm512_srli_epi32(tmp,27);
  _mm512_storeu_si512(compressed + 26, w0);
  tmp = _mm512_loadu_si512 (in + 28);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 4));
  w0 = _mm512_srli_epi32(tmp,28);
  _mm512_storeu_si512(compressed + 27, w1);
  tmp = _mm512_loadu_si512 (in + 29);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 3));
  w1 = _mm512_srli_epi32(tmp,29);
  _mm512_storeu_si512(compressed + 28, w0);
  tmp = _mm512_loadu_si512 (in + 30);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 2));
  w0 = _mm512_srli_epi32(tmp,30);
  _mm512_storeu_si512(compressed + 29, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(_mm512_loadu_si512 (in + 31) , 1));
  _mm512_storeu_si512(compressed + 30, w0);
}


/* we are going to pack 512 32-bit values, touching 32 512-bit words, using 1024 bytes */
static void avx512packblock32(const uint32_t * pin, __m512i * compressed) {
  const __m512i * in = (const __m512i *)  pin;
  /* we are going to touch  32 512-bit words */
  __m512i w0, w1;
  w0 = _mm512_loadu_si512 (in + 0);
  _mm512_storeu_si512(compressed + 0, w0);
  w1 = _mm512_loadu_si512 (in + 1);
  _mm512_storeu_si512(compressed + 1, w1);
  w0 = _mm512_loadu_si512 (in + 2);
  _mm512_storeu_si512(compressed + 2, w0);
  w1 = _mm512_loadu_si512 (in + 3);
  _mm512_storeu_si512(compressed + 3, w1);
  w0 = _mm512_loadu_si512 (in + 4);
  _mm512_storeu_si512(compressed + 4, w0);
  w1 = _mm512_loadu_si512 (in + 5);
  _mm512_storeu_si512(compressed + 5, w1);
  w0 = _mm512_loadu_si512 (in + 6);
  _mm512_storeu_si512(compressed + 6, w0);
  w1 = _mm512_loadu_si512 (in + 7);
  _mm512_storeu_si512(compressed + 7, w1);
  w0 = _mm512_loadu_si512 (in + 8);
  _mm512_storeu_si512(compressed + 8, w0);
  w1 = _mm512_loadu_si512 (in + 9);
  _mm512_storeu_si512(compressed + 9, w1);
  w0 = _mm512_loadu_si512 (in + 10);
  _mm512_storeu_si512(compressed + 10, w0);
  w1 = _mm512_loadu_si512 (in + 11);
  _mm512_storeu_si512(compressed + 11, w1);
  w0 = _mm512_loadu_si512 (in + 12);
  _mm512_storeu_si512(compressed + 12, w0);
  w1 = _mm512_loadu_si512 (in + 13);
  _mm512_storeu_si512(compressed + 13, w1);
  w0 = _mm512_loadu_si512 (in + 14);
  _mm512_storeu_si512(compressed + 14, w0);
  w1 = _mm512_loadu_si512 (in + 15);
  _mm512_storeu_si512(compressed + 15, w1);
  w0 = _mm512_loadu_si512 (in + 16);
  _mm512_storeu_si512(compressed + 16, w0);
  w1 = _mm512_loadu_si512 (in + 17);
  _mm512_storeu_si512(compressed + 17, w1);
  w0 = _mm512_loadu_si512 (in + 18);
  _mm512_storeu_si512(compressed + 18, w0);
  w1 = _mm512_loadu_si512 (in + 19);
  _mm512_storeu_si512(compressed + 19, w1);
  w0 = _mm512_loadu_si512 (in + 20);
  _mm512_storeu_si512(compressed + 20, w0);
  w1 = _mm512_loadu_si512 (in + 21);
  _mm512_storeu_si512(compressed + 21, w1);
  w0 = _mm512_loadu_si512 (in + 22);
  _mm512_storeu_si512(compressed + 22, w0);
  w1 = _mm512_loadu_si512 (in + 23);
  _mm512_storeu_si512(compressed + 23, w1);
  w0 = _mm512_loadu_si512 (in + 24);
  _mm512_storeu_si512(compressed + 24, w0);
  w1 = _mm512_loadu_si512 (in + 25);
  _mm512_storeu_si512(compressed + 25, w1);
  w0 = _mm512_loadu_si512 (in + 26);
  _mm512_storeu_si512(compressed + 26, w0);
  w1 = _mm512_loadu_si512 (in + 27);
  _mm512_storeu_si512(compressed + 27, w1);
  w0 = _mm512_loadu_si512 (in + 28);
  _mm512_storeu_si512(compressed + 28, w0);
  w1 = _mm512_loadu_si512 (in + 29);
  _mm512_storeu_si512(compressed + 29, w1);
  w0 = _mm512_loadu_si512 (in + 30);
  _mm512_storeu_si512(compressed + 30, w0);
  w1 = _mm512_loadu_si512 (in + 31);
  _mm512_storeu_si512(compressed + 31, w1);
}


static void avx512packblockmask0(const uint32_t * pin, __m512i * compressed) {
  (void)compressed;
  (void) pin; /* we consumed 512 32-bit integers */
}


/* we are going to pack 512 1-bit values, touching 1 512-bit words, using 32 bytes */
static void avx512packblockmask1(const uint32_t * pin, __m512i * compressed) {
  /* we are going to touch  1 512-bit word */
  __m512i w0;
  const __m512i * in = (const __m512i *) pin;
  const __m512i mask = _mm512_set1_epi32(1);
  w0 =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 0) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 1) )  , 1));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 2) )  , 2));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 3) )  , 3));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 4) )  , 4));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 5) )  , 5));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 6) )  , 6));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 7) )  , 7));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 8) )  , 8));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 9) )  , 9));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 10) )  , 10));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 11) )  , 11));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 12) )  , 12));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 13) )  , 13));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 14) )  , 14));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 15) )  , 15));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 16) )  , 16));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 17) )  , 17));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 18) )  , 18));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 19) )  , 19));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 20) )  , 20));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 21) )  , 21));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 22) )  , 22));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 23) )  , 23));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 24) )  , 24));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 25) )  , 25));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 26) )  , 26));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 27) )  , 27));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 28) )  , 28));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 29) )  , 29));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 30) )  , 30));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 31) )  , 31));
  _mm512_storeu_si512(compressed + 0, w0);
}


/* we are going to pack 512 2-bit values, touching 2 512-bit words, using 64 bytes */
static void avx512packblockmask2(const uint32_t * pin, __m512i * compressed) {
  /* we are going to touch  2 512-bit words */
  __m512i w0, w1;
  const __m512i * in = (const __m512i *) pin;
  const __m512i mask = _mm512_set1_epi32(3);
  w0 =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 0) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 1) )  , 2));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 2) )  , 4));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 3) )  , 6));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 4) )  , 8));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 5) )  , 10));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 6) )  , 12));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 7) )  , 14));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 8) )  , 16));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 9) )  , 18));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 10) )  , 20));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 11) )  , 22));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 12) )  , 24));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 13) )  , 26));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 14) )  , 28));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 15) )  , 30));
  _mm512_storeu_si512(compressed + 0, w0);
  w1 =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 16) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 17) )  , 2));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 18) )  , 4));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 19) )  , 6));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 20) )  , 8));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 21) )  , 10));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 22) )  , 12));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 23) )  , 14));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 24) )  , 16));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 25) )  , 18));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 26) )  , 20));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 27) )  , 22));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 28) )  , 24));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 29) )  , 26));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 30) )  , 28));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 31) )  , 30));
  _mm512_storeu_si512(compressed + 1, w1);
}


/* we are going to pack 512 3-bit values, touching 3 512-bit words, using 96 bytes */
static void avx512packblockmask3(const uint32_t * pin, __m512i * compressed) {
  /* we are going to touch  3 512-bit words */
  __m512i w0, w1;
  const __m512i * in = (const __m512i *) pin;
  const __m512i mask = _mm512_set1_epi32(7);
  __m512i tmp; /* used to store inputs at word boundary */
  w0 =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 0) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 1) )  , 3));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 2) )  , 6));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 3) )  , 9));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 4) )  , 12));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 5) )  , 15));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 6) )  , 18));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 7) )  , 21));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 8) )  , 24));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 9) )  , 27));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 10) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 30));
  w1 = _mm512_srli_epi32(tmp,2);
  _mm512_storeu_si512(compressed + 0, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 11) )  , 1));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 12) )  , 4));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 13) )  , 7));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 14) )  , 10));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 15) )  , 13));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 16) )  , 16));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 17) )  , 19));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 18) )  , 22));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 19) )  , 25));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 20) )  , 28));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 21) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 31));
  w0 = _mm512_srli_epi32(tmp,1);
  _mm512_storeu_si512(compressed + 1, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 22) )  , 2));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 23) )  , 5));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 24) )  , 8));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 25) )  , 11));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 26) )  , 14));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 27) )  , 17));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 28) )  , 20));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 29) )  , 23));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 30) )  , 26));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 31) )  , 29));
  _mm512_storeu_si512(compressed + 2, w0);
}


/* we are going to pack 512 4-bit values, touching 4 512-bit words, using 128 bytes */
static void avx512packblockmask4(const uint32_t * pin, __m512i * compressed) {
  /* we are going to touch  4 512-bit words */
  __m512i w0, w1;
  const __m512i * in = (const __m512i *) pin;
  const __m512i mask = _mm512_set1_epi32(15);
  w0 =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 0) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 1) )  , 4));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 2) )  , 8));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 3) )  , 12));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 4) )  , 16));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 5) )  , 20));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 6) )  , 24));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 7) )  , 28));
  _mm512_storeu_si512(compressed + 0, w0);
  w1 =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 8) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 9) )  , 4));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 10) )  , 8));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 11) )  , 12));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 12) )  , 16));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 13) )  , 20));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 14) )  , 24));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 15) )  , 28));
  _mm512_storeu_si512(compressed + 1, w1);
  w0 =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 16) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 17) )  , 4));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 18) )  , 8));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 19) )  , 12));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 20) )  , 16));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 21) )  , 20));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 22) )  , 24));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 23) )  , 28));
  _mm512_storeu_si512(compressed + 2, w0);
  w1 =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 24) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 25) )  , 4));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 26) )  , 8));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 27) )  , 12));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 28) )  , 16));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 29) )  , 20));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 30) )  , 24));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 31) )  , 28));
  _mm512_storeu_si512(compressed + 3, w1);
}


/* we are going to pack 512 5-bit values, touching 5 512-bit words, using 160 bytes */
static void avx512packblockmask5(const uint32_t * pin, __m512i * compressed) {
  /* we are going to touch  5 512-bit words */
  __m512i w0, w1;
  const __m512i * in = (const __m512i *) pin;
  const __m512i mask = _mm512_set1_epi32(31);
  __m512i tmp; /* used to store inputs at word boundary */
  w0 =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 0) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 1) )  , 5));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 2) )  , 10));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 3) )  , 15));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 4) )  , 20));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 5) )  , 25));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 6) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 30));
  w1 = _mm512_srli_epi32(tmp,2);
  _mm512_storeu_si512(compressed + 0, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 7) )  , 3));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 8) )  , 8));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 9) )  , 13));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 10) )  , 18));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 11) )  , 23));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 12) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 28));
  w0 = _mm512_srli_epi32(tmp,4);
  _mm512_storeu_si512(compressed + 1, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 13) )  , 1));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 14) )  , 6));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 15) )  , 11));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 16) )  , 16));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 17) )  , 21));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 18) )  , 26));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 19) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 31));
  w1 = _mm512_srli_epi32(tmp,1);
  _mm512_storeu_si512(compressed + 2, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 20) )  , 4));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 21) )  , 9));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 22) )  , 14));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 23) )  , 19));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 24) )  , 24));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 25) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 29));
  w0 = _mm512_srli_epi32(tmp,3);
  _mm512_storeu_si512(compressed + 3, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 26) )  , 2));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 27) )  , 7));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 28) )  , 12));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 29) )  , 17));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 30) )  , 22));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 31) )  , 27));
  _mm512_storeu_si512(compressed + 4, w0);
}


/* we are going to pack 512 6-bit values, touching 6 512-bit words, using 192 bytes */
static void avx512packblockmask6(const uint32_t * pin, __m512i * compressed) {
  /* we are going to touch  6 512-bit words */
  __m512i w0, w1;
  const __m512i * in = (const __m512i *) pin;
  const __m512i mask = _mm512_set1_epi32(63);
  __m512i tmp; /* used to store inputs at word boundary */
  w0 =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 0) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 1) )  , 6));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 2) )  , 12));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 3) )  , 18));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 4) )  , 24));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 5) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 30));
  w1 = _mm512_srli_epi32(tmp,2);
  _mm512_storeu_si512(compressed + 0, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 6) )  , 4));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 7) )  , 10));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 8) )  , 16));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 9) )  , 22));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 10) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 28));
  w0 = _mm512_srli_epi32(tmp,4);
  _mm512_storeu_si512(compressed + 1, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 11) )  , 2));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 12) )  , 8));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 13) )  , 14));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 14) )  , 20));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 15) )  , 26));
  _mm512_storeu_si512(compressed + 2, w0);
  w1 =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 16) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 17) )  , 6));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 18) )  , 12));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 19) )  , 18));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 20) )  , 24));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 21) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 30));
  w0 = _mm512_srli_epi32(tmp,2);
  _mm512_storeu_si512(compressed + 3, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 22) )  , 4));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 23) )  , 10));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 24) )  , 16));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 25) )  , 22));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 26) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 28));
  w1 = _mm512_srli_epi32(tmp,4);
  _mm512_storeu_si512(compressed + 4, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 27) )  , 2));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 28) )  , 8));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 29) )  , 14));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 30) )  , 20));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 31) )  , 26));
  _mm512_storeu_si512(compressed + 5, w1);
}


/* we are going to pack 512 7-bit values, touching 7 512-bit words, using 224 bytes */
static void avx512packblockmask7(const uint32_t * pin, __m512i * compressed) {
  /* we are going to touch  7 512-bit words */
  __m512i w0, w1;
  const __m512i * in = (const __m512i *) pin;
  const __m512i mask = _mm512_set1_epi32(127);
  __m512i tmp; /* used to store inputs at word boundary */
  w0 =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 0) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 1) )  , 7));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 2) )  , 14));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 3) )  , 21));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 4) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 28));
  w1 = _mm512_srli_epi32(tmp,4);
  _mm512_storeu_si512(compressed + 0, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 5) )  , 3));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 6) )  , 10));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 7) )  , 17));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 8) )  , 24));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 9) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 31));
  w0 = _mm512_srli_epi32(tmp,1);
  _mm512_storeu_si512(compressed + 1, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 10) )  , 6));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 11) )  , 13));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 12) )  , 20));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 13) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 27));
  w1 = _mm512_srli_epi32(tmp,5);
  _mm512_storeu_si512(compressed + 2, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 14) )  , 2));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 15) )  , 9));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 16) )  , 16));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 17) )  , 23));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 18) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 30));
  w0 = _mm512_srli_epi32(tmp,2);
  _mm512_storeu_si512(compressed + 3, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 19) )  , 5));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 20) )  , 12));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 21) )  , 19));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 22) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 26));
  w1 = _mm512_srli_epi32(tmp,6);
  _mm512_storeu_si512(compressed + 4, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 23) )  , 1));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 24) )  , 8));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 25) )  , 15));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 26) )  , 22));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 27) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 29));
  w0 = _mm512_srli_epi32(tmp,3);
  _mm512_storeu_si512(compressed + 5, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 28) )  , 4));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 29) )  , 11));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 30) )  , 18));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 31) )  , 25));
  _mm512_storeu_si512(compressed + 6, w0);
}


/* we are going to pack 512 8-bit values, touching 8 512-bit words, using 256 bytes */
static void avx512packblockmask8(const uint32_t * pin, __m512i * compressed) {
  /* we are going to touch  8 512-bit words */
  __m512i w0, w1;
  const __m512i * in = (const __m512i *) pin;
  const __m512i mask = _mm512_set1_epi32(255);
  w0 =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 0) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 1) )  , 8));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 2) )  , 16));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 3) )  , 24));
  _mm512_storeu_si512(compressed + 0, w0);
  w1 =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 4) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 5) )  , 8));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 6) )  , 16));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 7) )  , 24));
  _mm512_storeu_si512(compressed + 1, w1);
  w0 =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 8) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 9) )  , 8));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 10) )  , 16));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 11) )  , 24));
  _mm512_storeu_si512(compressed + 2, w0);
  w1 =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 12) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 13) )  , 8));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 14) )  , 16));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 15) )  , 24));
  _mm512_storeu_si512(compressed + 3, w1);
  w0 =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 16) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 17) )  , 8));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 18) )  , 16));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 19) )  , 24));
  _mm512_storeu_si512(compressed + 4, w0);
  w1 =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 20) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 21) )  , 8));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 22) )  , 16));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 23) )  , 24));
  _mm512_storeu_si512(compressed + 5, w1);
  w0 =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 24) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 25) )  , 8));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 26) )  , 16));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 27) )  , 24));
  _mm512_storeu_si512(compressed + 6, w0);
  w1 =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 28) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 29) )  , 8));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 30) )  , 16));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 31) )  , 24));
  _mm512_storeu_si512(compressed + 7, w1);
}


/* we are going to pack 512 9-bit values, touching 9 512-bit words, using 288 bytes */
static void avx512packblockmask9(const uint32_t * pin, __m512i * compressed) {
  /* we are going to touch  9 512-bit words */
  __m512i w0, w1;
  const __m512i * in = (const __m512i *) pin;
  const __m512i mask = _mm512_set1_epi32(511);
  __m512i tmp; /* used to store inputs at word boundary */
  w0 =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 0) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 1) )  , 9));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 2) )  , 18));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 3) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 27));
  w1 = _mm512_srli_epi32(tmp,5);
  _mm512_storeu_si512(compressed + 0, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 4) )  , 4));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 5) )  , 13));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 6) )  , 22));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 7) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 31));
  w0 = _mm512_srli_epi32(tmp,1);
  _mm512_storeu_si512(compressed + 1, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 8) )  , 8));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 9) )  , 17));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 10) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 26));
  w1 = _mm512_srli_epi32(tmp,6);
  _mm512_storeu_si512(compressed + 2, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 11) )  , 3));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 12) )  , 12));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 13) )  , 21));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 14) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 30));
  w0 = _mm512_srli_epi32(tmp,2);
  _mm512_storeu_si512(compressed + 3, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 15) )  , 7));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 16) )  , 16));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 17) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 25));
  w1 = _mm512_srli_epi32(tmp,7);
  _mm512_storeu_si512(compressed + 4, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 18) )  , 2));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 19) )  , 11));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 20) )  , 20));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 21) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 29));
  w0 = _mm512_srli_epi32(tmp,3);
  _mm512_storeu_si512(compressed + 5, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 22) )  , 6));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 23) )  , 15));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 24) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 24));
  w1 = _mm512_srli_epi32(tmp,8);
  _mm512_storeu_si512(compressed + 6, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 25) )  , 1));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 26) )  , 10));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 27) )  , 19));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 28) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 28));
  w0 = _mm512_srli_epi32(tmp,4);
  _mm512_storeu_si512(compressed + 7, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 29) )  , 5));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 30) )  , 14));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 31) )  , 23));
  _mm512_storeu_si512(compressed + 8, w0);
}


/* we are going to pack 512 10-bit values, touching 10 512-bit words, using 320 bytes */
static void avx512packblockmask10(const uint32_t * pin, __m512i * compressed) {
  /* we are going to touch  10 512-bit words */
  __m512i w0, w1;
  const __m512i * in = (const __m512i *) pin;
  const __m512i mask = _mm512_set1_epi32(1023);
  __m512i tmp; /* used to store inputs at word boundary */
  w0 =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 0) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 1) )  , 10));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 2) )  , 20));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 3) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 30));
  w1 = _mm512_srli_epi32(tmp,2);
  _mm512_storeu_si512(compressed + 0, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 4) )  , 8));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 5) )  , 18));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 6) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 28));
  w0 = _mm512_srli_epi32(tmp,4);
  _mm512_storeu_si512(compressed + 1, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 7) )  , 6));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 8) )  , 16));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 9) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 26));
  w1 = _mm512_srli_epi32(tmp,6);
  _mm512_storeu_si512(compressed + 2, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 10) )  , 4));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 11) )  , 14));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 12) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 24));
  w0 = _mm512_srli_epi32(tmp,8);
  _mm512_storeu_si512(compressed + 3, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 13) )  , 2));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 14) )  , 12));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 15) )  , 22));
  _mm512_storeu_si512(compressed + 4, w0);
  w1 =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 16) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 17) )  , 10));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 18) )  , 20));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 19) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 30));
  w0 = _mm512_srli_epi32(tmp,2);
  _mm512_storeu_si512(compressed + 5, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 20) )  , 8));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 21) )  , 18));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 22) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 28));
  w1 = _mm512_srli_epi32(tmp,4);
  _mm512_storeu_si512(compressed + 6, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 23) )  , 6));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 24) )  , 16));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 25) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 26));
  w0 = _mm512_srli_epi32(tmp,6);
  _mm512_storeu_si512(compressed + 7, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 26) )  , 4));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 27) )  , 14));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 28) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 24));
  w1 = _mm512_srli_epi32(tmp,8);
  _mm512_storeu_si512(compressed + 8, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 29) )  , 2));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 30) )  , 12));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 31) )  , 22));
  _mm512_storeu_si512(compressed + 9, w1);
}


/* we are going to pack 512 11-bit values, touching 11 512-bit words, using 352 bytes */
static void avx512packblockmask11(const uint32_t * pin, __m512i * compressed) {
  /* we are going to touch  11 512-bit words */
  __m512i w0, w1;
  const __m512i * in = (const __m512i *) pin;
  const __m512i mask = _mm512_set1_epi32(2047);
  __m512i tmp; /* used to store inputs at word boundary */
  w0 =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 0) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 1) )  , 11));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 2) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 22));
  w1 = _mm512_srli_epi32(tmp,10);
  _mm512_storeu_si512(compressed + 0, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 3) )  , 1));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 4) )  , 12));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 5) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 23));
  w0 = _mm512_srli_epi32(tmp,9);
  _mm512_storeu_si512(compressed + 1, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 6) )  , 2));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 7) )  , 13));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 8) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 24));
  w1 = _mm512_srli_epi32(tmp,8);
  _mm512_storeu_si512(compressed + 2, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 9) )  , 3));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 10) )  , 14));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 11) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 25));
  w0 = _mm512_srli_epi32(tmp,7);
  _mm512_storeu_si512(compressed + 3, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 12) )  , 4));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 13) )  , 15));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 14) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 26));
  w1 = _mm512_srli_epi32(tmp,6);
  _mm512_storeu_si512(compressed + 4, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 15) )  , 5));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 16) )  , 16));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 17) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 27));
  w0 = _mm512_srli_epi32(tmp,5);
  _mm512_storeu_si512(compressed + 5, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 18) )  , 6));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 19) )  , 17));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 20) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 28));
  w1 = _mm512_srli_epi32(tmp,4);
  _mm512_storeu_si512(compressed + 6, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 21) )  , 7));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 22) )  , 18));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 23) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 29));
  w0 = _mm512_srli_epi32(tmp,3);
  _mm512_storeu_si512(compressed + 7, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 24) )  , 8));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 25) )  , 19));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 26) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 30));
  w1 = _mm512_srli_epi32(tmp,2);
  _mm512_storeu_si512(compressed + 8, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 27) )  , 9));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 28) )  , 20));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 29) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 31));
  w0 = _mm512_srli_epi32(tmp,1);
  _mm512_storeu_si512(compressed + 9, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 30) )  , 10));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 31) )  , 21));
  _mm512_storeu_si512(compressed + 10, w0);
}


/* we are going to pack 512 12-bit values, touching 12 512-bit words, using 384 bytes */
static void avx512packblockmask12(const uint32_t * pin, __m512i * compressed) {
  /* we are going to touch  12 512-bit words */
  __m512i w0, w1;
  const __m512i * in = (const __m512i *) pin;
  const __m512i mask = _mm512_set1_epi32(4095);
  __m512i tmp; /* used to store inputs at word boundary */
  w0 =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 0) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 1) )  , 12));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 2) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 24));
  w1 = _mm512_srli_epi32(tmp,8);
  _mm512_storeu_si512(compressed + 0, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 3) )  , 4));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 4) )  , 16));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 5) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 28));
  w0 = _mm512_srli_epi32(tmp,4);
  _mm512_storeu_si512(compressed + 1, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 6) )  , 8));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 7) )  , 20));
  _mm512_storeu_si512(compressed + 2, w0);
  w1 =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 8) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 9) )  , 12));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 10) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 24));
  w0 = _mm512_srli_epi32(tmp,8);
  _mm512_storeu_si512(compressed + 3, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 11) )  , 4));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 12) )  , 16));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 13) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 28));
  w1 = _mm512_srli_epi32(tmp,4);
  _mm512_storeu_si512(compressed + 4, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 14) )  , 8));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 15) )  , 20));
  _mm512_storeu_si512(compressed + 5, w1);
  w0 =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 16) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 17) )  , 12));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 18) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 24));
  w1 = _mm512_srli_epi32(tmp,8);
  _mm512_storeu_si512(compressed + 6, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 19) )  , 4));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 20) )  , 16));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 21) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 28));
  w0 = _mm512_srli_epi32(tmp,4);
  _mm512_storeu_si512(compressed + 7, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 22) )  , 8));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 23) )  , 20));
  _mm512_storeu_si512(compressed + 8, w0);
  w1 =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 24) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 25) )  , 12));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 26) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 24));
  w0 = _mm512_srli_epi32(tmp,8);
  _mm512_storeu_si512(compressed + 9, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 27) )  , 4));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 28) )  , 16));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 29) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 28));
  w1 = _mm512_srli_epi32(tmp,4);
  _mm512_storeu_si512(compressed + 10, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 30) )  , 8));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 31) )  , 20));
  _mm512_storeu_si512(compressed + 11, w1);
}


/* we are going to pack 512 13-bit values, touching 13 512-bit words, using 416 bytes */
static void avx512packblockmask13(const uint32_t * pin, __m512i * compressed) {
  /* we are going to touch  13 512-bit words */
  __m512i w0, w1;
  const __m512i * in = (const __m512i *) pin;
  const __m512i mask = _mm512_set1_epi32(8191);
  __m512i tmp; /* used to store inputs at word boundary */
  w0 =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 0) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 1) )  , 13));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 2) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 26));
  w1 = _mm512_srli_epi32(tmp,6);
  _mm512_storeu_si512(compressed + 0, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 3) )  , 7));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 4) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 20));
  w0 = _mm512_srli_epi32(tmp,12);
  _mm512_storeu_si512(compressed + 1, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 5) )  , 1));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 6) )  , 14));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 7) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 27));
  w1 = _mm512_srli_epi32(tmp,5);
  _mm512_storeu_si512(compressed + 2, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 8) )  , 8));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 9) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 21));
  w0 = _mm512_srli_epi32(tmp,11);
  _mm512_storeu_si512(compressed + 3, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 10) )  , 2));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 11) )  , 15));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 12) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 28));
  w1 = _mm512_srli_epi32(tmp,4);
  _mm512_storeu_si512(compressed + 4, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 13) )  , 9));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 14) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 22));
  w0 = _mm512_srli_epi32(tmp,10);
  _mm512_storeu_si512(compressed + 5, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 15) )  , 3));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 16) )  , 16));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 17) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 29));
  w1 = _mm512_srli_epi32(tmp,3);
  _mm512_storeu_si512(compressed + 6, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 18) )  , 10));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 19) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 23));
  w0 = _mm512_srli_epi32(tmp,9);
  _mm512_storeu_si512(compressed + 7, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 20) )  , 4));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 21) )  , 17));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 22) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 30));
  w1 = _mm512_srli_epi32(tmp,2);
  _mm512_storeu_si512(compressed + 8, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 23) )  , 11));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 24) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 24));
  w0 = _mm512_srli_epi32(tmp,8);
  _mm512_storeu_si512(compressed + 9, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 25) )  , 5));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 26) )  , 18));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 27) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 31));
  w1 = _mm512_srli_epi32(tmp,1);
  _mm512_storeu_si512(compressed + 10, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 28) )  , 12));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 29) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 25));
  w0 = _mm512_srli_epi32(tmp,7);
  _mm512_storeu_si512(compressed + 11, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 30) )  , 6));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 31) )  , 19));
  _mm512_storeu_si512(compressed + 12, w0);
}


/* we are going to pack 512 14-bit values, touching 14 512-bit words, using 448 bytes */
static void avx512packblockmask14(const uint32_t * pin, __m512i * compressed) {
  /* we are going to touch  14 512-bit words */
  __m512i w0, w1;
  const __m512i * in = (const __m512i *) pin;
  const __m512i mask = _mm512_set1_epi32(16383);
  __m512i tmp; /* used to store inputs at word boundary */
  w0 =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 0) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 1) )  , 14));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 2) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 28));
  w1 = _mm512_srli_epi32(tmp,4);
  _mm512_storeu_si512(compressed + 0, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 3) )  , 10));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 4) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 24));
  w0 = _mm512_srli_epi32(tmp,8);
  _mm512_storeu_si512(compressed + 1, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 5) )  , 6));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 6) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 20));
  w1 = _mm512_srli_epi32(tmp,12);
  _mm512_storeu_si512(compressed + 2, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 7) )  , 2));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 8) )  , 16));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 9) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 30));
  w0 = _mm512_srli_epi32(tmp,2);
  _mm512_storeu_si512(compressed + 3, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 10) )  , 12));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 11) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 26));
  w1 = _mm512_srli_epi32(tmp,6);
  _mm512_storeu_si512(compressed + 4, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 12) )  , 8));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 13) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 22));
  w0 = _mm512_srli_epi32(tmp,10);
  _mm512_storeu_si512(compressed + 5, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 14) )  , 4));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 15) )  , 18));
  _mm512_storeu_si512(compressed + 6, w0);
  w1 =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 16) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 17) )  , 14));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 18) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 28));
  w0 = _mm512_srli_epi32(tmp,4);
  _mm512_storeu_si512(compressed + 7, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 19) )  , 10));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 20) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 24));
  w1 = _mm512_srli_epi32(tmp,8);
  _mm512_storeu_si512(compressed + 8, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 21) )  , 6));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 22) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 20));
  w0 = _mm512_srli_epi32(tmp,12);
  _mm512_storeu_si512(compressed + 9, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 23) )  , 2));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 24) )  , 16));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 25) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 30));
  w1 = _mm512_srli_epi32(tmp,2);
  _mm512_storeu_si512(compressed + 10, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 26) )  , 12));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 27) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 26));
  w0 = _mm512_srli_epi32(tmp,6);
  _mm512_storeu_si512(compressed + 11, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 28) )  , 8));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 29) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 22));
  w1 = _mm512_srli_epi32(tmp,10);
  _mm512_storeu_si512(compressed + 12, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 30) )  , 4));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 31) )  , 18));
  _mm512_storeu_si512(compressed + 13, w1);
}


/* we are going to pack 512 15-bit values, touching 15 512-bit words, using 480 bytes */
static void avx512packblockmask15(const uint32_t * pin, __m512i * compressed) {
  /* we are going to touch  15 512-bit words */
  __m512i w0, w1;
  const __m512i * in = (const __m512i *) pin;
  const __m512i mask = _mm512_set1_epi32(32767);
  __m512i tmp; /* used to store inputs at word boundary */
  w0 =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 0) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 1) )  , 15));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 2) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 30));
  w1 = _mm512_srli_epi32(tmp,2);
  _mm512_storeu_si512(compressed + 0, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 3) )  , 13));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 4) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 28));
  w0 = _mm512_srli_epi32(tmp,4);
  _mm512_storeu_si512(compressed + 1, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 5) )  , 11));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 6) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 26));
  w1 = _mm512_srli_epi32(tmp,6);
  _mm512_storeu_si512(compressed + 2, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 7) )  , 9));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 8) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 24));
  w0 = _mm512_srli_epi32(tmp,8);
  _mm512_storeu_si512(compressed + 3, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 9) )  , 7));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 10) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 22));
  w1 = _mm512_srli_epi32(tmp,10);
  _mm512_storeu_si512(compressed + 4, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 11) )  , 5));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 12) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 20));
  w0 = _mm512_srli_epi32(tmp,12);
  _mm512_storeu_si512(compressed + 5, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 13) )  , 3));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 14) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 18));
  w1 = _mm512_srli_epi32(tmp,14);
  _mm512_storeu_si512(compressed + 6, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 15) )  , 1));
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 16) )  , 16));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 17) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 31));
  w0 = _mm512_srli_epi32(tmp,1);
  _mm512_storeu_si512(compressed + 7, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 18) )  , 14));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 19) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 29));
  w1 = _mm512_srli_epi32(tmp,3);
  _mm512_storeu_si512(compressed + 8, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 20) )  , 12));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 21) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 27));
  w0 = _mm512_srli_epi32(tmp,5);
  _mm512_storeu_si512(compressed + 9, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 22) )  , 10));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 23) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 25));
  w1 = _mm512_srli_epi32(tmp,7);
  _mm512_storeu_si512(compressed + 10, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 24) )  , 8));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 25) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 23));
  w0 = _mm512_srli_epi32(tmp,9);
  _mm512_storeu_si512(compressed + 11, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 26) )  , 6));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 27) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 21));
  w1 = _mm512_srli_epi32(tmp,11);
  _mm512_storeu_si512(compressed + 12, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 28) )  , 4));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 29) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 19));
  w0 = _mm512_srli_epi32(tmp,13);
  _mm512_storeu_si512(compressed + 13, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 30) )  , 2));
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 31) )  , 17));
  _mm512_storeu_si512(compressed + 14, w0);
}


/* we are going to pack 512 16-bit values, touching 16 512-bit words, using 512 bytes */
static void avx512packblockmask16(const uint32_t * pin, __m512i * compressed) {
  /* we are going to touch  16 512-bit words */
  __m512i w0, w1;
  const __m512i * in = (const __m512i *) pin;
  const __m512i mask = _mm512_set1_epi32(65535);
  w0 =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 0) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 1) )  , 16));
  _mm512_storeu_si512(compressed + 0, w0);
  w1 =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 2) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 3) )  , 16));
  _mm512_storeu_si512(compressed + 1, w1);
  w0 =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 4) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 5) )  , 16));
  _mm512_storeu_si512(compressed + 2, w0);
  w1 =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 6) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 7) )  , 16));
  _mm512_storeu_si512(compressed + 3, w1);
  w0 =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 8) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 9) )  , 16));
  _mm512_storeu_si512(compressed + 4, w0);
  w1 =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 10) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 11) )  , 16));
  _mm512_storeu_si512(compressed + 5, w1);
  w0 =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 12) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 13) )  , 16));
  _mm512_storeu_si512(compressed + 6, w0);
  w1 =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 14) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 15) )  , 16));
  _mm512_storeu_si512(compressed + 7, w1);
  w0 =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 16) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 17) )  , 16));
  _mm512_storeu_si512(compressed + 8, w0);
  w1 =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 18) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 19) )  , 16));
  _mm512_storeu_si512(compressed + 9, w1);
  w0 =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 20) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 21) )  , 16));
  _mm512_storeu_si512(compressed + 10, w0);
  w1 =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 22) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 23) )  , 16));
  _mm512_storeu_si512(compressed + 11, w1);
  w0 =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 24) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 25) )  , 16));
  _mm512_storeu_si512(compressed + 12, w0);
  w1 =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 26) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 27) )  , 16));
  _mm512_storeu_si512(compressed + 13, w1);
  w0 =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 28) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 29) )  , 16));
  _mm512_storeu_si512(compressed + 14, w0);
  w1 =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 30) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 31) )  , 16));
  _mm512_storeu_si512(compressed + 15, w1);
}


/* we are going to pack 512 17-bit values, touching 17 512-bit words, using 544 bytes */
static void avx512packblockmask17(const uint32_t * pin, __m512i * compressed) {
  /* we are going to touch  17 512-bit words */
  __m512i w0, w1;
  const __m512i * in = (const __m512i *) pin;
  const __m512i mask = _mm512_set1_epi32(131071);
  __m512i tmp; /* used to store inputs at word boundary */
  w0 =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 0) ) ;
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 1) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 17));
  w1 = _mm512_srli_epi32(tmp,15);
  _mm512_storeu_si512(compressed + 0, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 2) )  , 2));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 3) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 19));
  w0 = _mm512_srli_epi32(tmp,13);
  _mm512_storeu_si512(compressed + 1, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 4) )  , 4));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 5) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 21));
  w1 = _mm512_srli_epi32(tmp,11);
  _mm512_storeu_si512(compressed + 2, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 6) )  , 6));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 7) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 23));
  w0 = _mm512_srli_epi32(tmp,9);
  _mm512_storeu_si512(compressed + 3, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 8) )  , 8));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 9) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 25));
  w1 = _mm512_srli_epi32(tmp,7);
  _mm512_storeu_si512(compressed + 4, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 10) )  , 10));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 11) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 27));
  w0 = _mm512_srli_epi32(tmp,5);
  _mm512_storeu_si512(compressed + 5, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 12) )  , 12));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 13) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 29));
  w1 = _mm512_srli_epi32(tmp,3);
  _mm512_storeu_si512(compressed + 6, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 14) )  , 14));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 15) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 31));
  w0 = _mm512_srli_epi32(tmp,1);
  _mm512_storeu_si512(compressed + 7, w1);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 16) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 16));
  w1 = _mm512_srli_epi32(tmp,16);
  _mm512_storeu_si512(compressed + 8, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 17) )  , 1));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 18) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 18));
  w0 = _mm512_srli_epi32(tmp,14);
  _mm512_storeu_si512(compressed + 9, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 19) )  , 3));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 20) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 20));
  w1 = _mm512_srli_epi32(tmp,12);
  _mm512_storeu_si512(compressed + 10, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 21) )  , 5));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 22) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 22));
  w0 = _mm512_srli_epi32(tmp,10);
  _mm512_storeu_si512(compressed + 11, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 23) )  , 7));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 24) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 24));
  w1 = _mm512_srli_epi32(tmp,8);
  _mm512_storeu_si512(compressed + 12, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 25) )  , 9));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 26) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 26));
  w0 = _mm512_srli_epi32(tmp,6);
  _mm512_storeu_si512(compressed + 13, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 27) )  , 11));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 28) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 28));
  w1 = _mm512_srli_epi32(tmp,4);
  _mm512_storeu_si512(compressed + 14, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 29) )  , 13));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 30) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 30));
  w0 = _mm512_srli_epi32(tmp,2);
  _mm512_storeu_si512(compressed + 15, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 31) )  , 15));
  _mm512_storeu_si512(compressed + 16, w0);
}


/* we are going to pack 512 18-bit values, touching 18 512-bit words, using 576 bytes */
static void avx512packblockmask18(const uint32_t * pin, __m512i * compressed) {
  /* we are going to touch  18 512-bit words */
  __m512i w0, w1;
  const __m512i * in = (const __m512i *) pin;
  const __m512i mask = _mm512_set1_epi32(262143);
  __m512i tmp; /* used to store inputs at word boundary */
  w0 =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 0) ) ;
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 1) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 18));
  w1 = _mm512_srli_epi32(tmp,14);
  _mm512_storeu_si512(compressed + 0, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 2) )  , 4));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 3) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 22));
  w0 = _mm512_srli_epi32(tmp,10);
  _mm512_storeu_si512(compressed + 1, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 4) )  , 8));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 5) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 26));
  w1 = _mm512_srli_epi32(tmp,6);
  _mm512_storeu_si512(compressed + 2, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 6) )  , 12));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 7) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 30));
  w0 = _mm512_srli_epi32(tmp,2);
  _mm512_storeu_si512(compressed + 3, w1);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 8) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 16));
  w1 = _mm512_srli_epi32(tmp,16);
  _mm512_storeu_si512(compressed + 4, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 9) )  , 2));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 10) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 20));
  w0 = _mm512_srli_epi32(tmp,12);
  _mm512_storeu_si512(compressed + 5, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 11) )  , 6));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 12) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 24));
  w1 = _mm512_srli_epi32(tmp,8);
  _mm512_storeu_si512(compressed + 6, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 13) )  , 10));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 14) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 28));
  w0 = _mm512_srli_epi32(tmp,4);
  _mm512_storeu_si512(compressed + 7, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 15) )  , 14));
  _mm512_storeu_si512(compressed + 8, w0);
  w1 =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 16) ) ;
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 17) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 18));
  w0 = _mm512_srli_epi32(tmp,14);
  _mm512_storeu_si512(compressed + 9, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 18) )  , 4));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 19) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 22));
  w1 = _mm512_srli_epi32(tmp,10);
  _mm512_storeu_si512(compressed + 10, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 20) )  , 8));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 21) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 26));
  w0 = _mm512_srli_epi32(tmp,6);
  _mm512_storeu_si512(compressed + 11, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 22) )  , 12));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 23) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 30));
  w1 = _mm512_srli_epi32(tmp,2);
  _mm512_storeu_si512(compressed + 12, w0);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 24) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 16));
  w0 = _mm512_srli_epi32(tmp,16);
  _mm512_storeu_si512(compressed + 13, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 25) )  , 2));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 26) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 20));
  w1 = _mm512_srli_epi32(tmp,12);
  _mm512_storeu_si512(compressed + 14, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 27) )  , 6));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 28) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 24));
  w0 = _mm512_srli_epi32(tmp,8);
  _mm512_storeu_si512(compressed + 15, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 29) )  , 10));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 30) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 28));
  w1 = _mm512_srli_epi32(tmp,4);
  _mm512_storeu_si512(compressed + 16, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 31) )  , 14));
  _mm512_storeu_si512(compressed + 17, w1);
}


/* we are going to pack 512 19-bit values, touching 19 512-bit words, using 608 bytes */
static void avx512packblockmask19(const uint32_t * pin, __m512i * compressed) {
  /* we are going to touch  19 512-bit words */
  __m512i w0, w1;
  const __m512i * in = (const __m512i *) pin;
  const __m512i mask = _mm512_set1_epi32(524287);
  __m512i tmp; /* used to store inputs at word boundary */
  w0 =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 0) ) ;
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 1) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 19));
  w1 = _mm512_srli_epi32(tmp,13);
  _mm512_storeu_si512(compressed + 0, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 2) )  , 6));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 3) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 25));
  w0 = _mm512_srli_epi32(tmp,7);
  _mm512_storeu_si512(compressed + 1, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 4) )  , 12));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 5) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 31));
  w1 = _mm512_srli_epi32(tmp,1);
  _mm512_storeu_si512(compressed + 2, w0);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 6) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 18));
  w0 = _mm512_srli_epi32(tmp,14);
  _mm512_storeu_si512(compressed + 3, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 7) )  , 5));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 8) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 24));
  w1 = _mm512_srli_epi32(tmp,8);
  _mm512_storeu_si512(compressed + 4, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 9) )  , 11));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 10) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 30));
  w0 = _mm512_srli_epi32(tmp,2);
  _mm512_storeu_si512(compressed + 5, w1);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 11) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 17));
  w1 = _mm512_srli_epi32(tmp,15);
  _mm512_storeu_si512(compressed + 6, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 12) )  , 4));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 13) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 23));
  w0 = _mm512_srli_epi32(tmp,9);
  _mm512_storeu_si512(compressed + 7, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 14) )  , 10));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 15) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 29));
  w1 = _mm512_srli_epi32(tmp,3);
  _mm512_storeu_si512(compressed + 8, w0);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 16) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 16));
  w0 = _mm512_srli_epi32(tmp,16);
  _mm512_storeu_si512(compressed + 9, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 17) )  , 3));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 18) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 22));
  w1 = _mm512_srli_epi32(tmp,10);
  _mm512_storeu_si512(compressed + 10, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 19) )  , 9));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 20) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 28));
  w0 = _mm512_srli_epi32(tmp,4);
  _mm512_storeu_si512(compressed + 11, w1);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 21) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 15));
  w1 = _mm512_srli_epi32(tmp,17);
  _mm512_storeu_si512(compressed + 12, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 22) )  , 2));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 23) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 21));
  w0 = _mm512_srli_epi32(tmp,11);
  _mm512_storeu_si512(compressed + 13, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 24) )  , 8));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 25) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 27));
  w1 = _mm512_srli_epi32(tmp,5);
  _mm512_storeu_si512(compressed + 14, w0);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 26) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 14));
  w0 = _mm512_srli_epi32(tmp,18);
  _mm512_storeu_si512(compressed + 15, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 27) )  , 1));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 28) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 20));
  w1 = _mm512_srli_epi32(tmp,12);
  _mm512_storeu_si512(compressed + 16, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 29) )  , 7));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 30) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 26));
  w0 = _mm512_srli_epi32(tmp,6);
  _mm512_storeu_si512(compressed + 17, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 31) )  , 13));
  _mm512_storeu_si512(compressed + 18, w0);
}


/* we are going to pack 512 20-bit values, touching 20 512-bit words, using 640 bytes */
static void avx512packblockmask20(const uint32_t * pin, __m512i * compressed) {
  /* we are going to touch  20 512-bit words */
  __m512i w0, w1;
  const __m512i * in = (const __m512i *) pin;
  const __m512i mask = _mm512_set1_epi32(1048575);
  __m512i tmp; /* used to store inputs at word boundary */
  w0 =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 0) ) ;
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 1) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 20));
  w1 = _mm512_srli_epi32(tmp,12);
  _mm512_storeu_si512(compressed + 0, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 2) )  , 8));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 3) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 28));
  w0 = _mm512_srli_epi32(tmp,4);
  _mm512_storeu_si512(compressed + 1, w1);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 4) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 16));
  w1 = _mm512_srli_epi32(tmp,16);
  _mm512_storeu_si512(compressed + 2, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 5) )  , 4));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 6) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 24));
  w0 = _mm512_srli_epi32(tmp,8);
  _mm512_storeu_si512(compressed + 3, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 7) )  , 12));
  _mm512_storeu_si512(compressed + 4, w0);
  w1 =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 8) ) ;
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 9) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 20));
  w0 = _mm512_srli_epi32(tmp,12);
  _mm512_storeu_si512(compressed + 5, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 10) )  , 8));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 11) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 28));
  w1 = _mm512_srli_epi32(tmp,4);
  _mm512_storeu_si512(compressed + 6, w0);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 12) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 16));
  w0 = _mm512_srli_epi32(tmp,16);
  _mm512_storeu_si512(compressed + 7, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 13) )  , 4));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 14) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 24));
  w1 = _mm512_srli_epi32(tmp,8);
  _mm512_storeu_si512(compressed + 8, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 15) )  , 12));
  _mm512_storeu_si512(compressed + 9, w1);
  w0 =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 16) ) ;
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 17) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 20));
  w1 = _mm512_srli_epi32(tmp,12);
  _mm512_storeu_si512(compressed + 10, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 18) )  , 8));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 19) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 28));
  w0 = _mm512_srli_epi32(tmp,4);
  _mm512_storeu_si512(compressed + 11, w1);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 20) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 16));
  w1 = _mm512_srli_epi32(tmp,16);
  _mm512_storeu_si512(compressed + 12, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 21) )  , 4));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 22) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 24));
  w0 = _mm512_srli_epi32(tmp,8);
  _mm512_storeu_si512(compressed + 13, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 23) )  , 12));
  _mm512_storeu_si512(compressed + 14, w0);
  w1 =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 24) ) ;
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 25) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 20));
  w0 = _mm512_srli_epi32(tmp,12);
  _mm512_storeu_si512(compressed + 15, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 26) )  , 8));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 27) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 28));
  w1 = _mm512_srli_epi32(tmp,4);
  _mm512_storeu_si512(compressed + 16, w0);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 28) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 16));
  w0 = _mm512_srli_epi32(tmp,16);
  _mm512_storeu_si512(compressed + 17, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 29) )  , 4));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 30) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 24));
  w1 = _mm512_srli_epi32(tmp,8);
  _mm512_storeu_si512(compressed + 18, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 31) )  , 12));
  _mm512_storeu_si512(compressed + 19, w1);
}


/* we are going to pack 512 21-bit values, touching 21 512-bit words, using 672 bytes */
static void avx512packblockmask21(const uint32_t * pin, __m512i * compressed) {
  /* we are going to touch  21 512-bit words */
  __m512i w0, w1;
  const __m512i * in = (const __m512i *) pin;
  const __m512i mask = _mm512_set1_epi32(2097151);
  __m512i tmp; /* used to store inputs at word boundary */
  w0 =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 0) ) ;
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 1) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 21));
  w1 = _mm512_srli_epi32(tmp,11);
  _mm512_storeu_si512(compressed + 0, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 2) )  , 10));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 3) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 31));
  w0 = _mm512_srli_epi32(tmp,1);
  _mm512_storeu_si512(compressed + 1, w1);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 4) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 20));
  w1 = _mm512_srli_epi32(tmp,12);
  _mm512_storeu_si512(compressed + 2, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 5) )  , 9));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 6) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 30));
  w0 = _mm512_srli_epi32(tmp,2);
  _mm512_storeu_si512(compressed + 3, w1);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 7) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 19));
  w1 = _mm512_srli_epi32(tmp,13);
  _mm512_storeu_si512(compressed + 4, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 8) )  , 8));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 9) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 29));
  w0 = _mm512_srli_epi32(tmp,3);
  _mm512_storeu_si512(compressed + 5, w1);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 10) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 18));
  w1 = _mm512_srli_epi32(tmp,14);
  _mm512_storeu_si512(compressed + 6, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 11) )  , 7));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 12) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 28));
  w0 = _mm512_srli_epi32(tmp,4);
  _mm512_storeu_si512(compressed + 7, w1);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 13) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 17));
  w1 = _mm512_srli_epi32(tmp,15);
  _mm512_storeu_si512(compressed + 8, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 14) )  , 6));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 15) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 27));
  w0 = _mm512_srli_epi32(tmp,5);
  _mm512_storeu_si512(compressed + 9, w1);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 16) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 16));
  w1 = _mm512_srli_epi32(tmp,16);
  _mm512_storeu_si512(compressed + 10, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 17) )  , 5));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 18) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 26));
  w0 = _mm512_srli_epi32(tmp,6);
  _mm512_storeu_si512(compressed + 11, w1);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 19) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 15));
  w1 = _mm512_srli_epi32(tmp,17);
  _mm512_storeu_si512(compressed + 12, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 20) )  , 4));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 21) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 25));
  w0 = _mm512_srli_epi32(tmp,7);
  _mm512_storeu_si512(compressed + 13, w1);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 22) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 14));
  w1 = _mm512_srli_epi32(tmp,18);
  _mm512_storeu_si512(compressed + 14, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 23) )  , 3));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 24) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 24));
  w0 = _mm512_srli_epi32(tmp,8);
  _mm512_storeu_si512(compressed + 15, w1);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 25) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 13));
  w1 = _mm512_srli_epi32(tmp,19);
  _mm512_storeu_si512(compressed + 16, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 26) )  , 2));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 27) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 23));
  w0 = _mm512_srli_epi32(tmp,9);
  _mm512_storeu_si512(compressed + 17, w1);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 28) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 12));
  w1 = _mm512_srli_epi32(tmp,20);
  _mm512_storeu_si512(compressed + 18, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 29) )  , 1));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 30) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 22));
  w0 = _mm512_srli_epi32(tmp,10);
  _mm512_storeu_si512(compressed + 19, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 31) )  , 11));
  _mm512_storeu_si512(compressed + 20, w0);
}


/* we are going to pack 512 22-bit values, touching 22 512-bit words, using 704 bytes */
static void avx512packblockmask22(const uint32_t * pin, __m512i * compressed) {
  /* we are going to touch  22 512-bit words */
  __m512i w0, w1;
  const __m512i * in = (const __m512i *) pin;
  const __m512i mask = _mm512_set1_epi32(4194303);
  __m512i tmp; /* used to store inputs at word boundary */
  w0 =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 0) ) ;
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 1) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 22));
  w1 = _mm512_srli_epi32(tmp,10);
  _mm512_storeu_si512(compressed + 0, w0);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 2) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 12));
  w0 = _mm512_srli_epi32(tmp,20);
  _mm512_storeu_si512(compressed + 1, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 3) )  , 2));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 4) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 24));
  w1 = _mm512_srli_epi32(tmp,8);
  _mm512_storeu_si512(compressed + 2, w0);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 5) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 14));
  w0 = _mm512_srli_epi32(tmp,18);
  _mm512_storeu_si512(compressed + 3, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 6) )  , 4));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 7) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 26));
  w1 = _mm512_srli_epi32(tmp,6);
  _mm512_storeu_si512(compressed + 4, w0);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 8) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 16));
  w0 = _mm512_srli_epi32(tmp,16);
  _mm512_storeu_si512(compressed + 5, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 9) )  , 6));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 10) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 28));
  w1 = _mm512_srli_epi32(tmp,4);
  _mm512_storeu_si512(compressed + 6, w0);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 11) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 18));
  w0 = _mm512_srli_epi32(tmp,14);
  _mm512_storeu_si512(compressed + 7, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 12) )  , 8));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 13) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 30));
  w1 = _mm512_srli_epi32(tmp,2);
  _mm512_storeu_si512(compressed + 8, w0);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 14) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 20));
  w0 = _mm512_srli_epi32(tmp,12);
  _mm512_storeu_si512(compressed + 9, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 15) )  , 10));
  _mm512_storeu_si512(compressed + 10, w0);
  w1 =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 16) ) ;
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 17) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 22));
  w0 = _mm512_srli_epi32(tmp,10);
  _mm512_storeu_si512(compressed + 11, w1);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 18) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 12));
  w1 = _mm512_srli_epi32(tmp,20);
  _mm512_storeu_si512(compressed + 12, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 19) )  , 2));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 20) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 24));
  w0 = _mm512_srli_epi32(tmp,8);
  _mm512_storeu_si512(compressed + 13, w1);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 21) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 14));
  w1 = _mm512_srli_epi32(tmp,18);
  _mm512_storeu_si512(compressed + 14, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 22) )  , 4));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 23) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 26));
  w0 = _mm512_srli_epi32(tmp,6);
  _mm512_storeu_si512(compressed + 15, w1);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 24) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 16));
  w1 = _mm512_srli_epi32(tmp,16);
  _mm512_storeu_si512(compressed + 16, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 25) )  , 6));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 26) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 28));
  w0 = _mm512_srli_epi32(tmp,4);
  _mm512_storeu_si512(compressed + 17, w1);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 27) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 18));
  w1 = _mm512_srli_epi32(tmp,14);
  _mm512_storeu_si512(compressed + 18, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 28) )  , 8));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 29) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 30));
  w0 = _mm512_srli_epi32(tmp,2);
  _mm512_storeu_si512(compressed + 19, w1);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 30) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 20));
  w1 = _mm512_srli_epi32(tmp,12);
  _mm512_storeu_si512(compressed + 20, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 31) )  , 10));
  _mm512_storeu_si512(compressed + 21, w1);
}


/* we are going to pack 512 23-bit values, touching 23 512-bit words, using 736 bytes */
static void avx512packblockmask23(const uint32_t * pin, __m512i * compressed) {
  /* we are going to touch  23 512-bit words */
  __m512i w0, w1;
  const __m512i * in = (const __m512i *) pin;
  const __m512i mask = _mm512_set1_epi32(8388607);
  __m512i tmp; /* used to store inputs at word boundary */
  w0 =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 0) ) ;
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 1) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 23));
  w1 = _mm512_srli_epi32(tmp,9);
  _mm512_storeu_si512(compressed + 0, w0);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 2) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 14));
  w0 = _mm512_srli_epi32(tmp,18);
  _mm512_storeu_si512(compressed + 1, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 3) )  , 5));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 4) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 28));
  w1 = _mm512_srli_epi32(tmp,4);
  _mm512_storeu_si512(compressed + 2, w0);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 5) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 19));
  w0 = _mm512_srli_epi32(tmp,13);
  _mm512_storeu_si512(compressed + 3, w1);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 6) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 10));
  w1 = _mm512_srli_epi32(tmp,22);
  _mm512_storeu_si512(compressed + 4, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 7) )  , 1));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 8) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 24));
  w0 = _mm512_srli_epi32(tmp,8);
  _mm512_storeu_si512(compressed + 5, w1);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 9) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 15));
  w1 = _mm512_srli_epi32(tmp,17);
  _mm512_storeu_si512(compressed + 6, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 10) )  , 6));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 11) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 29));
  w0 = _mm512_srli_epi32(tmp,3);
  _mm512_storeu_si512(compressed + 7, w1);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 12) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 20));
  w1 = _mm512_srli_epi32(tmp,12);
  _mm512_storeu_si512(compressed + 8, w0);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 13) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 11));
  w0 = _mm512_srli_epi32(tmp,21);
  _mm512_storeu_si512(compressed + 9, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 14) )  , 2));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 15) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 25));
  w1 = _mm512_srli_epi32(tmp,7);
  _mm512_storeu_si512(compressed + 10, w0);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 16) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 16));
  w0 = _mm512_srli_epi32(tmp,16);
  _mm512_storeu_si512(compressed + 11, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 17) )  , 7));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 18) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 30));
  w1 = _mm512_srli_epi32(tmp,2);
  _mm512_storeu_si512(compressed + 12, w0);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 19) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 21));
  w0 = _mm512_srli_epi32(tmp,11);
  _mm512_storeu_si512(compressed + 13, w1);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 20) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 12));
  w1 = _mm512_srli_epi32(tmp,20);
  _mm512_storeu_si512(compressed + 14, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 21) )  , 3));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 22) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 26));
  w0 = _mm512_srli_epi32(tmp,6);
  _mm512_storeu_si512(compressed + 15, w1);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 23) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 17));
  w1 = _mm512_srli_epi32(tmp,15);
  _mm512_storeu_si512(compressed + 16, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 24) )  , 8));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 25) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 31));
  w0 = _mm512_srli_epi32(tmp,1);
  _mm512_storeu_si512(compressed + 17, w1);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 26) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 22));
  w1 = _mm512_srli_epi32(tmp,10);
  _mm512_storeu_si512(compressed + 18, w0);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 27) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 13));
  w0 = _mm512_srli_epi32(tmp,19);
  _mm512_storeu_si512(compressed + 19, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 28) )  , 4));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 29) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 27));
  w1 = _mm512_srli_epi32(tmp,5);
  _mm512_storeu_si512(compressed + 20, w0);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 30) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 18));
  w0 = _mm512_srli_epi32(tmp,14);
  _mm512_storeu_si512(compressed + 21, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 31) )  , 9));
  _mm512_storeu_si512(compressed + 22, w0);
}


/* we are going to pack 512 24-bit values, touching 24 512-bit words, using 768 bytes */
static void avx512packblockmask24(const uint32_t * pin, __m512i * compressed) {
  /* we are going to touch  24 512-bit words */
  __m512i w0, w1;
  const __m512i * in = (const __m512i *) pin;
  const __m512i mask = _mm512_set1_epi32(16777215);
  __m512i tmp; /* used to store inputs at word boundary */
  w0 =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 0) ) ;
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 1) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 24));
  w1 = _mm512_srli_epi32(tmp,8);
  _mm512_storeu_si512(compressed + 0, w0);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 2) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 16));
  w0 = _mm512_srli_epi32(tmp,16);
  _mm512_storeu_si512(compressed + 1, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 3) )  , 8));
  _mm512_storeu_si512(compressed + 2, w0);
  w1 =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 4) ) ;
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 5) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 24));
  w0 = _mm512_srli_epi32(tmp,8);
  _mm512_storeu_si512(compressed + 3, w1);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 6) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 16));
  w1 = _mm512_srli_epi32(tmp,16);
  _mm512_storeu_si512(compressed + 4, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 7) )  , 8));
  _mm512_storeu_si512(compressed + 5, w1);
  w0 =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 8) ) ;
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 9) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 24));
  w1 = _mm512_srli_epi32(tmp,8);
  _mm512_storeu_si512(compressed + 6, w0);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 10) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 16));
  w0 = _mm512_srli_epi32(tmp,16);
  _mm512_storeu_si512(compressed + 7, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 11) )  , 8));
  _mm512_storeu_si512(compressed + 8, w0);
  w1 =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 12) ) ;
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 13) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 24));
  w0 = _mm512_srli_epi32(tmp,8);
  _mm512_storeu_si512(compressed + 9, w1);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 14) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 16));
  w1 = _mm512_srli_epi32(tmp,16);
  _mm512_storeu_si512(compressed + 10, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 15) )  , 8));
  _mm512_storeu_si512(compressed + 11, w1);
  w0 =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 16) ) ;
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 17) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 24));
  w1 = _mm512_srli_epi32(tmp,8);
  _mm512_storeu_si512(compressed + 12, w0);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 18) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 16));
  w0 = _mm512_srli_epi32(tmp,16);
  _mm512_storeu_si512(compressed + 13, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 19) )  , 8));
  _mm512_storeu_si512(compressed + 14, w0);
  w1 =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 20) ) ;
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 21) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 24));
  w0 = _mm512_srli_epi32(tmp,8);
  _mm512_storeu_si512(compressed + 15, w1);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 22) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 16));
  w1 = _mm512_srli_epi32(tmp,16);
  _mm512_storeu_si512(compressed + 16, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 23) )  , 8));
  _mm512_storeu_si512(compressed + 17, w1);
  w0 =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 24) ) ;
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 25) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 24));
  w1 = _mm512_srli_epi32(tmp,8);
  _mm512_storeu_si512(compressed + 18, w0);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 26) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 16));
  w0 = _mm512_srli_epi32(tmp,16);
  _mm512_storeu_si512(compressed + 19, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 27) )  , 8));
  _mm512_storeu_si512(compressed + 20, w0);
  w1 =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 28) ) ;
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 29) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 24));
  w0 = _mm512_srli_epi32(tmp,8);
  _mm512_storeu_si512(compressed + 21, w1);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 30) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 16));
  w1 = _mm512_srli_epi32(tmp,16);
  _mm512_storeu_si512(compressed + 22, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 31) )  , 8));
  _mm512_storeu_si512(compressed + 23, w1);
}


/* we are going to pack 512 25-bit values, touching 25 512-bit words, using 800 bytes */
static void avx512packblockmask25(const uint32_t * pin, __m512i * compressed) {
  /* we are going to touch  25 512-bit words */
  __m512i w0, w1;
  const __m512i * in = (const __m512i *) pin;
  const __m512i mask = _mm512_set1_epi32(33554431);
  __m512i tmp; /* used to store inputs at word boundary */
  w0 =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 0) ) ;
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 1) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 25));
  w1 = _mm512_srli_epi32(tmp,7);
  _mm512_storeu_si512(compressed + 0, w0);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 2) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 18));
  w0 = _mm512_srli_epi32(tmp,14);
  _mm512_storeu_si512(compressed + 1, w1);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 3) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 11));
  w1 = _mm512_srli_epi32(tmp,21);
  _mm512_storeu_si512(compressed + 2, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 4) )  , 4));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 5) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 29));
  w0 = _mm512_srli_epi32(tmp,3);
  _mm512_storeu_si512(compressed + 3, w1);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 6) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 22));
  w1 = _mm512_srli_epi32(tmp,10);
  _mm512_storeu_si512(compressed + 4, w0);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 7) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 15));
  w0 = _mm512_srli_epi32(tmp,17);
  _mm512_storeu_si512(compressed + 5, w1);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 8) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 8));
  w1 = _mm512_srli_epi32(tmp,24);
  _mm512_storeu_si512(compressed + 6, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 9) )  , 1));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 10) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 26));
  w0 = _mm512_srli_epi32(tmp,6);
  _mm512_storeu_si512(compressed + 7, w1);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 11) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 19));
  w1 = _mm512_srli_epi32(tmp,13);
  _mm512_storeu_si512(compressed + 8, w0);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 12) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 12));
  w0 = _mm512_srli_epi32(tmp,20);
  _mm512_storeu_si512(compressed + 9, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 13) )  , 5));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 14) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 30));
  w1 = _mm512_srli_epi32(tmp,2);
  _mm512_storeu_si512(compressed + 10, w0);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 15) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 23));
  w0 = _mm512_srli_epi32(tmp,9);
  _mm512_storeu_si512(compressed + 11, w1);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 16) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 16));
  w1 = _mm512_srli_epi32(tmp,16);
  _mm512_storeu_si512(compressed + 12, w0);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 17) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 9));
  w0 = _mm512_srli_epi32(tmp,23);
  _mm512_storeu_si512(compressed + 13, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 18) )  , 2));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 19) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 27));
  w1 = _mm512_srli_epi32(tmp,5);
  _mm512_storeu_si512(compressed + 14, w0);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 20) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 20));
  w0 = _mm512_srli_epi32(tmp,12);
  _mm512_storeu_si512(compressed + 15, w1);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 21) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 13));
  w1 = _mm512_srli_epi32(tmp,19);
  _mm512_storeu_si512(compressed + 16, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 22) )  , 6));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 23) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 31));
  w0 = _mm512_srli_epi32(tmp,1);
  _mm512_storeu_si512(compressed + 17, w1);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 24) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 24));
  w1 = _mm512_srli_epi32(tmp,8);
  _mm512_storeu_si512(compressed + 18, w0);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 25) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 17));
  w0 = _mm512_srli_epi32(tmp,15);
  _mm512_storeu_si512(compressed + 19, w1);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 26) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 10));
  w1 = _mm512_srli_epi32(tmp,22);
  _mm512_storeu_si512(compressed + 20, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 27) )  , 3));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 28) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 28));
  w0 = _mm512_srli_epi32(tmp,4);
  _mm512_storeu_si512(compressed + 21, w1);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 29) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 21));
  w1 = _mm512_srli_epi32(tmp,11);
  _mm512_storeu_si512(compressed + 22, w0);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 30) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 14));
  w0 = _mm512_srli_epi32(tmp,18);
  _mm512_storeu_si512(compressed + 23, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 31) )  , 7));
  _mm512_storeu_si512(compressed + 24, w0);
}


/* we are going to pack 512 26-bit values, touching 26 512-bit words, using 832 bytes */
static void avx512packblockmask26(const uint32_t * pin, __m512i * compressed) {
  /* we are going to touch  26 512-bit words */
  __m512i w0, w1;
  const __m512i * in = (const __m512i *) pin;
  const __m512i mask = _mm512_set1_epi32(67108863);
  __m512i tmp; /* used to store inputs at word boundary */
  w0 =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 0) ) ;
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 1) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 26));
  w1 = _mm512_srli_epi32(tmp,6);
  _mm512_storeu_si512(compressed + 0, w0);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 2) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 20));
  w0 = _mm512_srli_epi32(tmp,12);
  _mm512_storeu_si512(compressed + 1, w1);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 3) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 14));
  w1 = _mm512_srli_epi32(tmp,18);
  _mm512_storeu_si512(compressed + 2, w0);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 4) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 8));
  w0 = _mm512_srli_epi32(tmp,24);
  _mm512_storeu_si512(compressed + 3, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 5) )  , 2));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 6) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 28));
  w1 = _mm512_srli_epi32(tmp,4);
  _mm512_storeu_si512(compressed + 4, w0);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 7) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 22));
  w0 = _mm512_srli_epi32(tmp,10);
  _mm512_storeu_si512(compressed + 5, w1);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 8) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 16));
  w1 = _mm512_srli_epi32(tmp,16);
  _mm512_storeu_si512(compressed + 6, w0);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 9) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 10));
  w0 = _mm512_srli_epi32(tmp,22);
  _mm512_storeu_si512(compressed + 7, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 10) )  , 4));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 11) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 30));
  w1 = _mm512_srli_epi32(tmp,2);
  _mm512_storeu_si512(compressed + 8, w0);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 12) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 24));
  w0 = _mm512_srli_epi32(tmp,8);
  _mm512_storeu_si512(compressed + 9, w1);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 13) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 18));
  w1 = _mm512_srli_epi32(tmp,14);
  _mm512_storeu_si512(compressed + 10, w0);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 14) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 12));
  w0 = _mm512_srli_epi32(tmp,20);
  _mm512_storeu_si512(compressed + 11, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 15) )  , 6));
  _mm512_storeu_si512(compressed + 12, w0);
  w1 =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 16) ) ;
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 17) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 26));
  w0 = _mm512_srli_epi32(tmp,6);
  _mm512_storeu_si512(compressed + 13, w1);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 18) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 20));
  w1 = _mm512_srli_epi32(tmp,12);
  _mm512_storeu_si512(compressed + 14, w0);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 19) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 14));
  w0 = _mm512_srli_epi32(tmp,18);
  _mm512_storeu_si512(compressed + 15, w1);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 20) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 8));
  w1 = _mm512_srli_epi32(tmp,24);
  _mm512_storeu_si512(compressed + 16, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 21) )  , 2));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 22) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 28));
  w0 = _mm512_srli_epi32(tmp,4);
  _mm512_storeu_si512(compressed + 17, w1);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 23) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 22));
  w1 = _mm512_srli_epi32(tmp,10);
  _mm512_storeu_si512(compressed + 18, w0);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 24) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 16));
  w0 = _mm512_srli_epi32(tmp,16);
  _mm512_storeu_si512(compressed + 19, w1);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 25) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 10));
  w1 = _mm512_srli_epi32(tmp,22);
  _mm512_storeu_si512(compressed + 20, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 26) )  , 4));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 27) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 30));
  w0 = _mm512_srli_epi32(tmp,2);
  _mm512_storeu_si512(compressed + 21, w1);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 28) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 24));
  w1 = _mm512_srli_epi32(tmp,8);
  _mm512_storeu_si512(compressed + 22, w0);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 29) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 18));
  w0 = _mm512_srli_epi32(tmp,14);
  _mm512_storeu_si512(compressed + 23, w1);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 30) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 12));
  w1 = _mm512_srli_epi32(tmp,20);
  _mm512_storeu_si512(compressed + 24, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 31) )  , 6));
  _mm512_storeu_si512(compressed + 25, w1);
}


/* we are going to pack 512 27-bit values, touching 27 512-bit words, using 864 bytes */
static void avx512packblockmask27(const uint32_t * pin, __m512i * compressed) {
  /* we are going to touch  27 512-bit words */
  __m512i w0, w1;
  const __m512i * in = (const __m512i *) pin;
  const __m512i mask = _mm512_set1_epi32(134217727);
  __m512i tmp; /* used to store inputs at word boundary */
  w0 =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 0) ) ;
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 1) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 27));
  w1 = _mm512_srli_epi32(tmp,5);
  _mm512_storeu_si512(compressed + 0, w0);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 2) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 22));
  w0 = _mm512_srli_epi32(tmp,10);
  _mm512_storeu_si512(compressed + 1, w1);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 3) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 17));
  w1 = _mm512_srli_epi32(tmp,15);
  _mm512_storeu_si512(compressed + 2, w0);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 4) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 12));
  w0 = _mm512_srli_epi32(tmp,20);
  _mm512_storeu_si512(compressed + 3, w1);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 5) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 7));
  w1 = _mm512_srli_epi32(tmp,25);
  _mm512_storeu_si512(compressed + 4, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 6) )  , 2));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 7) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 29));
  w0 = _mm512_srli_epi32(tmp,3);
  _mm512_storeu_si512(compressed + 5, w1);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 8) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 24));
  w1 = _mm512_srli_epi32(tmp,8);
  _mm512_storeu_si512(compressed + 6, w0);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 9) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 19));
  w0 = _mm512_srli_epi32(tmp,13);
  _mm512_storeu_si512(compressed + 7, w1);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 10) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 14));
  w1 = _mm512_srli_epi32(tmp,18);
  _mm512_storeu_si512(compressed + 8, w0);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 11) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 9));
  w0 = _mm512_srli_epi32(tmp,23);
  _mm512_storeu_si512(compressed + 9, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 12) )  , 4));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 13) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 31));
  w1 = _mm512_srli_epi32(tmp,1);
  _mm512_storeu_si512(compressed + 10, w0);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 14) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 26));
  w0 = _mm512_srli_epi32(tmp,6);
  _mm512_storeu_si512(compressed + 11, w1);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 15) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 21));
  w1 = _mm512_srli_epi32(tmp,11);
  _mm512_storeu_si512(compressed + 12, w0);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 16) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 16));
  w0 = _mm512_srli_epi32(tmp,16);
  _mm512_storeu_si512(compressed + 13, w1);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 17) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 11));
  w1 = _mm512_srli_epi32(tmp,21);
  _mm512_storeu_si512(compressed + 14, w0);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 18) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 6));
  w0 = _mm512_srli_epi32(tmp,26);
  _mm512_storeu_si512(compressed + 15, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 19) )  , 1));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 20) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 28));
  w1 = _mm512_srli_epi32(tmp,4);
  _mm512_storeu_si512(compressed + 16, w0);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 21) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 23));
  w0 = _mm512_srli_epi32(tmp,9);
  _mm512_storeu_si512(compressed + 17, w1);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 22) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 18));
  w1 = _mm512_srli_epi32(tmp,14);
  _mm512_storeu_si512(compressed + 18, w0);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 23) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 13));
  w0 = _mm512_srli_epi32(tmp,19);
  _mm512_storeu_si512(compressed + 19, w1);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 24) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 8));
  w1 = _mm512_srli_epi32(tmp,24);
  _mm512_storeu_si512(compressed + 20, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 25) )  , 3));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 26) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 30));
  w0 = _mm512_srli_epi32(tmp,2);
  _mm512_storeu_si512(compressed + 21, w1);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 27) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 25));
  w1 = _mm512_srli_epi32(tmp,7);
  _mm512_storeu_si512(compressed + 22, w0);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 28) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 20));
  w0 = _mm512_srli_epi32(tmp,12);
  _mm512_storeu_si512(compressed + 23, w1);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 29) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 15));
  w1 = _mm512_srli_epi32(tmp,17);
  _mm512_storeu_si512(compressed + 24, w0);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 30) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 10));
  w0 = _mm512_srli_epi32(tmp,22);
  _mm512_storeu_si512(compressed + 25, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 31) )  , 5));
  _mm512_storeu_si512(compressed + 26, w0);
}


/* we are going to pack 512 28-bit values, touching 28 512-bit words, using 896 bytes */
static void avx512packblockmask28(const uint32_t * pin, __m512i * compressed) {
  /* we are going to touch  28 512-bit words */
  __m512i w0, w1;
  const __m512i * in = (const __m512i *) pin;
  const __m512i mask = _mm512_set1_epi32(268435455);
  __m512i tmp; /* used to store inputs at word boundary */
  w0 =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 0) ) ;
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 1) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 28));
  w1 = _mm512_srli_epi32(tmp,4);
  _mm512_storeu_si512(compressed + 0, w0);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 2) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 24));
  w0 = _mm512_srli_epi32(tmp,8);
  _mm512_storeu_si512(compressed + 1, w1);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 3) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 20));
  w1 = _mm512_srli_epi32(tmp,12);
  _mm512_storeu_si512(compressed + 2, w0);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 4) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 16));
  w0 = _mm512_srli_epi32(tmp,16);
  _mm512_storeu_si512(compressed + 3, w1);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 5) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 12));
  w1 = _mm512_srli_epi32(tmp,20);
  _mm512_storeu_si512(compressed + 4, w0);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 6) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 8));
  w0 = _mm512_srli_epi32(tmp,24);
  _mm512_storeu_si512(compressed + 5, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 7) )  , 4));
  _mm512_storeu_si512(compressed + 6, w0);
  w1 =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 8) ) ;
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 9) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 28));
  w0 = _mm512_srli_epi32(tmp,4);
  _mm512_storeu_si512(compressed + 7, w1);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 10) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 24));
  w1 = _mm512_srli_epi32(tmp,8);
  _mm512_storeu_si512(compressed + 8, w0);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 11) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 20));
  w0 = _mm512_srli_epi32(tmp,12);
  _mm512_storeu_si512(compressed + 9, w1);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 12) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 16));
  w1 = _mm512_srli_epi32(tmp,16);
  _mm512_storeu_si512(compressed + 10, w0);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 13) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 12));
  w0 = _mm512_srli_epi32(tmp,20);
  _mm512_storeu_si512(compressed + 11, w1);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 14) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 8));
  w1 = _mm512_srli_epi32(tmp,24);
  _mm512_storeu_si512(compressed + 12, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 15) )  , 4));
  _mm512_storeu_si512(compressed + 13, w1);
  w0 =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 16) ) ;
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 17) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 28));
  w1 = _mm512_srli_epi32(tmp,4);
  _mm512_storeu_si512(compressed + 14, w0);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 18) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 24));
  w0 = _mm512_srli_epi32(tmp,8);
  _mm512_storeu_si512(compressed + 15, w1);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 19) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 20));
  w1 = _mm512_srli_epi32(tmp,12);
  _mm512_storeu_si512(compressed + 16, w0);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 20) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 16));
  w0 = _mm512_srli_epi32(tmp,16);
  _mm512_storeu_si512(compressed + 17, w1);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 21) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 12));
  w1 = _mm512_srli_epi32(tmp,20);
  _mm512_storeu_si512(compressed + 18, w0);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 22) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 8));
  w0 = _mm512_srli_epi32(tmp,24);
  _mm512_storeu_si512(compressed + 19, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 23) )  , 4));
  _mm512_storeu_si512(compressed + 20, w0);
  w1 =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 24) ) ;
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 25) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 28));
  w0 = _mm512_srli_epi32(tmp,4);
  _mm512_storeu_si512(compressed + 21, w1);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 26) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 24));
  w1 = _mm512_srli_epi32(tmp,8);
  _mm512_storeu_si512(compressed + 22, w0);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 27) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 20));
  w0 = _mm512_srli_epi32(tmp,12);
  _mm512_storeu_si512(compressed + 23, w1);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 28) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 16));
  w1 = _mm512_srli_epi32(tmp,16);
  _mm512_storeu_si512(compressed + 24, w0);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 29) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 12));
  w0 = _mm512_srli_epi32(tmp,20);
  _mm512_storeu_si512(compressed + 25, w1);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 30) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 8));
  w1 = _mm512_srli_epi32(tmp,24);
  _mm512_storeu_si512(compressed + 26, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 31) )  , 4));
  _mm512_storeu_si512(compressed + 27, w1);
}


/* we are going to pack 512 29-bit values, touching 29 512-bit words, using 928 bytes */
static void avx512packblockmask29(const uint32_t * pin, __m512i * compressed) {
  /* we are going to touch  29 512-bit words */
  __m512i w0, w1;
  const __m512i * in = (const __m512i *) pin;
  const __m512i mask = _mm512_set1_epi32(536870911);
  __m512i tmp; /* used to store inputs at word boundary */
  w0 =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 0) ) ;
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 1) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 29));
  w1 = _mm512_srli_epi32(tmp,3);
  _mm512_storeu_si512(compressed + 0, w0);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 2) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 26));
  w0 = _mm512_srli_epi32(tmp,6);
  _mm512_storeu_si512(compressed + 1, w1);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 3) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 23));
  w1 = _mm512_srli_epi32(tmp,9);
  _mm512_storeu_si512(compressed + 2, w0);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 4) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 20));
  w0 = _mm512_srli_epi32(tmp,12);
  _mm512_storeu_si512(compressed + 3, w1);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 5) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 17));
  w1 = _mm512_srli_epi32(tmp,15);
  _mm512_storeu_si512(compressed + 4, w0);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 6) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 14));
  w0 = _mm512_srli_epi32(tmp,18);
  _mm512_storeu_si512(compressed + 5, w1);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 7) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 11));
  w1 = _mm512_srli_epi32(tmp,21);
  _mm512_storeu_si512(compressed + 6, w0);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 8) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 8));
  w0 = _mm512_srli_epi32(tmp,24);
  _mm512_storeu_si512(compressed + 7, w1);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 9) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 5));
  w1 = _mm512_srli_epi32(tmp,27);
  _mm512_storeu_si512(compressed + 8, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 10) )  , 2));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 11) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 31));
  w0 = _mm512_srli_epi32(tmp,1);
  _mm512_storeu_si512(compressed + 9, w1);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 12) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 28));
  w1 = _mm512_srli_epi32(tmp,4);
  _mm512_storeu_si512(compressed + 10, w0);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 13) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 25));
  w0 = _mm512_srli_epi32(tmp,7);
  _mm512_storeu_si512(compressed + 11, w1);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 14) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 22));
  w1 = _mm512_srli_epi32(tmp,10);
  _mm512_storeu_si512(compressed + 12, w0);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 15) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 19));
  w0 = _mm512_srli_epi32(tmp,13);
  _mm512_storeu_si512(compressed + 13, w1);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 16) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 16));
  w1 = _mm512_srli_epi32(tmp,16);
  _mm512_storeu_si512(compressed + 14, w0);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 17) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 13));
  w0 = _mm512_srli_epi32(tmp,19);
  _mm512_storeu_si512(compressed + 15, w1);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 18) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 10));
  w1 = _mm512_srli_epi32(tmp,22);
  _mm512_storeu_si512(compressed + 16, w0);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 19) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 7));
  w0 = _mm512_srli_epi32(tmp,25);
  _mm512_storeu_si512(compressed + 17, w1);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 20) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 4));
  w1 = _mm512_srli_epi32(tmp,28);
  _mm512_storeu_si512(compressed + 18, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 21) )  , 1));
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 22) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 30));
  w0 = _mm512_srli_epi32(tmp,2);
  _mm512_storeu_si512(compressed + 19, w1);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 23) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 27));
  w1 = _mm512_srli_epi32(tmp,5);
  _mm512_storeu_si512(compressed + 20, w0);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 24) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 24));
  w0 = _mm512_srli_epi32(tmp,8);
  _mm512_storeu_si512(compressed + 21, w1);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 25) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 21));
  w1 = _mm512_srli_epi32(tmp,11);
  _mm512_storeu_si512(compressed + 22, w0);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 26) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 18));
  w0 = _mm512_srli_epi32(tmp,14);
  _mm512_storeu_si512(compressed + 23, w1);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 27) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 15));
  w1 = _mm512_srli_epi32(tmp,17);
  _mm512_storeu_si512(compressed + 24, w0);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 28) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 12));
  w0 = _mm512_srli_epi32(tmp,20);
  _mm512_storeu_si512(compressed + 25, w1);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 29) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 9));
  w1 = _mm512_srli_epi32(tmp,23);
  _mm512_storeu_si512(compressed + 26, w0);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 30) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 6));
  w0 = _mm512_srli_epi32(tmp,26);
  _mm512_storeu_si512(compressed + 27, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 31) )  , 3));
  _mm512_storeu_si512(compressed + 28, w0);
}


/* we are going to pack 512 30-bit values, touching 30 512-bit words, using 960 bytes */
static void avx512packblockmask30(const uint32_t * pin, __m512i * compressed) {
  /* we are going to touch  30 512-bit words */
  __m512i w0, w1;
  const __m512i * in = (const __m512i *) pin;
  const __m512i mask = _mm512_set1_epi32(1073741823);
  __m512i tmp; /* used to store inputs at word boundary */
  w0 =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 0) ) ;
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 1) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 30));
  w1 = _mm512_srli_epi32(tmp,2);
  _mm512_storeu_si512(compressed + 0, w0);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 2) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 28));
  w0 = _mm512_srli_epi32(tmp,4);
  _mm512_storeu_si512(compressed + 1, w1);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 3) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 26));
  w1 = _mm512_srli_epi32(tmp,6);
  _mm512_storeu_si512(compressed + 2, w0);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 4) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 24));
  w0 = _mm512_srli_epi32(tmp,8);
  _mm512_storeu_si512(compressed + 3, w1);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 5) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 22));
  w1 = _mm512_srli_epi32(tmp,10);
  _mm512_storeu_si512(compressed + 4, w0);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 6) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 20));
  w0 = _mm512_srli_epi32(tmp,12);
  _mm512_storeu_si512(compressed + 5, w1);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 7) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 18));
  w1 = _mm512_srli_epi32(tmp,14);
  _mm512_storeu_si512(compressed + 6, w0);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 8) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 16));
  w0 = _mm512_srli_epi32(tmp,16);
  _mm512_storeu_si512(compressed + 7, w1);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 9) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 14));
  w1 = _mm512_srli_epi32(tmp,18);
  _mm512_storeu_si512(compressed + 8, w0);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 10) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 12));
  w0 = _mm512_srli_epi32(tmp,20);
  _mm512_storeu_si512(compressed + 9, w1);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 11) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 10));
  w1 = _mm512_srli_epi32(tmp,22);
  _mm512_storeu_si512(compressed + 10, w0);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 12) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 8));
  w0 = _mm512_srli_epi32(tmp,24);
  _mm512_storeu_si512(compressed + 11, w1);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 13) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 6));
  w1 = _mm512_srli_epi32(tmp,26);
  _mm512_storeu_si512(compressed + 12, w0);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 14) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 4));
  w0 = _mm512_srli_epi32(tmp,28);
  _mm512_storeu_si512(compressed + 13, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 15) )  , 2));
  _mm512_storeu_si512(compressed + 14, w0);
  w1 =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 16) ) ;
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 17) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 30));
  w0 = _mm512_srli_epi32(tmp,2);
  _mm512_storeu_si512(compressed + 15, w1);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 18) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 28));
  w1 = _mm512_srli_epi32(tmp,4);
  _mm512_storeu_si512(compressed + 16, w0);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 19) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 26));
  w0 = _mm512_srli_epi32(tmp,6);
  _mm512_storeu_si512(compressed + 17, w1);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 20) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 24));
  w1 = _mm512_srli_epi32(tmp,8);
  _mm512_storeu_si512(compressed + 18, w0);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 21) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 22));
  w0 = _mm512_srli_epi32(tmp,10);
  _mm512_storeu_si512(compressed + 19, w1);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 22) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 20));
  w1 = _mm512_srli_epi32(tmp,12);
  _mm512_storeu_si512(compressed + 20, w0);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 23) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 18));
  w0 = _mm512_srli_epi32(tmp,14);
  _mm512_storeu_si512(compressed + 21, w1);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 24) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 16));
  w1 = _mm512_srli_epi32(tmp,16);
  _mm512_storeu_si512(compressed + 22, w0);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 25) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 14));
  w0 = _mm512_srli_epi32(tmp,18);
  _mm512_storeu_si512(compressed + 23, w1);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 26) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 12));
  w1 = _mm512_srli_epi32(tmp,20);
  _mm512_storeu_si512(compressed + 24, w0);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 27) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 10));
  w0 = _mm512_srli_epi32(tmp,22);
  _mm512_storeu_si512(compressed + 25, w1);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 28) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 8));
  w1 = _mm512_srli_epi32(tmp,24);
  _mm512_storeu_si512(compressed + 26, w0);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 29) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 6));
  w0 = _mm512_srli_epi32(tmp,26);
  _mm512_storeu_si512(compressed + 27, w1);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 30) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 4));
  w1 = _mm512_srli_epi32(tmp,28);
  _mm512_storeu_si512(compressed + 28, w0);
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 31) )  , 2));
  _mm512_storeu_si512(compressed + 29, w1);
}


/* we are going to pack 512 31-bit values, touching 31 512-bit words, using 992 bytes */
static void avx512packblockmask31(const uint32_t * pin, __m512i * compressed) {
  /* we are going to touch  31 512-bit words */
  __m512i w0, w1;
  const __m512i * in = (const __m512i *) pin;
  const __m512i mask = _mm512_set1_epi32(2147483647);
  __m512i tmp; /* used to store inputs at word boundary */
  w0 =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 0) ) ;
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 1) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 31));
  w1 = _mm512_srli_epi32(tmp,1);
  _mm512_storeu_si512(compressed + 0, w0);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 2) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 30));
  w0 = _mm512_srli_epi32(tmp,2);
  _mm512_storeu_si512(compressed + 1, w1);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 3) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 29));
  w1 = _mm512_srli_epi32(tmp,3);
  _mm512_storeu_si512(compressed + 2, w0);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 4) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 28));
  w0 = _mm512_srli_epi32(tmp,4);
  _mm512_storeu_si512(compressed + 3, w1);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 5) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 27));
  w1 = _mm512_srli_epi32(tmp,5);
  _mm512_storeu_si512(compressed + 4, w0);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 6) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 26));
  w0 = _mm512_srli_epi32(tmp,6);
  _mm512_storeu_si512(compressed + 5, w1);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 7) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 25));
  w1 = _mm512_srli_epi32(tmp,7);
  _mm512_storeu_si512(compressed + 6, w0);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 8) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 24));
  w0 = _mm512_srli_epi32(tmp,8);
  _mm512_storeu_si512(compressed + 7, w1);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 9) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 23));
  w1 = _mm512_srli_epi32(tmp,9);
  _mm512_storeu_si512(compressed + 8, w0);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 10) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 22));
  w0 = _mm512_srli_epi32(tmp,10);
  _mm512_storeu_si512(compressed + 9, w1);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 11) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 21));
  w1 = _mm512_srli_epi32(tmp,11);
  _mm512_storeu_si512(compressed + 10, w0);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 12) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 20));
  w0 = _mm512_srli_epi32(tmp,12);
  _mm512_storeu_si512(compressed + 11, w1);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 13) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 19));
  w1 = _mm512_srli_epi32(tmp,13);
  _mm512_storeu_si512(compressed + 12, w0);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 14) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 18));
  w0 = _mm512_srli_epi32(tmp,14);
  _mm512_storeu_si512(compressed + 13, w1);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 15) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 17));
  w1 = _mm512_srli_epi32(tmp,15);
  _mm512_storeu_si512(compressed + 14, w0);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 16) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 16));
  w0 = _mm512_srli_epi32(tmp,16);
  _mm512_storeu_si512(compressed + 15, w1);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 17) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 15));
  w1 = _mm512_srli_epi32(tmp,17);
  _mm512_storeu_si512(compressed + 16, w0);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 18) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 14));
  w0 = _mm512_srli_epi32(tmp,18);
  _mm512_storeu_si512(compressed + 17, w1);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 19) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 13));
  w1 = _mm512_srli_epi32(tmp,19);
  _mm512_storeu_si512(compressed + 18, w0);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 20) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 12));
  w0 = _mm512_srli_epi32(tmp,20);
  _mm512_storeu_si512(compressed + 19, w1);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 21) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 11));
  w1 = _mm512_srli_epi32(tmp,21);
  _mm512_storeu_si512(compressed + 20, w0);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 22) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 10));
  w0 = _mm512_srli_epi32(tmp,22);
  _mm512_storeu_si512(compressed + 21, w1);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 23) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 9));
  w1 = _mm512_srli_epi32(tmp,23);
  _mm512_storeu_si512(compressed + 22, w0);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 24) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 8));
  w0 = _mm512_srli_epi32(tmp,24);
  _mm512_storeu_si512(compressed + 23, w1);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 25) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 7));
  w1 = _mm512_srli_epi32(tmp,25);
  _mm512_storeu_si512(compressed + 24, w0);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 26) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 6));
  w0 = _mm512_srli_epi32(tmp,26);
  _mm512_storeu_si512(compressed + 25, w1);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 27) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 5));
  w1 = _mm512_srli_epi32(tmp,27);
  _mm512_storeu_si512(compressed + 26, w0);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 28) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 4));
  w0 = _mm512_srli_epi32(tmp,28);
  _mm512_storeu_si512(compressed + 27, w1);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 29) ) ;
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32(tmp , 3));
  w1 = _mm512_srli_epi32(tmp,29);
  _mm512_storeu_si512(compressed + 28, w0);
  tmp =  _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 30) ) ;
  w1 = _mm512_or_si512(w1,_mm512_slli_epi32(tmp , 2));
  w0 = _mm512_srli_epi32(tmp,30);
  _mm512_storeu_si512(compressed + 29, w1);
  w0 = _mm512_or_si512(w0,_mm512_slli_epi32( _mm512_and_si512 ( mask,  _mm512_loadu_si512 (in + 31) )  , 1));
  _mm512_storeu_si512(compressed + 30, w0);
}


/* we are going to pack 512 32-bit values, touching 32 512-bit words, using 1024 bytes */
static void avx512packblockmask32(const uint32_t * pin, __m512i * compressed) {
  /* we are going to touch  32 512-bit words */
  __m512i w0, w1;
  const __m512i * in = (const __m512i *) pin;
  w0 =  _mm512_loadu_si512 (in + 0) ;
  _mm512_storeu_si512(compressed + 0, w0);
  w1 =  _mm512_loadu_si512 (in + 1) ;
  _mm512_storeu_si512(compressed + 1, w1);
  w0 =  _mm512_loadu_si512 (in + 2) ;
  _mm512_storeu_si512(compressed + 2, w0);
  w1 =  _mm512_loadu_si512 (in + 3) ;
  _mm512_storeu_si512(compressed + 3, w1);
  w0 =  _mm512_loadu_si512 (in + 4) ;
  _mm512_storeu_si512(compressed + 4, w0);
  w1 =  _mm512_loadu_si512 (in + 5) ;
  _mm512_storeu_si512(compressed + 5, w1);
  w0 =  _mm512_loadu_si512 (in + 6) ;
  _mm512_storeu_si512(compressed + 6, w0);
  w1 =  _mm512_loadu_si512 (in + 7) ;
  _mm512_storeu_si512(compressed + 7, w1);
  w0 =  _mm512_loadu_si512 (in + 8) ;
  _mm512_storeu_si512(compressed + 8, w0);
  w1 =  _mm512_loadu_si512 (in + 9) ;
  _mm512_storeu_si512(compressed + 9, w1);
  w0 =  _mm512_loadu_si512 (in + 10) ;
  _mm512_storeu_si512(compressed + 10, w0);
  w1 =  _mm512_loadu_si512 (in + 11) ;
  _mm512_storeu_si512(compressed + 11, w1);
  w0 =  _mm512_loadu_si512 (in + 12) ;
  _mm512_storeu_si512(compressed + 12, w0);
  w1 =  _mm512_loadu_si512 (in + 13) ;
  _mm512_storeu_si512(compressed + 13, w1);
  w0 =  _mm512_loadu_si512 (in + 14) ;
  _mm512_storeu_si512(compressed + 14, w0);
  w1 =  _mm512_loadu_si512 (in + 15) ;
  _mm512_storeu_si512(compressed + 15, w1);
  w0 =  _mm512_loadu_si512 (in + 16) ;
  _mm512_storeu_si512(compressed + 16, w0);
  w1 =  _mm512_loadu_si512 (in + 17) ;
  _mm512_storeu_si512(compressed + 17, w1);
  w0 =  _mm512_loadu_si512 (in + 18) ;
  _mm512_storeu_si512(compressed + 18, w0);
  w1 =  _mm512_loadu_si512 (in + 19) ;
  _mm512_storeu_si512(compressed + 19, w1);
  w0 =  _mm512_loadu_si512 (in + 20) ;
  _mm512_storeu_si512(compressed + 20, w0);
  w1 =  _mm512_loadu_si512 (in + 21) ;
  _mm512_storeu_si512(compressed + 21, w1);
  w0 =  _mm512_loadu_si512 (in + 22) ;
  _mm512_storeu_si512(compressed + 22, w0);
  w1 =  _mm512_loadu_si512 (in + 23) ;
  _mm512_storeu_si512(compressed + 23, w1);
  w0 =  _mm512_loadu_si512 (in + 24) ;
  _mm512_storeu_si512(compressed + 24, w0);
  w1 =  _mm512_loadu_si512 (in + 25) ;
  _mm512_storeu_si512(compressed + 25, w1);
  w0 =  _mm512_loadu_si512 (in + 26) ;
  _mm512_storeu_si512(compressed + 26, w0);
  w1 =  _mm512_loadu_si512 (in + 27) ;
  _mm512_storeu_si512(compressed + 27, w1);
  w0 =  _mm512_loadu_si512 (in + 28) ;
  _mm512_storeu_si512(compressed + 28, w0);
  w1 =  _mm512_loadu_si512 (in + 29) ;
  _mm512_storeu_si512(compressed + 29, w1);
  w0 =  _mm512_loadu_si512 (in + 30) ;
  _mm512_storeu_si512(compressed + 30, w0);
  w1 =  _mm512_loadu_si512 (in + 31) ;
  _mm512_storeu_si512(compressed + 31, w1);
}

static void avx512unpackblock0(const __m512i * compressed, uint32_t * pout) {
  (void) compressed;
  memset(pout,0,512);
}


/* we packed 512 1-bit values, touching 1 512-bit words, using 32 bytes */
static void avx512unpackblock1(const __m512i * compressed, uint32_t * pout) {
  /* we are going to access  1 512-bit word */
  __m512i w0;
  __m512i * out = (__m512i *) pout;
  const __m512i mask = _mm512_set1_epi32(1);
  w0 = _mm512_loadu_si512 (compressed);
  _mm512_storeu_si512(out + 0,  _mm512_and_si512 ( mask,  w0 ) );
  _mm512_storeu_si512(out + 1,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 1) ) );
  _mm512_storeu_si512(out + 2,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 2) ) );
  _mm512_storeu_si512(out + 3,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 3) ) );
  _mm512_storeu_si512(out + 4,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 4) ) );
  _mm512_storeu_si512(out + 5,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 5) ) );
  _mm512_storeu_si512(out + 6,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 6) ) );
  _mm512_storeu_si512(out + 7,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 7) ) );
  _mm512_storeu_si512(out + 8,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 8) ) );
  _mm512_storeu_si512(out + 9,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 9) ) );
  _mm512_storeu_si512(out + 10,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 10) ) );
  _mm512_storeu_si512(out + 11,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 11) ) );
  _mm512_storeu_si512(out + 12,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 12) ) );
  _mm512_storeu_si512(out + 13,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 13) ) );
  _mm512_storeu_si512(out + 14,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 14) ) );
  _mm512_storeu_si512(out + 15,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 15) ) );
  _mm512_storeu_si512(out + 16,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 16) ) );
  _mm512_storeu_si512(out + 17,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 17) ) );
  _mm512_storeu_si512(out + 18,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 18) ) );
  _mm512_storeu_si512(out + 19,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 19) ) );
  _mm512_storeu_si512(out + 20,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 20) ) );
  _mm512_storeu_si512(out + 21,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 21) ) );
  _mm512_storeu_si512(out + 22,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 22) ) );
  _mm512_storeu_si512(out + 23,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 23) ) );
  _mm512_storeu_si512(out + 24,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 24) ) );
  _mm512_storeu_si512(out + 25,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 25) ) );
  _mm512_storeu_si512(out + 26,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 26) ) );
  _mm512_storeu_si512(out + 27,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 27) ) );
  _mm512_storeu_si512(out + 28,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 28) ) );
  _mm512_storeu_si512(out + 29,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 29) ) );
  _mm512_storeu_si512(out + 30,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 30) ) );
  _mm512_storeu_si512(out + 31, _mm512_srli_epi32( w0 , 31) );
}


/* we packed 512 2-bit values, touching 2 512-bit words, using 64 bytes */
static void avx512unpackblock2(const __m512i * compressed, uint32_t * pout) {
  /* we are going to access  2 512-bit words */
  __m512i w0, w1;
  __m512i * out = (__m512i *) pout;
  const __m512i mask = _mm512_set1_epi32(3);
  w0 = _mm512_loadu_si512 (compressed);
  _mm512_storeu_si512(out + 0,  _mm512_and_si512 ( mask,  w0 ) );
  _mm512_storeu_si512(out + 1,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 2) ) );
  _mm512_storeu_si512(out + 2,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 4) ) );
  _mm512_storeu_si512(out + 3,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 6) ) );
  _mm512_storeu_si512(out + 4,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 8) ) );
  _mm512_storeu_si512(out + 5,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 10) ) );
  _mm512_storeu_si512(out + 6,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 12) ) );
  _mm512_storeu_si512(out + 7,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 14) ) );
  _mm512_storeu_si512(out + 8,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 16) ) );
  _mm512_storeu_si512(out + 9,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 18) ) );
  _mm512_storeu_si512(out + 10,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 20) ) );
  _mm512_storeu_si512(out + 11,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 22) ) );
  _mm512_storeu_si512(out + 12,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 24) ) );
  _mm512_storeu_si512(out + 13,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 26) ) );
  _mm512_storeu_si512(out + 14,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 28) ) );
  _mm512_storeu_si512(out + 15, _mm512_srli_epi32( w0 , 30) );
  w1 = _mm512_loadu_si512 (compressed + 1);
  _mm512_storeu_si512(out + 16,  _mm512_and_si512 ( mask,  w1 ) );
  _mm512_storeu_si512(out + 17,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 2) ) );
  _mm512_storeu_si512(out + 18,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 4) ) );
  _mm512_storeu_si512(out + 19,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 6) ) );
  _mm512_storeu_si512(out + 20,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 8) ) );
  _mm512_storeu_si512(out + 21,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 10) ) );
  _mm512_storeu_si512(out + 22,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 12) ) );
  _mm512_storeu_si512(out + 23,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 14) ) );
  _mm512_storeu_si512(out + 24,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 16) ) );
  _mm512_storeu_si512(out + 25,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 18) ) );
  _mm512_storeu_si512(out + 26,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 20) ) );
  _mm512_storeu_si512(out + 27,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 22) ) );
  _mm512_storeu_si512(out + 28,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 24) ) );
  _mm512_storeu_si512(out + 29,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 26) ) );
  _mm512_storeu_si512(out + 30,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 28) ) );
  _mm512_storeu_si512(out + 31, _mm512_srli_epi32( w1 , 30) );
}


/* we packed 512 3-bit values, touching 3 512-bit words, using 96 bytes */
static void avx512unpackblock3(const __m512i * compressed, uint32_t * pout) {
  /* we are going to access  3 512-bit words */
  __m512i w0, w1;
  __m512i * out = (__m512i *) pout;
  const __m512i mask = _mm512_set1_epi32(7);
  w0 = _mm512_loadu_si512 (compressed);
  _mm512_storeu_si512(out + 0,  _mm512_and_si512 ( mask,  w0 ) );
  _mm512_storeu_si512(out + 1,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 3) ) );
  _mm512_storeu_si512(out + 2,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 6) ) );
  _mm512_storeu_si512(out + 3,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 9) ) );
  _mm512_storeu_si512(out + 4,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 12) ) );
  _mm512_storeu_si512(out + 5,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 15) ) );
  _mm512_storeu_si512(out + 6,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 18) ) );
  _mm512_storeu_si512(out + 7,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 21) ) );
  _mm512_storeu_si512(out + 8,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 24) ) );
  _mm512_storeu_si512(out + 9,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 27) ) );
  w1 = _mm512_loadu_si512 (compressed + 1);
  _mm512_storeu_si512(out + 10,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 30) ,_mm512_slli_epi32( w1 , 2 ) ) ) );
  _mm512_storeu_si512(out + 11,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 1) ) );
  _mm512_storeu_si512(out + 12,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 4) ) );
  _mm512_storeu_si512(out + 13,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 7) ) );
  _mm512_storeu_si512(out + 14,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 10) ) );
  _mm512_storeu_si512(out + 15,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 13) ) );
  _mm512_storeu_si512(out + 16,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 16) ) );
  _mm512_storeu_si512(out + 17,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 19) ) );
  _mm512_storeu_si512(out + 18,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 22) ) );
  _mm512_storeu_si512(out + 19,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 25) ) );
  _mm512_storeu_si512(out + 20,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 28) ) );
  w0 = _mm512_loadu_si512 (compressed + 2);
  _mm512_storeu_si512(out + 21,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 31) ,_mm512_slli_epi32( w0 , 1 ) ) ) );
  _mm512_storeu_si512(out + 22,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 2) ) );
  _mm512_storeu_si512(out + 23,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 5) ) );
  _mm512_storeu_si512(out + 24,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 8) ) );
  _mm512_storeu_si512(out + 25,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 11) ) );
  _mm512_storeu_si512(out + 26,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 14) ) );
  _mm512_storeu_si512(out + 27,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 17) ) );
  _mm512_storeu_si512(out + 28,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 20) ) );
  _mm512_storeu_si512(out + 29,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 23) ) );
  _mm512_storeu_si512(out + 30,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 26) ) );
  _mm512_storeu_si512(out + 31, _mm512_srli_epi32( w0 , 29) );
}


/* we packed 512 4-bit values, touching 4 512-bit words, using 128 bytes */
static void avx512unpackblock4(const __m512i * compressed, uint32_t * pout) {
  /* we are going to access  4 512-bit words */
  __m512i w0, w1;
  __m512i * out = (__m512i *) pout;
  const __m512i mask = _mm512_set1_epi32(15);
  w0 = _mm512_loadu_si512 (compressed);
  _mm512_storeu_si512(out + 0,  _mm512_and_si512 ( mask,  w0 ) );
  _mm512_storeu_si512(out + 1,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 4) ) );
  _mm512_storeu_si512(out + 2,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 8) ) );
  _mm512_storeu_si512(out + 3,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 12) ) );
  _mm512_storeu_si512(out + 4,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 16) ) );
  _mm512_storeu_si512(out + 5,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 20) ) );
  _mm512_storeu_si512(out + 6,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 24) ) );
  _mm512_storeu_si512(out + 7, _mm512_srli_epi32( w0 , 28) );
  w1 = _mm512_loadu_si512 (compressed + 1);
  _mm512_storeu_si512(out + 8,  _mm512_and_si512 ( mask,  w1 ) );
  _mm512_storeu_si512(out + 9,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 4) ) );
  _mm512_storeu_si512(out + 10,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 8) ) );
  _mm512_storeu_si512(out + 11,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 12) ) );
  _mm512_storeu_si512(out + 12,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 16) ) );
  _mm512_storeu_si512(out + 13,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 20) ) );
  _mm512_storeu_si512(out + 14,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 24) ) );
  _mm512_storeu_si512(out + 15, _mm512_srli_epi32( w1 , 28) );
  w0 = _mm512_loadu_si512 (compressed + 2);
  _mm512_storeu_si512(out + 16,  _mm512_and_si512 ( mask,  w0 ) );
  _mm512_storeu_si512(out + 17,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 4) ) );
  _mm512_storeu_si512(out + 18,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 8) ) );
  _mm512_storeu_si512(out + 19,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 12) ) );
  _mm512_storeu_si512(out + 20,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 16) ) );
  _mm512_storeu_si512(out + 21,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 20) ) );
  _mm512_storeu_si512(out + 22,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 24) ) );
  _mm512_storeu_si512(out + 23, _mm512_srli_epi32( w0 , 28) );
  w1 = _mm512_loadu_si512 (compressed + 3);
  _mm512_storeu_si512(out + 24,  _mm512_and_si512 ( mask,  w1 ) );
  _mm512_storeu_si512(out + 25,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 4) ) );
  _mm512_storeu_si512(out + 26,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 8) ) );
  _mm512_storeu_si512(out + 27,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 12) ) );
  _mm512_storeu_si512(out + 28,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 16) ) );
  _mm512_storeu_si512(out + 29,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 20) ) );
  _mm512_storeu_si512(out + 30,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 24) ) );
  _mm512_storeu_si512(out + 31, _mm512_srli_epi32( w1 , 28) );
}


/* we packed 512 5-bit values, touching 5 512-bit words, using 160 bytes */
static void avx512unpackblock5(const __m512i * compressed, uint32_t * pout) {
  /* we are going to access  5 512-bit words */
  __m512i w0, w1;
  __m512i * out = (__m512i *) pout;
  const __m512i mask = _mm512_set1_epi32(31);
  w0 = _mm512_loadu_si512 (compressed);
  _mm512_storeu_si512(out + 0,  _mm512_and_si512 ( mask,  w0 ) );
  _mm512_storeu_si512(out + 1,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 5) ) );
  _mm512_storeu_si512(out + 2,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 10) ) );
  _mm512_storeu_si512(out + 3,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 15) ) );
  _mm512_storeu_si512(out + 4,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 20) ) );
  _mm512_storeu_si512(out + 5,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 25) ) );
  w1 = _mm512_loadu_si512 (compressed + 1);
  _mm512_storeu_si512(out + 6,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 30) ,_mm512_slli_epi32( w1 , 2 ) ) ) );
  _mm512_storeu_si512(out + 7,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 3) ) );
  _mm512_storeu_si512(out + 8,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 8) ) );
  _mm512_storeu_si512(out + 9,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 13) ) );
  _mm512_storeu_si512(out + 10,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 18) ) );
  _mm512_storeu_si512(out + 11,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 23) ) );
  w0 = _mm512_loadu_si512 (compressed + 2);
  _mm512_storeu_si512(out + 12,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 28) ,_mm512_slli_epi32( w0 , 4 ) ) ) );
  _mm512_storeu_si512(out + 13,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 1) ) );
  _mm512_storeu_si512(out + 14,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 6) ) );
  _mm512_storeu_si512(out + 15,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 11) ) );
  _mm512_storeu_si512(out + 16,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 16) ) );
  _mm512_storeu_si512(out + 17,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 21) ) );
  _mm512_storeu_si512(out + 18,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 26) ) );
  w1 = _mm512_loadu_si512 (compressed + 3);
  _mm512_storeu_si512(out + 19,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 31) ,_mm512_slli_epi32( w1 , 1 ) ) ) );
  _mm512_storeu_si512(out + 20,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 4) ) );
  _mm512_storeu_si512(out + 21,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 9) ) );
  _mm512_storeu_si512(out + 22,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 14) ) );
  _mm512_storeu_si512(out + 23,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 19) ) );
  _mm512_storeu_si512(out + 24,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 24) ) );
  w0 = _mm512_loadu_si512 (compressed + 4);
  _mm512_storeu_si512(out + 25,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 29) ,_mm512_slli_epi32( w0 , 3 ) ) ) );
  _mm512_storeu_si512(out + 26,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 2) ) );
  _mm512_storeu_si512(out + 27,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 7) ) );
  _mm512_storeu_si512(out + 28,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 12) ) );
  _mm512_storeu_si512(out + 29,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 17) ) );
  _mm512_storeu_si512(out + 30,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 22) ) );
  _mm512_storeu_si512(out + 31, _mm512_srli_epi32( w0 , 27) );
}


/* we packed 512 6-bit values, touching 6 512-bit words, using 192 bytes */
static void avx512unpackblock6(const __m512i * compressed, uint32_t * pout) {
  /* we are going to access  6 512-bit words */
  __m512i w0, w1;
  __m512i * out = (__m512i *) pout;
  const __m512i mask = _mm512_set1_epi32(63);
  w0 = _mm512_loadu_si512 (compressed);
  _mm512_storeu_si512(out + 0,  _mm512_and_si512 ( mask,  w0 ) );
  _mm512_storeu_si512(out + 1,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 6) ) );
  _mm512_storeu_si512(out + 2,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 12) ) );
  _mm512_storeu_si512(out + 3,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 18) ) );
  _mm512_storeu_si512(out + 4,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 24) ) );
  w1 = _mm512_loadu_si512 (compressed + 1);
  _mm512_storeu_si512(out + 5,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 30) ,_mm512_slli_epi32( w1 , 2 ) ) ) );
  _mm512_storeu_si512(out + 6,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 4) ) );
  _mm512_storeu_si512(out + 7,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 10) ) );
  _mm512_storeu_si512(out + 8,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 16) ) );
  _mm512_storeu_si512(out + 9,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 22) ) );
  w0 = _mm512_loadu_si512 (compressed + 2);
  _mm512_storeu_si512(out + 10,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 28) ,_mm512_slli_epi32( w0 , 4 ) ) ) );
  _mm512_storeu_si512(out + 11,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 2) ) );
  _mm512_storeu_si512(out + 12,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 8) ) );
  _mm512_storeu_si512(out + 13,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 14) ) );
  _mm512_storeu_si512(out + 14,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 20) ) );
  _mm512_storeu_si512(out + 15, _mm512_srli_epi32( w0 , 26) );
  w1 = _mm512_loadu_si512 (compressed + 3);
  _mm512_storeu_si512(out + 16,  _mm512_and_si512 ( mask,  w1 ) );
  _mm512_storeu_si512(out + 17,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 6) ) );
  _mm512_storeu_si512(out + 18,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 12) ) );
  _mm512_storeu_si512(out + 19,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 18) ) );
  _mm512_storeu_si512(out + 20,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 24) ) );
  w0 = _mm512_loadu_si512 (compressed + 4);
  _mm512_storeu_si512(out + 21,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 30) ,_mm512_slli_epi32( w0 , 2 ) ) ) );
  _mm512_storeu_si512(out + 22,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 4) ) );
  _mm512_storeu_si512(out + 23,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 10) ) );
  _mm512_storeu_si512(out + 24,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 16) ) );
  _mm512_storeu_si512(out + 25,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 22) ) );
  w1 = _mm512_loadu_si512 (compressed + 5);
  _mm512_storeu_si512(out + 26,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 28) ,_mm512_slli_epi32( w1 , 4 ) ) ) );
  _mm512_storeu_si512(out + 27,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 2) ) );
  _mm512_storeu_si512(out + 28,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 8) ) );
  _mm512_storeu_si512(out + 29,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 14) ) );
  _mm512_storeu_si512(out + 30,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 20) ) );
  _mm512_storeu_si512(out + 31, _mm512_srli_epi32( w1 , 26) );
}


/* we packed 512 7-bit values, touching 7 512-bit words, using 224 bytes */
static void avx512unpackblock7(const __m512i * compressed, uint32_t * pout) {
  /* we are going to access  7 512-bit words */
  __m512i w0, w1;
  __m512i * out = (__m512i *) pout;
  const __m512i mask = _mm512_set1_epi32(127);
  w0 = _mm512_loadu_si512 (compressed);
  _mm512_storeu_si512(out + 0,  _mm512_and_si512 ( mask,  w0 ) );
  _mm512_storeu_si512(out + 1,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 7) ) );
  _mm512_storeu_si512(out + 2,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 14) ) );
  _mm512_storeu_si512(out + 3,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 21) ) );
  w1 = _mm512_loadu_si512 (compressed + 1);
  _mm512_storeu_si512(out + 4,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 28) ,_mm512_slli_epi32( w1 , 4 ) ) ) );
  _mm512_storeu_si512(out + 5,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 3) ) );
  _mm512_storeu_si512(out + 6,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 10) ) );
  _mm512_storeu_si512(out + 7,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 17) ) );
  _mm512_storeu_si512(out + 8,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 24) ) );
  w0 = _mm512_loadu_si512 (compressed + 2);
  _mm512_storeu_si512(out + 9,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 31) ,_mm512_slli_epi32( w0 , 1 ) ) ) );
  _mm512_storeu_si512(out + 10,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 6) ) );
  _mm512_storeu_si512(out + 11,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 13) ) );
  _mm512_storeu_si512(out + 12,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 20) ) );
  w1 = _mm512_loadu_si512 (compressed + 3);
  _mm512_storeu_si512(out + 13,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 27) ,_mm512_slli_epi32( w1 , 5 ) ) ) );
  _mm512_storeu_si512(out + 14,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 2) ) );
  _mm512_storeu_si512(out + 15,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 9) ) );
  _mm512_storeu_si512(out + 16,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 16) ) );
  _mm512_storeu_si512(out + 17,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 23) ) );
  w0 = _mm512_loadu_si512 (compressed + 4);
  _mm512_storeu_si512(out + 18,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 30) ,_mm512_slli_epi32( w0 , 2 ) ) ) );
  _mm512_storeu_si512(out + 19,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 5) ) );
  _mm512_storeu_si512(out + 20,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 12) ) );
  _mm512_storeu_si512(out + 21,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 19) ) );
  w1 = _mm512_loadu_si512 (compressed + 5);
  _mm512_storeu_si512(out + 22,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 26) ,_mm512_slli_epi32( w1 , 6 ) ) ) );
  _mm512_storeu_si512(out + 23,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 1) ) );
  _mm512_storeu_si512(out + 24,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 8) ) );
  _mm512_storeu_si512(out + 25,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 15) ) );
  _mm512_storeu_si512(out + 26,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 22) ) );
  w0 = _mm512_loadu_si512 (compressed + 6);
  _mm512_storeu_si512(out + 27,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 29) ,_mm512_slli_epi32( w0 , 3 ) ) ) );
  _mm512_storeu_si512(out + 28,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 4) ) );
  _mm512_storeu_si512(out + 29,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 11) ) );
  _mm512_storeu_si512(out + 30,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 18) ) );
  _mm512_storeu_si512(out + 31, _mm512_srli_epi32( w0 , 25) );
}


/* we packed 512 8-bit values, touching 8 512-bit words, using 256 bytes */
static void avx512unpackblock8(const __m512i * compressed, uint32_t * pout) {
  /* we are going to access  8 512-bit words */
  __m512i w0, w1;
  __m512i * out = (__m512i *) pout;
  const __m512i mask = _mm512_set1_epi32(255);
  w0 = _mm512_loadu_si512 (compressed);
  _mm512_storeu_si512(out + 0,  _mm512_and_si512 ( mask,  w0 ) );
  _mm512_storeu_si512(out + 1,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 8) ) );
  _mm512_storeu_si512(out + 2,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 16) ) );
  _mm512_storeu_si512(out + 3, _mm512_srli_epi32( w0 , 24) );
  w1 = _mm512_loadu_si512 (compressed + 1);
  _mm512_storeu_si512(out + 4,  _mm512_and_si512 ( mask,  w1 ) );
  _mm512_storeu_si512(out + 5,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 8) ) );
  _mm512_storeu_si512(out + 6,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 16) ) );
  _mm512_storeu_si512(out + 7, _mm512_srli_epi32( w1 , 24) );
  w0 = _mm512_loadu_si512 (compressed + 2);
  _mm512_storeu_si512(out + 8,  _mm512_and_si512 ( mask,  w0 ) );
  _mm512_storeu_si512(out + 9,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 8) ) );
  _mm512_storeu_si512(out + 10,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 16) ) );
  _mm512_storeu_si512(out + 11, _mm512_srli_epi32( w0 , 24) );
  w1 = _mm512_loadu_si512 (compressed + 3);
  _mm512_storeu_si512(out + 12,  _mm512_and_si512 ( mask,  w1 ) );
  _mm512_storeu_si512(out + 13,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 8) ) );
  _mm512_storeu_si512(out + 14,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 16) ) );
  _mm512_storeu_si512(out + 15, _mm512_srli_epi32( w1 , 24) );
  w0 = _mm512_loadu_si512 (compressed + 4);
  _mm512_storeu_si512(out + 16,  _mm512_and_si512 ( mask,  w0 ) );
  _mm512_storeu_si512(out + 17,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 8) ) );
  _mm512_storeu_si512(out + 18,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 16) ) );
  _mm512_storeu_si512(out + 19, _mm512_srli_epi32( w0 , 24) );
  w1 = _mm512_loadu_si512 (compressed + 5);
  _mm512_storeu_si512(out + 20,  _mm512_and_si512 ( mask,  w1 ) );
  _mm512_storeu_si512(out + 21,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 8) ) );
  _mm512_storeu_si512(out + 22,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 16) ) );
  _mm512_storeu_si512(out + 23, _mm512_srli_epi32( w1 , 24) );
  w0 = _mm512_loadu_si512 (compressed + 6);
  _mm512_storeu_si512(out + 24,  _mm512_and_si512 ( mask,  w0 ) );
  _mm512_storeu_si512(out + 25,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 8) ) );
  _mm512_storeu_si512(out + 26,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 16) ) );
  _mm512_storeu_si512(out + 27, _mm512_srli_epi32( w0 , 24) );
  w1 = _mm512_loadu_si512 (compressed + 7);
  _mm512_storeu_si512(out + 28,  _mm512_and_si512 ( mask,  w1 ) );
  _mm512_storeu_si512(out + 29,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 8) ) );
  _mm512_storeu_si512(out + 30,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 16) ) );
  _mm512_storeu_si512(out + 31, _mm512_srli_epi32( w1 , 24) );
}


/* we packed 512 9-bit values, touching 9 512-bit words, using 288 bytes */
static void avx512unpackblock9(const __m512i * compressed, uint32_t * pout) {
  /* we are going to access  9 512-bit words */
  __m512i w0, w1;
  __m512i * out = (__m512i *) pout;
  const __m512i mask = _mm512_set1_epi32(511);
  w0 = _mm512_loadu_si512 (compressed);
  _mm512_storeu_si512(out + 0,  _mm512_and_si512 ( mask,  w0 ) );
  _mm512_storeu_si512(out + 1,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 9) ) );
  _mm512_storeu_si512(out + 2,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 18) ) );
  w1 = _mm512_loadu_si512 (compressed + 1);
  _mm512_storeu_si512(out + 3,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 27) ,_mm512_slli_epi32( w1 , 5 ) ) ) );
  _mm512_storeu_si512(out + 4,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 4) ) );
  _mm512_storeu_si512(out + 5,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 13) ) );
  _mm512_storeu_si512(out + 6,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 22) ) );
  w0 = _mm512_loadu_si512 (compressed + 2);
  _mm512_storeu_si512(out + 7,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 31) ,_mm512_slli_epi32( w0 , 1 ) ) ) );
  _mm512_storeu_si512(out + 8,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 8) ) );
  _mm512_storeu_si512(out + 9,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 17) ) );
  w1 = _mm512_loadu_si512 (compressed + 3);
  _mm512_storeu_si512(out + 10,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 26) ,_mm512_slli_epi32( w1 , 6 ) ) ) );
  _mm512_storeu_si512(out + 11,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 3) ) );
  _mm512_storeu_si512(out + 12,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 12) ) );
  _mm512_storeu_si512(out + 13,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 21) ) );
  w0 = _mm512_loadu_si512 (compressed + 4);
  _mm512_storeu_si512(out + 14,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 30) ,_mm512_slli_epi32( w0 , 2 ) ) ) );
  _mm512_storeu_si512(out + 15,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 7) ) );
  _mm512_storeu_si512(out + 16,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 16) ) );
  w1 = _mm512_loadu_si512 (compressed + 5);
  _mm512_storeu_si512(out + 17,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 25) ,_mm512_slli_epi32( w1 , 7 ) ) ) );
  _mm512_storeu_si512(out + 18,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 2) ) );
  _mm512_storeu_si512(out + 19,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 11) ) );
  _mm512_storeu_si512(out + 20,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 20) ) );
  w0 = _mm512_loadu_si512 (compressed + 6);
  _mm512_storeu_si512(out + 21,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 29) ,_mm512_slli_epi32( w0 , 3 ) ) ) );
  _mm512_storeu_si512(out + 22,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 6) ) );
  _mm512_storeu_si512(out + 23,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 15) ) );
  w1 = _mm512_loadu_si512 (compressed + 7);
  _mm512_storeu_si512(out + 24,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 24) ,_mm512_slli_epi32( w1 , 8 ) ) ) );
  _mm512_storeu_si512(out + 25,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 1) ) );
  _mm512_storeu_si512(out + 26,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 10) ) );
  _mm512_storeu_si512(out + 27,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 19) ) );
  w0 = _mm512_loadu_si512 (compressed + 8);
  _mm512_storeu_si512(out + 28,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 28) ,_mm512_slli_epi32( w0 , 4 ) ) ) );
  _mm512_storeu_si512(out + 29,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 5) ) );
  _mm512_storeu_si512(out + 30,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 14) ) );
  _mm512_storeu_si512(out + 31, _mm512_srli_epi32( w0 , 23) );
}


/* we packed 512 10-bit values, touching 10 512-bit words, using 320 bytes */
static void avx512unpackblock10(const __m512i * compressed, uint32_t * pout) {
  /* we are going to access  10 512-bit words */
  __m512i w0, w1;
  __m512i * out = (__m512i *) pout;
  const __m512i mask = _mm512_set1_epi32(1023);
  w0 = _mm512_loadu_si512 (compressed);
  _mm512_storeu_si512(out + 0,  _mm512_and_si512 ( mask,  w0 ) );
  _mm512_storeu_si512(out + 1,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 10) ) );
  _mm512_storeu_si512(out + 2,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 20) ) );
  w1 = _mm512_loadu_si512 (compressed + 1);
  _mm512_storeu_si512(out + 3,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 30) ,_mm512_slli_epi32( w1 , 2 ) ) ) );
  _mm512_storeu_si512(out + 4,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 8) ) );
  _mm512_storeu_si512(out + 5,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 18) ) );
  w0 = _mm512_loadu_si512 (compressed + 2);
  _mm512_storeu_si512(out + 6,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 28) ,_mm512_slli_epi32( w0 , 4 ) ) ) );
  _mm512_storeu_si512(out + 7,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 6) ) );
  _mm512_storeu_si512(out + 8,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 16) ) );
  w1 = _mm512_loadu_si512 (compressed + 3);
  _mm512_storeu_si512(out + 9,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 26) ,_mm512_slli_epi32( w1 , 6 ) ) ) );
  _mm512_storeu_si512(out + 10,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 4) ) );
  _mm512_storeu_si512(out + 11,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 14) ) );
  w0 = _mm512_loadu_si512 (compressed + 4);
  _mm512_storeu_si512(out + 12,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 24) ,_mm512_slli_epi32( w0 , 8 ) ) ) );
  _mm512_storeu_si512(out + 13,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 2) ) );
  _mm512_storeu_si512(out + 14,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 12) ) );
  _mm512_storeu_si512(out + 15, _mm512_srli_epi32( w0 , 22) );
  w1 = _mm512_loadu_si512 (compressed + 5);
  _mm512_storeu_si512(out + 16,  _mm512_and_si512 ( mask,  w1 ) );
  _mm512_storeu_si512(out + 17,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 10) ) );
  _mm512_storeu_si512(out + 18,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 20) ) );
  w0 = _mm512_loadu_si512 (compressed + 6);
  _mm512_storeu_si512(out + 19,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 30) ,_mm512_slli_epi32( w0 , 2 ) ) ) );
  _mm512_storeu_si512(out + 20,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 8) ) );
  _mm512_storeu_si512(out + 21,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 18) ) );
  w1 = _mm512_loadu_si512 (compressed + 7);
  _mm512_storeu_si512(out + 22,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 28) ,_mm512_slli_epi32( w1 , 4 ) ) ) );
  _mm512_storeu_si512(out + 23,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 6) ) );
  _mm512_storeu_si512(out + 24,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 16) ) );
  w0 = _mm512_loadu_si512 (compressed + 8);
  _mm512_storeu_si512(out + 25,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 26) ,_mm512_slli_epi32( w0 , 6 ) ) ) );
  _mm512_storeu_si512(out + 26,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 4) ) );
  _mm512_storeu_si512(out + 27,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 14) ) );
  w1 = _mm512_loadu_si512 (compressed + 9);
  _mm512_storeu_si512(out + 28,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 24) ,_mm512_slli_epi32( w1 , 8 ) ) ) );
  _mm512_storeu_si512(out + 29,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 2) ) );
  _mm512_storeu_si512(out + 30,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 12) ) );
  _mm512_storeu_si512(out + 31, _mm512_srli_epi32( w1 , 22) );
}


/* we packed 512 11-bit values, touching 11 512-bit words, using 352 bytes */
static void avx512unpackblock11(const __m512i * compressed, uint32_t * pout) {
  /* we are going to access  11 512-bit words */
  __m512i w0, w1;
  __m512i * out = (__m512i *) pout;
  const __m512i mask = _mm512_set1_epi32(2047);
  w0 = _mm512_loadu_si512 (compressed);
  _mm512_storeu_si512(out + 0,  _mm512_and_si512 ( mask,  w0 ) );
  _mm512_storeu_si512(out + 1,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 11) ) );
  w1 = _mm512_loadu_si512 (compressed + 1);
  _mm512_storeu_si512(out + 2,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 22) ,_mm512_slli_epi32( w1 , 10 ) ) ) );
  _mm512_storeu_si512(out + 3,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 1) ) );
  _mm512_storeu_si512(out + 4,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 12) ) );
  w0 = _mm512_loadu_si512 (compressed + 2);
  _mm512_storeu_si512(out + 5,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 23) ,_mm512_slli_epi32( w0 , 9 ) ) ) );
  _mm512_storeu_si512(out + 6,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 2) ) );
  _mm512_storeu_si512(out + 7,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 13) ) );
  w1 = _mm512_loadu_si512 (compressed + 3);
  _mm512_storeu_si512(out + 8,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 24) ,_mm512_slli_epi32( w1 , 8 ) ) ) );
  _mm512_storeu_si512(out + 9,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 3) ) );
  _mm512_storeu_si512(out + 10,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 14) ) );
  w0 = _mm512_loadu_si512 (compressed + 4);
  _mm512_storeu_si512(out + 11,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 25) ,_mm512_slli_epi32( w0 , 7 ) ) ) );
  _mm512_storeu_si512(out + 12,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 4) ) );
  _mm512_storeu_si512(out + 13,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 15) ) );
  w1 = _mm512_loadu_si512 (compressed + 5);
  _mm512_storeu_si512(out + 14,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 26) ,_mm512_slli_epi32( w1 , 6 ) ) ) );
  _mm512_storeu_si512(out + 15,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 5) ) );
  _mm512_storeu_si512(out + 16,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 16) ) );
  w0 = _mm512_loadu_si512 (compressed + 6);
  _mm512_storeu_si512(out + 17,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 27) ,_mm512_slli_epi32( w0 , 5 ) ) ) );
  _mm512_storeu_si512(out + 18,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 6) ) );
  _mm512_storeu_si512(out + 19,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 17) ) );
  w1 = _mm512_loadu_si512 (compressed + 7);
  _mm512_storeu_si512(out + 20,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 28) ,_mm512_slli_epi32( w1 , 4 ) ) ) );
  _mm512_storeu_si512(out + 21,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 7) ) );
  _mm512_storeu_si512(out + 22,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 18) ) );
  w0 = _mm512_loadu_si512 (compressed + 8);
  _mm512_storeu_si512(out + 23,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 29) ,_mm512_slli_epi32( w0 , 3 ) ) ) );
  _mm512_storeu_si512(out + 24,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 8) ) );
  _mm512_storeu_si512(out + 25,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 19) ) );
  w1 = _mm512_loadu_si512 (compressed + 9);
  _mm512_storeu_si512(out + 26,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 30) ,_mm512_slli_epi32( w1 , 2 ) ) ) );
  _mm512_storeu_si512(out + 27,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 9) ) );
  _mm512_storeu_si512(out + 28,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 20) ) );
  w0 = _mm512_loadu_si512 (compressed + 10);
  _mm512_storeu_si512(out + 29,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 31) ,_mm512_slli_epi32( w0 , 1 ) ) ) );
  _mm512_storeu_si512(out + 30,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 10) ) );
  _mm512_storeu_si512(out + 31, _mm512_srli_epi32( w0 , 21) );
}


/* we packed 512 12-bit values, touching 12 512-bit words, using 384 bytes */
static void avx512unpackblock12(const __m512i * compressed, uint32_t * pout) {
  /* we are going to access  12 512-bit words */
  __m512i w0, w1;
  __m512i * out = (__m512i *) pout;
  const __m512i mask = _mm512_set1_epi32(4095);
  w0 = _mm512_loadu_si512 (compressed);
  _mm512_storeu_si512(out + 0,  _mm512_and_si512 ( mask,  w0 ) );
  _mm512_storeu_si512(out + 1,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 12) ) );
  w1 = _mm512_loadu_si512 (compressed + 1);
  _mm512_storeu_si512(out + 2,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 24) ,_mm512_slli_epi32( w1 , 8 ) ) ) );
  _mm512_storeu_si512(out + 3,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 4) ) );
  _mm512_storeu_si512(out + 4,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 16) ) );
  w0 = _mm512_loadu_si512 (compressed + 2);
  _mm512_storeu_si512(out + 5,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 28) ,_mm512_slli_epi32( w0 , 4 ) ) ) );
  _mm512_storeu_si512(out + 6,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 8) ) );
  _mm512_storeu_si512(out + 7, _mm512_srli_epi32( w0 , 20) );
  w1 = _mm512_loadu_si512 (compressed + 3);
  _mm512_storeu_si512(out + 8,  _mm512_and_si512 ( mask,  w1 ) );
  _mm512_storeu_si512(out + 9,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 12) ) );
  w0 = _mm512_loadu_si512 (compressed + 4);
  _mm512_storeu_si512(out + 10,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 24) ,_mm512_slli_epi32( w0 , 8 ) ) ) );
  _mm512_storeu_si512(out + 11,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 4) ) );
  _mm512_storeu_si512(out + 12,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 16) ) );
  w1 = _mm512_loadu_si512 (compressed + 5);
  _mm512_storeu_si512(out + 13,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 28) ,_mm512_slli_epi32( w1 , 4 ) ) ) );
  _mm512_storeu_si512(out + 14,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 8) ) );
  _mm512_storeu_si512(out + 15, _mm512_srli_epi32( w1 , 20) );
  w0 = _mm512_loadu_si512 (compressed + 6);
  _mm512_storeu_si512(out + 16,  _mm512_and_si512 ( mask,  w0 ) );
  _mm512_storeu_si512(out + 17,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 12) ) );
  w1 = _mm512_loadu_si512 (compressed + 7);
  _mm512_storeu_si512(out + 18,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 24) ,_mm512_slli_epi32( w1 , 8 ) ) ) );
  _mm512_storeu_si512(out + 19,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 4) ) );
  _mm512_storeu_si512(out + 20,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 16) ) );
  w0 = _mm512_loadu_si512 (compressed + 8);
  _mm512_storeu_si512(out + 21,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 28) ,_mm512_slli_epi32( w0 , 4 ) ) ) );
  _mm512_storeu_si512(out + 22,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 8) ) );
  _mm512_storeu_si512(out + 23, _mm512_srli_epi32( w0 , 20) );
  w1 = _mm512_loadu_si512 (compressed + 9);
  _mm512_storeu_si512(out + 24,  _mm512_and_si512 ( mask,  w1 ) );
  _mm512_storeu_si512(out + 25,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 12) ) );
  w0 = _mm512_loadu_si512 (compressed + 10);
  _mm512_storeu_si512(out + 26,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 24) ,_mm512_slli_epi32( w0 , 8 ) ) ) );
  _mm512_storeu_si512(out + 27,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 4) ) );
  _mm512_storeu_si512(out + 28,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 16) ) );
  w1 = _mm512_loadu_si512 (compressed + 11);
  _mm512_storeu_si512(out + 29,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 28) ,_mm512_slli_epi32( w1 , 4 ) ) ) );
  _mm512_storeu_si512(out + 30,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 8) ) );
  _mm512_storeu_si512(out + 31, _mm512_srli_epi32( w1 , 20) );
}


/* we packed 512 13-bit values, touching 13 512-bit words, using 416 bytes */
static void avx512unpackblock13(const __m512i * compressed, uint32_t * pout) {
  /* we are going to access  13 512-bit words */
  __m512i w0, w1;
  __m512i * out = (__m512i *) pout;
  const __m512i mask = _mm512_set1_epi32(8191);
  w0 = _mm512_loadu_si512 (compressed);
  _mm512_storeu_si512(out + 0,  _mm512_and_si512 ( mask,  w0 ) );
  _mm512_storeu_si512(out + 1,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 13) ) );
  w1 = _mm512_loadu_si512 (compressed + 1);
  _mm512_storeu_si512(out + 2,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 26) ,_mm512_slli_epi32( w1 , 6 ) ) ) );
  _mm512_storeu_si512(out + 3,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 7) ) );
  w0 = _mm512_loadu_si512 (compressed + 2);
  _mm512_storeu_si512(out + 4,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 20) ,_mm512_slli_epi32( w0 , 12 ) ) ) );
  _mm512_storeu_si512(out + 5,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 1) ) );
  _mm512_storeu_si512(out + 6,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 14) ) );
  w1 = _mm512_loadu_si512 (compressed + 3);
  _mm512_storeu_si512(out + 7,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 27) ,_mm512_slli_epi32( w1 , 5 ) ) ) );
  _mm512_storeu_si512(out + 8,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 8) ) );
  w0 = _mm512_loadu_si512 (compressed + 4);
  _mm512_storeu_si512(out + 9,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 21) ,_mm512_slli_epi32( w0 , 11 ) ) ) );
  _mm512_storeu_si512(out + 10,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 2) ) );
  _mm512_storeu_si512(out + 11,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 15) ) );
  w1 = _mm512_loadu_si512 (compressed + 5);
  _mm512_storeu_si512(out + 12,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 28) ,_mm512_slli_epi32( w1 , 4 ) ) ) );
  _mm512_storeu_si512(out + 13,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 9) ) );
  w0 = _mm512_loadu_si512 (compressed + 6);
  _mm512_storeu_si512(out + 14,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 22) ,_mm512_slli_epi32( w0 , 10 ) ) ) );
  _mm512_storeu_si512(out + 15,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 3) ) );
  _mm512_storeu_si512(out + 16,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 16) ) );
  w1 = _mm512_loadu_si512 (compressed + 7);
  _mm512_storeu_si512(out + 17,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 29) ,_mm512_slli_epi32( w1 , 3 ) ) ) );
  _mm512_storeu_si512(out + 18,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 10) ) );
  w0 = _mm512_loadu_si512 (compressed + 8);
  _mm512_storeu_si512(out + 19,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 23) ,_mm512_slli_epi32( w0 , 9 ) ) ) );
  _mm512_storeu_si512(out + 20,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 4) ) );
  _mm512_storeu_si512(out + 21,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 17) ) );
  w1 = _mm512_loadu_si512 (compressed + 9);
  _mm512_storeu_si512(out + 22,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 30) ,_mm512_slli_epi32( w1 , 2 ) ) ) );
  _mm512_storeu_si512(out + 23,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 11) ) );
  w0 = _mm512_loadu_si512 (compressed + 10);
  _mm512_storeu_si512(out + 24,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 24) ,_mm512_slli_epi32( w0 , 8 ) ) ) );
  _mm512_storeu_si512(out + 25,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 5) ) );
  _mm512_storeu_si512(out + 26,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 18) ) );
  w1 = _mm512_loadu_si512 (compressed + 11);
  _mm512_storeu_si512(out + 27,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 31) ,_mm512_slli_epi32( w1 , 1 ) ) ) );
  _mm512_storeu_si512(out + 28,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 12) ) );
  w0 = _mm512_loadu_si512 (compressed + 12);
  _mm512_storeu_si512(out + 29,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 25) ,_mm512_slli_epi32( w0 , 7 ) ) ) );
  _mm512_storeu_si512(out + 30,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 6) ) );
  _mm512_storeu_si512(out + 31, _mm512_srli_epi32( w0 , 19) );
}


/* we packed 512 14-bit values, touching 14 512-bit words, using 448 bytes */
static void avx512unpackblock14(const __m512i * compressed, uint32_t * pout) {
  /* we are going to access  14 512-bit words */
  __m512i w0, w1;
  __m512i * out = (__m512i *) pout;
  const __m512i mask = _mm512_set1_epi32(16383);
  w0 = _mm512_loadu_si512 (compressed);
  _mm512_storeu_si512(out + 0,  _mm512_and_si512 ( mask,  w0 ) );
  _mm512_storeu_si512(out + 1,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 14) ) );
  w1 = _mm512_loadu_si512 (compressed + 1);
  _mm512_storeu_si512(out + 2,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 28) ,_mm512_slli_epi32( w1 , 4 ) ) ) );
  _mm512_storeu_si512(out + 3,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 10) ) );
  w0 = _mm512_loadu_si512 (compressed + 2);
  _mm512_storeu_si512(out + 4,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 24) ,_mm512_slli_epi32( w0 , 8 ) ) ) );
  _mm512_storeu_si512(out + 5,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 6) ) );
  w1 = _mm512_loadu_si512 (compressed + 3);
  _mm512_storeu_si512(out + 6,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 20) ,_mm512_slli_epi32( w1 , 12 ) ) ) );
  _mm512_storeu_si512(out + 7,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 2) ) );
  _mm512_storeu_si512(out + 8,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 16) ) );
  w0 = _mm512_loadu_si512 (compressed + 4);
  _mm512_storeu_si512(out + 9,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 30) ,_mm512_slli_epi32( w0 , 2 ) ) ) );
  _mm512_storeu_si512(out + 10,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 12) ) );
  w1 = _mm512_loadu_si512 (compressed + 5);
  _mm512_storeu_si512(out + 11,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 26) ,_mm512_slli_epi32( w1 , 6 ) ) ) );
  _mm512_storeu_si512(out + 12,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 8) ) );
  w0 = _mm512_loadu_si512 (compressed + 6);
  _mm512_storeu_si512(out + 13,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 22) ,_mm512_slli_epi32( w0 , 10 ) ) ) );
  _mm512_storeu_si512(out + 14,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 4) ) );
  _mm512_storeu_si512(out + 15, _mm512_srli_epi32( w0 , 18) );
  w1 = _mm512_loadu_si512 (compressed + 7);
  _mm512_storeu_si512(out + 16,  _mm512_and_si512 ( mask,  w1 ) );
  _mm512_storeu_si512(out + 17,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 14) ) );
  w0 = _mm512_loadu_si512 (compressed + 8);
  _mm512_storeu_si512(out + 18,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 28) ,_mm512_slli_epi32( w0 , 4 ) ) ) );
  _mm512_storeu_si512(out + 19,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 10) ) );
  w1 = _mm512_loadu_si512 (compressed + 9);
  _mm512_storeu_si512(out + 20,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 24) ,_mm512_slli_epi32( w1 , 8 ) ) ) );
  _mm512_storeu_si512(out + 21,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 6) ) );
  w0 = _mm512_loadu_si512 (compressed + 10);
  _mm512_storeu_si512(out + 22,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 20) ,_mm512_slli_epi32( w0 , 12 ) ) ) );
  _mm512_storeu_si512(out + 23,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 2) ) );
  _mm512_storeu_si512(out + 24,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 16) ) );
  w1 = _mm512_loadu_si512 (compressed + 11);
  _mm512_storeu_si512(out + 25,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 30) ,_mm512_slli_epi32( w1 , 2 ) ) ) );
  _mm512_storeu_si512(out + 26,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 12) ) );
  w0 = _mm512_loadu_si512 (compressed + 12);
  _mm512_storeu_si512(out + 27,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 26) ,_mm512_slli_epi32( w0 , 6 ) ) ) );
  _mm512_storeu_si512(out + 28,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 8) ) );
  w1 = _mm512_loadu_si512 (compressed + 13);
  _mm512_storeu_si512(out + 29,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 22) ,_mm512_slli_epi32( w1 , 10 ) ) ) );
  _mm512_storeu_si512(out + 30,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 4) ) );
  _mm512_storeu_si512(out + 31, _mm512_srli_epi32( w1 , 18) );
}


/* we packed 512 15-bit values, touching 15 512-bit words, using 480 bytes */
static void avx512unpackblock15(const __m512i * compressed, uint32_t * pout) {
  /* we are going to access  15 512-bit words */
  __m512i w0, w1;
  __m512i * out = (__m512i *) pout;
  const __m512i mask = _mm512_set1_epi32(32767);
  w0 = _mm512_loadu_si512 (compressed);
  _mm512_storeu_si512(out + 0,  _mm512_and_si512 ( mask,  w0 ) );
  _mm512_storeu_si512(out + 1,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 15) ) );
  w1 = _mm512_loadu_si512 (compressed + 1);
  _mm512_storeu_si512(out + 2,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 30) ,_mm512_slli_epi32( w1 , 2 ) ) ) );
  _mm512_storeu_si512(out + 3,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 13) ) );
  w0 = _mm512_loadu_si512 (compressed + 2);
  _mm512_storeu_si512(out + 4,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 28) ,_mm512_slli_epi32( w0 , 4 ) ) ) );
  _mm512_storeu_si512(out + 5,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 11) ) );
  w1 = _mm512_loadu_si512 (compressed + 3);
  _mm512_storeu_si512(out + 6,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 26) ,_mm512_slli_epi32( w1 , 6 ) ) ) );
  _mm512_storeu_si512(out + 7,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 9) ) );
  w0 = _mm512_loadu_si512 (compressed + 4);
  _mm512_storeu_si512(out + 8,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 24) ,_mm512_slli_epi32( w0 , 8 ) ) ) );
  _mm512_storeu_si512(out + 9,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 7) ) );
  w1 = _mm512_loadu_si512 (compressed + 5);
  _mm512_storeu_si512(out + 10,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 22) ,_mm512_slli_epi32( w1 , 10 ) ) ) );
  _mm512_storeu_si512(out + 11,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 5) ) );
  w0 = _mm512_loadu_si512 (compressed + 6);
  _mm512_storeu_si512(out + 12,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 20) ,_mm512_slli_epi32( w0 , 12 ) ) ) );
  _mm512_storeu_si512(out + 13,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 3) ) );
  w1 = _mm512_loadu_si512 (compressed + 7);
  _mm512_storeu_si512(out + 14,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 18) ,_mm512_slli_epi32( w1 , 14 ) ) ) );
  _mm512_storeu_si512(out + 15,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 1) ) );
  _mm512_storeu_si512(out + 16,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 16) ) );
  w0 = _mm512_loadu_si512 (compressed + 8);
  _mm512_storeu_si512(out + 17,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 31) ,_mm512_slli_epi32( w0 , 1 ) ) ) );
  _mm512_storeu_si512(out + 18,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 14) ) );
  w1 = _mm512_loadu_si512 (compressed + 9);
  _mm512_storeu_si512(out + 19,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 29) ,_mm512_slli_epi32( w1 , 3 ) ) ) );
  _mm512_storeu_si512(out + 20,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 12) ) );
  w0 = _mm512_loadu_si512 (compressed + 10);
  _mm512_storeu_si512(out + 21,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 27) ,_mm512_slli_epi32( w0 , 5 ) ) ) );
  _mm512_storeu_si512(out + 22,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 10) ) );
  w1 = _mm512_loadu_si512 (compressed + 11);
  _mm512_storeu_si512(out + 23,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 25) ,_mm512_slli_epi32( w1 , 7 ) ) ) );
  _mm512_storeu_si512(out + 24,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 8) ) );
  w0 = _mm512_loadu_si512 (compressed + 12);
  _mm512_storeu_si512(out + 25,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 23) ,_mm512_slli_epi32( w0 , 9 ) ) ) );
  _mm512_storeu_si512(out + 26,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 6) ) );
  w1 = _mm512_loadu_si512 (compressed + 13);
  _mm512_storeu_si512(out + 27,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 21) ,_mm512_slli_epi32( w1 , 11 ) ) ) );
  _mm512_storeu_si512(out + 28,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 4) ) );
  w0 = _mm512_loadu_si512 (compressed + 14);
  _mm512_storeu_si512(out + 29,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 19) ,_mm512_slli_epi32( w0 , 13 ) ) ) );
  _mm512_storeu_si512(out + 30,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 2) ) );
  _mm512_storeu_si512(out + 31, _mm512_srli_epi32( w0 , 17) );
}


/* we packed 512 16-bit values, touching 16 512-bit words, using 512 bytes */
static void avx512unpackblock16(const __m512i * compressed, uint32_t * pout) {
  /* we are going to access  16 512-bit words */
  __m512i w0, w1;
  __m512i * out = (__m512i *) pout;
  const __m512i mask = _mm512_set1_epi32(65535);
  w0 = _mm512_loadu_si512 (compressed);
  _mm512_storeu_si512(out + 0,  _mm512_and_si512 ( mask,  w0 ) );
  _mm512_storeu_si512(out + 1, _mm512_srli_epi32( w0 , 16) );
  w1 = _mm512_loadu_si512 (compressed + 1);
  _mm512_storeu_si512(out + 2,  _mm512_and_si512 ( mask,  w1 ) );
  _mm512_storeu_si512(out + 3, _mm512_srli_epi32( w1 , 16) );
  w0 = _mm512_loadu_si512 (compressed + 2);
  _mm512_storeu_si512(out + 4,  _mm512_and_si512 ( mask,  w0 ) );
  _mm512_storeu_si512(out + 5, _mm512_srli_epi32( w0 , 16) );
  w1 = _mm512_loadu_si512 (compressed + 3);
  _mm512_storeu_si512(out + 6,  _mm512_and_si512 ( mask,  w1 ) );
  _mm512_storeu_si512(out + 7, _mm512_srli_epi32( w1 , 16) );
  w0 = _mm512_loadu_si512 (compressed + 4);
  _mm512_storeu_si512(out + 8,  _mm512_and_si512 ( mask,  w0 ) );
  _mm512_storeu_si512(out + 9, _mm512_srli_epi32( w0 , 16) );
  w1 = _mm512_loadu_si512 (compressed + 5);
  _mm512_storeu_si512(out + 10,  _mm512_and_si512 ( mask,  w1 ) );
  _mm512_storeu_si512(out + 11, _mm512_srli_epi32( w1 , 16) );
  w0 = _mm512_loadu_si512 (compressed + 6);
  _mm512_storeu_si512(out + 12,  _mm512_and_si512 ( mask,  w0 ) );
  _mm512_storeu_si512(out + 13, _mm512_srli_epi32( w0 , 16) );
  w1 = _mm512_loadu_si512 (compressed + 7);
  _mm512_storeu_si512(out + 14,  _mm512_and_si512 ( mask,  w1 ) );
  _mm512_storeu_si512(out + 15, _mm512_srli_epi32( w1 , 16) );
  w0 = _mm512_loadu_si512 (compressed + 8);
  _mm512_storeu_si512(out + 16,  _mm512_and_si512 ( mask,  w0 ) );
  _mm512_storeu_si512(out + 17, _mm512_srli_epi32( w0 , 16) );
  w1 = _mm512_loadu_si512 (compressed + 9);
  _mm512_storeu_si512(out + 18,  _mm512_and_si512 ( mask,  w1 ) );
  _mm512_storeu_si512(out + 19, _mm512_srli_epi32( w1 , 16) );
  w0 = _mm512_loadu_si512 (compressed + 10);
  _mm512_storeu_si512(out + 20,  _mm512_and_si512 ( mask,  w0 ) );
  _mm512_storeu_si512(out + 21, _mm512_srli_epi32( w0 , 16) );
  w1 = _mm512_loadu_si512 (compressed + 11);
  _mm512_storeu_si512(out + 22,  _mm512_and_si512 ( mask,  w1 ) );
  _mm512_storeu_si512(out + 23, _mm512_srli_epi32( w1 , 16) );
  w0 = _mm512_loadu_si512 (compressed + 12);
  _mm512_storeu_si512(out + 24,  _mm512_and_si512 ( mask,  w0 ) );
  _mm512_storeu_si512(out + 25, _mm512_srli_epi32( w0 , 16) );
  w1 = _mm512_loadu_si512 (compressed + 13);
  _mm512_storeu_si512(out + 26,  _mm512_and_si512 ( mask,  w1 ) );
  _mm512_storeu_si512(out + 27, _mm512_srli_epi32( w1 , 16) );
  w0 = _mm512_loadu_si512 (compressed + 14);
  _mm512_storeu_si512(out + 28,  _mm512_and_si512 ( mask,  w0 ) );
  _mm512_storeu_si512(out + 29, _mm512_srli_epi32( w0 , 16) );
  w1 = _mm512_loadu_si512 (compressed + 15);
  _mm512_storeu_si512(out + 30,  _mm512_and_si512 ( mask,  w1 ) );
  _mm512_storeu_si512(out + 31, _mm512_srli_epi32( w1 , 16) );
}


/* we packed 512 17-bit values, touching 17 512-bit words, using 544 bytes */
static void avx512unpackblock17(const __m512i * compressed, uint32_t * pout) {
  /* we are going to access  17 512-bit words */
  __m512i w0, w1;
  __m512i * out = (__m512i *) pout;
  const __m512i mask = _mm512_set1_epi32(131071);
  w0 = _mm512_loadu_si512 (compressed);
  _mm512_storeu_si512(out + 0,  _mm512_and_si512 ( mask,  w0 ) );
  w1 = _mm512_loadu_si512 (compressed + 1);
  _mm512_storeu_si512(out + 1,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 17) ,_mm512_slli_epi32( w1 , 15 ) ) ) );
  _mm512_storeu_si512(out + 2,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 2) ) );
  w0 = _mm512_loadu_si512 (compressed + 2);
  _mm512_storeu_si512(out + 3,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 19) ,_mm512_slli_epi32( w0 , 13 ) ) ) );
  _mm512_storeu_si512(out + 4,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 4) ) );
  w1 = _mm512_loadu_si512 (compressed + 3);
  _mm512_storeu_si512(out + 5,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 21) ,_mm512_slli_epi32( w1 , 11 ) ) ) );
  _mm512_storeu_si512(out + 6,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 6) ) );
  w0 = _mm512_loadu_si512 (compressed + 4);
  _mm512_storeu_si512(out + 7,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 23) ,_mm512_slli_epi32( w0 , 9 ) ) ) );
  _mm512_storeu_si512(out + 8,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 8) ) );
  w1 = _mm512_loadu_si512 (compressed + 5);
  _mm512_storeu_si512(out + 9,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 25) ,_mm512_slli_epi32( w1 , 7 ) ) ) );
  _mm512_storeu_si512(out + 10,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 10) ) );
  w0 = _mm512_loadu_si512 (compressed + 6);
  _mm512_storeu_si512(out + 11,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 27) ,_mm512_slli_epi32( w0 , 5 ) ) ) );
  _mm512_storeu_si512(out + 12,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 12) ) );
  w1 = _mm512_loadu_si512 (compressed + 7);
  _mm512_storeu_si512(out + 13,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 29) ,_mm512_slli_epi32( w1 , 3 ) ) ) );
  _mm512_storeu_si512(out + 14,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 14) ) );
  w0 = _mm512_loadu_si512 (compressed + 8);
  _mm512_storeu_si512(out + 15,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 31) ,_mm512_slli_epi32( w0 , 1 ) ) ) );
  w1 = _mm512_loadu_si512 (compressed + 9);
  _mm512_storeu_si512(out + 16,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 16) ,_mm512_slli_epi32( w1 , 16 ) ) ) );
  _mm512_storeu_si512(out + 17,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 1) ) );
  w0 = _mm512_loadu_si512 (compressed + 10);
  _mm512_storeu_si512(out + 18,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 18) ,_mm512_slli_epi32( w0 , 14 ) ) ) );
  _mm512_storeu_si512(out + 19,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 3) ) );
  w1 = _mm512_loadu_si512 (compressed + 11);
  _mm512_storeu_si512(out + 20,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 20) ,_mm512_slli_epi32( w1 , 12 ) ) ) );
  _mm512_storeu_si512(out + 21,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 5) ) );
  w0 = _mm512_loadu_si512 (compressed + 12);
  _mm512_storeu_si512(out + 22,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 22) ,_mm512_slli_epi32( w0 , 10 ) ) ) );
  _mm512_storeu_si512(out + 23,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 7) ) );
  w1 = _mm512_loadu_si512 (compressed + 13);
  _mm512_storeu_si512(out + 24,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 24) ,_mm512_slli_epi32( w1 , 8 ) ) ) );
  _mm512_storeu_si512(out + 25,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 9) ) );
  w0 = _mm512_loadu_si512 (compressed + 14);
  _mm512_storeu_si512(out + 26,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 26) ,_mm512_slli_epi32( w0 , 6 ) ) ) );
  _mm512_storeu_si512(out + 27,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 11) ) );
  w1 = _mm512_loadu_si512 (compressed + 15);
  _mm512_storeu_si512(out + 28,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 28) ,_mm512_slli_epi32( w1 , 4 ) ) ) );
  _mm512_storeu_si512(out + 29,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 13) ) );
  w0 = _mm512_loadu_si512 (compressed + 16);
  _mm512_storeu_si512(out + 30,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 30) ,_mm512_slli_epi32( w0 , 2 ) ) ) );
  _mm512_storeu_si512(out + 31, _mm512_srli_epi32( w0 , 15) );
}


/* we packed 512 18-bit values, touching 18 512-bit words, using 576 bytes */
static void avx512unpackblock18(const __m512i * compressed, uint32_t * pout) {
  /* we are going to access  18 512-bit words */
  __m512i w0, w1;
  __m512i * out = (__m512i *) pout;
  const __m512i mask = _mm512_set1_epi32(262143);
  w0 = _mm512_loadu_si512 (compressed);
  _mm512_storeu_si512(out + 0,  _mm512_and_si512 ( mask,  w0 ) );
  w1 = _mm512_loadu_si512 (compressed + 1);
  _mm512_storeu_si512(out + 1,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 18) ,_mm512_slli_epi32( w1 , 14 ) ) ) );
  _mm512_storeu_si512(out + 2,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 4) ) );
  w0 = _mm512_loadu_si512 (compressed + 2);
  _mm512_storeu_si512(out + 3,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 22) ,_mm512_slli_epi32( w0 , 10 ) ) ) );
  _mm512_storeu_si512(out + 4,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 8) ) );
  w1 = _mm512_loadu_si512 (compressed + 3);
  _mm512_storeu_si512(out + 5,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 26) ,_mm512_slli_epi32( w1 , 6 ) ) ) );
  _mm512_storeu_si512(out + 6,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 12) ) );
  w0 = _mm512_loadu_si512 (compressed + 4);
  _mm512_storeu_si512(out + 7,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 30) ,_mm512_slli_epi32( w0 , 2 ) ) ) );
  w1 = _mm512_loadu_si512 (compressed + 5);
  _mm512_storeu_si512(out + 8,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 16) ,_mm512_slli_epi32( w1 , 16 ) ) ) );
  _mm512_storeu_si512(out + 9,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 2) ) );
  w0 = _mm512_loadu_si512 (compressed + 6);
  _mm512_storeu_si512(out + 10,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 20) ,_mm512_slli_epi32( w0 , 12 ) ) ) );
  _mm512_storeu_si512(out + 11,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 6) ) );
  w1 = _mm512_loadu_si512 (compressed + 7);
  _mm512_storeu_si512(out + 12,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 24) ,_mm512_slli_epi32( w1 , 8 ) ) ) );
  _mm512_storeu_si512(out + 13,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 10) ) );
  w0 = _mm512_loadu_si512 (compressed + 8);
  _mm512_storeu_si512(out + 14,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 28) ,_mm512_slli_epi32( w0 , 4 ) ) ) );
  _mm512_storeu_si512(out + 15, _mm512_srli_epi32( w0 , 14) );
  w1 = _mm512_loadu_si512 (compressed + 9);
  _mm512_storeu_si512(out + 16,  _mm512_and_si512 ( mask,  w1 ) );
  w0 = _mm512_loadu_si512 (compressed + 10);
  _mm512_storeu_si512(out + 17,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 18) ,_mm512_slli_epi32( w0 , 14 ) ) ) );
  _mm512_storeu_si512(out + 18,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 4) ) );
  w1 = _mm512_loadu_si512 (compressed + 11);
  _mm512_storeu_si512(out + 19,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 22) ,_mm512_slli_epi32( w1 , 10 ) ) ) );
  _mm512_storeu_si512(out + 20,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 8) ) );
  w0 = _mm512_loadu_si512 (compressed + 12);
  _mm512_storeu_si512(out + 21,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 26) ,_mm512_slli_epi32( w0 , 6 ) ) ) );
  _mm512_storeu_si512(out + 22,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 12) ) );
  w1 = _mm512_loadu_si512 (compressed + 13);
  _mm512_storeu_si512(out + 23,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 30) ,_mm512_slli_epi32( w1 , 2 ) ) ) );
  w0 = _mm512_loadu_si512 (compressed + 14);
  _mm512_storeu_si512(out + 24,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 16) ,_mm512_slli_epi32( w0 , 16 ) ) ) );
  _mm512_storeu_si512(out + 25,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 2) ) );
  w1 = _mm512_loadu_si512 (compressed + 15);
  _mm512_storeu_si512(out + 26,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 20) ,_mm512_slli_epi32( w1 , 12 ) ) ) );
  _mm512_storeu_si512(out + 27,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 6) ) );
  w0 = _mm512_loadu_si512 (compressed + 16);
  _mm512_storeu_si512(out + 28,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 24) ,_mm512_slli_epi32( w0 , 8 ) ) ) );
  _mm512_storeu_si512(out + 29,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 10) ) );
  w1 = _mm512_loadu_si512 (compressed + 17);
  _mm512_storeu_si512(out + 30,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 28) ,_mm512_slli_epi32( w1 , 4 ) ) ) );
  _mm512_storeu_si512(out + 31, _mm512_srli_epi32( w1 , 14) );
}


/* we packed 512 19-bit values, touching 19 512-bit words, using 608 bytes */
static void avx512unpackblock19(const __m512i * compressed, uint32_t * pout) {
  /* we are going to access  19 512-bit words */
  __m512i w0, w1;
  __m512i * out = (__m512i *) pout;
  const __m512i mask = _mm512_set1_epi32(524287);
  w0 = _mm512_loadu_si512 (compressed);
  _mm512_storeu_si512(out + 0,  _mm512_and_si512 ( mask,  w0 ) );
  w1 = _mm512_loadu_si512 (compressed + 1);
  _mm512_storeu_si512(out + 1,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 19) ,_mm512_slli_epi32( w1 , 13 ) ) ) );
  _mm512_storeu_si512(out + 2,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 6) ) );
  w0 = _mm512_loadu_si512 (compressed + 2);
  _mm512_storeu_si512(out + 3,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 25) ,_mm512_slli_epi32( w0 , 7 ) ) ) );
  _mm512_storeu_si512(out + 4,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 12) ) );
  w1 = _mm512_loadu_si512 (compressed + 3);
  _mm512_storeu_si512(out + 5,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 31) ,_mm512_slli_epi32( w1 , 1 ) ) ) );
  w0 = _mm512_loadu_si512 (compressed + 4);
  _mm512_storeu_si512(out + 6,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 18) ,_mm512_slli_epi32( w0 , 14 ) ) ) );
  _mm512_storeu_si512(out + 7,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 5) ) );
  w1 = _mm512_loadu_si512 (compressed + 5);
  _mm512_storeu_si512(out + 8,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 24) ,_mm512_slli_epi32( w1 , 8 ) ) ) );
  _mm512_storeu_si512(out + 9,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 11) ) );
  w0 = _mm512_loadu_si512 (compressed + 6);
  _mm512_storeu_si512(out + 10,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 30) ,_mm512_slli_epi32( w0 , 2 ) ) ) );
  w1 = _mm512_loadu_si512 (compressed + 7);
  _mm512_storeu_si512(out + 11,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 17) ,_mm512_slli_epi32( w1 , 15 ) ) ) );
  _mm512_storeu_si512(out + 12,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 4) ) );
  w0 = _mm512_loadu_si512 (compressed + 8);
  _mm512_storeu_si512(out + 13,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 23) ,_mm512_slli_epi32( w0 , 9 ) ) ) );
  _mm512_storeu_si512(out + 14,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 10) ) );
  w1 = _mm512_loadu_si512 (compressed + 9);
  _mm512_storeu_si512(out + 15,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 29) ,_mm512_slli_epi32( w1 , 3 ) ) ) );
  w0 = _mm512_loadu_si512 (compressed + 10);
  _mm512_storeu_si512(out + 16,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 16) ,_mm512_slli_epi32( w0 , 16 ) ) ) );
  _mm512_storeu_si512(out + 17,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 3) ) );
  w1 = _mm512_loadu_si512 (compressed + 11);
  _mm512_storeu_si512(out + 18,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 22) ,_mm512_slli_epi32( w1 , 10 ) ) ) );
  _mm512_storeu_si512(out + 19,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 9) ) );
  w0 = _mm512_loadu_si512 (compressed + 12);
  _mm512_storeu_si512(out + 20,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 28) ,_mm512_slli_epi32( w0 , 4 ) ) ) );
  w1 = _mm512_loadu_si512 (compressed + 13);
  _mm512_storeu_si512(out + 21,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 15) ,_mm512_slli_epi32( w1 , 17 ) ) ) );
  _mm512_storeu_si512(out + 22,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 2) ) );
  w0 = _mm512_loadu_si512 (compressed + 14);
  _mm512_storeu_si512(out + 23,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 21) ,_mm512_slli_epi32( w0 , 11 ) ) ) );
  _mm512_storeu_si512(out + 24,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 8) ) );
  w1 = _mm512_loadu_si512 (compressed + 15);
  _mm512_storeu_si512(out + 25,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 27) ,_mm512_slli_epi32( w1 , 5 ) ) ) );
  w0 = _mm512_loadu_si512 (compressed + 16);
  _mm512_storeu_si512(out + 26,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 14) ,_mm512_slli_epi32( w0 , 18 ) ) ) );
  _mm512_storeu_si512(out + 27,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 1) ) );
  w1 = _mm512_loadu_si512 (compressed + 17);
  _mm512_storeu_si512(out + 28,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 20) ,_mm512_slli_epi32( w1 , 12 ) ) ) );
  _mm512_storeu_si512(out + 29,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 7) ) );
  w0 = _mm512_loadu_si512 (compressed + 18);
  _mm512_storeu_si512(out + 30,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 26) ,_mm512_slli_epi32( w0 , 6 ) ) ) );
  _mm512_storeu_si512(out + 31, _mm512_srli_epi32( w0 , 13) );
}


/* we packed 512 20-bit values, touching 20 512-bit words, using 640 bytes */
static void avx512unpackblock20(const __m512i * compressed, uint32_t * pout) {
  /* we are going to access  20 512-bit words */
  __m512i w0, w1;
  __m512i * out = (__m512i *) pout;
  const __m512i mask = _mm512_set1_epi32(1048575);
  w0 = _mm512_loadu_si512 (compressed);
  _mm512_storeu_si512(out + 0,  _mm512_and_si512 ( mask,  w0 ) );
  w1 = _mm512_loadu_si512 (compressed + 1);
  _mm512_storeu_si512(out + 1,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 20) ,_mm512_slli_epi32( w1 , 12 ) ) ) );
  _mm512_storeu_si512(out + 2,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 8) ) );
  w0 = _mm512_loadu_si512 (compressed + 2);
  _mm512_storeu_si512(out + 3,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 28) ,_mm512_slli_epi32( w0 , 4 ) ) ) );
  w1 = _mm512_loadu_si512 (compressed + 3);
  _mm512_storeu_si512(out + 4,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 16) ,_mm512_slli_epi32( w1 , 16 ) ) ) );
  _mm512_storeu_si512(out + 5,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 4) ) );
  w0 = _mm512_loadu_si512 (compressed + 4);
  _mm512_storeu_si512(out + 6,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 24) ,_mm512_slli_epi32( w0 , 8 ) ) ) );
  _mm512_storeu_si512(out + 7, _mm512_srli_epi32( w0 , 12) );
  w1 = _mm512_loadu_si512 (compressed + 5);
  _mm512_storeu_si512(out + 8,  _mm512_and_si512 ( mask,  w1 ) );
  w0 = _mm512_loadu_si512 (compressed + 6);
  _mm512_storeu_si512(out + 9,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 20) ,_mm512_slli_epi32( w0 , 12 ) ) ) );
  _mm512_storeu_si512(out + 10,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 8) ) );
  w1 = _mm512_loadu_si512 (compressed + 7);
  _mm512_storeu_si512(out + 11,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 28) ,_mm512_slli_epi32( w1 , 4 ) ) ) );
  w0 = _mm512_loadu_si512 (compressed + 8);
  _mm512_storeu_si512(out + 12,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 16) ,_mm512_slli_epi32( w0 , 16 ) ) ) );
  _mm512_storeu_si512(out + 13,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 4) ) );
  w1 = _mm512_loadu_si512 (compressed + 9);
  _mm512_storeu_si512(out + 14,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 24) ,_mm512_slli_epi32( w1 , 8 ) ) ) );
  _mm512_storeu_si512(out + 15, _mm512_srli_epi32( w1 , 12) );
  w0 = _mm512_loadu_si512 (compressed + 10);
  _mm512_storeu_si512(out + 16,  _mm512_and_si512 ( mask,  w0 ) );
  w1 = _mm512_loadu_si512 (compressed + 11);
  _mm512_storeu_si512(out + 17,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 20) ,_mm512_slli_epi32( w1 , 12 ) ) ) );
  _mm512_storeu_si512(out + 18,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 8) ) );
  w0 = _mm512_loadu_si512 (compressed + 12);
  _mm512_storeu_si512(out + 19,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 28) ,_mm512_slli_epi32( w0 , 4 ) ) ) );
  w1 = _mm512_loadu_si512 (compressed + 13);
  _mm512_storeu_si512(out + 20,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 16) ,_mm512_slli_epi32( w1 , 16 ) ) ) );
  _mm512_storeu_si512(out + 21,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 4) ) );
  w0 = _mm512_loadu_si512 (compressed + 14);
  _mm512_storeu_si512(out + 22,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 24) ,_mm512_slli_epi32( w0 , 8 ) ) ) );
  _mm512_storeu_si512(out + 23, _mm512_srli_epi32( w0 , 12) );
  w1 = _mm512_loadu_si512 (compressed + 15);
  _mm512_storeu_si512(out + 24,  _mm512_and_si512 ( mask,  w1 ) );
  w0 = _mm512_loadu_si512 (compressed + 16);
  _mm512_storeu_si512(out + 25,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 20) ,_mm512_slli_epi32( w0 , 12 ) ) ) );
  _mm512_storeu_si512(out + 26,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 8) ) );
  w1 = _mm512_loadu_si512 (compressed + 17);
  _mm512_storeu_si512(out + 27,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 28) ,_mm512_slli_epi32( w1 , 4 ) ) ) );
  w0 = _mm512_loadu_si512 (compressed + 18);
  _mm512_storeu_si512(out + 28,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 16) ,_mm512_slli_epi32( w0 , 16 ) ) ) );
  _mm512_storeu_si512(out + 29,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 4) ) );
  w1 = _mm512_loadu_si512 (compressed + 19);
  _mm512_storeu_si512(out + 30,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 24) ,_mm512_slli_epi32( w1 , 8 ) ) ) );
  _mm512_storeu_si512(out + 31, _mm512_srli_epi32( w1 , 12) );
}


/* we packed 512 21-bit values, touching 21 512-bit words, using 672 bytes */
static void avx512unpackblock21(const __m512i * compressed, uint32_t * pout) {
  /* we are going to access  21 512-bit words */
  __m512i w0, w1;
  __m512i * out = (__m512i *) pout;
  const __m512i mask = _mm512_set1_epi32(2097151);
  w0 = _mm512_loadu_si512 (compressed);
  _mm512_storeu_si512(out + 0,  _mm512_and_si512 ( mask,  w0 ) );
  w1 = _mm512_loadu_si512 (compressed + 1);
  _mm512_storeu_si512(out + 1,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 21) ,_mm512_slli_epi32( w1 , 11 ) ) ) );
  _mm512_storeu_si512(out + 2,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 10) ) );
  w0 = _mm512_loadu_si512 (compressed + 2);
  _mm512_storeu_si512(out + 3,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 31) ,_mm512_slli_epi32( w0 , 1 ) ) ) );
  w1 = _mm512_loadu_si512 (compressed + 3);
  _mm512_storeu_si512(out + 4,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 20) ,_mm512_slli_epi32( w1 , 12 ) ) ) );
  _mm512_storeu_si512(out + 5,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 9) ) );
  w0 = _mm512_loadu_si512 (compressed + 4);
  _mm512_storeu_si512(out + 6,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 30) ,_mm512_slli_epi32( w0 , 2 ) ) ) );
  w1 = _mm512_loadu_si512 (compressed + 5);
  _mm512_storeu_si512(out + 7,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 19) ,_mm512_slli_epi32( w1 , 13 ) ) ) );
  _mm512_storeu_si512(out + 8,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 8) ) );
  w0 = _mm512_loadu_si512 (compressed + 6);
  _mm512_storeu_si512(out + 9,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 29) ,_mm512_slli_epi32( w0 , 3 ) ) ) );
  w1 = _mm512_loadu_si512 (compressed + 7);
  _mm512_storeu_si512(out + 10,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 18) ,_mm512_slli_epi32( w1 , 14 ) ) ) );
  _mm512_storeu_si512(out + 11,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 7) ) );
  w0 = _mm512_loadu_si512 (compressed + 8);
  _mm512_storeu_si512(out + 12,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 28) ,_mm512_slli_epi32( w0 , 4 ) ) ) );
  w1 = _mm512_loadu_si512 (compressed + 9);
  _mm512_storeu_si512(out + 13,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 17) ,_mm512_slli_epi32( w1 , 15 ) ) ) );
  _mm512_storeu_si512(out + 14,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 6) ) );
  w0 = _mm512_loadu_si512 (compressed + 10);
  _mm512_storeu_si512(out + 15,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 27) ,_mm512_slli_epi32( w0 , 5 ) ) ) );
  w1 = _mm512_loadu_si512 (compressed + 11);
  _mm512_storeu_si512(out + 16,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 16) ,_mm512_slli_epi32( w1 , 16 ) ) ) );
  _mm512_storeu_si512(out + 17,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 5) ) );
  w0 = _mm512_loadu_si512 (compressed + 12);
  _mm512_storeu_si512(out + 18,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 26) ,_mm512_slli_epi32( w0 , 6 ) ) ) );
  w1 = _mm512_loadu_si512 (compressed + 13);
  _mm512_storeu_si512(out + 19,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 15) ,_mm512_slli_epi32( w1 , 17 ) ) ) );
  _mm512_storeu_si512(out + 20,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 4) ) );
  w0 = _mm512_loadu_si512 (compressed + 14);
  _mm512_storeu_si512(out + 21,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 25) ,_mm512_slli_epi32( w0 , 7 ) ) ) );
  w1 = _mm512_loadu_si512 (compressed + 15);
  _mm512_storeu_si512(out + 22,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 14) ,_mm512_slli_epi32( w1 , 18 ) ) ) );
  _mm512_storeu_si512(out + 23,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 3) ) );
  w0 = _mm512_loadu_si512 (compressed + 16);
  _mm512_storeu_si512(out + 24,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 24) ,_mm512_slli_epi32( w0 , 8 ) ) ) );
  w1 = _mm512_loadu_si512 (compressed + 17);
  _mm512_storeu_si512(out + 25,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 13) ,_mm512_slli_epi32( w1 , 19 ) ) ) );
  _mm512_storeu_si512(out + 26,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 2) ) );
  w0 = _mm512_loadu_si512 (compressed + 18);
  _mm512_storeu_si512(out + 27,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 23) ,_mm512_slli_epi32( w0 , 9 ) ) ) );
  w1 = _mm512_loadu_si512 (compressed + 19);
  _mm512_storeu_si512(out + 28,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 12) ,_mm512_slli_epi32( w1 , 20 ) ) ) );
  _mm512_storeu_si512(out + 29,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 1) ) );
  w0 = _mm512_loadu_si512 (compressed + 20);
  _mm512_storeu_si512(out + 30,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 22) ,_mm512_slli_epi32( w0 , 10 ) ) ) );
  _mm512_storeu_si512(out + 31, _mm512_srli_epi32( w0 , 11) );
}


/* we packed 512 22-bit values, touching 22 512-bit words, using 704 bytes */
static void avx512unpackblock22(const __m512i * compressed, uint32_t * pout) {
  /* we are going to access  22 512-bit words */
  __m512i w0, w1;
  __m512i * out = (__m512i *) pout;
  const __m512i mask = _mm512_set1_epi32(4194303);
  w0 = _mm512_loadu_si512 (compressed);
  _mm512_storeu_si512(out + 0,  _mm512_and_si512 ( mask,  w0 ) );
  w1 = _mm512_loadu_si512 (compressed + 1);
  _mm512_storeu_si512(out + 1,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 22) ,_mm512_slli_epi32( w1 , 10 ) ) ) );
  w0 = _mm512_loadu_si512 (compressed + 2);
  _mm512_storeu_si512(out + 2,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 12) ,_mm512_slli_epi32( w0 , 20 ) ) ) );
  _mm512_storeu_si512(out + 3,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 2) ) );
  w1 = _mm512_loadu_si512 (compressed + 3);
  _mm512_storeu_si512(out + 4,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 24) ,_mm512_slli_epi32( w1 , 8 ) ) ) );
  w0 = _mm512_loadu_si512 (compressed + 4);
  _mm512_storeu_si512(out + 5,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 14) ,_mm512_slli_epi32( w0 , 18 ) ) ) );
  _mm512_storeu_si512(out + 6,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 4) ) );
  w1 = _mm512_loadu_si512 (compressed + 5);
  _mm512_storeu_si512(out + 7,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 26) ,_mm512_slli_epi32( w1 , 6 ) ) ) );
  w0 = _mm512_loadu_si512 (compressed + 6);
  _mm512_storeu_si512(out + 8,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 16) ,_mm512_slli_epi32( w0 , 16 ) ) ) );
  _mm512_storeu_si512(out + 9,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 6) ) );
  w1 = _mm512_loadu_si512 (compressed + 7);
  _mm512_storeu_si512(out + 10,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 28) ,_mm512_slli_epi32( w1 , 4 ) ) ) );
  w0 = _mm512_loadu_si512 (compressed + 8);
  _mm512_storeu_si512(out + 11,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 18) ,_mm512_slli_epi32( w0 , 14 ) ) ) );
  _mm512_storeu_si512(out + 12,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 8) ) );
  w1 = _mm512_loadu_si512 (compressed + 9);
  _mm512_storeu_si512(out + 13,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 30) ,_mm512_slli_epi32( w1 , 2 ) ) ) );
  w0 = _mm512_loadu_si512 (compressed + 10);
  _mm512_storeu_si512(out + 14,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 20) ,_mm512_slli_epi32( w0 , 12 ) ) ) );
  _mm512_storeu_si512(out + 15, _mm512_srli_epi32( w0 , 10) );
  w1 = _mm512_loadu_si512 (compressed + 11);
  _mm512_storeu_si512(out + 16,  _mm512_and_si512 ( mask,  w1 ) );
  w0 = _mm512_loadu_si512 (compressed + 12);
  _mm512_storeu_si512(out + 17,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 22) ,_mm512_slli_epi32( w0 , 10 ) ) ) );
  w1 = _mm512_loadu_si512 (compressed + 13);
  _mm512_storeu_si512(out + 18,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 12) ,_mm512_slli_epi32( w1 , 20 ) ) ) );
  _mm512_storeu_si512(out + 19,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 2) ) );
  w0 = _mm512_loadu_si512 (compressed + 14);
  _mm512_storeu_si512(out + 20,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 24) ,_mm512_slli_epi32( w0 , 8 ) ) ) );
  w1 = _mm512_loadu_si512 (compressed + 15);
  _mm512_storeu_si512(out + 21,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 14) ,_mm512_slli_epi32( w1 , 18 ) ) ) );
  _mm512_storeu_si512(out + 22,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 4) ) );
  w0 = _mm512_loadu_si512 (compressed + 16);
  _mm512_storeu_si512(out + 23,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 26) ,_mm512_slli_epi32( w0 , 6 ) ) ) );
  w1 = _mm512_loadu_si512 (compressed + 17);
  _mm512_storeu_si512(out + 24,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 16) ,_mm512_slli_epi32( w1 , 16 ) ) ) );
  _mm512_storeu_si512(out + 25,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 6) ) );
  w0 = _mm512_loadu_si512 (compressed + 18);
  _mm512_storeu_si512(out + 26,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 28) ,_mm512_slli_epi32( w0 , 4 ) ) ) );
  w1 = _mm512_loadu_si512 (compressed + 19);
  _mm512_storeu_si512(out + 27,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 18) ,_mm512_slli_epi32( w1 , 14 ) ) ) );
  _mm512_storeu_si512(out + 28,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 8) ) );
  w0 = _mm512_loadu_si512 (compressed + 20);
  _mm512_storeu_si512(out + 29,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 30) ,_mm512_slli_epi32( w0 , 2 ) ) ) );
  w1 = _mm512_loadu_si512 (compressed + 21);
  _mm512_storeu_si512(out + 30,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 20) ,_mm512_slli_epi32( w1 , 12 ) ) ) );
  _mm512_storeu_si512(out + 31, _mm512_srli_epi32( w1 , 10) );
}


/* we packed 512 23-bit values, touching 23 512-bit words, using 736 bytes */
static void avx512unpackblock23(const __m512i * compressed, uint32_t * pout) {
  /* we are going to access  23 512-bit words */
  __m512i w0, w1;
  __m512i * out = (__m512i *) pout;
  const __m512i mask = _mm512_set1_epi32(8388607);
  w0 = _mm512_loadu_si512 (compressed);
  _mm512_storeu_si512(out + 0,  _mm512_and_si512 ( mask,  w0 ) );
  w1 = _mm512_loadu_si512 (compressed + 1);
  _mm512_storeu_si512(out + 1,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 23) ,_mm512_slli_epi32( w1 , 9 ) ) ) );
  w0 = _mm512_loadu_si512 (compressed + 2);
  _mm512_storeu_si512(out + 2,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 14) ,_mm512_slli_epi32( w0 , 18 ) ) ) );
  _mm512_storeu_si512(out + 3,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 5) ) );
  w1 = _mm512_loadu_si512 (compressed + 3);
  _mm512_storeu_si512(out + 4,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 28) ,_mm512_slli_epi32( w1 , 4 ) ) ) );
  w0 = _mm512_loadu_si512 (compressed + 4);
  _mm512_storeu_si512(out + 5,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 19) ,_mm512_slli_epi32( w0 , 13 ) ) ) );
  w1 = _mm512_loadu_si512 (compressed + 5);
  _mm512_storeu_si512(out + 6,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 10) ,_mm512_slli_epi32( w1 , 22 ) ) ) );
  _mm512_storeu_si512(out + 7,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 1) ) );
  w0 = _mm512_loadu_si512 (compressed + 6);
  _mm512_storeu_si512(out + 8,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 24) ,_mm512_slli_epi32( w0 , 8 ) ) ) );
  w1 = _mm512_loadu_si512 (compressed + 7);
  _mm512_storeu_si512(out + 9,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 15) ,_mm512_slli_epi32( w1 , 17 ) ) ) );
  _mm512_storeu_si512(out + 10,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 6) ) );
  w0 = _mm512_loadu_si512 (compressed + 8);
  _mm512_storeu_si512(out + 11,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 29) ,_mm512_slli_epi32( w0 , 3 ) ) ) );
  w1 = _mm512_loadu_si512 (compressed + 9);
  _mm512_storeu_si512(out + 12,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 20) ,_mm512_slli_epi32( w1 , 12 ) ) ) );
  w0 = _mm512_loadu_si512 (compressed + 10);
  _mm512_storeu_si512(out + 13,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 11) ,_mm512_slli_epi32( w0 , 21 ) ) ) );
  _mm512_storeu_si512(out + 14,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 2) ) );
  w1 = _mm512_loadu_si512 (compressed + 11);
  _mm512_storeu_si512(out + 15,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 25) ,_mm512_slli_epi32( w1 , 7 ) ) ) );
  w0 = _mm512_loadu_si512 (compressed + 12);
  _mm512_storeu_si512(out + 16,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 16) ,_mm512_slli_epi32( w0 , 16 ) ) ) );
  _mm512_storeu_si512(out + 17,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 7) ) );
  w1 = _mm512_loadu_si512 (compressed + 13);
  _mm512_storeu_si512(out + 18,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 30) ,_mm512_slli_epi32( w1 , 2 ) ) ) );
  w0 = _mm512_loadu_si512 (compressed + 14);
  _mm512_storeu_si512(out + 19,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 21) ,_mm512_slli_epi32( w0 , 11 ) ) ) );
  w1 = _mm512_loadu_si512 (compressed + 15);
  _mm512_storeu_si512(out + 20,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 12) ,_mm512_slli_epi32( w1 , 20 ) ) ) );
  _mm512_storeu_si512(out + 21,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 3) ) );
  w0 = _mm512_loadu_si512 (compressed + 16);
  _mm512_storeu_si512(out + 22,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 26) ,_mm512_slli_epi32( w0 , 6 ) ) ) );
  w1 = _mm512_loadu_si512 (compressed + 17);
  _mm512_storeu_si512(out + 23,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 17) ,_mm512_slli_epi32( w1 , 15 ) ) ) );
  _mm512_storeu_si512(out + 24,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 8) ) );
  w0 = _mm512_loadu_si512 (compressed + 18);
  _mm512_storeu_si512(out + 25,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 31) ,_mm512_slli_epi32( w0 , 1 ) ) ) );
  w1 = _mm512_loadu_si512 (compressed + 19);
  _mm512_storeu_si512(out + 26,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 22) ,_mm512_slli_epi32( w1 , 10 ) ) ) );
  w0 = _mm512_loadu_si512 (compressed + 20);
  _mm512_storeu_si512(out + 27,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 13) ,_mm512_slli_epi32( w0 , 19 ) ) ) );
  _mm512_storeu_si512(out + 28,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 4) ) );
  w1 = _mm512_loadu_si512 (compressed + 21);
  _mm512_storeu_si512(out + 29,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 27) ,_mm512_slli_epi32( w1 , 5 ) ) ) );
  w0 = _mm512_loadu_si512 (compressed + 22);
  _mm512_storeu_si512(out + 30,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 18) ,_mm512_slli_epi32( w0 , 14 ) ) ) );
  _mm512_storeu_si512(out + 31, _mm512_srli_epi32( w0 , 9) );
}


/* we packed 512 24-bit values, touching 24 512-bit words, using 768 bytes */
static void avx512unpackblock24(const __m512i * compressed, uint32_t * pout) {
  /* we are going to access  24 512-bit words */
  __m512i w0, w1;
  __m512i * out = (__m512i *) pout;
  const __m512i mask = _mm512_set1_epi32(16777215);
  w0 = _mm512_loadu_si512 (compressed);
  _mm512_storeu_si512(out + 0,  _mm512_and_si512 ( mask,  w0 ) );
  w1 = _mm512_loadu_si512 (compressed + 1);
  _mm512_storeu_si512(out + 1,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 24) ,_mm512_slli_epi32( w1 , 8 ) ) ) );
  w0 = _mm512_loadu_si512 (compressed + 2);
  _mm512_storeu_si512(out + 2,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 16) ,_mm512_slli_epi32( w0 , 16 ) ) ) );
  _mm512_storeu_si512(out + 3, _mm512_srli_epi32( w0 , 8) );
  w1 = _mm512_loadu_si512 (compressed + 3);
  _mm512_storeu_si512(out + 4,  _mm512_and_si512 ( mask,  w1 ) );
  w0 = _mm512_loadu_si512 (compressed + 4);
  _mm512_storeu_si512(out + 5,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 24) ,_mm512_slli_epi32( w0 , 8 ) ) ) );
  w1 = _mm512_loadu_si512 (compressed + 5);
  _mm512_storeu_si512(out + 6,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 16) ,_mm512_slli_epi32( w1 , 16 ) ) ) );
  _mm512_storeu_si512(out + 7, _mm512_srli_epi32( w1 , 8) );
  w0 = _mm512_loadu_si512 (compressed + 6);
  _mm512_storeu_si512(out + 8,  _mm512_and_si512 ( mask,  w0 ) );
  w1 = _mm512_loadu_si512 (compressed + 7);
  _mm512_storeu_si512(out + 9,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 24) ,_mm512_slli_epi32( w1 , 8 ) ) ) );
  w0 = _mm512_loadu_si512 (compressed + 8);
  _mm512_storeu_si512(out + 10,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 16) ,_mm512_slli_epi32( w0 , 16 ) ) ) );
  _mm512_storeu_si512(out + 11, _mm512_srli_epi32( w0 , 8) );
  w1 = _mm512_loadu_si512 (compressed + 9);
  _mm512_storeu_si512(out + 12,  _mm512_and_si512 ( mask,  w1 ) );
  w0 = _mm512_loadu_si512 (compressed + 10);
  _mm512_storeu_si512(out + 13,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 24) ,_mm512_slli_epi32( w0 , 8 ) ) ) );
  w1 = _mm512_loadu_si512 (compressed + 11);
  _mm512_storeu_si512(out + 14,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 16) ,_mm512_slli_epi32( w1 , 16 ) ) ) );
  _mm512_storeu_si512(out + 15, _mm512_srli_epi32( w1 , 8) );
  w0 = _mm512_loadu_si512 (compressed + 12);
  _mm512_storeu_si512(out + 16,  _mm512_and_si512 ( mask,  w0 ) );
  w1 = _mm512_loadu_si512 (compressed + 13);
  _mm512_storeu_si512(out + 17,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 24) ,_mm512_slli_epi32( w1 , 8 ) ) ) );
  w0 = _mm512_loadu_si512 (compressed + 14);
  _mm512_storeu_si512(out + 18,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 16) ,_mm512_slli_epi32( w0 , 16 ) ) ) );
  _mm512_storeu_si512(out + 19, _mm512_srli_epi32( w0 , 8) );
  w1 = _mm512_loadu_si512 (compressed + 15);
  _mm512_storeu_si512(out + 20,  _mm512_and_si512 ( mask,  w1 ) );
  w0 = _mm512_loadu_si512 (compressed + 16);
  _mm512_storeu_si512(out + 21,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 24) ,_mm512_slli_epi32( w0 , 8 ) ) ) );
  w1 = _mm512_loadu_si512 (compressed + 17);
  _mm512_storeu_si512(out + 22,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 16) ,_mm512_slli_epi32( w1 , 16 ) ) ) );
  _mm512_storeu_si512(out + 23, _mm512_srli_epi32( w1 , 8) );
  w0 = _mm512_loadu_si512 (compressed + 18);
  _mm512_storeu_si512(out + 24,  _mm512_and_si512 ( mask,  w0 ) );
  w1 = _mm512_loadu_si512 (compressed + 19);
  _mm512_storeu_si512(out + 25,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 24) ,_mm512_slli_epi32( w1 , 8 ) ) ) );
  w0 = _mm512_loadu_si512 (compressed + 20);
  _mm512_storeu_si512(out + 26,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 16) ,_mm512_slli_epi32( w0 , 16 ) ) ) );
  _mm512_storeu_si512(out + 27, _mm512_srli_epi32( w0 , 8) );
  w1 = _mm512_loadu_si512 (compressed + 21);
  _mm512_storeu_si512(out + 28,  _mm512_and_si512 ( mask,  w1 ) );
  w0 = _mm512_loadu_si512 (compressed + 22);
  _mm512_storeu_si512(out + 29,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 24) ,_mm512_slli_epi32( w0 , 8 ) ) ) );
  w1 = _mm512_loadu_si512 (compressed + 23);
  _mm512_storeu_si512(out + 30,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 16) ,_mm512_slli_epi32( w1 , 16 ) ) ) );
  _mm512_storeu_si512(out + 31, _mm512_srli_epi32( w1 , 8) );
}


/* we packed 512 25-bit values, touching 25 512-bit words, using 800 bytes */
static void avx512unpackblock25(const __m512i * compressed, uint32_t * pout) {
  /* we are going to access  25 512-bit words */
  __m512i w0, w1;
  __m512i * out = (__m512i *) pout;
  const __m512i mask = _mm512_set1_epi32(33554431);
  w0 = _mm512_loadu_si512 (compressed);
  _mm512_storeu_si512(out + 0,  _mm512_and_si512 ( mask,  w0 ) );
  w1 = _mm512_loadu_si512 (compressed + 1);
  _mm512_storeu_si512(out + 1,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 25) ,_mm512_slli_epi32( w1 , 7 ) ) ) );
  w0 = _mm512_loadu_si512 (compressed + 2);
  _mm512_storeu_si512(out + 2,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 18) ,_mm512_slli_epi32( w0 , 14 ) ) ) );
  w1 = _mm512_loadu_si512 (compressed + 3);
  _mm512_storeu_si512(out + 3,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 11) ,_mm512_slli_epi32( w1 , 21 ) ) ) );
  _mm512_storeu_si512(out + 4,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 4) ) );
  w0 = _mm512_loadu_si512 (compressed + 4);
  _mm512_storeu_si512(out + 5,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 29) ,_mm512_slli_epi32( w0 , 3 ) ) ) );
  w1 = _mm512_loadu_si512 (compressed + 5);
  _mm512_storeu_si512(out + 6,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 22) ,_mm512_slli_epi32( w1 , 10 ) ) ) );
  w0 = _mm512_loadu_si512 (compressed + 6);
  _mm512_storeu_si512(out + 7,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 15) ,_mm512_slli_epi32( w0 , 17 ) ) ) );
  w1 = _mm512_loadu_si512 (compressed + 7);
  _mm512_storeu_si512(out + 8,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 8) ,_mm512_slli_epi32( w1 , 24 ) ) ) );
  _mm512_storeu_si512(out + 9,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 1) ) );
  w0 = _mm512_loadu_si512 (compressed + 8);
  _mm512_storeu_si512(out + 10,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 26) ,_mm512_slli_epi32( w0 , 6 ) ) ) );
  w1 = _mm512_loadu_si512 (compressed + 9);
  _mm512_storeu_si512(out + 11,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 19) ,_mm512_slli_epi32( w1 , 13 ) ) ) );
  w0 = _mm512_loadu_si512 (compressed + 10);
  _mm512_storeu_si512(out + 12,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 12) ,_mm512_slli_epi32( w0 , 20 ) ) ) );
  _mm512_storeu_si512(out + 13,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 5) ) );
  w1 = _mm512_loadu_si512 (compressed + 11);
  _mm512_storeu_si512(out + 14,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 30) ,_mm512_slli_epi32( w1 , 2 ) ) ) );
  w0 = _mm512_loadu_si512 (compressed + 12);
  _mm512_storeu_si512(out + 15,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 23) ,_mm512_slli_epi32( w0 , 9 ) ) ) );
  w1 = _mm512_loadu_si512 (compressed + 13);
  _mm512_storeu_si512(out + 16,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 16) ,_mm512_slli_epi32( w1 , 16 ) ) ) );
  w0 = _mm512_loadu_si512 (compressed + 14);
  _mm512_storeu_si512(out + 17,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 9) ,_mm512_slli_epi32( w0 , 23 ) ) ) );
  _mm512_storeu_si512(out + 18,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 2) ) );
  w1 = _mm512_loadu_si512 (compressed + 15);
  _mm512_storeu_si512(out + 19,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 27) ,_mm512_slli_epi32( w1 , 5 ) ) ) );
  w0 = _mm512_loadu_si512 (compressed + 16);
  _mm512_storeu_si512(out + 20,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 20) ,_mm512_slli_epi32( w0 , 12 ) ) ) );
  w1 = _mm512_loadu_si512 (compressed + 17);
  _mm512_storeu_si512(out + 21,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 13) ,_mm512_slli_epi32( w1 , 19 ) ) ) );
  _mm512_storeu_si512(out + 22,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 6) ) );
  w0 = _mm512_loadu_si512 (compressed + 18);
  _mm512_storeu_si512(out + 23,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 31) ,_mm512_slli_epi32( w0 , 1 ) ) ) );
  w1 = _mm512_loadu_si512 (compressed + 19);
  _mm512_storeu_si512(out + 24,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 24) ,_mm512_slli_epi32( w1 , 8 ) ) ) );
  w0 = _mm512_loadu_si512 (compressed + 20);
  _mm512_storeu_si512(out + 25,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 17) ,_mm512_slli_epi32( w0 , 15 ) ) ) );
  w1 = _mm512_loadu_si512 (compressed + 21);
  _mm512_storeu_si512(out + 26,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 10) ,_mm512_slli_epi32( w1 , 22 ) ) ) );
  _mm512_storeu_si512(out + 27,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 3) ) );
  w0 = _mm512_loadu_si512 (compressed + 22);
  _mm512_storeu_si512(out + 28,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 28) ,_mm512_slli_epi32( w0 , 4 ) ) ) );
  w1 = _mm512_loadu_si512 (compressed + 23);
  _mm512_storeu_si512(out + 29,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 21) ,_mm512_slli_epi32( w1 , 11 ) ) ) );
  w0 = _mm512_loadu_si512 (compressed + 24);
  _mm512_storeu_si512(out + 30,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 14) ,_mm512_slli_epi32( w0 , 18 ) ) ) );
  _mm512_storeu_si512(out + 31, _mm512_srli_epi32( w0 , 7) );
}


/* we packed 512 26-bit values, touching 26 512-bit words, using 832 bytes */
static void avx512unpackblock26(const __m512i * compressed, uint32_t * pout) {
  /* we are going to access  26 512-bit words */
  __m512i w0, w1;
  __m512i * out = (__m512i *) pout;
  const __m512i mask = _mm512_set1_epi32(67108863);
  w0 = _mm512_loadu_si512 (compressed);
  _mm512_storeu_si512(out + 0,  _mm512_and_si512 ( mask,  w0 ) );
  w1 = _mm512_loadu_si512 (compressed + 1);
  _mm512_storeu_si512(out + 1,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 26) ,_mm512_slli_epi32( w1 , 6 ) ) ) );
  w0 = _mm512_loadu_si512 (compressed + 2);
  _mm512_storeu_si512(out + 2,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 20) ,_mm512_slli_epi32( w0 , 12 ) ) ) );
  w1 = _mm512_loadu_si512 (compressed + 3);
  _mm512_storeu_si512(out + 3,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 14) ,_mm512_slli_epi32( w1 , 18 ) ) ) );
  w0 = _mm512_loadu_si512 (compressed + 4);
  _mm512_storeu_si512(out + 4,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 8) ,_mm512_slli_epi32( w0 , 24 ) ) ) );
  _mm512_storeu_si512(out + 5,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 2) ) );
  w1 = _mm512_loadu_si512 (compressed + 5);
  _mm512_storeu_si512(out + 6,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 28) ,_mm512_slli_epi32( w1 , 4 ) ) ) );
  w0 = _mm512_loadu_si512 (compressed + 6);
  _mm512_storeu_si512(out + 7,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 22) ,_mm512_slli_epi32( w0 , 10 ) ) ) );
  w1 = _mm512_loadu_si512 (compressed + 7);
  _mm512_storeu_si512(out + 8,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 16) ,_mm512_slli_epi32( w1 , 16 ) ) ) );
  w0 = _mm512_loadu_si512 (compressed + 8);
  _mm512_storeu_si512(out + 9,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 10) ,_mm512_slli_epi32( w0 , 22 ) ) ) );
  _mm512_storeu_si512(out + 10,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 4) ) );
  w1 = _mm512_loadu_si512 (compressed + 9);
  _mm512_storeu_si512(out + 11,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 30) ,_mm512_slli_epi32( w1 , 2 ) ) ) );
  w0 = _mm512_loadu_si512 (compressed + 10);
  _mm512_storeu_si512(out + 12,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 24) ,_mm512_slli_epi32( w0 , 8 ) ) ) );
  w1 = _mm512_loadu_si512 (compressed + 11);
  _mm512_storeu_si512(out + 13,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 18) ,_mm512_slli_epi32( w1 , 14 ) ) ) );
  w0 = _mm512_loadu_si512 (compressed + 12);
  _mm512_storeu_si512(out + 14,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 12) ,_mm512_slli_epi32( w0 , 20 ) ) ) );
  _mm512_storeu_si512(out + 15, _mm512_srli_epi32( w0 , 6) );
  w1 = _mm512_loadu_si512 (compressed + 13);
  _mm512_storeu_si512(out + 16,  _mm512_and_si512 ( mask,  w1 ) );
  w0 = _mm512_loadu_si512 (compressed + 14);
  _mm512_storeu_si512(out + 17,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 26) ,_mm512_slli_epi32( w0 , 6 ) ) ) );
  w1 = _mm512_loadu_si512 (compressed + 15);
  _mm512_storeu_si512(out + 18,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 20) ,_mm512_slli_epi32( w1 , 12 ) ) ) );
  w0 = _mm512_loadu_si512 (compressed + 16);
  _mm512_storeu_si512(out + 19,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 14) ,_mm512_slli_epi32( w0 , 18 ) ) ) );
  w1 = _mm512_loadu_si512 (compressed + 17);
  _mm512_storeu_si512(out + 20,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 8) ,_mm512_slli_epi32( w1 , 24 ) ) ) );
  _mm512_storeu_si512(out + 21,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 2) ) );
  w0 = _mm512_loadu_si512 (compressed + 18);
  _mm512_storeu_si512(out + 22,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 28) ,_mm512_slli_epi32( w0 , 4 ) ) ) );
  w1 = _mm512_loadu_si512 (compressed + 19);
  _mm512_storeu_si512(out + 23,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 22) ,_mm512_slli_epi32( w1 , 10 ) ) ) );
  w0 = _mm512_loadu_si512 (compressed + 20);
  _mm512_storeu_si512(out + 24,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 16) ,_mm512_slli_epi32( w0 , 16 ) ) ) );
  w1 = _mm512_loadu_si512 (compressed + 21);
  _mm512_storeu_si512(out + 25,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 10) ,_mm512_slli_epi32( w1 , 22 ) ) ) );
  _mm512_storeu_si512(out + 26,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 4) ) );
  w0 = _mm512_loadu_si512 (compressed + 22);
  _mm512_storeu_si512(out + 27,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 30) ,_mm512_slli_epi32( w0 , 2 ) ) ) );
  w1 = _mm512_loadu_si512 (compressed + 23);
  _mm512_storeu_si512(out + 28,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 24) ,_mm512_slli_epi32( w1 , 8 ) ) ) );
  w0 = _mm512_loadu_si512 (compressed + 24);
  _mm512_storeu_si512(out + 29,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 18) ,_mm512_slli_epi32( w0 , 14 ) ) ) );
  w1 = _mm512_loadu_si512 (compressed + 25);
  _mm512_storeu_si512(out + 30,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 12) ,_mm512_slli_epi32( w1 , 20 ) ) ) );
  _mm512_storeu_si512(out + 31, _mm512_srli_epi32( w1 , 6) );
}


/* we packed 512 27-bit values, touching 27 512-bit words, using 864 bytes */
static void avx512unpackblock27(const __m512i * compressed, uint32_t * pout) {
  /* we are going to access  27 512-bit words */
  __m512i w0, w1;
  __m512i * out = (__m512i *) pout;
  const __m512i mask = _mm512_set1_epi32(134217727);
  w0 = _mm512_loadu_si512 (compressed);
  _mm512_storeu_si512(out + 0,  _mm512_and_si512 ( mask,  w0 ) );
  w1 = _mm512_loadu_si512 (compressed + 1);
  _mm512_storeu_si512(out + 1,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 27) ,_mm512_slli_epi32( w1 , 5 ) ) ) );
  w0 = _mm512_loadu_si512 (compressed + 2);
  _mm512_storeu_si512(out + 2,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 22) ,_mm512_slli_epi32( w0 , 10 ) ) ) );
  w1 = _mm512_loadu_si512 (compressed + 3);
  _mm512_storeu_si512(out + 3,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 17) ,_mm512_slli_epi32( w1 , 15 ) ) ) );
  w0 = _mm512_loadu_si512 (compressed + 4);
  _mm512_storeu_si512(out + 4,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 12) ,_mm512_slli_epi32( w0 , 20 ) ) ) );
  w1 = _mm512_loadu_si512 (compressed + 5);
  _mm512_storeu_si512(out + 5,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 7) ,_mm512_slli_epi32( w1 , 25 ) ) ) );
  _mm512_storeu_si512(out + 6,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 2) ) );
  w0 = _mm512_loadu_si512 (compressed + 6);
  _mm512_storeu_si512(out + 7,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 29) ,_mm512_slli_epi32( w0 , 3 ) ) ) );
  w1 = _mm512_loadu_si512 (compressed + 7);
  _mm512_storeu_si512(out + 8,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 24) ,_mm512_slli_epi32( w1 , 8 ) ) ) );
  w0 = _mm512_loadu_si512 (compressed + 8);
  _mm512_storeu_si512(out + 9,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 19) ,_mm512_slli_epi32( w0 , 13 ) ) ) );
  w1 = _mm512_loadu_si512 (compressed + 9);
  _mm512_storeu_si512(out + 10,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 14) ,_mm512_slli_epi32( w1 , 18 ) ) ) );
  w0 = _mm512_loadu_si512 (compressed + 10);
  _mm512_storeu_si512(out + 11,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 9) ,_mm512_slli_epi32( w0 , 23 ) ) ) );
  _mm512_storeu_si512(out + 12,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 4) ) );
  w1 = _mm512_loadu_si512 (compressed + 11);
  _mm512_storeu_si512(out + 13,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 31) ,_mm512_slli_epi32( w1 , 1 ) ) ) );
  w0 = _mm512_loadu_si512 (compressed + 12);
  _mm512_storeu_si512(out + 14,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 26) ,_mm512_slli_epi32( w0 , 6 ) ) ) );
  w1 = _mm512_loadu_si512 (compressed + 13);
  _mm512_storeu_si512(out + 15,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 21) ,_mm512_slli_epi32( w1 , 11 ) ) ) );
  w0 = _mm512_loadu_si512 (compressed + 14);
  _mm512_storeu_si512(out + 16,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 16) ,_mm512_slli_epi32( w0 , 16 ) ) ) );
  w1 = _mm512_loadu_si512 (compressed + 15);
  _mm512_storeu_si512(out + 17,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 11) ,_mm512_slli_epi32( w1 , 21 ) ) ) );
  w0 = _mm512_loadu_si512 (compressed + 16);
  _mm512_storeu_si512(out + 18,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 6) ,_mm512_slli_epi32( w0 , 26 ) ) ) );
  _mm512_storeu_si512(out + 19,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w0 , 1) ) );
  w1 = _mm512_loadu_si512 (compressed + 17);
  _mm512_storeu_si512(out + 20,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 28) ,_mm512_slli_epi32( w1 , 4 ) ) ) );
  w0 = _mm512_loadu_si512 (compressed + 18);
  _mm512_storeu_si512(out + 21,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 23) ,_mm512_slli_epi32( w0 , 9 ) ) ) );
  w1 = _mm512_loadu_si512 (compressed + 19);
  _mm512_storeu_si512(out + 22,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 18) ,_mm512_slli_epi32( w1 , 14 ) ) ) );
  w0 = _mm512_loadu_si512 (compressed + 20);
  _mm512_storeu_si512(out + 23,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 13) ,_mm512_slli_epi32( w0 , 19 ) ) ) );
  w1 = _mm512_loadu_si512 (compressed + 21);
  _mm512_storeu_si512(out + 24,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 8) ,_mm512_slli_epi32( w1 , 24 ) ) ) );
  _mm512_storeu_si512(out + 25,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 3) ) );
  w0 = _mm512_loadu_si512 (compressed + 22);
  _mm512_storeu_si512(out + 26,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 30) ,_mm512_slli_epi32( w0 , 2 ) ) ) );
  w1 = _mm512_loadu_si512 (compressed + 23);
  _mm512_storeu_si512(out + 27,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 25) ,_mm512_slli_epi32( w1 , 7 ) ) ) );
  w0 = _mm512_loadu_si512 (compressed + 24);
  _mm512_storeu_si512(out + 28,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 20) ,_mm512_slli_epi32( w0 , 12 ) ) ) );
  w1 = _mm512_loadu_si512 (compressed + 25);
  _mm512_storeu_si512(out + 29,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 15) ,_mm512_slli_epi32( w1 , 17 ) ) ) );
  w0 = _mm512_loadu_si512 (compressed + 26);
  _mm512_storeu_si512(out + 30,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 10) ,_mm512_slli_epi32( w0 , 22 ) ) ) );
  _mm512_storeu_si512(out + 31, _mm512_srli_epi32( w0 , 5) );
}


/* we packed 512 28-bit values, touching 28 512-bit words, using 896 bytes */
static void avx512unpackblock28(const __m512i * compressed, uint32_t * pout) {
  /* we are going to access  28 512-bit words */
  __m512i w0, w1;
  __m512i * out = (__m512i *) pout;
  const __m512i mask = _mm512_set1_epi32(268435455);
  w0 = _mm512_loadu_si512 (compressed);
  _mm512_storeu_si512(out + 0,  _mm512_and_si512 ( mask,  w0 ) );
  w1 = _mm512_loadu_si512 (compressed + 1);
  _mm512_storeu_si512(out + 1,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 28) ,_mm512_slli_epi32( w1 , 4 ) ) ) );
  w0 = _mm512_loadu_si512 (compressed + 2);
  _mm512_storeu_si512(out + 2,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 24) ,_mm512_slli_epi32( w0 , 8 ) ) ) );
  w1 = _mm512_loadu_si512 (compressed + 3);
  _mm512_storeu_si512(out + 3,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 20) ,_mm512_slli_epi32( w1 , 12 ) ) ) );
  w0 = _mm512_loadu_si512 (compressed + 4);
  _mm512_storeu_si512(out + 4,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 16) ,_mm512_slli_epi32( w0 , 16 ) ) ) );
  w1 = _mm512_loadu_si512 (compressed + 5);
  _mm512_storeu_si512(out + 5,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 12) ,_mm512_slli_epi32( w1 , 20 ) ) ) );
  w0 = _mm512_loadu_si512 (compressed + 6);
  _mm512_storeu_si512(out + 6,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 8) ,_mm512_slli_epi32( w0 , 24 ) ) ) );
  _mm512_storeu_si512(out + 7, _mm512_srli_epi32( w0 , 4) );
  w1 = _mm512_loadu_si512 (compressed + 7);
  _mm512_storeu_si512(out + 8,  _mm512_and_si512 ( mask,  w1 ) );
  w0 = _mm512_loadu_si512 (compressed + 8);
  _mm512_storeu_si512(out + 9,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 28) ,_mm512_slli_epi32( w0 , 4 ) ) ) );
  w1 = _mm512_loadu_si512 (compressed + 9);
  _mm512_storeu_si512(out + 10,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 24) ,_mm512_slli_epi32( w1 , 8 ) ) ) );
  w0 = _mm512_loadu_si512 (compressed + 10);
  _mm512_storeu_si512(out + 11,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 20) ,_mm512_slli_epi32( w0 , 12 ) ) ) );
  w1 = _mm512_loadu_si512 (compressed + 11);
  _mm512_storeu_si512(out + 12,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 16) ,_mm512_slli_epi32( w1 , 16 ) ) ) );
  w0 = _mm512_loadu_si512 (compressed + 12);
  _mm512_storeu_si512(out + 13,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 12) ,_mm512_slli_epi32( w0 , 20 ) ) ) );
  w1 = _mm512_loadu_si512 (compressed + 13);
  _mm512_storeu_si512(out + 14,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 8) ,_mm512_slli_epi32( w1 , 24 ) ) ) );
  _mm512_storeu_si512(out + 15, _mm512_srli_epi32( w1 , 4) );
  w0 = _mm512_loadu_si512 (compressed + 14);
  _mm512_storeu_si512(out + 16,  _mm512_and_si512 ( mask,  w0 ) );
  w1 = _mm512_loadu_si512 (compressed + 15);
  _mm512_storeu_si512(out + 17,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 28) ,_mm512_slli_epi32( w1 , 4 ) ) ) );
  w0 = _mm512_loadu_si512 (compressed + 16);
  _mm512_storeu_si512(out + 18,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 24) ,_mm512_slli_epi32( w0 , 8 ) ) ) );
  w1 = _mm512_loadu_si512 (compressed + 17);
  _mm512_storeu_si512(out + 19,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 20) ,_mm512_slli_epi32( w1 , 12 ) ) ) );
  w0 = _mm512_loadu_si512 (compressed + 18);
  _mm512_storeu_si512(out + 20,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 16) ,_mm512_slli_epi32( w0 , 16 ) ) ) );
  w1 = _mm512_loadu_si512 (compressed + 19);
  _mm512_storeu_si512(out + 21,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 12) ,_mm512_slli_epi32( w1 , 20 ) ) ) );
  w0 = _mm512_loadu_si512 (compressed + 20);
  _mm512_storeu_si512(out + 22,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 8) ,_mm512_slli_epi32( w0 , 24 ) ) ) );
  _mm512_storeu_si512(out + 23, _mm512_srli_epi32( w0 , 4) );
  w1 = _mm512_loadu_si512 (compressed + 21);
  _mm512_storeu_si512(out + 24,  _mm512_and_si512 ( mask,  w1 ) );
  w0 = _mm512_loadu_si512 (compressed + 22);
  _mm512_storeu_si512(out + 25,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 28) ,_mm512_slli_epi32( w0 , 4 ) ) ) );
  w1 = _mm512_loadu_si512 (compressed + 23);
  _mm512_storeu_si512(out + 26,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 24) ,_mm512_slli_epi32( w1 , 8 ) ) ) );
  w0 = _mm512_loadu_si512 (compressed + 24);
  _mm512_storeu_si512(out + 27,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 20) ,_mm512_slli_epi32( w0 , 12 ) ) ) );
  w1 = _mm512_loadu_si512 (compressed + 25);
  _mm512_storeu_si512(out + 28,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 16) ,_mm512_slli_epi32( w1 , 16 ) ) ) );
  w0 = _mm512_loadu_si512 (compressed + 26);
  _mm512_storeu_si512(out + 29,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 12) ,_mm512_slli_epi32( w0 , 20 ) ) ) );
  w1 = _mm512_loadu_si512 (compressed + 27);
  _mm512_storeu_si512(out + 30,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 8) ,_mm512_slli_epi32( w1 , 24 ) ) ) );
  _mm512_storeu_si512(out + 31, _mm512_srli_epi32( w1 , 4) );
}


/* we packed 512 29-bit values, touching 29 512-bit words, using 928 bytes */
static void avx512unpackblock29(const __m512i * compressed, uint32_t * pout) {
  /* we are going to access  29 512-bit words */
  __m512i w0, w1;
  __m512i * out = (__m512i *) pout;
  const __m512i mask = _mm512_set1_epi32(536870911);
  w0 = _mm512_loadu_si512 (compressed);
  _mm512_storeu_si512(out + 0,  _mm512_and_si512 ( mask,  w0 ) );
  w1 = _mm512_loadu_si512 (compressed + 1);
  _mm512_storeu_si512(out + 1,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 29) ,_mm512_slli_epi32( w1 , 3 ) ) ) );
  w0 = _mm512_loadu_si512 (compressed + 2);
  _mm512_storeu_si512(out + 2,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 26) ,_mm512_slli_epi32( w0 , 6 ) ) ) );
  w1 = _mm512_loadu_si512 (compressed + 3);
  _mm512_storeu_si512(out + 3,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 23) ,_mm512_slli_epi32( w1 , 9 ) ) ) );
  w0 = _mm512_loadu_si512 (compressed + 4);
  _mm512_storeu_si512(out + 4,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 20) ,_mm512_slli_epi32( w0 , 12 ) ) ) );
  w1 = _mm512_loadu_si512 (compressed + 5);
  _mm512_storeu_si512(out + 5,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 17) ,_mm512_slli_epi32( w1 , 15 ) ) ) );
  w0 = _mm512_loadu_si512 (compressed + 6);
  _mm512_storeu_si512(out + 6,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 14) ,_mm512_slli_epi32( w0 , 18 ) ) ) );
  w1 = _mm512_loadu_si512 (compressed + 7);
  _mm512_storeu_si512(out + 7,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 11) ,_mm512_slli_epi32( w1 , 21 ) ) ) );
  w0 = _mm512_loadu_si512 (compressed + 8);
  _mm512_storeu_si512(out + 8,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 8) ,_mm512_slli_epi32( w0 , 24 ) ) ) );
  w1 = _mm512_loadu_si512 (compressed + 9);
  _mm512_storeu_si512(out + 9,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 5) ,_mm512_slli_epi32( w1 , 27 ) ) ) );
  _mm512_storeu_si512(out + 10,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 2) ) );
  w0 = _mm512_loadu_si512 (compressed + 10);
  _mm512_storeu_si512(out + 11,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 31) ,_mm512_slli_epi32( w0 , 1 ) ) ) );
  w1 = _mm512_loadu_si512 (compressed + 11);
  _mm512_storeu_si512(out + 12,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 28) ,_mm512_slli_epi32( w1 , 4 ) ) ) );
  w0 = _mm512_loadu_si512 (compressed + 12);
  _mm512_storeu_si512(out + 13,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 25) ,_mm512_slli_epi32( w0 , 7 ) ) ) );
  w1 = _mm512_loadu_si512 (compressed + 13);
  _mm512_storeu_si512(out + 14,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 22) ,_mm512_slli_epi32( w1 , 10 ) ) ) );
  w0 = _mm512_loadu_si512 (compressed + 14);
  _mm512_storeu_si512(out + 15,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 19) ,_mm512_slli_epi32( w0 , 13 ) ) ) );
  w1 = _mm512_loadu_si512 (compressed + 15);
  _mm512_storeu_si512(out + 16,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 16) ,_mm512_slli_epi32( w1 , 16 ) ) ) );
  w0 = _mm512_loadu_si512 (compressed + 16);
  _mm512_storeu_si512(out + 17,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 13) ,_mm512_slli_epi32( w0 , 19 ) ) ) );
  w1 = _mm512_loadu_si512 (compressed + 17);
  _mm512_storeu_si512(out + 18,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 10) ,_mm512_slli_epi32( w1 , 22 ) ) ) );
  w0 = _mm512_loadu_si512 (compressed + 18);
  _mm512_storeu_si512(out + 19,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 7) ,_mm512_slli_epi32( w0 , 25 ) ) ) );
  w1 = _mm512_loadu_si512 (compressed + 19);
  _mm512_storeu_si512(out + 20,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 4) ,_mm512_slli_epi32( w1 , 28 ) ) ) );
  _mm512_storeu_si512(out + 21,  _mm512_and_si512 ( mask, _mm512_srli_epi32( w1 , 1) ) );
  w0 = _mm512_loadu_si512 (compressed + 20);
  _mm512_storeu_si512(out + 22,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 30) ,_mm512_slli_epi32( w0 , 2 ) ) ) );
  w1 = _mm512_loadu_si512 (compressed + 21);
  _mm512_storeu_si512(out + 23,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 27) ,_mm512_slli_epi32( w1 , 5 ) ) ) );
  w0 = _mm512_loadu_si512 (compressed + 22);
  _mm512_storeu_si512(out + 24,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 24) ,_mm512_slli_epi32( w0 , 8 ) ) ) );
  w1 = _mm512_loadu_si512 (compressed + 23);
  _mm512_storeu_si512(out + 25,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 21) ,_mm512_slli_epi32( w1 , 11 ) ) ) );
  w0 = _mm512_loadu_si512 (compressed + 24);
  _mm512_storeu_si512(out + 26,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 18) ,_mm512_slli_epi32( w0 , 14 ) ) ) );
  w1 = _mm512_loadu_si512 (compressed + 25);
  _mm512_storeu_si512(out + 27,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 15) ,_mm512_slli_epi32( w1 , 17 ) ) ) );
  w0 = _mm512_loadu_si512 (compressed + 26);
  _mm512_storeu_si512(out + 28,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 12) ,_mm512_slli_epi32( w0 , 20 ) ) ) );
  w1 = _mm512_loadu_si512 (compressed + 27);
  _mm512_storeu_si512(out + 29,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 9) ,_mm512_slli_epi32( w1 , 23 ) ) ) );
  w0 = _mm512_loadu_si512 (compressed + 28);
  _mm512_storeu_si512(out + 30,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 6) ,_mm512_slli_epi32( w0 , 26 ) ) ) );
  _mm512_storeu_si512(out + 31, _mm512_srli_epi32( w0 , 3) );
}


/* we packed 512 30-bit values, touching 30 512-bit words, using 960 bytes */
static void avx512unpackblock30(const __m512i * compressed, uint32_t * pout) {
  /* we are going to access  30 512-bit words */
  __m512i w0, w1;
  __m512i * out = (__m512i *) pout;
  const __m512i mask = _mm512_set1_epi32(1073741823);
  w0 = _mm512_loadu_si512 (compressed);
  _mm512_storeu_si512(out + 0,  _mm512_and_si512 ( mask,  w0 ) );
  w1 = _mm512_loadu_si512 (compressed + 1);
  _mm512_storeu_si512(out + 1,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 30) ,_mm512_slli_epi32( w1 , 2 ) ) ) );
  w0 = _mm512_loadu_si512 (compressed + 2);
  _mm512_storeu_si512(out + 2,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 28) ,_mm512_slli_epi32( w0 , 4 ) ) ) );
  w1 = _mm512_loadu_si512 (compressed + 3);
  _mm512_storeu_si512(out + 3,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 26) ,_mm512_slli_epi32( w1 , 6 ) ) ) );
  w0 = _mm512_loadu_si512 (compressed + 4);
  _mm512_storeu_si512(out + 4,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 24) ,_mm512_slli_epi32( w0 , 8 ) ) ) );
  w1 = _mm512_loadu_si512 (compressed + 5);
  _mm512_storeu_si512(out + 5,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 22) ,_mm512_slli_epi32( w1 , 10 ) ) ) );
  w0 = _mm512_loadu_si512 (compressed + 6);
  _mm512_storeu_si512(out + 6,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 20) ,_mm512_slli_epi32( w0 , 12 ) ) ) );
  w1 = _mm512_loadu_si512 (compressed + 7);
  _mm512_storeu_si512(out + 7,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 18) ,_mm512_slli_epi32( w1 , 14 ) ) ) );
  w0 = _mm512_loadu_si512 (compressed + 8);
  _mm512_storeu_si512(out + 8,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 16) ,_mm512_slli_epi32( w0 , 16 ) ) ) );
  w1 = _mm512_loadu_si512 (compressed + 9);
  _mm512_storeu_si512(out + 9,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 14) ,_mm512_slli_epi32( w1 , 18 ) ) ) );
  w0 = _mm512_loadu_si512 (compressed + 10);
  _mm512_storeu_si512(out + 10,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 12) ,_mm512_slli_epi32( w0 , 20 ) ) ) );
  w1 = _mm512_loadu_si512 (compressed + 11);
  _mm512_storeu_si512(out + 11,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 10) ,_mm512_slli_epi32( w1 , 22 ) ) ) );
  w0 = _mm512_loadu_si512 (compressed + 12);
  _mm512_storeu_si512(out + 12,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 8) ,_mm512_slli_epi32( w0 , 24 ) ) ) );
  w1 = _mm512_loadu_si512 (compressed + 13);
  _mm512_storeu_si512(out + 13,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 6) ,_mm512_slli_epi32( w1 , 26 ) ) ) );
  w0 = _mm512_loadu_si512 (compressed + 14);
  _mm512_storeu_si512(out + 14,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 4) ,_mm512_slli_epi32( w0 , 28 ) ) ) );
  _mm512_storeu_si512(out + 15, _mm512_srli_epi32( w0 , 2) );
  w1 = _mm512_loadu_si512 (compressed + 15);
  _mm512_storeu_si512(out + 16,  _mm512_and_si512 ( mask,  w1 ) );
  w0 = _mm512_loadu_si512 (compressed + 16);
  _mm512_storeu_si512(out + 17,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 30) ,_mm512_slli_epi32( w0 , 2 ) ) ) );
  w1 = _mm512_loadu_si512 (compressed + 17);
  _mm512_storeu_si512(out + 18,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 28) ,_mm512_slli_epi32( w1 , 4 ) ) ) );
  w0 = _mm512_loadu_si512 (compressed + 18);
  _mm512_storeu_si512(out + 19,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 26) ,_mm512_slli_epi32( w0 , 6 ) ) ) );
  w1 = _mm512_loadu_si512 (compressed + 19);
  _mm512_storeu_si512(out + 20,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 24) ,_mm512_slli_epi32( w1 , 8 ) ) ) );
  w0 = _mm512_loadu_si512 (compressed + 20);
  _mm512_storeu_si512(out + 21,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 22) ,_mm512_slli_epi32( w0 , 10 ) ) ) );
  w1 = _mm512_loadu_si512 (compressed + 21);
  _mm512_storeu_si512(out + 22,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 20) ,_mm512_slli_epi32( w1 , 12 ) ) ) );
  w0 = _mm512_loadu_si512 (compressed + 22);
  _mm512_storeu_si512(out + 23,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 18) ,_mm512_slli_epi32( w0 , 14 ) ) ) );
  w1 = _mm512_loadu_si512 (compressed + 23);
  _mm512_storeu_si512(out + 24,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 16) ,_mm512_slli_epi32( w1 , 16 ) ) ) );
  w0 = _mm512_loadu_si512 (compressed + 24);
  _mm512_storeu_si512(out + 25,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 14) ,_mm512_slli_epi32( w0 , 18 ) ) ) );
  w1 = _mm512_loadu_si512 (compressed + 25);
  _mm512_storeu_si512(out + 26,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 12) ,_mm512_slli_epi32( w1 , 20 ) ) ) );
  w0 = _mm512_loadu_si512 (compressed + 26);
  _mm512_storeu_si512(out + 27,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 10) ,_mm512_slli_epi32( w0 , 22 ) ) ) );
  w1 = _mm512_loadu_si512 (compressed + 27);
  _mm512_storeu_si512(out + 28,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 8) ,_mm512_slli_epi32( w1 , 24 ) ) ) );
  w0 = _mm512_loadu_si512 (compressed + 28);
  _mm512_storeu_si512(out + 29,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 6) ,_mm512_slli_epi32( w0 , 26 ) ) ) );
  w1 = _mm512_loadu_si512 (compressed + 29);
  _mm512_storeu_si512(out + 30,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 4) ,_mm512_slli_epi32( w1 , 28 ) ) ) );
  _mm512_storeu_si512(out + 31, _mm512_srli_epi32( w1 , 2) );
}


/* we packed 512 31-bit values, touching 31 512-bit words, using 992 bytes */
static void avx512unpackblock31(const __m512i * compressed, uint32_t * pout) {
  /* we are going to access  31 512-bit words */
  __m512i w0, w1;
  __m512i * out = (__m512i *) pout;
  const __m512i mask = _mm512_set1_epi32(2147483647);
  w0 = _mm512_loadu_si512 (compressed);
  _mm512_storeu_si512(out + 0,  _mm512_and_si512 ( mask,  w0 ) );
  w1 = _mm512_loadu_si512 (compressed + 1);
  _mm512_storeu_si512(out + 1,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 31) ,_mm512_slli_epi32( w1 , 1 ) ) ) );
  w0 = _mm512_loadu_si512 (compressed + 2);
  _mm512_storeu_si512(out + 2,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 30) ,_mm512_slli_epi32( w0 , 2 ) ) ) );
  w1 = _mm512_loadu_si512 (compressed + 3);
  _mm512_storeu_si512(out + 3,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 29) ,_mm512_slli_epi32( w1 , 3 ) ) ) );
  w0 = _mm512_loadu_si512 (compressed + 4);
  _mm512_storeu_si512(out + 4,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 28) ,_mm512_slli_epi32( w0 , 4 ) ) ) );
  w1 = _mm512_loadu_si512 (compressed + 5);
  _mm512_storeu_si512(out + 5,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 27) ,_mm512_slli_epi32( w1 , 5 ) ) ) );
  w0 = _mm512_loadu_si512 (compressed + 6);
  _mm512_storeu_si512(out + 6,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 26) ,_mm512_slli_epi32( w0 , 6 ) ) ) );
  w1 = _mm512_loadu_si512 (compressed + 7);
  _mm512_storeu_si512(out + 7,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 25) ,_mm512_slli_epi32( w1 , 7 ) ) ) );
  w0 = _mm512_loadu_si512 (compressed + 8);
  _mm512_storeu_si512(out + 8,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 24) ,_mm512_slli_epi32( w0 , 8 ) ) ) );
  w1 = _mm512_loadu_si512 (compressed + 9);
  _mm512_storeu_si512(out + 9,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 23) ,_mm512_slli_epi32( w1 , 9 ) ) ) );
  w0 = _mm512_loadu_si512 (compressed + 10);
  _mm512_storeu_si512(out + 10,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 22) ,_mm512_slli_epi32( w0 , 10 ) ) ) );
  w1 = _mm512_loadu_si512 (compressed + 11);
  _mm512_storeu_si512(out + 11,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 21) ,_mm512_slli_epi32( w1 , 11 ) ) ) );
  w0 = _mm512_loadu_si512 (compressed + 12);
  _mm512_storeu_si512(out + 12,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 20) ,_mm512_slli_epi32( w0 , 12 ) ) ) );
  w1 = _mm512_loadu_si512 (compressed + 13);
  _mm512_storeu_si512(out + 13,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 19) ,_mm512_slli_epi32( w1 , 13 ) ) ) );
  w0 = _mm512_loadu_si512 (compressed + 14);
  _mm512_storeu_si512(out + 14,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 18) ,_mm512_slli_epi32( w0 , 14 ) ) ) );
  w1 = _mm512_loadu_si512 (compressed + 15);
  _mm512_storeu_si512(out + 15,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 17) ,_mm512_slli_epi32( w1 , 15 ) ) ) );
  w0 = _mm512_loadu_si512 (compressed + 16);
  _mm512_storeu_si512(out + 16,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 16) ,_mm512_slli_epi32( w0 , 16 ) ) ) );
  w1 = _mm512_loadu_si512 (compressed + 17);
  _mm512_storeu_si512(out + 17,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 15) ,_mm512_slli_epi32( w1 , 17 ) ) ) );
  w0 = _mm512_loadu_si512 (compressed + 18);
  _mm512_storeu_si512(out + 18,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 14) ,_mm512_slli_epi32( w0 , 18 ) ) ) );
  w1 = _mm512_loadu_si512 (compressed + 19);
  _mm512_storeu_si512(out + 19,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 13) ,_mm512_slli_epi32( w1 , 19 ) ) ) );
  w0 = _mm512_loadu_si512 (compressed + 20);
  _mm512_storeu_si512(out + 20,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 12) ,_mm512_slli_epi32( w0 , 20 ) ) ) );
  w1 = _mm512_loadu_si512 (compressed + 21);
  _mm512_storeu_si512(out + 21,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 11) ,_mm512_slli_epi32( w1 , 21 ) ) ) );
  w0 = _mm512_loadu_si512 (compressed + 22);
  _mm512_storeu_si512(out + 22,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 10) ,_mm512_slli_epi32( w0 , 22 ) ) ) );
  w1 = _mm512_loadu_si512 (compressed + 23);
  _mm512_storeu_si512(out + 23,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 9) ,_mm512_slli_epi32( w1 , 23 ) ) ) );
  w0 = _mm512_loadu_si512 (compressed + 24);
  _mm512_storeu_si512(out + 24,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 8) ,_mm512_slli_epi32( w0 , 24 ) ) ) );
  w1 = _mm512_loadu_si512 (compressed + 25);
  _mm512_storeu_si512(out + 25,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 7) ,_mm512_slli_epi32( w1 , 25 ) ) ) );
  w0 = _mm512_loadu_si512 (compressed + 26);
  _mm512_storeu_si512(out + 26,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 6) ,_mm512_slli_epi32( w0 , 26 ) ) ) );
  w1 = _mm512_loadu_si512 (compressed + 27);
  _mm512_storeu_si512(out + 27,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 5) ,_mm512_slli_epi32( w1 , 27 ) ) ) );
  w0 = _mm512_loadu_si512 (compressed + 28);
  _mm512_storeu_si512(out + 28,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 4) ,_mm512_slli_epi32( w0 , 28 ) ) ) );
  w1 = _mm512_loadu_si512 (compressed + 29);
  _mm512_storeu_si512(out + 29,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w0 , 3) ,_mm512_slli_epi32( w1 , 29 ) ) ) );
  w0 = _mm512_loadu_si512 (compressed + 30);
  _mm512_storeu_si512(out + 30,
     _mm512_and_si512 ( mask,  _mm512_or_si512 (_mm512_srli_epi32( w1 , 2) ,_mm512_slli_epi32( w0 , 30 ) ) ) );
  _mm512_storeu_si512(out + 31, _mm512_srli_epi32( w0 , 1) );
}


/* we packed 512 32-bit values, touching 32 512-bit words, using 1024 bytes */
static void avx512unpackblock32(const __m512i * compressed, uint32_t * pout) {
  /* we are going to access  32 512-bit words */
  __m512i w0, w1;
  __m512i * out = (__m512i *) pout;
  w0 = _mm512_loadu_si512 (compressed);
  _mm512_storeu_si512(out + 0,  w0 );
  w1 = _mm512_loadu_si512 (compressed + 1);
  _mm512_storeu_si512(out + 1,  w1 );
  w0 = _mm512_loadu_si512 (compressed + 2);
  _mm512_storeu_si512(out + 2,  w0 );
  w1 = _mm512_loadu_si512 (compressed + 3);
  _mm512_storeu_si512(out + 3,  w1 );
  w0 = _mm512_loadu_si512 (compressed + 4);
  _mm512_storeu_si512(out + 4,  w0 );
  w1 = _mm512_loadu_si512 (compressed + 5);
  _mm512_storeu_si512(out + 5,  w1 );
  w0 = _mm512_loadu_si512 (compressed + 6);
  _mm512_storeu_si512(out + 6,  w0 );
  w1 = _mm512_loadu_si512 (compressed + 7);
  _mm512_storeu_si512(out + 7,  w1 );
  w0 = _mm512_loadu_si512 (compressed + 8);
  _mm512_storeu_si512(out + 8,  w0 );
  w1 = _mm512_loadu_si512 (compressed + 9);
  _mm512_storeu_si512(out + 9,  w1 );
  w0 = _mm512_loadu_si512 (compressed + 10);
  _mm512_storeu_si512(out + 10,  w0 );
  w1 = _mm512_loadu_si512 (compressed + 11);
  _mm512_storeu_si512(out + 11,  w1 );
  w0 = _mm512_loadu_si512 (compressed + 12);
  _mm512_storeu_si512(out + 12,  w0 );
  w1 = _mm512_loadu_si512 (compressed + 13);
  _mm512_storeu_si512(out + 13,  w1 );
  w0 = _mm512_loadu_si512 (compressed + 14);
  _mm512_storeu_si512(out + 14,  w0 );
  w1 = _mm512_loadu_si512 (compressed + 15);
  _mm512_storeu_si512(out + 15,  w1 );
  w0 = _mm512_loadu_si512 (compressed + 16);
  _mm512_storeu_si512(out + 16,  w0 );
  w1 = _mm512_loadu_si512 (compressed + 17);
  _mm512_storeu_si512(out + 17,  w1 );
  w0 = _mm512_loadu_si512 (compressed + 18);
  _mm512_storeu_si512(out + 18,  w0 );
  w1 = _mm512_loadu_si512 (compressed + 19);
  _mm512_storeu_si512(out + 19,  w1 );
  w0 = _mm512_loadu_si512 (compressed + 20);
  _mm512_storeu_si512(out + 20,  w0 );
  w1 = _mm512_loadu_si512 (compressed + 21);
  _mm512_storeu_si512(out + 21,  w1 );
  w0 = _mm512_loadu_si512 (compressed + 22);
  _mm512_storeu_si512(out + 22,  w0 );
  w1 = _mm512_loadu_si512 (compressed + 23);
  _mm512_storeu_si512(out + 23,  w1 );
  w0 = _mm512_loadu_si512 (compressed + 24);
  _mm512_storeu_si512(out + 24,  w0 );
  w1 = _mm512_loadu_si512 (compressed + 25);
  _mm512_storeu_si512(out + 25,  w1 );
  w0 = _mm512_loadu_si512 (compressed + 26);
  _mm512_storeu_si512(out + 26,  w0 );
  w1 = _mm512_loadu_si512 (compressed + 27);
  _mm512_storeu_si512(out + 27,  w1 );
  w0 = _mm512_loadu_si512 (compressed + 28);
  _mm512_storeu_si512(out + 28,  w0 );
  w1 = _mm512_loadu_si512 (compressed + 29);
  _mm512_storeu_si512(out + 29,  w1 );
  w0 = _mm512_loadu_si512 (compressed + 30);
  _mm512_storeu_si512(out + 30,  w0 );
  w1 = _mm512_loadu_si512 (compressed + 31);
  _mm512_storeu_si512(out + 31,  w1 );
}

static avx512packblockfnc avx512funcPackArr[] = {
&avx512packblock0,
&avx512packblock1,
&avx512packblock2,
&avx512packblock3,
&avx512packblock4,
&avx512packblock5,
&avx512packblock6,
&avx512packblock7,
&avx512packblock8,
&avx512packblock9,
&avx512packblock10,
&avx512packblock11,
&avx512packblock12,
&avx512packblock13,
&avx512packblock14,
&avx512packblock15,
&avx512packblock16,
&avx512packblock17,
&avx512packblock18,
&avx512packblock19,
&avx512packblock20,
&avx512packblock21,
&avx512packblock22,
&avx512packblock23,
&avx512packblock24,
&avx512packblock25,
&avx512packblock26,
&avx512packblock27,
&avx512packblock28,
&avx512packblock29,
&avx512packblock30,
&avx512packblock31,
&avx512packblock32
};
static avx512packblockfnc avx512funcPackMaskArr[] = {
&avx512packblockmask0,
&avx512packblockmask1,
&avx512packblockmask2,
&avx512packblockmask3,
&avx512packblockmask4,
&avx512packblockmask5,
&avx512packblockmask6,
&avx512packblockmask7,
&avx512packblockmask8,
&avx512packblockmask9,
&avx512packblockmask10,
&avx512packblockmask11,
&avx512packblockmask12,
&avx512packblockmask13,
&avx512packblockmask14,
&avx512packblockmask15,
&avx512packblockmask16,
&avx512packblockmask17,
&avx512packblockmask18,
&avx512packblockmask19,
&avx512packblockmask20,
&avx512packblockmask21,
&avx512packblockmask22,
&avx512packblockmask23,
&avx512packblockmask24,
&avx512packblockmask25,
&avx512packblockmask26,
&avx512packblockmask27,
&avx512packblockmask28,
&avx512packblockmask29,
&avx512packblockmask30,
&avx512packblockmask31,
&avx512packblockmask32
};
static avx512unpackblockfnc avx512funcUnpackArr[] = {
&avx512unpackblock0,
&avx512unpackblock1,
&avx512unpackblock2,
&avx512unpackblock3,
&avx512unpackblock4,
&avx512unpackblock5,
&avx512unpackblock6,
&avx512unpackblock7,
&avx512unpackblock8,
&avx512unpackblock9,
&avx512unpackblock10,
&avx512unpackblock11,
&avx512unpackblock12,
&avx512unpackblock13,
&avx512unpackblock14,
&avx512unpackblock15,
&avx512unpackblock16,
&avx512unpackblock17,
&avx512unpackblock18,
&avx512unpackblock19,
&avx512unpackblock20,
&avx512unpackblock21,
&avx512unpackblock22,
&avx512unpackblock23,
&avx512unpackblock24,
&avx512unpackblock25,
&avx512unpackblock26,
&avx512unpackblock27,
&avx512unpackblock28,
&avx512unpackblock29,
&avx512unpackblock30,
&avx512unpackblock31,
&avx512unpackblock32
};
/**  avx512packing **/








/* reads 512 values from "in", writes  "bit" 512-bit vectors to "out" */
void avx512pack(const uint32_t *  in,__m512i *  out, const uint32_t bit) {
	avx512funcPackMaskArr[bit](in,out);
}

/* reads 512 values from "in", writes  "bit" 512-bit vectors to "out" */
void avx512packwithoutmask(const uint32_t *  in,__m512i *  out, const uint32_t bit) {
	avx512funcPackArr[bit](in,out);
}

/* reads  "bit" 512-bit vectors from "in", writes  512 values to "out" */
void avx512unpack(const __m512i *  in,uint32_t *  out, const uint32_t bit) {
	avx512funcUnpackArr[bit](in,out);
}

#endif /* __AVX512F__ */

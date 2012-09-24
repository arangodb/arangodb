
#include "RandMT.h"

using namespace std;

//
// uint32_t must be an unsigned integer type capable of holding at least 32
// bits; exactly 32 should be fastest, but 64 is better on an Alpha with
// GCC at -O3 optimization so try your options and see what's best for you
//

RandMT::RandMT() {
  seedMT(1U);
}

RandMT::RandMT(uint32_t seed) {
  seedMT(seed);
}

void RandMT::seedMT(uint32_t seed) {
  //
  // We initialize state[0..(N-1)] via the generator
  //
  //   x_new = (69069 * x_old) mod 2^32
  //
  // from Line 15 of Table 1, p. 106, Sec. 3.3.4 of Knuth's
  // _The Art of Computer Programming_, Volume 2, 3rd ed.
  //
  // Notes (SJC): I do not know what the initial state requirements
  // of the Mersenne Twister are, but it seems this seeding generator
  // could be better.  It achieves the maximum period for its modulus
  // (2^30) iff x_initial is odd (p. 20-21, Sec. 3.2.1.2, Knuth); if
  // x_initial can be even, you have sequences like 0, 0, 0, ...;
  // 2^31, 2^31, 2^31, ...; 2^30, 2^30, 2^30, ...; 2^29, 2^29 + 2^31,
  // 2^29, 2^29 + 2^31, ..., etc. so I force seed to be odd below.
  //
  // Even if x_initial is odd, if x_initial is 1 mod 4 then
  //
  //   the          lowest bit of x is always 1,
  //   the  next-to-lowest bit of x is always 0,
  //   the 2nd-from-lowest bit of x alternates      ... 0 1 0 1 0 1 0 1 ... ,
  //   the 3rd-from-lowest bit of x 4-cycles        ... 0 1 1 0 0 1 1 0 ... ,
  //   the 4th-from-lowest bit of x has the 8-cycle ... 0 0 0 1 1 1 1 0 ... ,
  //    ...
  //
  // and if x_initial is 3 mod 4 then
  //
  //   the          lowest bit of x is always 1,
  //   the  next-to-lowest bit of x is always 1,
  //   the 2nd-from-lowest bit of x alternates      ... 0 1 0 1 0 1 0 1 ... ,
  //   the 3rd-from-lowest bit of x 4-cycles        ... 0 0 1 1 0 0 1 1 ... ,
  //   the 4th-from-lowest bit of x has the 8-cycle ... 0 0 1 1 1 1 0 0 ... ,
  //    ...
  //
  // The generator's potency (min. s>=0 with (69069-1)^s = 0 mod 2^32) is
  // 16, which seems to be alright by p. 25, Sec. 3.2.1.3 of Knuth.  It
  // also does well in the dimension 2..5 spectral tests, but it could be
  // better in dimension 6 (Line 15, Table 1, p. 106, Sec. 3.3.4, Knuth).
  //
  // Note that the random number user does not see the values generated
  // here directly since reloadMT() will always munge them first, so maybe
  // none of all of this matters.  In fact, the seed values made here could
  // even be extra-special desirable if the Mersenne Twister theory says
  // so-- that's why the only change I made is to restrict to odd seeds.
  //
  initseed = seed;

  register uint32_t x = (seed | 1U) & 0xFFFFFFFFU, *s = state;
  register int    j;
  left = 0;
  for(*s++=x, j=N; --j; *s++ = (x*=69069U) & 0xFFFFFFFFU);
}

uint32_t RandMT::reloadMT(void) {
  register uint32_t *p0=state, *p2=state+2, *pM=state+M, s0, s1;
  register int    j;

  if(left < -1)
    seedMT(initseed);

  left=N-1, next=state+1;

  for(s0=state[0], s1=state[1], j=N-M+1; --j; s0=s1, s1=*p2++)
    *p0++ = *pM++ ^ (mixBits(s0, s1) >> 1) ^ (loBit(s1) ? K : 0U);

  for(pM=state, j=M; --j; s0=s1, s1=*p2++)
    *p0++ = *pM++ ^ (mixBits(s0, s1) >> 1) ^ (loBit(s1) ? K : 0U);

  s1=state[0], *p0 = *pM ^ (mixBits(s0, s1) >> 1) ^ (loBit(s1) ? K : 0U);
  s1 ^= (s1 >> 11);
  s1 ^= (s1 <<  7) & 0x9D2C5680U;
  s1 ^= (s1 << 15) & 0xEFC60000U;
  return(s1 ^ (s1 >> 18));
}

uint32_t RandMT::randomMT(void) {
  uint32_t y;

  if(--left < 0)
    return(reloadMT());

  y  = *next++;
  y ^= (y >> 11);
  y ^= (y <<  7) & 0x9D2C5680U;
  y ^= (y << 15) & 0xEFC60000U;
  return(y ^ (y >> 18));
}


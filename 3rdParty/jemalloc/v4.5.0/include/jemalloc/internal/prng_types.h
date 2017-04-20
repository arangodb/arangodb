#ifndef JEMALLOC_INTERNAL_PRNG_TYPES_H
#define JEMALLOC_INTERNAL_PRNG_TYPES_H

/*
 * Simple linear congruential pseudo-random number generator:
 *
 *   prng(y) = (a*x + c) % m
 *
 * where the following constants ensure maximal period:
 *
 *   a == Odd number (relatively prime to 2^n), and (a-1) is a multiple of 4.
 *   c == Odd number (relatively prime to 2^n).
 *   m == 2^32
 *
 * See Knuth's TAOCP 3rd Ed., Vol. 2, pg. 17 for details on these constraints.
 *
 * This choice of m has the disadvantage that the quality of the bits is
 * proportional to bit position.  For example, the lowest bit has a cycle of 2,
 * the next has a cycle of 4, etc.  For this reason, we prefer to use the upper
 * bits.
 */

#define PRNG_A_32	UINT32_C(1103515241)
#define PRNG_C_32	UINT32_C(12347)

#define PRNG_A_64	UINT64_C(6364136223846793005)
#define PRNG_C_64	UINT64_C(1442695040888963407)

#endif /* JEMALLOC_INTERNAL_PRNG_TYPES_H */

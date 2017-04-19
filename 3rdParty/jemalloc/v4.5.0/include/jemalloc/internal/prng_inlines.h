#ifndef JEMALLOC_INTERNAL_PRNG_INLINES_H
#define JEMALLOC_INTERNAL_PRNG_INLINES_H

#ifndef JEMALLOC_ENABLE_INLINE
uint32_t	prng_state_next_u32(uint32_t state);
uint64_t	prng_state_next_u64(uint64_t state);
size_t	prng_state_next_zu(size_t state);

uint32_t	prng_lg_range_u32(uint32_t *state, unsigned lg_range,
    bool atomic);
uint64_t	prng_lg_range_u64(uint64_t *state, unsigned lg_range);
size_t	prng_lg_range_zu(size_t *state, unsigned lg_range, bool atomic);

uint32_t	prng_range_u32(uint32_t *state, uint32_t range, bool atomic);
uint64_t	prng_range_u64(uint64_t *state, uint64_t range);
size_t	prng_range_zu(size_t *state, size_t range, bool atomic);
#endif

#if (defined(JEMALLOC_ENABLE_INLINE) || defined(JEMALLOC_PRNG_C_))
JEMALLOC_ALWAYS_INLINE uint32_t
prng_state_next_u32(uint32_t state) {
	return (state * PRNG_A_32) + PRNG_C_32;
}

JEMALLOC_ALWAYS_INLINE uint64_t
prng_state_next_u64(uint64_t state) {
	return (state * PRNG_A_64) + PRNG_C_64;
}

JEMALLOC_ALWAYS_INLINE size_t
prng_state_next_zu(size_t state) {
#if LG_SIZEOF_PTR == 2
	return (state * PRNG_A_32) + PRNG_C_32;
#elif LG_SIZEOF_PTR == 3
	return (state * PRNG_A_64) + PRNG_C_64;
#else
#error Unsupported pointer size
#endif
}

JEMALLOC_ALWAYS_INLINE uint32_t
prng_lg_range_u32(uint32_t *state, unsigned lg_range, bool atomic) {
	uint32_t ret, state1;

	assert(lg_range > 0);
	assert(lg_range <= 32);

	if (atomic) {
		uint32_t state0;

		do {
			state0 = atomic_read_u32(state);
			state1 = prng_state_next_u32(state0);
		} while (atomic_cas_u32(state, state0, state1));
	} else {
		state1 = prng_state_next_u32(*state);
		*state = state1;
	}
	ret = state1 >> (32 - lg_range);

	return ret;
}

/* 64-bit atomic operations cannot be supported on all relevant platforms. */
JEMALLOC_ALWAYS_INLINE uint64_t
prng_lg_range_u64(uint64_t *state, unsigned lg_range) {
	uint64_t ret, state1;

	assert(lg_range > 0);
	assert(lg_range <= 64);

	state1 = prng_state_next_u64(*state);
	*state = state1;
	ret = state1 >> (64 - lg_range);

	return ret;
}

JEMALLOC_ALWAYS_INLINE size_t
prng_lg_range_zu(size_t *state, unsigned lg_range, bool atomic) {
	size_t ret, state1;

	assert(lg_range > 0);
	assert(lg_range <= ZU(1) << (3 + LG_SIZEOF_PTR));

	if (atomic) {
		size_t state0;

		do {
			state0 = atomic_read_zu(state);
			state1 = prng_state_next_zu(state0);
		} while (atomic_cas_zu(state, state0, state1));
	} else {
		state1 = prng_state_next_zu(*state);
		*state = state1;
	}
	ret = state1 >> ((ZU(1) << (3 + LG_SIZEOF_PTR)) - lg_range);

	return ret;
}

JEMALLOC_ALWAYS_INLINE uint32_t
prng_range_u32(uint32_t *state, uint32_t range, bool atomic) {
	uint32_t ret;
	unsigned lg_range;

	assert(range > 1);

	/* Compute the ceiling of lg(range). */
	lg_range = ffs_u32(pow2_ceil_u32(range)) - 1;

	/* Generate a result in [0..range) via repeated trial. */
	do {
		ret = prng_lg_range_u32(state, lg_range, atomic);
	} while (ret >= range);

	return ret;
}

JEMALLOC_ALWAYS_INLINE uint64_t
prng_range_u64(uint64_t *state, uint64_t range) {
	uint64_t ret;
	unsigned lg_range;

	assert(range > 1);

	/* Compute the ceiling of lg(range). */
	lg_range = ffs_u64(pow2_ceil_u64(range)) - 1;

	/* Generate a result in [0..range) via repeated trial. */
	do {
		ret = prng_lg_range_u64(state, lg_range);
	} while (ret >= range);

	return ret;
}

JEMALLOC_ALWAYS_INLINE size_t
prng_range_zu(size_t *state, size_t range, bool atomic) {
	size_t ret;
	unsigned lg_range;

	assert(range > 1);

	/* Compute the ceiling of lg(range). */
	lg_range = ffs_u64(pow2_ceil_u64(range)) - 1;

	/* Generate a result in [0..range) via repeated trial. */
	do {
		ret = prng_lg_range_zu(state, lg_range, atomic);
	} while (ret >= range);

	return ret;
}
#endif

#endif /* JEMALLOC_INTERNAL_PRNG_INLINES_H */

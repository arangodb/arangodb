#ifndef JEMALLOC_INTERNAL_CKH_STRUCTS_H
#define JEMALLOC_INTERNAL_CKH_STRUCTS_H

/* Hash table cell. */
struct ckhc_s {
	const void	*key;
	const void	*data;
};

struct ckh_s {
#ifdef CKH_COUNT
	/* Counters used to get an idea of performance. */
	uint64_t	ngrows;
	uint64_t	nshrinks;
	uint64_t	nshrinkfails;
	uint64_t	ninserts;
	uint64_t	nrelocs;
#endif

	/* Used for pseudo-random number generation. */
	uint64_t	prng_state;

	/* Total number of items. */
	size_t		count;

	/*
	 * Minimum and current number of hash table buckets.  There are
	 * 2^LG_CKH_BUCKET_CELLS cells per bucket.
	 */
	unsigned	lg_minbuckets;
	unsigned	lg_curbuckets;

	/* Hash and comparison functions. */
	ckh_hash_t	*hash;
	ckh_keycomp_t	*keycomp;

	/* Hash table with 2^lg_curbuckets buckets. */
	ckhc_t		*tab;
};

#endif /* JEMALLOC_INTERNAL_CKH_STRUCTS_H */

#ifndef JEMALLOC_INTERNAL_WITNESS_STRUCTS_H
#define JEMALLOC_INTERNAL_WITNESS_STRUCTS_H

struct witness_s {
	/* Name, used for printing lock order reversal messages. */
	const char		*name;

	/*
	 * Witness rank, where 0 is lowest and UINT_MAX is highest.  Witnesses
	 * must be acquired in order of increasing rank.
	 */
	witness_rank_t		rank;

	/*
	 * If two witnesses are of equal rank and they have the samp comp
	 * function pointer, it is called as a last attempt to differentiate
	 * between witnesses of equal rank.
	 */
	witness_comp_t		*comp;

	/* Opaque data, passed to comp(). */
	void			*opaque;

	/* Linkage for thread's currently owned locks. */
	ql_elm(witness_t)	link;
};

#endif /* JEMALLOC_INTERNAL_WITNESS_STRUCTS_H */

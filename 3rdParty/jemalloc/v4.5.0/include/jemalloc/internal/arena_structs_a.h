#ifndef JEMALLOC_INTERNAL_ARENA_STRUCTS_A_H
#define JEMALLOC_INTERNAL_ARENA_STRUCTS_A_H

struct arena_slab_data_s {
	/* Index of bin this slab is associated with. */
	szind_t		binind;

	/* Number of free regions in slab. */
	unsigned	nfree;

	/* Per region allocated/deallocated bitmap. */
	bitmap_t	bitmap[BITMAP_GROUPS_MAX];
};

#endif /* JEMALLOC_INTERNAL_ARENA_STRUCTS_A_H */

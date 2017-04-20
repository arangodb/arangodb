#ifndef JEMALLOC_INTERNAL_BITMAP_STRUCTS_H
#define JEMALLOC_INTERNAL_BITMAP_STRUCTS_H

struct bitmap_level_s {
	/* Offset of this level's groups within the array of groups. */
	size_t group_offset;
};

struct bitmap_info_s {
	/* Logical number of bits in bitmap (stored at bottom level). */
	size_t nbits;

#ifdef BITMAP_USE_TREE
	/* Number of levels necessary for nbits. */
	unsigned nlevels;

	/*
	 * Only the first (nlevels+1) elements are used, and levels are ordered
	 * bottom to top (e.g. the bottom level is stored in levels[0]).
	 */
	bitmap_level_t levels[BITMAP_MAX_LEVELS+1];
#else /* BITMAP_USE_TREE */
	/* Number of groups necessary for nbits. */
	size_t ngroups;
#endif /* BITMAP_USE_TREE */
};

#endif /* JEMALLOC_INTERNAL_BITMAP_STRUCTS_H */

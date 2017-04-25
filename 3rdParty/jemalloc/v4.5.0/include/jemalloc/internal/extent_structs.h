#ifndef JEMALLOC_INTERNAL_EXTENT_STRUCTS_H
#define JEMALLOC_INTERNAL_EXTENT_STRUCTS_H

typedef enum {
	extent_state_active   = 0,
	extent_state_dirty    = 1,
	extent_state_retained = 2
} extent_state_t;

/* Extent (span of pages).  Use accessor functions for e_* fields. */
struct extent_s {
	/* Arena from which this extent came, if any. */
	arena_t			*e_arena;

	/* Pointer to the extent that this structure is responsible for. */
	void			*e_addr;

	/* Extent size. */
	size_t			e_size;

	/*
	 * Usable size, typically smaller than extent size due to large_pad or
	 * promotion of sampled small regions.
	 */
	size_t			e_usize;

	/*
	 * Serial number (potentially non-unique).
	 *
	 * In principle serial numbers can wrap around on 32-bit systems if
	 * JEMALLOC_MUNMAP is defined, but as long as comparison functions fall
	 * back on address comparison for equal serial numbers, stable (if
	 * imperfect) ordering is maintained.
	 *
	 * Serial numbers may not be unique even in the absence of wrap-around,
	 * e.g. when splitting an extent and assigning the same serial number to
	 * both resulting adjacent extents.
	 */
	size_t			e_sn;

	/* Extent state. */
	extent_state_t		e_state;

	/*
	 * The zeroed flag is used by extent recycling code to track whether
	 * memory is zero-filled.
	 */
	bool			e_zeroed;

	/*
	 * True if physical memory is committed to the extent, whether
	 * explicitly or implicitly as on a system that overcommits and
	 * satisfies physical memory needs on demand via soft page faults.
	 */
	bool			e_committed;

	/*
	 * The slab flag indicates whether the extent is used for a slab of
	 * small regions.  This helps differentiate small size classes, and it
	 * indicates whether interior pointers can be looked up via iealloc().
	 */
	bool			e_slab;

	union {
		/* Small region slab metadata. */
		arena_slab_data_t	e_slab_data;

		/* Profile counters, used for large objects. */
		union {
			void		*e_prof_tctx_pun;
			prof_tctx_t	*e_prof_tctx;
		};
	};

	/*
	 * List linkage, used by a variety of lists:
	 * - arena_bin_t's slabs_full
	 * - extents_t's LRU
	 * - stashed dirty extents
	 * - arena's large allocations
	 * - arena's extent structure freelist
	 */
	ql_elm(extent_t)	ql_link;

	/* Linkage for per size class sn/address-ordered heaps. */
	phn(extent_t)		ph_link;
};
typedef ql_head(extent_t) extent_list_t;
typedef ph(extent_t) extent_heap_t;

/* Quantized collection of extents, with built-in LRU queue. */
struct extents_s {
	malloc_mutex_t		mtx;

	/*
	 * Quantized per size class heaps of extents.
	 *
	 * Synchronization: mtx.
	 */
	extent_heap_t		heaps[NPSIZES+1];

	/*
	 * LRU of all extents in heaps.
	 *
	 * Synchronization: mtx.
	 */
	extent_list_t		lru;

	/*
	 * Page sum for all extents in heaps.
	 *
	 * Synchronization: atomic.
	 */
	size_t			npages;

	/* All stored extents must be in the same state. */
	extent_state_t		state;

	/* If true, try to coalesce during extent deallocation. */
	bool			try_coalesce;
};

#endif /* JEMALLOC_INTERNAL_EXTENT_STRUCTS_H */

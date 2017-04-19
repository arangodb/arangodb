#define JEMALLOC_BASE_C_
#include "jemalloc/internal/jemalloc_internal.h"

/******************************************************************************/
/* Data. */

static base_t	*b0;

/******************************************************************************/

static void *
base_map(extent_hooks_t *extent_hooks, unsigned ind, size_t size) {
	void *addr;
	bool zero = true;
	bool commit = true;

	assert(size == HUGEPAGE_CEILING(size));

	if (extent_hooks == &extent_hooks_default) {
		addr = extent_alloc_mmap(NULL, size, PAGE, &zero, &commit);
	} else {
		addr = extent_hooks->alloc(extent_hooks, NULL, size, PAGE,
		    &zero, &commit, ind);
	}

	return addr;
}

static void
base_unmap(extent_hooks_t *extent_hooks, unsigned ind, void *addr,
    size_t size) {
	/*
	 * Cascade through dalloc, decommit, purge_lazy, and purge_forced,
	 * stopping at first success.  This cascade is performed for consistency
	 * with the cascade in extent_dalloc_wrapper() because an application's
	 * custom hooks may not support e.g. dalloc.  This function is only ever
	 * called as a side effect of arena destruction, so although it might
	 * seem pointless to do anything besides dalloc here, the application
	 * may in fact want the end state of all associated virtual memory to in
	 * some consistent-but-allocated state.
	 */
	if (extent_hooks == &extent_hooks_default) {
		if (!extent_dalloc_mmap(addr, size)) {
			return;
		}
		if (!pages_decommit(addr, size)) {
			return;
		}
		if (!pages_purge_lazy(addr, size)) {
			return;
		}
		if (!pages_purge_forced(addr, size)) {
			return;
		}
		/* Nothing worked.  This should never happen. */
		not_reached();
	} else {
		if (extent_hooks->dalloc != NULL &&
		    !extent_hooks->dalloc(extent_hooks, addr, size, true,
		    ind)) {
			return;
		}
		if (extent_hooks->decommit != NULL &&
		    !extent_hooks->decommit(extent_hooks, addr, size, 0, size,
		    ind)) {
			return;
		}
		if (extent_hooks->purge_lazy != NULL &&
		    !extent_hooks->purge_lazy(extent_hooks, addr, size, 0, size,
		    ind)) {
			return;
		}
		if (extent_hooks->purge_forced != NULL &&
		    !extent_hooks->purge_forced(extent_hooks, addr, size, 0,
		    size, ind)) {
			return;
		}
		/* Nothing worked.  That's the application's problem. */
	}
}

static void
base_extent_init(size_t *extent_sn_next, extent_t *extent, void *addr,
    size_t size) {
	size_t sn;

	sn = *extent_sn_next;
	(*extent_sn_next)++;

	extent_init(extent, NULL, addr, size, 0, sn, extent_state_active, true,
	    true, false);
}

static void *
base_extent_bump_alloc_helper(extent_t *extent, size_t *gap_size, size_t size,
    size_t alignment) {
	void *ret;

	assert(alignment == ALIGNMENT_CEILING(alignment, QUANTUM));
	assert(size == ALIGNMENT_CEILING(size, alignment));

	*gap_size = ALIGNMENT_CEILING((uintptr_t)extent_addr_get(extent),
	    alignment) - (uintptr_t)extent_addr_get(extent);
	ret = (void *)((uintptr_t)extent_addr_get(extent) + *gap_size);
	assert(extent_size_get(extent) >= *gap_size + size);
	extent_init(extent, NULL, (void *)((uintptr_t)extent_addr_get(extent) +
	    *gap_size + size), extent_size_get(extent) - *gap_size - size, 0,
	    extent_sn_get(extent), extent_state_active, true, true, false);
	return ret;
}

static void
base_extent_bump_alloc_post(tsdn_t *tsdn, base_t *base, extent_t *extent,
    size_t gap_size, void *addr, size_t size) {
	if (extent_size_get(extent) > 0) {
		/*
		 * Compute the index for the largest size class that does not
		 * exceed extent's size.
		 */
		szind_t index_floor = size2index(extent_size_get(extent) + 1) -
		    1;
		extent_heap_insert(&base->avail[index_floor], extent);
	}

	if (config_stats) {
		base->allocated += size;
		/*
		 * Add one PAGE to base_resident for every page boundary that is
		 * crossed by the new allocation.
		 */
		base->resident += PAGE_CEILING((uintptr_t)addr + size) -
		    PAGE_CEILING((uintptr_t)addr - gap_size);
		assert(base->allocated <= base->resident);
		assert(base->resident <= base->mapped);
	}
}

static void *
base_extent_bump_alloc(tsdn_t *tsdn, base_t *base, extent_t *extent,
    size_t size, size_t alignment) {
	void *ret;
	size_t gap_size;

	ret = base_extent_bump_alloc_helper(extent, &gap_size, size, alignment);
	base_extent_bump_alloc_post(tsdn, base, extent, gap_size, ret, size);
	return ret;
}

/*
 * Allocate a block of virtual memory that is large enough to start with a
 * base_block_t header, followed by an object of specified size and alignment.
 * On success a pointer to the initialized base_block_t header is returned.
 */
static base_block_t *
base_block_alloc(extent_hooks_t *extent_hooks, unsigned ind,
    size_t *extent_sn_next, size_t size, size_t alignment) {
	base_block_t *block;
	size_t usize, header_size, gap_size, block_size;

	alignment = ALIGNMENT_CEILING(alignment, QUANTUM);
	usize = ALIGNMENT_CEILING(size, alignment);
	header_size = sizeof(base_block_t);
	gap_size = ALIGNMENT_CEILING(header_size, alignment) - header_size;
	block_size = HUGEPAGE_CEILING(header_size + gap_size + usize);
	block = (base_block_t *)base_map(extent_hooks, ind, block_size);
	if (block == NULL) {
		return NULL;
	}
	block->size = block_size;
	block->next = NULL;
	assert(block_size >= header_size);
	base_extent_init(extent_sn_next, &block->extent,
	    (void *)((uintptr_t)block + header_size), block_size - header_size);
	return block;
}

/*
 * Allocate an extent that is at least as large as specified size, with
 * specified alignment.
 */
static extent_t *
base_extent_alloc(tsdn_t *tsdn, base_t *base, size_t size, size_t alignment) {
	extent_hooks_t *extent_hooks = base_extent_hooks_get(base);
	base_block_t *block;

	malloc_mutex_assert_owner(tsdn, &base->mtx);

	block = base_block_alloc(extent_hooks, base_ind_get(base),
	    &base->extent_sn_next, size, alignment);
	if (block == NULL) {
		return NULL;
	}
	block->next = base->blocks;
	base->blocks = block;
	if (config_stats) {
		base->allocated += sizeof(base_block_t);
		base->resident += PAGE_CEILING(sizeof(base_block_t));
		base->mapped += block->size;
		assert(base->allocated <= base->resident);
		assert(base->resident <= base->mapped);
	}
	return &block->extent;
}

base_t *
b0get(void) {
	return b0;
}

base_t *
base_new(tsdn_t *tsdn, unsigned ind, extent_hooks_t *extent_hooks) {
	base_t *base;
	size_t extent_sn_next, base_alignment, base_size, gap_size;
	base_block_t *block;
	szind_t i;

	extent_sn_next = 0;
	block = base_block_alloc(extent_hooks, ind, &extent_sn_next,
	    sizeof(base_t), QUANTUM);
	if (block == NULL) {
		return NULL;
	}

	base_alignment = CACHELINE;
	base_size = ALIGNMENT_CEILING(sizeof(base_t), base_alignment);
	base = (base_t *)base_extent_bump_alloc_helper(&block->extent,
	    &gap_size, base_size, base_alignment);
	base->ind = ind;
	base->extent_hooks = extent_hooks;
	if (malloc_mutex_init(&base->mtx, "base", WITNESS_RANK_BASE)) {
		base_unmap(extent_hooks, ind, block, block->size);
		return NULL;
	}
	base->extent_sn_next = extent_sn_next;
	base->blocks = block;
	for (i = 0; i < NSIZES; i++) {
		extent_heap_new(&base->avail[i]);
	}
	if (config_stats) {
		base->allocated = sizeof(base_block_t);
		base->resident = PAGE_CEILING(sizeof(base_block_t));
		base->mapped = block->size;
		assert(base->allocated <= base->resident);
		assert(base->resident <= base->mapped);
	}
	base_extent_bump_alloc_post(tsdn, base, &block->extent, gap_size, base,
	    base_size);

	return base;
}

void
base_delete(base_t *base) {
	extent_hooks_t *extent_hooks = base_extent_hooks_get(base);
	base_block_t *next = base->blocks;
	do {
		base_block_t *block = next;
		next = block->next;
		base_unmap(extent_hooks, base_ind_get(base), block,
		    block->size);
	} while (next != NULL);
}

extent_hooks_t *
base_extent_hooks_get(base_t *base) {
	return (extent_hooks_t *)atomic_read_p(&base->extent_hooks_pun);
}

extent_hooks_t *
base_extent_hooks_set(base_t *base, extent_hooks_t *extent_hooks) {
	extent_hooks_t *old_extent_hooks = base_extent_hooks_get(base);
	union {
		extent_hooks_t	**h;
		void		**v;
	} u;

	u.h = &base->extent_hooks;
	atomic_write_p(u.v, extent_hooks);

	return old_extent_hooks;
}

/*
 * base_alloc() returns zeroed memory, which is always demand-zeroed for the
 * auto arenas, in order to make multi-page sparse data structures such as radix
 * tree nodes efficient with respect to physical memory usage.  Upon success a
 * pointer to at least size bytes with specified alignment is returned.  Note
 * that size is rounded up to the nearest multiple of alignment to avoid false
 * sharing.
 */
void *
base_alloc(tsdn_t *tsdn, base_t *base, size_t size, size_t alignment) {
	void *ret;
	size_t usize, asize;
	szind_t i;
	extent_t *extent;

	alignment = QUANTUM_CEILING(alignment);
	usize = ALIGNMENT_CEILING(size, alignment);
	asize = usize + alignment - QUANTUM;

	extent = NULL;
	malloc_mutex_lock(tsdn, &base->mtx);
	for (i = size2index(asize); i < NSIZES; i++) {
		extent = extent_heap_remove_first(&base->avail[i]);
		if (extent != NULL) {
			/* Use existing space. */
			break;
		}
	}
	if (extent == NULL) {
		/* Try to allocate more space. */
		extent = base_extent_alloc(tsdn, base, usize, alignment);
	}
	if (extent == NULL) {
		ret = NULL;
		goto label_return;
	}

	ret = base_extent_bump_alloc(tsdn, base, extent, usize, alignment);
label_return:
	malloc_mutex_unlock(tsdn, &base->mtx);
	return ret;
}

void
base_stats_get(tsdn_t *tsdn, base_t *base, size_t *allocated, size_t *resident,
    size_t *mapped) {
	cassert(config_stats);

	malloc_mutex_lock(tsdn, &base->mtx);
	assert(base->allocated <= base->resident);
	assert(base->resident <= base->mapped);
	*allocated = base->allocated;
	*resident = base->resident;
	*mapped = base->mapped;
	malloc_mutex_unlock(tsdn, &base->mtx);
}

void
base_prefork(tsdn_t *tsdn, base_t *base) {
	malloc_mutex_prefork(tsdn, &base->mtx);
}

void
base_postfork_parent(tsdn_t *tsdn, base_t *base) {
	malloc_mutex_postfork_parent(tsdn, &base->mtx);
}

void
base_postfork_child(tsdn_t *tsdn, base_t *base) {
	malloc_mutex_postfork_child(tsdn, &base->mtx);
}

bool
base_boot(tsdn_t *tsdn) {
	b0 = base_new(tsdn, 0, (extent_hooks_t *)&extent_hooks_default);
	return (b0 == NULL);
}

#define JEMALLOC_BASE_C_
#include "jemalloc/internal/jemalloc_preamble.h"
#include "jemalloc/internal/jemalloc_internal_includes.h"

#include "jemalloc/internal/assert.h"
#include "jemalloc/internal/extent_mmap.h"
#include "jemalloc/internal/mutex.h"
#include "jemalloc/internal/sz.h"

/******************************************************************************/
/* Data. */

static base_t	*b0;

/******************************************************************************/

static void *
base_map(tsdn_t *tsdn, extent_hooks_t *extent_hooks, unsigned ind, size_t size) {
	void *addr;
	bool zero = true;
	bool commit = true;

	assert(size == HUGEPAGE_CEILING(size));

	if (extent_hooks == &extent_hooks_default) {
		addr = extent_alloc_mmap(NULL, size, PAGE, &zero, &commit);
	} else {
		/* No arena context as we are creating new arenas. */
		tsd_t *tsd = tsdn_null(tsdn) ? tsd_fetch() : tsdn_tsd(tsdn);
		pre_reentrancy(tsd, NULL);
		addr = extent_hooks->alloc(extent_hooks, NULL, size, PAGE,
		    &zero, &commit, ind);
		post_reentrancy(tsd);
	}

	return addr;
}

static void
base_unmap(tsdn_t *tsdn, extent_hooks_t *extent_hooks, unsigned ind, void *addr,
    size_t size) {
	/*
	 * Cascade through dalloc, decommit, purge_forced, and purge_lazy,
	 * stopping at first success.  This cascade is performed for consistency
	 * with the cascade in extent_dalloc_wrapper() because an application's
	 * custom hooks may not support e.g. dalloc.  This function is only ever
	 * called as a side effect of arena destruction, so although it might
	 * seem pointless to do anything besides dalloc here, the application
	 * may in fact want the end state of all associated virtual memory to be
	 * in some consistent-but-allocated state.
	 */
	if (extent_hooks == &extent_hooks_default) {
		if (!extent_dalloc_mmap(addr, size)) {
			return;
		}
		if (!pages_decommit(addr, size)) {
			return;
		}
		if (!pages_purge_forced(addr, size)) {
			return;
		}
		if (!pages_purge_lazy(addr, size)) {
			return;
		}
		/* Nothing worked.  This should never happen. */
		not_reached();
	} else {
		tsd_t *tsd = tsdn_null(tsdn) ? tsd_fetch() : tsdn_tsd(tsdn);
		pre_reentrancy(tsd, NULL);
		if (extent_hooks->dalloc != NULL &&
		    !extent_hooks->dalloc(extent_hooks, addr, size, true,
		    ind)) {
			goto label_done;
		}
		if (extent_hooks->decommit != NULL &&
		    !extent_hooks->decommit(extent_hooks, addr, size, 0, size,
		    ind)) {
			goto label_done;
		}
		if (extent_hooks->purge_forced != NULL &&
		    !extent_hooks->purge_forced(extent_hooks, addr, size, 0,
		    size, ind)) {
			goto label_done;
		}
		if (extent_hooks->purge_lazy != NULL &&
		    !extent_hooks->purge_lazy(extent_hooks, addr, size, 0, size,
		    ind)) {
			goto label_done;
		}
		/* Nothing worked.  That's the application's problem. */
	label_done:
		post_reentrancy(tsd);
		return;
	}
}

static void
base_extent_init(size_t *extent_sn_next, extent_t *extent, void *addr,
    size_t size) {
	size_t sn;

	sn = *extent_sn_next;
	(*extent_sn_next)++;

	extent_binit(extent, addr, size, sn);
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
	assert(extent_bsize_get(extent) >= *gap_size + size);
	extent_binit(extent, (void *)((uintptr_t)extent_addr_get(extent) +
	    *gap_size + size), extent_bsize_get(extent) - *gap_size - size,
	    extent_sn_get(extent));
	return ret;
}

static void
base_extent_bump_alloc_post(tsdn_t *tsdn, base_t *base, extent_t *extent,
    size_t gap_size, void *addr, size_t size) {
	if (extent_bsize_get(extent) > 0) {
		/*
		 * Compute the index for the largest size class that does not
		 * exceed extent's size.
		 */
		szind_t index_floor =
		    sz_size2index(extent_bsize_get(extent) + 1) - 1;
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
base_block_alloc(tsdn_t *tsdn, extent_hooks_t *extent_hooks, unsigned ind,
    pszind_t *pind_last, size_t *extent_sn_next, size_t size,
    size_t alignment) {
	alignment = ALIGNMENT_CEILING(alignment, QUANTUM);
	size_t usize = ALIGNMENT_CEILING(size, alignment);
	size_t header_size = sizeof(base_block_t);
	size_t gap_size = ALIGNMENT_CEILING(header_size, alignment) -
	    header_size;
	/*
	 * Create increasingly larger blocks in order to limit the total number
	 * of disjoint virtual memory ranges.  Choose the next size in the page
	 * size class series (skipping size classes that are not a multiple of
	 * HUGEPAGE), or a size large enough to satisfy the requested size and
	 * alignment, whichever is larger.
	 */
	size_t min_block_size = HUGEPAGE_CEILING(sz_psz2u(header_size + gap_size
	    + usize));
	pszind_t pind_next = (*pind_last + 1 < NPSIZES) ? *pind_last + 1 :
	    *pind_last;
	size_t next_block_size = HUGEPAGE_CEILING(sz_pind2sz(pind_next));
	size_t block_size = (min_block_size > next_block_size) ? min_block_size
	    : next_block_size;
	base_block_t *block = (base_block_t *)base_map(tsdn, extent_hooks, ind,
	    block_size);
	if (block == NULL) {
		return NULL;
	}
	*pind_last = sz_psz2ind(block_size);
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
	malloc_mutex_assert_owner(tsdn, &base->mtx);

	extent_hooks_t *extent_hooks = base_extent_hooks_get(base);
	/*
	 * Drop mutex during base_block_alloc(), because an extent hook will be
	 * called.
	 */
	malloc_mutex_unlock(tsdn, &base->mtx);
	base_block_t *block = base_block_alloc(tsdn, extent_hooks,
	    base_ind_get(base), &base->pind_last, &base->extent_sn_next, size,
	    alignment);
	malloc_mutex_lock(tsdn, &base->mtx);
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
	pszind_t pind_last = 0;
	size_t extent_sn_next = 0;
	base_block_t *block = base_block_alloc(tsdn, extent_hooks, ind,
	    &pind_last, &extent_sn_next, sizeof(base_t), QUANTUM);
	if (block == NULL) {
		return NULL;
	}

	size_t gap_size;
	size_t base_alignment = CACHELINE;
	size_t base_size = ALIGNMENT_CEILING(sizeof(base_t), base_alignment);
	base_t *base = (base_t *)base_extent_bump_alloc_helper(&block->extent,
	    &gap_size, base_size, base_alignment);
	base->ind = ind;
	atomic_store_p(&base->extent_hooks, extent_hooks, ATOMIC_RELAXED);
	if (malloc_mutex_init(&base->mtx, "base", WITNESS_RANK_BASE,
	    malloc_mutex_rank_exclusive)) {
		base_unmap(tsdn, extent_hooks, ind, block, block->size);
		return NULL;
	}
	base->pind_last = pind_last;
	base->extent_sn_next = extent_sn_next;
	base->blocks = block;
	for (szind_t i = 0; i < NSIZES; i++) {
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
base_delete(tsdn_t *tsdn, base_t *base) {
	extent_hooks_t *extent_hooks = base_extent_hooks_get(base);
	base_block_t *next = base->blocks;
	do {
		base_block_t *block = next;
		next = block->next;
		base_unmap(tsdn, extent_hooks, base_ind_get(base), block,
		    block->size);
	} while (next != NULL);
}

extent_hooks_t *
base_extent_hooks_get(base_t *base) {
	return (extent_hooks_t *)atomic_load_p(&base->extent_hooks,
	    ATOMIC_ACQUIRE);
}

extent_hooks_t *
base_extent_hooks_set(base_t *base, extent_hooks_t *extent_hooks) {
	extent_hooks_t *old_extent_hooks = base_extent_hooks_get(base);
	atomic_store_p(&base->extent_hooks, extent_hooks, ATOMIC_RELEASE);
	return old_extent_hooks;
}

static void *
base_alloc_impl(tsdn_t *tsdn, base_t *base, size_t size, size_t alignment,
    size_t *esn) {
	alignment = QUANTUM_CEILING(alignment);
	size_t usize = ALIGNMENT_CEILING(size, alignment);
	size_t asize = usize + alignment - QUANTUM;

	extent_t *extent = NULL;
	malloc_mutex_lock(tsdn, &base->mtx);
	for (szind_t i = sz_size2index(asize); i < NSIZES; i++) {
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
	void *ret;
	if (extent == NULL) {
		ret = NULL;
		goto label_return;
	}

	ret = base_extent_bump_alloc(tsdn, base, extent, usize, alignment);
	if (esn != NULL) {
		*esn = extent_sn_get(extent);
	}
label_return:
	malloc_mutex_unlock(tsdn, &base->mtx);
	return ret;
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
	return base_alloc_impl(tsdn, base, size, alignment, NULL);
}

extent_t *
base_alloc_extent(tsdn_t *tsdn, base_t *base) {
	size_t esn;
	extent_t *extent = base_alloc_impl(tsdn, base, sizeof(extent_t),
	    CACHELINE, &esn);
	if (extent == NULL) {
		return NULL;
	}
	extent_esn_set(extent, esn);
	return extent;
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

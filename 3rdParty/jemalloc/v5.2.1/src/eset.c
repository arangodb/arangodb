#include "jemalloc/internal/jemalloc_preamble.h"
#include "jemalloc/internal/jemalloc_internal_includes.h"

#include "jemalloc/internal/eset.h"

#define ESET_NPSIZES (SC_NPSIZES + 1)

void
eset_init(eset_t *eset, extent_state_t state) {
	for (unsigned i = 0; i < ESET_NPSIZES; i++) {
		edata_heap_new(&eset->heaps[i]);
	}
	fb_init(eset->bitmap, ESET_NPSIZES);
	edata_list_inactive_init(&eset->lru);
	atomic_store_zu(&eset->npages, 0, ATOMIC_RELAXED);
	eset->state = state;
}

size_t
eset_npages_get(eset_t *eset) {
	return atomic_load_zu(&eset->npages, ATOMIC_RELAXED);
}

size_t
eset_nextents_get(eset_t *eset, pszind_t pind) {
	return atomic_load_zu(&eset->nextents[pind], ATOMIC_RELAXED);
}

size_t
eset_nbytes_get(eset_t *eset, pszind_t pind) {
	return atomic_load_zu(&eset->nbytes[pind], ATOMIC_RELAXED);
}

static void
eset_stats_add(eset_t *eset, pszind_t pind, size_t sz) {
	size_t cur = atomic_load_zu(&eset->nextents[pind], ATOMIC_RELAXED);
	atomic_store_zu(&eset->nextents[pind], cur + 1, ATOMIC_RELAXED);
	cur = atomic_load_zu(&eset->nbytes[pind], ATOMIC_RELAXED);
	atomic_store_zu(&eset->nbytes[pind], cur + sz, ATOMIC_RELAXED);
}

static void
eset_stats_sub(eset_t *eset, pszind_t pind, size_t sz) {
	size_t cur = atomic_load_zu(&eset->nextents[pind], ATOMIC_RELAXED);
	atomic_store_zu(&eset->nextents[pind], cur - 1, ATOMIC_RELAXED);
	cur = atomic_load_zu(&eset->nbytes[pind], ATOMIC_RELAXED);
	atomic_store_zu(&eset->nbytes[pind], cur - sz, ATOMIC_RELAXED);
}

void
eset_insert(eset_t *eset, edata_t *edata) {
	assert(edata_state_get(edata) == eset->state);

	size_t size = edata_size_get(edata);
	size_t psz = sz_psz_quantize_floor(size);
	pszind_t pind = sz_psz2ind(psz);
	if (edata_heap_empty(&eset->heaps[pind])) {
		fb_set(eset->bitmap, ESET_NPSIZES, (size_t)pind);
	}
	edata_heap_insert(&eset->heaps[pind], edata);

	if (config_stats) {
		eset_stats_add(eset, pind, size);
	}

	edata_list_inactive_append(&eset->lru, edata);
	size_t npages = size >> LG_PAGE;
	/*
	 * All modifications to npages hold the mutex (as asserted above), so we
	 * don't need an atomic fetch-add; we can get by with a load followed by
	 * a store.
	 */
	size_t cur_eset_npages =
	    atomic_load_zu(&eset->npages, ATOMIC_RELAXED);
	atomic_store_zu(&eset->npages, cur_eset_npages + npages,
	    ATOMIC_RELAXED);
}

void
eset_remove(eset_t *eset, edata_t *edata) {
	assert(edata_state_get(edata) == eset->state ||
	    edata_state_in_transition(edata_state_get(edata)));

	size_t size = edata_size_get(edata);
	size_t psz = sz_psz_quantize_floor(size);
	pszind_t pind = sz_psz2ind(psz);
	edata_heap_remove(&eset->heaps[pind], edata);

	if (config_stats) {
		eset_stats_sub(eset, pind, size);
	}

	if (edata_heap_empty(&eset->heaps[pind])) {
		fb_unset(eset->bitmap, ESET_NPSIZES, (size_t)pind);
	}
	edata_list_inactive_remove(&eset->lru, edata);
	size_t npages = size >> LG_PAGE;
	/*
	 * As in eset_insert, we hold eset->mtx and so don't need atomic
	 * operations for updating eset->npages.
	 */
	/*
	 * This class is not thread-safe in general; we rely on external
	 * synchronization for all mutating operations.
	 */
	size_t cur_extents_npages =
	    atomic_load_zu(&eset->npages, ATOMIC_RELAXED);
	assert(cur_extents_npages >= npages);
	atomic_store_zu(&eset->npages,
	    cur_extents_npages - (size >> LG_PAGE), ATOMIC_RELAXED);
}

/*
 * Find an extent with size [min_size, max_size) to satisfy the alignment
 * requirement.  For each size, try only the first extent in the heap.
 */
static edata_t *
eset_fit_alignment(eset_t *eset, size_t min_size, size_t max_size,
    size_t alignment) {
        pszind_t pind = sz_psz2ind(sz_psz_quantize_ceil(min_size));
        pszind_t pind_max = sz_psz2ind(sz_psz_quantize_ceil(max_size));

	for (pszind_t i =
	    (pszind_t)fb_ffs(eset->bitmap, ESET_NPSIZES, (size_t)pind);
	    i < pind_max;
	    i = (pszind_t)fb_ffs(eset->bitmap, ESET_NPSIZES, (size_t)i + 1)) {
		assert(i < SC_NPSIZES);
		assert(!edata_heap_empty(&eset->heaps[i]));
		edata_t *edata = edata_heap_first(&eset->heaps[i]);
		uintptr_t base = (uintptr_t)edata_base_get(edata);
		size_t candidate_size = edata_size_get(edata);
		assert(candidate_size >= min_size);

		uintptr_t next_align = ALIGNMENT_CEILING((uintptr_t)base,
		    PAGE_CEILING(alignment));
		if (base > next_align || base + candidate_size <= next_align) {
			/* Overflow or not crossing the next alignment. */
			continue;
		}

		size_t leadsize = next_align - base;
		if (candidate_size - leadsize >= min_size) {
			return edata;
		}
	}

	return NULL;
}

/*
 * Do first-fit extent selection, i.e. select the oldest/lowest extent that is
 * large enough.
 *
 * lg_max_fit is the (log of the) maximum ratio between the requested size and
 * the returned size that we'll allow.  This can reduce fragmentation by
 * avoiding reusing and splitting large extents for smaller sizes.  In practice,
 * it's set to opt_lg_extent_max_active_fit for the dirty eset and SC_PTR_BITS
 * for others.
 */
static edata_t *
eset_first_fit(eset_t *eset, size_t size, bool exact_only,
    unsigned lg_max_fit) {
	edata_t *ret = NULL;

	pszind_t pind = sz_psz2ind(sz_psz_quantize_ceil(size));

	if (exact_only) {
		return edata_heap_empty(&eset->heaps[pind]) ? NULL :
		    edata_heap_first(&eset->heaps[pind]);
	}

	for (pszind_t i =
	    (pszind_t)fb_ffs(eset->bitmap, ESET_NPSIZES, (size_t)pind);
	    i < ESET_NPSIZES;
	    i = (pszind_t)fb_ffs(eset->bitmap, ESET_NPSIZES, (size_t)i + 1)) {
		assert(!edata_heap_empty(&eset->heaps[i]));
		edata_t *edata = edata_heap_first(&eset->heaps[i]);
		assert(edata_size_get(edata) >= size);
		if (lg_max_fit == SC_PTR_BITS) {
			/*
			 * We'll shift by this below, and shifting out all the
			 * bits is undefined.  Decreasing is safe, since the
			 * page size is larger than 1 byte.
			 */
			lg_max_fit = SC_PTR_BITS - 1;
		}
		if ((sz_pind2sz(i) >> lg_max_fit) > size) {
			break;
		}
		if (ret == NULL || edata_snad_comp(edata, ret) < 0) {
			ret = edata;
		}
		if (i == SC_NPSIZES) {
			break;
		}
		assert(i < SC_NPSIZES);
	}

	return ret;
}

edata_t *
eset_fit(eset_t *eset, size_t esize, size_t alignment, bool exact_only,
    unsigned lg_max_fit) {
	size_t max_size = esize + PAGE_CEILING(alignment) - PAGE;
	/* Beware size_t wrap-around. */
	if (max_size < esize) {
		return NULL;
	}

	edata_t *edata = eset_first_fit(eset, max_size, exact_only, lg_max_fit);

	if (alignment > PAGE && edata == NULL) {
		/*
		 * max_size guarantees the alignment requirement but is rather
		 * pessimistic.  Next we try to satisfy the aligned allocation
		 * with sizes in [esize, max_size).
		 */
		edata = eset_fit_alignment(eset, esize, max_size, alignment);
	}

	return edata;
}

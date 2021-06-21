#include "jemalloc/internal/jemalloc_preamble.h"
#include "jemalloc/internal/jemalloc_internal_includes.h"

#include "jemalloc/internal/hpa_central.h"

void
hpa_central_init(hpa_central_t *central, edata_cache_t *edata_cache,
    emap_t *emap) {
	central->emap = emap;
	edata_cache_small_init(&central->ecs, edata_cache);
	eset_init(&central->eset, extent_state_dirty);
	central->sn_next = 0;
}

/*
 * Returns the trail, or NULL in case of failure (which can only occur in case
 * of an emap operation failure; i.e. OOM).
 */
static edata_t *
hpa_central_split(tsdn_t *tsdn, hpa_central_t *central, edata_t *edata,
    size_t size) {
	edata_t *trail = edata_cache_small_get(tsdn, &central->ecs);
	if (trail == NULL) {
		return NULL;
	}
	size_t cursize = edata_size_get(edata);
	edata_init(trail, edata_arena_ind_get(edata),
	    (void *)((uintptr_t)edata_base_get(edata) + size), cursize - size,
	    /* slab */ false, SC_NSIZES, edata_sn_get(edata),
	    edata_state_get(edata), edata_zeroed_get(edata),
	    edata_committed_get(edata), EXTENT_PAI_HPA, EXTENT_NOT_HEAD);

	emap_prepare_t prepare;
	bool err = emap_split_prepare(tsdn, central->emap, &prepare, edata,
	    size, trail, cursize - size);
	assert(edata_state_get(edata) == edata_state_get(trail));
	if (err) {
		edata_cache_small_put(tsdn, &central->ecs, trail);
		return NULL;
	}
	assert(edata_state_get(edata) == edata_state_get(trail));

	edata_size_set(edata, size);
	emap_split_commit(tsdn, central->emap, &prepare, edata, size, trail,
	    cursize - size);

	return trail;
}

edata_t *
hpa_central_alloc_reuse(tsdn_t *tsdn, hpa_central_t *central,
    size_t size_min, size_t size_goal) {
	assert((size_min & PAGE_MASK) == 0);
	assert((size_goal & PAGE_MASK) == 0);

	/*
	 * Fragmentation avoidance is more important in the HPA than giving the
	 * user their preferred amount of space, since we expect the average
	 * unused extent to be more costly (PAC extents can get purged away
	 * easily at any granularity; HPA extents are much more difficult to
	 * purge away if they get stranded).  So we always search for the
	 * earliest (in first-fit ordering) extent that can satisfy the request,
	 * and use it, regardless of the goal size.
	 */
	edata_t *edata = eset_fit(&central->eset, size_min, PAGE,
	    /* exact_only */ false, /* lg_max_fit */ SC_PTR_BITS);
	if (edata == NULL) {
		return NULL;
	}

	eset_remove(&central->eset, edata);
	/* Maybe the first fit is also under the limit. */
	if (edata_size_get(edata) <= size_goal) {
		goto label_success;
	}

	/* Otherwise, split. */
	edata_t *trail = hpa_central_split(tsdn, central, edata, size_goal);
	if (trail == NULL) {
		eset_insert(&central->eset, edata);
		return NULL;
	}
	emap_assert_mapped(tsdn, central->emap, trail);
	eset_insert(&central->eset, trail);

label_success:
	emap_assert_mapped(tsdn, central->emap, edata);
	assert(edata_size_get(edata) >= size_min);
	/*
	 * We don't yet support purging in the hpa_central; everything should be
	 * dirty.
	 */
	assert(edata_state_get(edata) == extent_state_dirty);
	assert(edata_base_get(edata) == edata_addr_get(edata));
	emap_update_edata_state(tsdn, central->emap, edata,
	    extent_state_active);
	return edata;
}

bool
hpa_central_alloc_grow(tsdn_t *tsdn, hpa_central_t *central,
    size_t size, edata_t *edata) {
	assert((size & PAGE_MASK) == 0);
	assert(edata_base_get(edata) == edata_addr_get(edata));
	assert(edata_size_get(edata) >= size);
	assert(edata_arena_ind_get(edata)
	    == base_ind_get(central->ecs.fallback->base));
	assert(edata_is_head_get(edata));
	assert(edata_state_get(edata) == extent_state_active);
	assert(edata_pai_get(edata) == EXTENT_PAI_HPA);
	assert(edata_slab_get(edata) == false);
	assert(edata_szind_get_maybe_invalid(edata) == SC_NSIZES);

	/* edata should be a new alloc, and hence not already mapped. */
	emap_assert_not_mapped(tsdn, central->emap, edata);

	size_t cursize = edata_size_get(edata);

	bool err = emap_register_boundary(tsdn, central->emap, edata, SC_NSIZES,
	    /* slab */ false);
	if (err) {
		return true;
	}
	/* No splitting is necessary. */
	if (cursize == size) {
		size_t sn = central->sn_next++;
		edata_sn_set(edata, sn);
		return false;
	}

	/* We should split. */
	edata_t *trail = hpa_central_split(tsdn, central, edata, size);
	if (trail == NULL) {
		emap_deregister_boundary(tsdn, central->emap, NULL);
		return true;
	}
	size_t sn = central->sn_next++;
	edata_sn_set(edata, sn);
	edata_sn_set(trail, sn);

	emap_update_edata_state(tsdn, central->emap, trail, extent_state_dirty);
	eset_insert(&central->eset, trail);
	return false;
}

/* Merges b into a, freeing b back to the edata cache.. */
static void
hpa_central_dalloc_merge(tsdn_t *tsdn, hpa_central_t *central, edata_t *a,
    edata_t *b) {
	assert(emap_edata_is_acquired(tsdn, central->emap, a));
	assert(emap_edata_is_acquired(tsdn, central->emap, b));

	emap_prepare_t prepare;
	emap_merge_prepare(tsdn, central->emap, &prepare, a, b);
	edata_size_set(a, edata_size_get(a) + edata_size_get(b));
	emap_merge_commit(tsdn, central->emap, &prepare, a, b);
	edata_cache_small_put(tsdn, &central->ecs, b);
}

void
hpa_central_dalloc(tsdn_t *tsdn, hpa_central_t *central, edata_t *edata) {
	assert(edata_state_get(edata) == extent_state_active);
	assert(edata_ps_get(edata) == NULL);

	/*
	 * These should really be called at the pa interface level, but
	 * currently they're not.
	 */
	edata_addr_set(edata, edata_base_get(edata));
	edata_zeroed_set(edata, false);

	/*
	 *  Merge forward first, so that the original *edata stays active state
	 *  for the second acquire (only necessary for sanity checking).
	 */
	edata_t *trail = emap_try_acquire_edata_neighbor(tsdn, central->emap,
	    edata, EXTENT_PAI_HPA, extent_state_dirty, /* forward */ true);
	if (trail != NULL) {
		eset_remove(&central->eset, trail);
		hpa_central_dalloc_merge(tsdn, central, edata, trail);
	}
	edata_t *lead = emap_try_acquire_edata_neighbor(tsdn, central->emap,
	    edata, EXTENT_PAI_HPA, extent_state_dirty, /* forward */ false);
	if (lead != NULL) {
		eset_remove(&central->eset, lead);
		hpa_central_dalloc_merge(tsdn, central, lead, edata);
		edata = lead;
	}

	emap_update_edata_state(tsdn, central->emap, edata, extent_state_dirty);
	eset_insert(&central->eset, edata);
}

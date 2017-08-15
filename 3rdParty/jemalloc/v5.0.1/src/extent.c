#define JEMALLOC_EXTENT_C_
#include "jemalloc/internal/jemalloc_preamble.h"
#include "jemalloc/internal/jemalloc_internal_includes.h"

#include "jemalloc/internal/assert.h"
#include "jemalloc/internal/extent_dss.h"
#include "jemalloc/internal/extent_mmap.h"
#include "jemalloc/internal/ph.h"
#include "jemalloc/internal/rtree.h"
#include "jemalloc/internal/mutex.h"
#include "jemalloc/internal/mutex_pool.h"

/******************************************************************************/
/* Data. */

rtree_t		extents_rtree;
/* Keyed by the address of the extent_t being protected. */
mutex_pool_t	extent_mutex_pool;

static const bitmap_info_t extents_bitmap_info =
    BITMAP_INFO_INITIALIZER(NPSIZES+1);

static void *extent_alloc_default(extent_hooks_t *extent_hooks, void *new_addr,
    size_t size, size_t alignment, bool *zero, bool *commit,
    unsigned arena_ind);
static bool extent_dalloc_default(extent_hooks_t *extent_hooks, void *addr,
    size_t size, bool committed, unsigned arena_ind);
static void extent_destroy_default(extent_hooks_t *extent_hooks, void *addr,
    size_t size, bool committed, unsigned arena_ind);
static bool extent_commit_default(extent_hooks_t *extent_hooks, void *addr,
    size_t size, size_t offset, size_t length, unsigned arena_ind);
static bool extent_commit_impl(tsdn_t *tsdn, arena_t *arena,
    extent_hooks_t **r_extent_hooks, extent_t *extent, size_t offset,
    size_t length, bool growing_retained);
static bool extent_decommit_default(extent_hooks_t *extent_hooks,
    void *addr, size_t size, size_t offset, size_t length, unsigned arena_ind);
#ifdef PAGES_CAN_PURGE_LAZY
static bool extent_purge_lazy_default(extent_hooks_t *extent_hooks, void *addr,
    size_t size, size_t offset, size_t length, unsigned arena_ind);
#endif
static bool extent_purge_lazy_impl(tsdn_t *tsdn, arena_t *arena,
    extent_hooks_t **r_extent_hooks, extent_t *extent, size_t offset,
    size_t length, bool growing_retained);
#ifdef PAGES_CAN_PURGE_FORCED
static bool extent_purge_forced_default(extent_hooks_t *extent_hooks,
    void *addr, size_t size, size_t offset, size_t length, unsigned arena_ind);
#endif
static bool extent_purge_forced_impl(tsdn_t *tsdn, arena_t *arena,
    extent_hooks_t **r_extent_hooks, extent_t *extent, size_t offset,
    size_t length, bool growing_retained);
#ifdef JEMALLOC_MAPS_COALESCE
static bool extent_split_default(extent_hooks_t *extent_hooks, void *addr,
    size_t size, size_t size_a, size_t size_b, bool committed,
    unsigned arena_ind);
#endif
static extent_t *extent_split_impl(tsdn_t *tsdn, arena_t *arena,
    extent_hooks_t **r_extent_hooks, extent_t *extent, size_t size_a,
    szind_t szind_a, bool slab_a, size_t size_b, szind_t szind_b, bool slab_b,
    bool growing_retained);
#ifdef JEMALLOC_MAPS_COALESCE
static bool extent_merge_default(extent_hooks_t *extent_hooks, void *addr_a,
    size_t size_a, void *addr_b, size_t size_b, bool committed,
    unsigned arena_ind);
#endif
static bool extent_merge_impl(tsdn_t *tsdn, arena_t *arena,
    extent_hooks_t **r_extent_hooks, extent_t *a, extent_t *b,
    bool growing_retained);

const extent_hooks_t	extent_hooks_default = {
	extent_alloc_default,
	extent_dalloc_default,
	extent_destroy_default,
	extent_commit_default,
	extent_decommit_default
#ifdef PAGES_CAN_PURGE_LAZY
	,
	extent_purge_lazy_default
#else
	,
	NULL
#endif
#ifdef PAGES_CAN_PURGE_FORCED
	,
	extent_purge_forced_default
#else
	,
	NULL
#endif
#ifdef JEMALLOC_MAPS_COALESCE
	,
	extent_split_default,
	extent_merge_default
#endif
};

/* Used exclusively for gdump triggering. */
static atomic_zu_t curpages;
static atomic_zu_t highpages;

/******************************************************************************/
/*
 * Function prototypes for static functions that are referenced prior to
 * definition.
 */

static void extent_deregister(tsdn_t *tsdn, extent_t *extent);
static extent_t *extent_recycle(tsdn_t *tsdn, arena_t *arena,
    extent_hooks_t **r_extent_hooks, extents_t *extents, void *new_addr,
    size_t usize, size_t pad, size_t alignment, bool slab, szind_t szind,
    bool *zero, bool *commit, bool growing_retained);
static extent_t *extent_try_coalesce(tsdn_t *tsdn, arena_t *arena,
    extent_hooks_t **r_extent_hooks, rtree_ctx_t *rtree_ctx, extents_t *extents,
    extent_t *extent, bool *coalesced, bool growing_retained);
static void extent_record(tsdn_t *tsdn, arena_t *arena,
    extent_hooks_t **r_extent_hooks, extents_t *extents, extent_t *extent,
    bool growing_retained);

/******************************************************************************/

rb_gen(UNUSED, extent_avail_, extent_tree_t, extent_t, rb_link,
    extent_esnead_comp)

typedef enum {
	lock_result_success,
	lock_result_failure,
	lock_result_no_extent
} lock_result_t;

static lock_result_t
extent_rtree_leaf_elm_try_lock(tsdn_t *tsdn, rtree_leaf_elm_t *elm,
    extent_t **result) {
	extent_t *extent1 = rtree_leaf_elm_extent_read(tsdn, &extents_rtree,
	    elm, true);

	if (extent1 == NULL) {
		return lock_result_no_extent;
	}
	/*
	 * It's possible that the extent changed out from under us, and with it
	 * the leaf->extent mapping.  We have to recheck while holding the lock.
	 */
	extent_lock(tsdn, extent1);
	extent_t *extent2 = rtree_leaf_elm_extent_read(tsdn,
	    &extents_rtree, elm, true);

	if (extent1 == extent2) {
		*result = extent1;
		return lock_result_success;
	} else {
		extent_unlock(tsdn, extent1);
		return lock_result_failure;
	}
}

/*
 * Returns a pool-locked extent_t * if there's one associated with the given
 * address, and NULL otherwise.
 */
static extent_t *
extent_lock_from_addr(tsdn_t *tsdn, rtree_ctx_t *rtree_ctx, void *addr) {
	extent_t *ret = NULL;
	rtree_leaf_elm_t *elm = rtree_leaf_elm_lookup(tsdn, &extents_rtree,
	    rtree_ctx, (uintptr_t)addr, false, false);
	if (elm == NULL) {
		return NULL;
	}
	lock_result_t lock_result;
	do {
		lock_result = extent_rtree_leaf_elm_try_lock(tsdn, elm, &ret);
	} while (lock_result == lock_result_failure);
	return ret;
}

extent_t *
extent_alloc(tsdn_t *tsdn, arena_t *arena) {
	malloc_mutex_lock(tsdn, &arena->extent_avail_mtx);
	extent_t *extent = extent_avail_first(&arena->extent_avail);
	if (extent == NULL) {
		malloc_mutex_unlock(tsdn, &arena->extent_avail_mtx);
		return base_alloc_extent(tsdn, arena->base);
	}
	extent_avail_remove(&arena->extent_avail, extent);
	malloc_mutex_unlock(tsdn, &arena->extent_avail_mtx);
	return extent;
}

void
extent_dalloc(tsdn_t *tsdn, arena_t *arena, extent_t *extent) {
	malloc_mutex_lock(tsdn, &arena->extent_avail_mtx);
	extent_avail_insert(&arena->extent_avail, extent);
	malloc_mutex_unlock(tsdn, &arena->extent_avail_mtx);
}

extent_hooks_t *
extent_hooks_get(arena_t *arena) {
	return base_extent_hooks_get(arena->base);
}

extent_hooks_t *
extent_hooks_set(tsd_t *tsd, arena_t *arena, extent_hooks_t *extent_hooks) {
	background_thread_info_t *info;
	if (have_background_thread) {
		info = arena_background_thread_info_get(arena);
		malloc_mutex_lock(tsd_tsdn(tsd), &info->mtx);
	}
	extent_hooks_t *ret = base_extent_hooks_set(arena->base, extent_hooks);
	if (have_background_thread) {
		malloc_mutex_unlock(tsd_tsdn(tsd), &info->mtx);
	}

	return ret;
}

static void
extent_hooks_assure_initialized(arena_t *arena,
    extent_hooks_t **r_extent_hooks) {
	if (*r_extent_hooks == EXTENT_HOOKS_INITIALIZER) {
		*r_extent_hooks = extent_hooks_get(arena);
	}
}

#ifndef JEMALLOC_JET
static
#endif
size_t
extent_size_quantize_floor(size_t size) {
	size_t ret;
	pszind_t pind;

	assert(size > 0);
	assert((size & PAGE_MASK) == 0);

	pind = sz_psz2ind(size - sz_large_pad + 1);
	if (pind == 0) {
		/*
		 * Avoid underflow.  This short-circuit would also do the right
		 * thing for all sizes in the range for which there are
		 * PAGE-spaced size classes, but it's simplest to just handle
		 * the one case that would cause erroneous results.
		 */
		return size;
	}
	ret = sz_pind2sz(pind - 1) + sz_large_pad;
	assert(ret <= size);
	return ret;
}

#ifndef JEMALLOC_JET
static
#endif
size_t
extent_size_quantize_ceil(size_t size) {
	size_t ret;

	assert(size > 0);
	assert(size - sz_large_pad <= LARGE_MAXCLASS);
	assert((size & PAGE_MASK) == 0);

	ret = extent_size_quantize_floor(size);
	if (ret < size) {
		/*
		 * Skip a quantization that may have an adequately large extent,
		 * because under-sized extents may be mixed in.  This only
		 * happens when an unusual size is requested, i.e. for aligned
		 * allocation, and is just one of several places where linear
		 * search would potentially find sufficiently aligned available
		 * memory somewhere lower.
		 */
		ret = sz_pind2sz(sz_psz2ind(ret - sz_large_pad + 1)) +
		    sz_large_pad;
	}
	return ret;
}

/* Generate pairing heap functions. */
ph_gen(, extent_heap_, extent_heap_t, extent_t, ph_link, extent_snad_comp)

bool
extents_init(tsdn_t *tsdn, extents_t *extents, extent_state_t state,
    bool delay_coalesce) {
	if (malloc_mutex_init(&extents->mtx, "extents", WITNESS_RANK_EXTENTS,
	    malloc_mutex_rank_exclusive)) {
		return true;
	}
	for (unsigned i = 0; i < NPSIZES+1; i++) {
		extent_heap_new(&extents->heaps[i]);
	}
	bitmap_init(extents->bitmap, &extents_bitmap_info, true);
	extent_list_init(&extents->lru);
	atomic_store_zu(&extents->npages, 0, ATOMIC_RELAXED);
	extents->state = state;
	extents->delay_coalesce = delay_coalesce;
	return false;
}

extent_state_t
extents_state_get(const extents_t *extents) {
	return extents->state;
}

size_t
extents_npages_get(extents_t *extents) {
	return atomic_load_zu(&extents->npages, ATOMIC_RELAXED);
}

static void
extents_insert_locked(tsdn_t *tsdn, extents_t *extents, extent_t *extent,
    bool preserve_lru) {
	malloc_mutex_assert_owner(tsdn, &extents->mtx);
	assert(extent_state_get(extent) == extents->state);

	size_t size = extent_size_get(extent);
	size_t psz = extent_size_quantize_floor(size);
	pszind_t pind = sz_psz2ind(psz);
	if (extent_heap_empty(&extents->heaps[pind])) {
		bitmap_unset(extents->bitmap, &extents_bitmap_info,
		    (size_t)pind);
	}
	extent_heap_insert(&extents->heaps[pind], extent);
	if (!preserve_lru) {
		extent_list_append(&extents->lru, extent);
	}
	size_t npages = size >> LG_PAGE;
	/*
	 * All modifications to npages hold the mutex (as asserted above), so we
	 * don't need an atomic fetch-add; we can get by with a load followed by
	 * a store.
	 */
	size_t cur_extents_npages =
	    atomic_load_zu(&extents->npages, ATOMIC_RELAXED);
	atomic_store_zu(&extents->npages, cur_extents_npages + npages,
	    ATOMIC_RELAXED);
}

static void
extents_remove_locked(tsdn_t *tsdn, extents_t *extents, extent_t *extent,
    bool preserve_lru) {
	malloc_mutex_assert_owner(tsdn, &extents->mtx);
	assert(extent_state_get(extent) == extents->state);

	size_t size = extent_size_get(extent);
	size_t psz = extent_size_quantize_floor(size);
	pszind_t pind = sz_psz2ind(psz);
	extent_heap_remove(&extents->heaps[pind], extent);
	if (extent_heap_empty(&extents->heaps[pind])) {
		bitmap_set(extents->bitmap, &extents_bitmap_info,
		    (size_t)pind);
	}
	if (!preserve_lru) {
		extent_list_remove(&extents->lru, extent);
	}
	size_t npages = size >> LG_PAGE;
	/*
	 * As in extents_insert_locked, we hold extents->mtx and so don't need
	 * atomic operations for updating extents->npages.
	 */
	size_t cur_extents_npages =
	    atomic_load_zu(&extents->npages, ATOMIC_RELAXED);
	assert(cur_extents_npages >= npages);
	atomic_store_zu(&extents->npages,
	    cur_extents_npages - (size >> LG_PAGE), ATOMIC_RELAXED);
}

/* Do any-best-fit extent selection, i.e. select any extent that best fits. */
static extent_t *
extents_best_fit_locked(tsdn_t *tsdn, arena_t *arena, extents_t *extents,
    size_t size) {
	pszind_t pind = sz_psz2ind(extent_size_quantize_ceil(size));
	pszind_t i = (pszind_t)bitmap_ffu(extents->bitmap, &extents_bitmap_info,
	    (size_t)pind);
	if (i < NPSIZES+1) {
		assert(!extent_heap_empty(&extents->heaps[i]));
		extent_t *extent = extent_heap_any(&extents->heaps[i]);
		assert(extent_size_get(extent) >= size);
		return extent;
	}

	return NULL;
}

/*
 * Do first-fit extent selection, i.e. select the oldest/lowest extent that is
 * large enough.
 */
static extent_t *
extents_first_fit_locked(tsdn_t *tsdn, arena_t *arena, extents_t *extents,
    size_t size) {
	extent_t *ret = NULL;

	pszind_t pind = sz_psz2ind(extent_size_quantize_ceil(size));
	for (pszind_t i = (pszind_t)bitmap_ffu(extents->bitmap,
	    &extents_bitmap_info, (size_t)pind); i < NPSIZES+1; i =
	    (pszind_t)bitmap_ffu(extents->bitmap, &extents_bitmap_info,
	    (size_t)i+1)) {
		assert(!extent_heap_empty(&extents->heaps[i]));
		extent_t *extent = extent_heap_first(&extents->heaps[i]);
		assert(extent_size_get(extent) >= size);
		if (ret == NULL || extent_snad_comp(extent, ret) < 0) {
			ret = extent;
		}
		if (i == NPSIZES) {
			break;
		}
		assert(i < NPSIZES);
	}

	return ret;
}

/*
 * Do {best,first}-fit extent selection, where the selection policy choice is
 * based on extents->delay_coalesce.  Best-fit selection requires less
 * searching, but its layout policy is less stable and may cause higher virtual
 * memory fragmentation as a side effect.
 */
static extent_t *
extents_fit_locked(tsdn_t *tsdn, arena_t *arena, extents_t *extents,
    size_t size) {
	malloc_mutex_assert_owner(tsdn, &extents->mtx);

	return extents->delay_coalesce ? extents_best_fit_locked(tsdn, arena,
	    extents, size) : extents_first_fit_locked(tsdn, arena, extents,
	    size);
}

static bool
extent_try_delayed_coalesce(tsdn_t *tsdn, arena_t *arena,
    extent_hooks_t **r_extent_hooks, rtree_ctx_t *rtree_ctx, extents_t *extents,
    extent_t *extent) {
	extent_state_set(extent, extent_state_active);
	bool coalesced;
	extent = extent_try_coalesce(tsdn, arena, r_extent_hooks, rtree_ctx,
	    extents, extent, &coalesced, false);
	extent_state_set(extent, extents_state_get(extents));

	if (!coalesced) {
		return true;
	}
	extents_insert_locked(tsdn, extents, extent, true);
	return false;
}

extent_t *
extents_alloc(tsdn_t *tsdn, arena_t *arena, extent_hooks_t **r_extent_hooks,
    extents_t *extents, void *new_addr, size_t size, size_t pad,
    size_t alignment, bool slab, szind_t szind, bool *zero, bool *commit) {
	assert(size + pad != 0);
	assert(alignment != 0);
	witness_assert_depth_to_rank(tsdn_witness_tsdp_get(tsdn),
	    WITNESS_RANK_CORE, 0);

	return extent_recycle(tsdn, arena, r_extent_hooks, extents, new_addr,
	    size, pad, alignment, slab, szind, zero, commit, false);
}

void
extents_dalloc(tsdn_t *tsdn, arena_t *arena, extent_hooks_t **r_extent_hooks,
    extents_t *extents, extent_t *extent) {
	assert(extent_base_get(extent) != NULL);
	assert(extent_size_get(extent) != 0);
	witness_assert_depth_to_rank(tsdn_witness_tsdp_get(tsdn),
	    WITNESS_RANK_CORE, 0);

	extent_addr_set(extent, extent_base_get(extent));
	extent_zeroed_set(extent, false);

	extent_record(tsdn, arena, r_extent_hooks, extents, extent, false);
}

extent_t *
extents_evict(tsdn_t *tsdn, arena_t *arena, extent_hooks_t **r_extent_hooks,
    extents_t *extents, size_t npages_min) {
	rtree_ctx_t rtree_ctx_fallback;
	rtree_ctx_t *rtree_ctx = tsdn_rtree_ctx(tsdn, &rtree_ctx_fallback);

	malloc_mutex_lock(tsdn, &extents->mtx);

	/*
	 * Get the LRU coalesced extent, if any.  If coalescing was delayed,
	 * the loop will iterate until the LRU extent is fully coalesced.
	 */
	extent_t *extent;
	while (true) {
		/* Get the LRU extent, if any. */
		extent = extent_list_first(&extents->lru);
		if (extent == NULL) {
			goto label_return;
		}
		/* Check the eviction limit. */
		size_t npages = extent_size_get(extent) >> LG_PAGE;
		size_t extents_npages = atomic_load_zu(&extents->npages,
		    ATOMIC_RELAXED);
		if (extents_npages - npages < npages_min) {
			extent = NULL;
			goto label_return;
		}
		extents_remove_locked(tsdn, extents, extent, false);
		if (!extents->delay_coalesce) {
			break;
		}
		/* Try to coalesce. */
		if (extent_try_delayed_coalesce(tsdn, arena, r_extent_hooks,
		    rtree_ctx, extents, extent)) {
			break;
		}
		/*
		 * The LRU extent was just coalesced and the result placed in
		 * the LRU at its neighbor's position.  Start over.
		 */
	}

	/*
	 * Either mark the extent active or deregister it to protect against
	 * concurrent operations.
	 */
	switch (extents_state_get(extents)) {
	case extent_state_active:
		not_reached();
	case extent_state_dirty:
	case extent_state_muzzy:
		extent_state_set(extent, extent_state_active);
		break;
	case extent_state_retained:
		extent_deregister(tsdn, extent);
		break;
	default:
		not_reached();
	}

label_return:
	malloc_mutex_unlock(tsdn, &extents->mtx);
	return extent;
}

static void
extents_leak(tsdn_t *tsdn, arena_t *arena, extent_hooks_t **r_extent_hooks,
    extents_t *extents, extent_t *extent, bool growing_retained) {
	/*
	 * Leak extent after making sure its pages have already been purged, so
	 * that this is only a virtual memory leak.
	 */
	if (extents_state_get(extents) == extent_state_dirty) {
		if (extent_purge_lazy_impl(tsdn, arena, r_extent_hooks,
		    extent, 0, extent_size_get(extent), growing_retained)) {
			extent_purge_forced_impl(tsdn, arena, r_extent_hooks,
			    extent, 0, extent_size_get(extent),
			    growing_retained);
		}
	}
	extent_dalloc(tsdn, arena, extent);
}

void
extents_prefork(tsdn_t *tsdn, extents_t *extents) {
	malloc_mutex_prefork(tsdn, &extents->mtx);
}

void
extents_postfork_parent(tsdn_t *tsdn, extents_t *extents) {
	malloc_mutex_postfork_parent(tsdn, &extents->mtx);
}

void
extents_postfork_child(tsdn_t *tsdn, extents_t *extents) {
	malloc_mutex_postfork_child(tsdn, &extents->mtx);
}

static void
extent_deactivate_locked(tsdn_t *tsdn, arena_t *arena, extents_t *extents,
    extent_t *extent, bool preserve_lru) {
	assert(extent_arena_get(extent) == arena);
	assert(extent_state_get(extent) == extent_state_active);

	extent_state_set(extent, extents_state_get(extents));
	extents_insert_locked(tsdn, extents, extent, preserve_lru);
}

static void
extent_deactivate(tsdn_t *tsdn, arena_t *arena, extents_t *extents,
    extent_t *extent, bool preserve_lru) {
	malloc_mutex_lock(tsdn, &extents->mtx);
	extent_deactivate_locked(tsdn, arena, extents, extent, preserve_lru);
	malloc_mutex_unlock(tsdn, &extents->mtx);
}

static void
extent_activate_locked(tsdn_t *tsdn, arena_t *arena, extents_t *extents,
    extent_t *extent, bool preserve_lru) {
	assert(extent_arena_get(extent) == arena);
	assert(extent_state_get(extent) == extents_state_get(extents));

	extents_remove_locked(tsdn, extents, extent, preserve_lru);
	extent_state_set(extent, extent_state_active);
}

static bool
extent_rtree_leaf_elms_lookup(tsdn_t *tsdn, rtree_ctx_t *rtree_ctx,
    const extent_t *extent, bool dependent, bool init_missing,
    rtree_leaf_elm_t **r_elm_a, rtree_leaf_elm_t **r_elm_b) {
	*r_elm_a = rtree_leaf_elm_lookup(tsdn, &extents_rtree, rtree_ctx,
	    (uintptr_t)extent_base_get(extent), dependent, init_missing);
	if (!dependent && *r_elm_a == NULL) {
		return true;
	}
	assert(*r_elm_a != NULL);

	*r_elm_b = rtree_leaf_elm_lookup(tsdn, &extents_rtree, rtree_ctx,
	    (uintptr_t)extent_last_get(extent), dependent, init_missing);
	if (!dependent && *r_elm_b == NULL) {
		return true;
	}
	assert(*r_elm_b != NULL);

	return false;
}

static void
extent_rtree_write_acquired(tsdn_t *tsdn, rtree_leaf_elm_t *elm_a,
    rtree_leaf_elm_t *elm_b, extent_t *extent, szind_t szind, bool slab) {
	rtree_leaf_elm_write(tsdn, &extents_rtree, elm_a, extent, szind, slab);
	if (elm_b != NULL) {
		rtree_leaf_elm_write(tsdn, &extents_rtree, elm_b, extent, szind,
		    slab);
	}
}

static void
extent_interior_register(tsdn_t *tsdn, rtree_ctx_t *rtree_ctx, extent_t *extent,
    szind_t szind) {
	assert(extent_slab_get(extent));

	/* Register interior. */
	for (size_t i = 1; i < (extent_size_get(extent) >> LG_PAGE) - 1; i++) {
		rtree_write(tsdn, &extents_rtree, rtree_ctx,
		    (uintptr_t)extent_base_get(extent) + (uintptr_t)(i <<
		    LG_PAGE), extent, szind, true);
	}
}

static void
extent_gdump_add(tsdn_t *tsdn, const extent_t *extent) {
	cassert(config_prof);
	/* prof_gdump() requirement. */
	witness_assert_depth_to_rank(tsdn_witness_tsdp_get(tsdn),
	    WITNESS_RANK_CORE, 0);

	if (opt_prof && extent_state_get(extent) == extent_state_active) {
		size_t nadd = extent_size_get(extent) >> LG_PAGE;
		size_t cur = atomic_fetch_add_zu(&curpages, nadd,
		    ATOMIC_RELAXED) + nadd;
		size_t high = atomic_load_zu(&highpages, ATOMIC_RELAXED);
		while (cur > high && !atomic_compare_exchange_weak_zu(
		    &highpages, &high, cur, ATOMIC_RELAXED, ATOMIC_RELAXED)) {
			/*
			 * Don't refresh cur, because it may have decreased
			 * since this thread lost the highpages update race.
			 * Note that high is updated in case of CAS failure.
			 */
		}
		if (cur > high && prof_gdump_get_unlocked()) {
			prof_gdump(tsdn);
		}
	}
}

static void
extent_gdump_sub(tsdn_t *tsdn, const extent_t *extent) {
	cassert(config_prof);

	if (opt_prof && extent_state_get(extent) == extent_state_active) {
		size_t nsub = extent_size_get(extent) >> LG_PAGE;
		assert(atomic_load_zu(&curpages, ATOMIC_RELAXED) >= nsub);
		atomic_fetch_sub_zu(&curpages, nsub, ATOMIC_RELAXED);
	}
}

static bool
extent_register_impl(tsdn_t *tsdn, extent_t *extent, bool gdump_add) {
	rtree_ctx_t rtree_ctx_fallback;
	rtree_ctx_t *rtree_ctx = tsdn_rtree_ctx(tsdn, &rtree_ctx_fallback);
	rtree_leaf_elm_t *elm_a, *elm_b;

	/*
	 * We need to hold the lock to protect against a concurrent coalesce
	 * operation that sees us in a partial state.
	 */
	extent_lock(tsdn, extent);

	if (extent_rtree_leaf_elms_lookup(tsdn, rtree_ctx, extent, false, true,
	    &elm_a, &elm_b)) {
		return true;
	}

	szind_t szind = extent_szind_get_maybe_invalid(extent);
	bool slab = extent_slab_get(extent);
	extent_rtree_write_acquired(tsdn, elm_a, elm_b, extent, szind, slab);
	if (slab) {
		extent_interior_register(tsdn, rtree_ctx, extent, szind);
	}

	extent_unlock(tsdn, extent);

	if (config_prof && gdump_add) {
		extent_gdump_add(tsdn, extent);
	}

	return false;
}

static bool
extent_register(tsdn_t *tsdn, extent_t *extent) {
	return extent_register_impl(tsdn, extent, true);
}

static bool
extent_register_no_gdump_add(tsdn_t *tsdn, extent_t *extent) {
	return extent_register_impl(tsdn, extent, false);
}

static void
extent_reregister(tsdn_t *tsdn, extent_t *extent) {
	bool err = extent_register(tsdn, extent);
	assert(!err);
}

static void
extent_interior_deregister(tsdn_t *tsdn, rtree_ctx_t *rtree_ctx,
    extent_t *extent) {
	size_t i;

	assert(extent_slab_get(extent));

	for (i = 1; i < (extent_size_get(extent) >> LG_PAGE) - 1; i++) {
		rtree_clear(tsdn, &extents_rtree, rtree_ctx,
		    (uintptr_t)extent_base_get(extent) + (uintptr_t)(i <<
		    LG_PAGE));
	}
}

static void
extent_deregister(tsdn_t *tsdn, extent_t *extent) {
	rtree_ctx_t rtree_ctx_fallback;
	rtree_ctx_t *rtree_ctx = tsdn_rtree_ctx(tsdn, &rtree_ctx_fallback);
	rtree_leaf_elm_t *elm_a, *elm_b;
	extent_rtree_leaf_elms_lookup(tsdn, rtree_ctx, extent, true, false,
	    &elm_a, &elm_b);

	extent_lock(tsdn, extent);

	extent_rtree_write_acquired(tsdn, elm_a, elm_b, NULL, NSIZES, false);
	if (extent_slab_get(extent)) {
		extent_interior_deregister(tsdn, rtree_ctx, extent);
		extent_slab_set(extent, false);
	}

	extent_unlock(tsdn, extent);

	if (config_prof) {
		extent_gdump_sub(tsdn, extent);
	}
}

static extent_t *
extent_recycle_extract(tsdn_t *tsdn, arena_t *arena,
    extent_hooks_t **r_extent_hooks, rtree_ctx_t *rtree_ctx, extents_t *extents,
    void *new_addr, size_t size, size_t pad, size_t alignment, bool slab,
    bool *zero, bool *commit, bool growing_retained) {
	witness_assert_depth_to_rank(tsdn_witness_tsdp_get(tsdn),
	    WITNESS_RANK_CORE, growing_retained ? 1 : 0);
	assert(alignment > 0);
	if (config_debug && new_addr != NULL) {
		/*
		 * Non-NULL new_addr has two use cases:
		 *
		 *   1) Recycle a known-extant extent, e.g. during purging.
		 *   2) Perform in-place expanding reallocation.
		 *
		 * Regardless of use case, new_addr must either refer to a
		 * non-existing extent, or to the base of an extant extent,
		 * since only active slabs support interior lookups (which of
		 * course cannot be recycled).
		 */
		assert(PAGE_ADDR2BASE(new_addr) == new_addr);
		assert(pad == 0);
		assert(alignment <= PAGE);
	}

	size_t esize = size + pad;
	size_t alloc_size = esize + PAGE_CEILING(alignment) - PAGE;
	/* Beware size_t wrap-around. */
	if (alloc_size < esize) {
		return NULL;
	}
	malloc_mutex_lock(tsdn, &extents->mtx);
	extent_hooks_assure_initialized(arena, r_extent_hooks);
	extent_t *extent;
	if (new_addr != NULL) {
		extent = extent_lock_from_addr(tsdn, rtree_ctx, new_addr);
		if (extent != NULL) {
			/*
			 * We might null-out extent to report an error, but we
			 * still need to unlock the associated mutex after.
			 */
			extent_t *unlock_extent = extent;
			assert(extent_base_get(extent) == new_addr);
			if (extent_arena_get(extent) != arena ||
			    extent_size_get(extent) < esize ||
			    extent_state_get(extent) !=
			    extents_state_get(extents)) {
				extent = NULL;
			}
			extent_unlock(tsdn, unlock_extent);
		}
	} else {
		extent = extents_fit_locked(tsdn, arena, extents, alloc_size);
	}
	if (extent == NULL) {
		malloc_mutex_unlock(tsdn, &extents->mtx);
		return NULL;
	}

	extent_activate_locked(tsdn, arena, extents, extent, false);
	malloc_mutex_unlock(tsdn, &extents->mtx);

	if (extent_zeroed_get(extent)) {
		*zero = true;
	}
	if (extent_committed_get(extent)) {
		*commit = true;
	}

	return extent;
}

static extent_t *
extent_recycle_split(tsdn_t *tsdn, arena_t *arena,
    extent_hooks_t **r_extent_hooks, rtree_ctx_t *rtree_ctx, extents_t *extents,
    void *new_addr, size_t size, size_t pad, size_t alignment, bool slab,
    szind_t szind, extent_t *extent, bool growing_retained) {
	size_t esize = size + pad;
	size_t leadsize = ALIGNMENT_CEILING((uintptr_t)extent_base_get(extent),
	    PAGE_CEILING(alignment)) - (uintptr_t)extent_base_get(extent);
	assert(new_addr == NULL || leadsize == 0);
	assert(extent_size_get(extent) >= leadsize + esize);
	size_t trailsize = extent_size_get(extent) - leadsize - esize;

	/* Split the lead. */
	if (leadsize != 0) {
		extent_t *lead = extent;
		extent = extent_split_impl(tsdn, arena, r_extent_hooks,
		    lead, leadsize, NSIZES, false, esize + trailsize, szind,
		    slab, growing_retained);
		if (extent == NULL) {
			extent_deregister(tsdn, lead);
			extents_leak(tsdn, arena, r_extent_hooks, extents,
			    lead, growing_retained);
			return NULL;
		}
		extent_deactivate(tsdn, arena, extents, lead, false);
	}

	/* Split the trail. */
	if (trailsize != 0) {
		extent_t *trail = extent_split_impl(tsdn, arena,
		    r_extent_hooks, extent, esize, szind, slab, trailsize,
		    NSIZES, false, growing_retained);
		if (trail == NULL) {
			extent_deregister(tsdn, extent);
			extents_leak(tsdn, arena, r_extent_hooks, extents,
			    extent, growing_retained);
			return NULL;
		}
		extent_deactivate(tsdn, arena, extents, trail, false);
	} else if (leadsize == 0) {
		/*
		 * Splitting causes szind to be set as a side effect, but no
		 * splitting occurred.
		 */
		extent_szind_set(extent, szind);
		if (szind != NSIZES) {
			rtree_szind_slab_update(tsdn, &extents_rtree, rtree_ctx,
			    (uintptr_t)extent_addr_get(extent), szind, slab);
			if (slab && extent_size_get(extent) > PAGE) {
				rtree_szind_slab_update(tsdn, &extents_rtree,
				    rtree_ctx,
				    (uintptr_t)extent_past_get(extent) -
				    (uintptr_t)PAGE, szind, slab);
			}
		}
	}

	return extent;
}

static extent_t *
extent_recycle(tsdn_t *tsdn, arena_t *arena, extent_hooks_t **r_extent_hooks,
    extents_t *extents, void *new_addr, size_t size, size_t pad,
    size_t alignment, bool slab, szind_t szind, bool *zero, bool *commit,
    bool growing_retained) {
	witness_assert_depth_to_rank(tsdn_witness_tsdp_get(tsdn),
	    WITNESS_RANK_CORE, growing_retained ? 1 : 0);
	assert(new_addr == NULL || !slab);
	assert(pad == 0 || !slab);
	assert(!*zero || !slab);

	rtree_ctx_t rtree_ctx_fallback;
	rtree_ctx_t *rtree_ctx = tsdn_rtree_ctx(tsdn, &rtree_ctx_fallback);

	bool committed = false;
	extent_t *extent = extent_recycle_extract(tsdn, arena, r_extent_hooks,
	    rtree_ctx, extents, new_addr, size, pad, alignment, slab, zero,
	    &committed, growing_retained);
	if (extent == NULL) {
		return NULL;
	}
	if (committed) {
		*commit = true;
	}

	extent = extent_recycle_split(tsdn, arena, r_extent_hooks, rtree_ctx,
	    extents, new_addr, size, pad, alignment, slab, szind, extent,
	    growing_retained);
	if (extent == NULL) {
		return NULL;
	}

	if (*commit && !extent_committed_get(extent)) {
		if (extent_commit_impl(tsdn, arena, r_extent_hooks, extent,
		    0, extent_size_get(extent), growing_retained)) {
			extent_record(tsdn, arena, r_extent_hooks, extents,
			    extent, growing_retained);
			return NULL;
		}
		extent_zeroed_set(extent, true);
	}

	if (pad != 0) {
		extent_addr_randomize(tsdn, extent, alignment);
	}
	assert(extent_state_get(extent) == extent_state_active);
	if (slab) {
		extent_slab_set(extent, slab);
		extent_interior_register(tsdn, rtree_ctx, extent, szind);
	}

	if (*zero) {
		void *addr = extent_base_get(extent);
		size_t size = extent_size_get(extent);
		if (!extent_zeroed_get(extent)) {
			if (pages_purge_forced(addr, size)) {
				memset(addr, 0, size);
			}
		} else if (config_debug) {
			size_t *p = (size_t *)(uintptr_t)addr;
			for (size_t i = 0; i < size / sizeof(size_t); i++) {
				assert(p[i] == 0);
			}
		}
	}
	return extent;
}

/*
 * If the caller specifies (!*zero), it is still possible to receive zeroed
 * memory, in which case *zero is toggled to true.  arena_extent_alloc() takes
 * advantage of this to avoid demanding zeroed extents, but taking advantage of
 * them if they are returned.
 */
static void *
extent_alloc_core(tsdn_t *tsdn, arena_t *arena, void *new_addr, size_t size,
    size_t alignment, bool *zero, bool *commit, dss_prec_t dss_prec) {
	void *ret;

	assert(size != 0);
	assert(alignment != 0);

	/* "primary" dss. */
	if (have_dss && dss_prec == dss_prec_primary && (ret =
	    extent_alloc_dss(tsdn, arena, new_addr, size, alignment, zero,
	    commit)) != NULL) {
		return ret;
	}
	/* mmap. */
	if ((ret = extent_alloc_mmap(new_addr, size, alignment, zero, commit))
	    != NULL) {
		return ret;
	}
	/* "secondary" dss. */
	if (have_dss && dss_prec == dss_prec_secondary && (ret =
	    extent_alloc_dss(tsdn, arena, new_addr, size, alignment, zero,
	    commit)) != NULL) {
		return ret;
	}

	/* All strategies for allocation failed. */
	return NULL;
}

static void *
extent_alloc_default_impl(tsdn_t *tsdn, arena_t *arena, void *new_addr,
    size_t size, size_t alignment, bool *zero, bool *commit) {
	void *ret;

	ret = extent_alloc_core(tsdn, arena, new_addr, size, alignment, zero,
	    commit, (dss_prec_t)atomic_load_u(&arena->dss_prec,
	    ATOMIC_RELAXED));
	return ret;
}

static void *
extent_alloc_default(extent_hooks_t *extent_hooks, void *new_addr, size_t size,
    size_t alignment, bool *zero, bool *commit, unsigned arena_ind) {
	tsdn_t *tsdn;
	arena_t *arena;

	tsdn = tsdn_fetch();
	arena = arena_get(tsdn, arena_ind, false);
	/*
	 * The arena we're allocating on behalf of must have been initialized
	 * already.
	 */
	assert(arena != NULL);

	return extent_alloc_default_impl(tsdn, arena, new_addr, size,
	    alignment, zero, commit);
}

static void
extent_hook_pre_reentrancy(tsdn_t *tsdn, arena_t *arena) {
	tsd_t *tsd = tsdn_null(tsdn) ? tsd_fetch() : tsdn_tsd(tsdn);
	pre_reentrancy(tsd, arena);
}

static void
extent_hook_post_reentrancy(tsdn_t *tsdn) {
	tsd_t *tsd = tsdn_null(tsdn) ? tsd_fetch() : tsdn_tsd(tsdn);
	post_reentrancy(tsd);
}

/*
 * If virtual memory is retained, create increasingly larger extents from which
 * to split requested extents in order to limit the total number of disjoint
 * virtual memory ranges retained by each arena.
 */
static extent_t *
extent_grow_retained(tsdn_t *tsdn, arena_t *arena,
    extent_hooks_t **r_extent_hooks, size_t size, size_t pad, size_t alignment,
    bool slab, szind_t szind, bool *zero, bool *commit) {
	malloc_mutex_assert_owner(tsdn, &arena->extent_grow_mtx);
	assert(pad == 0 || !slab);
	assert(!*zero || !slab);

	size_t esize = size + pad;
	size_t alloc_size_min = esize + PAGE_CEILING(alignment) - PAGE;
	/* Beware size_t wrap-around. */
	if (alloc_size_min < esize) {
		goto label_err;
	}
	/*
	 * Find the next extent size in the series that would be large enough to
	 * satisfy this request.
	 */
	pszind_t egn_skip = 0;
	size_t alloc_size = sz_pind2sz(arena->extent_grow_next + egn_skip);
	while (alloc_size < alloc_size_min) {
		egn_skip++;
		if (arena->extent_grow_next + egn_skip == NPSIZES) {
			/* Outside legal range. */
			goto label_err;
		}
		assert(arena->extent_grow_next + egn_skip < NPSIZES);
		alloc_size = sz_pind2sz(arena->extent_grow_next + egn_skip);
	}

	extent_t *extent = extent_alloc(tsdn, arena);
	if (extent == NULL) {
		goto label_err;
	}
	bool zeroed = false;
	bool committed = false;

	void *ptr;
	if (*r_extent_hooks == &extent_hooks_default) {
		ptr = extent_alloc_core(tsdn, arena, NULL, alloc_size, PAGE,
		    &zeroed, &committed, (dss_prec_t)atomic_load_u(
		    &arena->dss_prec, ATOMIC_RELAXED));
	} else {
		extent_hook_pre_reentrancy(tsdn, arena);
		ptr = (*r_extent_hooks)->alloc(*r_extent_hooks, NULL,
		    alloc_size, PAGE, &zeroed, &committed,
		    arena_ind_get(arena));
		extent_hook_post_reentrancy(tsdn);
	}

	extent_init(extent, arena, ptr, alloc_size, false, NSIZES,
	    arena_extent_sn_next(arena), extent_state_active, zeroed,
	    committed);
	if (ptr == NULL) {
		extent_dalloc(tsdn, arena, extent);
		goto label_err;
	}
	if (extent_register_no_gdump_add(tsdn, extent)) {
		extents_leak(tsdn, arena, r_extent_hooks,
		    &arena->extents_retained, extent, true);
		goto label_err;
	}

	size_t leadsize = ALIGNMENT_CEILING((uintptr_t)ptr,
	    PAGE_CEILING(alignment)) - (uintptr_t)ptr;
	assert(alloc_size >= leadsize + esize);
	size_t trailsize = alloc_size - leadsize - esize;
	if (extent_zeroed_get(extent) && extent_committed_get(extent)) {
		*zero = true;
	}
	if (extent_committed_get(extent)) {
		*commit = true;
	}

	/* Split the lead. */
	if (leadsize != 0) {
		extent_t *lead = extent;
		extent = extent_split_impl(tsdn, arena, r_extent_hooks, lead,
		    leadsize, NSIZES, false, esize + trailsize, szind, slab,
		    true);
		if (extent == NULL) {
			extent_deregister(tsdn, lead);
			extents_leak(tsdn, arena, r_extent_hooks,
			    &arena->extents_retained, lead, true);
			goto label_err;
		}
		extent_record(tsdn, arena, r_extent_hooks,
		    &arena->extents_retained, lead, true);
	}

	/* Split the trail. */
	if (trailsize != 0) {
		extent_t *trail = extent_split_impl(tsdn, arena, r_extent_hooks,
		    extent, esize, szind, slab, trailsize, NSIZES, false, true);
		if (trail == NULL) {
			extent_deregister(tsdn, extent);
			extents_leak(tsdn, arena, r_extent_hooks,
			    &arena->extents_retained, extent, true);
			goto label_err;
		}
		extent_record(tsdn, arena, r_extent_hooks,
		    &arena->extents_retained, trail, true);
	} else if (leadsize == 0) {
		/*
		 * Splitting causes szind to be set as a side effect, but no
		 * splitting occurred.
		 */
		rtree_ctx_t rtree_ctx_fallback;
		rtree_ctx_t *rtree_ctx = tsdn_rtree_ctx(tsdn,
		    &rtree_ctx_fallback);

		extent_szind_set(extent, szind);
		if (szind != NSIZES) {
			rtree_szind_slab_update(tsdn, &extents_rtree, rtree_ctx,
			    (uintptr_t)extent_addr_get(extent), szind, slab);
			if (slab && extent_size_get(extent) > PAGE) {
				rtree_szind_slab_update(tsdn, &extents_rtree,
				    rtree_ctx,
				    (uintptr_t)extent_past_get(extent) -
				    (uintptr_t)PAGE, szind, slab);
			}
		}
	}

	if (*commit && !extent_committed_get(extent)) {
		if (extent_commit_impl(tsdn, arena, r_extent_hooks, extent, 0,
		    extent_size_get(extent), true)) {
			extent_record(tsdn, arena, r_extent_hooks,
			    &arena->extents_retained, extent, true);
			goto label_err;
		}
		extent_zeroed_set(extent, true);
	}

	/*
	 * Increment extent_grow_next if doing so wouldn't exceed the legal
	 * range.
	 */
	if (arena->extent_grow_next + egn_skip + 1 < NPSIZES) {
		arena->extent_grow_next += egn_skip + 1;
	} else {
		arena->extent_grow_next = NPSIZES - 1;
	}
	/* All opportunities for failure are past. */
	malloc_mutex_unlock(tsdn, &arena->extent_grow_mtx);

	if (config_prof) {
		/* Adjust gdump stats now that extent is final size. */
		extent_gdump_add(tsdn, extent);
	}
	if (pad != 0) {
		extent_addr_randomize(tsdn, extent, alignment);
	}
	if (slab) {
		rtree_ctx_t rtree_ctx_fallback;
		rtree_ctx_t *rtree_ctx = tsdn_rtree_ctx(tsdn,
		    &rtree_ctx_fallback);

		extent_slab_set(extent, true);
		extent_interior_register(tsdn, rtree_ctx, extent, szind);
	}
	if (*zero && !extent_zeroed_get(extent)) {
		void *addr = extent_base_get(extent);
		size_t size = extent_size_get(extent);
		if (pages_purge_forced(addr, size)) {
			memset(addr, 0, size);
		}
	}

	return extent;
label_err:
	malloc_mutex_unlock(tsdn, &arena->extent_grow_mtx);
	return NULL;
}

static extent_t *
extent_alloc_retained(tsdn_t *tsdn, arena_t *arena,
    extent_hooks_t **r_extent_hooks, void *new_addr, size_t size, size_t pad,
    size_t alignment, bool slab, szind_t szind, bool *zero, bool *commit) {
	assert(size != 0);
	assert(alignment != 0);

	malloc_mutex_lock(tsdn, &arena->extent_grow_mtx);

	extent_t *extent = extent_recycle(tsdn, arena, r_extent_hooks,
	    &arena->extents_retained, new_addr, size, pad, alignment, slab,
	    szind, zero, commit, true);
	if (extent != NULL) {
		malloc_mutex_unlock(tsdn, &arena->extent_grow_mtx);
		if (config_prof) {
			extent_gdump_add(tsdn, extent);
		}
	} else if (opt_retain && new_addr == NULL) {
		extent = extent_grow_retained(tsdn, arena, r_extent_hooks, size,
		    pad, alignment, slab, szind, zero, commit);
		/* extent_grow_retained() always releases extent_grow_mtx. */
	} else {
		malloc_mutex_unlock(tsdn, &arena->extent_grow_mtx);
	}
	malloc_mutex_assert_not_owner(tsdn, &arena->extent_grow_mtx);

	return extent;
}

static extent_t *
extent_alloc_wrapper_hard(tsdn_t *tsdn, arena_t *arena,
    extent_hooks_t **r_extent_hooks, void *new_addr, size_t size, size_t pad,
    size_t alignment, bool slab, szind_t szind, bool *zero, bool *commit) {
	size_t esize = size + pad;
	extent_t *extent = extent_alloc(tsdn, arena);
	if (extent == NULL) {
		return NULL;
	}
	void *addr;
	if (*r_extent_hooks == &extent_hooks_default) {
		/* Call directly to propagate tsdn. */
		addr = extent_alloc_default_impl(tsdn, arena, new_addr, esize,
		    alignment, zero, commit);
	} else {
		extent_hook_pre_reentrancy(tsdn, arena);
		addr = (*r_extent_hooks)->alloc(*r_extent_hooks, new_addr,
		    esize, alignment, zero, commit, arena_ind_get(arena));
		extent_hook_post_reentrancy(tsdn);
	}
	if (addr == NULL) {
		extent_dalloc(tsdn, arena, extent);
		return NULL;
	}
	extent_init(extent, arena, addr, esize, slab, szind,
	    arena_extent_sn_next(arena), extent_state_active, zero, commit);
	if (pad != 0) {
		extent_addr_randomize(tsdn, extent, alignment);
	}
	if (extent_register(tsdn, extent)) {
		extents_leak(tsdn, arena, r_extent_hooks,
		    &arena->extents_retained, extent, false);
		return NULL;
	}

	return extent;
}

extent_t *
extent_alloc_wrapper(tsdn_t *tsdn, arena_t *arena,
    extent_hooks_t **r_extent_hooks, void *new_addr, size_t size, size_t pad,
    size_t alignment, bool slab, szind_t szind, bool *zero, bool *commit) {
	witness_assert_depth_to_rank(tsdn_witness_tsdp_get(tsdn),
	    WITNESS_RANK_CORE, 0);

	extent_hooks_assure_initialized(arena, r_extent_hooks);

	extent_t *extent = extent_alloc_retained(tsdn, arena, r_extent_hooks,
	    new_addr, size, pad, alignment, slab, szind, zero, commit);
	if (extent == NULL) {
		extent = extent_alloc_wrapper_hard(tsdn, arena, r_extent_hooks,
		    new_addr, size, pad, alignment, slab, szind, zero, commit);
	}

	return extent;
}

static bool
extent_can_coalesce(arena_t *arena, extents_t *extents, const extent_t *inner,
    const extent_t *outer) {
	assert(extent_arena_get(inner) == arena);
	if (extent_arena_get(outer) != arena) {
		return false;
	}

	assert(extent_state_get(inner) == extent_state_active);
	if (extent_state_get(outer) != extents->state) {
		return false;
	}

	if (extent_committed_get(inner) != extent_committed_get(outer)) {
		return false;
	}

	return true;
}

static bool
extent_coalesce(tsdn_t *tsdn, arena_t *arena, extent_hooks_t **r_extent_hooks,
    extents_t *extents, extent_t *inner, extent_t *outer, bool forward,
    bool growing_retained) {
	assert(extent_can_coalesce(arena, extents, inner, outer));

	if (forward && extents->delay_coalesce) {
		/*
		 * The extent that remains after coalescing must occupy the
		 * outer extent's position in the LRU.  For forward coalescing,
		 * swap the inner extent into the LRU.
		 */
		extent_list_replace(&extents->lru, outer, inner);
	}
	extent_activate_locked(tsdn, arena, extents, outer,
	    extents->delay_coalesce);

	malloc_mutex_unlock(tsdn, &extents->mtx);
	bool err = extent_merge_impl(tsdn, arena, r_extent_hooks,
	    forward ? inner : outer, forward ? outer : inner, growing_retained);
	malloc_mutex_lock(tsdn, &extents->mtx);

	if (err) {
		if (forward && extents->delay_coalesce) {
			extent_list_replace(&extents->lru, inner, outer);
		}
		extent_deactivate_locked(tsdn, arena, extents, outer,
		    extents->delay_coalesce);
	}

	return err;
}

static extent_t *
extent_try_coalesce(tsdn_t *tsdn, arena_t *arena,
    extent_hooks_t **r_extent_hooks, rtree_ctx_t *rtree_ctx, extents_t *extents,
    extent_t *extent, bool *coalesced, bool growing_retained) {
	/*
	 * Continue attempting to coalesce until failure, to protect against
	 * races with other threads that are thwarted by this one.
	 */
	bool again;
	do {
		again = false;

		/* Try to coalesce forward. */
		extent_t *next = extent_lock_from_addr(tsdn, rtree_ctx,
		    extent_past_get(extent));
		if (next != NULL) {
			/*
			 * extents->mtx only protects against races for
			 * like-state extents, so call extent_can_coalesce()
			 * before releasing next's pool lock.
			 */
			bool can_coalesce = extent_can_coalesce(arena, extents,
			    extent, next);

			extent_unlock(tsdn, next);

			if (can_coalesce && !extent_coalesce(tsdn, arena,
			    r_extent_hooks, extents, extent, next, true,
			    growing_retained)) {
				if (extents->delay_coalesce) {
					/* Do minimal coalescing. */
					*coalesced = true;
					return extent;
				}
				again = true;
			}
		}

		/* Try to coalesce backward. */
		extent_t *prev = extent_lock_from_addr(tsdn, rtree_ctx,
		    extent_before_get(extent));
		if (prev != NULL) {
			bool can_coalesce = extent_can_coalesce(arena, extents,
			    extent, prev);
			extent_unlock(tsdn, prev);

			if (can_coalesce && !extent_coalesce(tsdn, arena,
			    r_extent_hooks, extents, extent, prev, false,
			    growing_retained)) {
				extent = prev;
				if (extents->delay_coalesce) {
					/* Do minimal coalescing. */
					*coalesced = true;
					return extent;
				}
				again = true;
			}
		}
	} while (again);

	if (extents->delay_coalesce) {
		*coalesced = false;
	}
	return extent;
}

static void
extent_record(tsdn_t *tsdn, arena_t *arena, extent_hooks_t **r_extent_hooks,
    extents_t *extents, extent_t *extent, bool growing_retained) {
	rtree_ctx_t rtree_ctx_fallback;
	rtree_ctx_t *rtree_ctx = tsdn_rtree_ctx(tsdn, &rtree_ctx_fallback);

	assert((extents_state_get(extents) != extent_state_dirty &&
	    extents_state_get(extents) != extent_state_muzzy) ||
	    !extent_zeroed_get(extent));

	malloc_mutex_lock(tsdn, &extents->mtx);
	extent_hooks_assure_initialized(arena, r_extent_hooks);

	extent_szind_set(extent, NSIZES);
	if (extent_slab_get(extent)) {
		extent_interior_deregister(tsdn, rtree_ctx, extent);
		extent_slab_set(extent, false);
	}

	assert(rtree_extent_read(tsdn, &extents_rtree, rtree_ctx,
	    (uintptr_t)extent_base_get(extent), true) == extent);

	if (!extents->delay_coalesce) {
		extent = extent_try_coalesce(tsdn, arena, r_extent_hooks,
		    rtree_ctx, extents, extent, NULL, growing_retained);
	}

	extent_deactivate_locked(tsdn, arena, extents, extent, false);

	malloc_mutex_unlock(tsdn, &extents->mtx);
}

void
extent_dalloc_gap(tsdn_t *tsdn, arena_t *arena, extent_t *extent) {
	extent_hooks_t *extent_hooks = EXTENT_HOOKS_INITIALIZER;

	witness_assert_depth_to_rank(tsdn_witness_tsdp_get(tsdn),
	    WITNESS_RANK_CORE, 0);

	if (extent_register(tsdn, extent)) {
		extents_leak(tsdn, arena, &extent_hooks,
		    &arena->extents_retained, extent, false);
		return;
	}
	extent_dalloc_wrapper(tsdn, arena, &extent_hooks, extent);
}

static bool
extent_dalloc_default_impl(void *addr, size_t size) {
	if (!have_dss || !extent_in_dss(addr)) {
		return extent_dalloc_mmap(addr, size);
	}
	return true;
}

static bool
extent_dalloc_default(extent_hooks_t *extent_hooks, void *addr, size_t size,
    bool committed, unsigned arena_ind) {
	return extent_dalloc_default_impl(addr, size);
}

static bool
extent_dalloc_wrapper_try(tsdn_t *tsdn, arena_t *arena,
    extent_hooks_t **r_extent_hooks, extent_t *extent) {
	bool err;

	assert(extent_base_get(extent) != NULL);
	assert(extent_size_get(extent) != 0);
	witness_assert_depth_to_rank(tsdn_witness_tsdp_get(tsdn),
	    WITNESS_RANK_CORE, 0);

	extent_addr_set(extent, extent_base_get(extent));

	extent_hooks_assure_initialized(arena, r_extent_hooks);
	/* Try to deallocate. */
	if (*r_extent_hooks == &extent_hooks_default) {
		/* Call directly to propagate tsdn. */
		err = extent_dalloc_default_impl(extent_base_get(extent),
		    extent_size_get(extent));
	} else {
		extent_hook_pre_reentrancy(tsdn, arena);
		err = ((*r_extent_hooks)->dalloc == NULL ||
		    (*r_extent_hooks)->dalloc(*r_extent_hooks,
		    extent_base_get(extent), extent_size_get(extent),
		    extent_committed_get(extent), arena_ind_get(arena)));
		extent_hook_post_reentrancy(tsdn);
	}

	if (!err) {
		extent_dalloc(tsdn, arena, extent);
	}

	return err;
}

void
extent_dalloc_wrapper(tsdn_t *tsdn, arena_t *arena,
    extent_hooks_t **r_extent_hooks, extent_t *extent) {
	witness_assert_depth_to_rank(tsdn_witness_tsdp_get(tsdn),
	    WITNESS_RANK_CORE, 0);

	/*
	 * Deregister first to avoid a race with other allocating threads, and
	 * reregister if deallocation fails.
	 */
	extent_deregister(tsdn, extent);
	if (!extent_dalloc_wrapper_try(tsdn, arena, r_extent_hooks, extent)) {
		return;
	}

	extent_reregister(tsdn, extent);
	if (*r_extent_hooks != &extent_hooks_default) {
		extent_hook_pre_reentrancy(tsdn, arena);
	}
	/* Try to decommit; purge if that fails. */
	bool zeroed;
	if (!extent_committed_get(extent)) {
		zeroed = true;
	} else if (!extent_decommit_wrapper(tsdn, arena, r_extent_hooks, extent,
	    0, extent_size_get(extent))) {
		zeroed = true;
	} else if ((*r_extent_hooks)->purge_forced != NULL &&
	    !(*r_extent_hooks)->purge_forced(*r_extent_hooks,
	    extent_base_get(extent), extent_size_get(extent), 0,
	    extent_size_get(extent), arena_ind_get(arena))) {
		zeroed = true;
	} else if (extent_state_get(extent) == extent_state_muzzy ||
	    ((*r_extent_hooks)->purge_lazy != NULL &&
	    !(*r_extent_hooks)->purge_lazy(*r_extent_hooks,
	    extent_base_get(extent), extent_size_get(extent), 0,
	    extent_size_get(extent), arena_ind_get(arena)))) {
		zeroed = false;
	} else {
		zeroed = false;
	}
	if (*r_extent_hooks != &extent_hooks_default) {
		extent_hook_post_reentrancy(tsdn);
	}
	extent_zeroed_set(extent, zeroed);

	if (config_prof) {
		extent_gdump_sub(tsdn, extent);
	}

	extent_record(tsdn, arena, r_extent_hooks, &arena->extents_retained,
	    extent, false);
}

static void
extent_destroy_default_impl(void *addr, size_t size) {
	if (!have_dss || !extent_in_dss(addr)) {
		pages_unmap(addr, size);
	}
}

static void
extent_destroy_default(extent_hooks_t *extent_hooks, void *addr, size_t size,
    bool committed, unsigned arena_ind) {
	extent_destroy_default_impl(addr, size);
}

void
extent_destroy_wrapper(tsdn_t *tsdn, arena_t *arena,
    extent_hooks_t **r_extent_hooks, extent_t *extent) {
	assert(extent_base_get(extent) != NULL);
	assert(extent_size_get(extent) != 0);
	witness_assert_depth_to_rank(tsdn_witness_tsdp_get(tsdn),
	    WITNESS_RANK_CORE, 0);

	/* Deregister first to avoid a race with other allocating threads. */
	extent_deregister(tsdn, extent);

	extent_addr_set(extent, extent_base_get(extent));

	extent_hooks_assure_initialized(arena, r_extent_hooks);
	/* Try to destroy; silently fail otherwise. */
	if (*r_extent_hooks == &extent_hooks_default) {
		/* Call directly to propagate tsdn. */
		extent_destroy_default_impl(extent_base_get(extent),
		    extent_size_get(extent));
	} else if ((*r_extent_hooks)->destroy != NULL) {
		extent_hook_pre_reentrancy(tsdn, arena);
		(*r_extent_hooks)->destroy(*r_extent_hooks,
		    extent_base_get(extent), extent_size_get(extent),
		    extent_committed_get(extent), arena_ind_get(arena));
		extent_hook_post_reentrancy(tsdn);
	}

	extent_dalloc(tsdn, arena, extent);
}

static bool
extent_commit_default(extent_hooks_t *extent_hooks, void *addr, size_t size,
    size_t offset, size_t length, unsigned arena_ind) {
	return pages_commit((void *)((uintptr_t)addr + (uintptr_t)offset),
	    length);
}

static bool
extent_commit_impl(tsdn_t *tsdn, arena_t *arena,
    extent_hooks_t **r_extent_hooks, extent_t *extent, size_t offset,
    size_t length, bool growing_retained) {
	witness_assert_depth_to_rank(tsdn_witness_tsdp_get(tsdn),
	    WITNESS_RANK_CORE, growing_retained ? 1 : 0);

	extent_hooks_assure_initialized(arena, r_extent_hooks);
	if (*r_extent_hooks != &extent_hooks_default) {
		extent_hook_pre_reentrancy(tsdn, arena);
	}
	bool err = ((*r_extent_hooks)->commit == NULL ||
	    (*r_extent_hooks)->commit(*r_extent_hooks, extent_base_get(extent),
	    extent_size_get(extent), offset, length, arena_ind_get(arena)));
	if (*r_extent_hooks != &extent_hooks_default) {
		extent_hook_post_reentrancy(tsdn);
	}
	extent_committed_set(extent, extent_committed_get(extent) || !err);
	return err;
}

bool
extent_commit_wrapper(tsdn_t *tsdn, arena_t *arena,
    extent_hooks_t **r_extent_hooks, extent_t *extent, size_t offset,
    size_t length) {
	return extent_commit_impl(tsdn, arena, r_extent_hooks, extent, offset,
	    length, false);
}

static bool
extent_decommit_default(extent_hooks_t *extent_hooks, void *addr, size_t size,
    size_t offset, size_t length, unsigned arena_ind) {
	return pages_decommit((void *)((uintptr_t)addr + (uintptr_t)offset),
	    length);
}

bool
extent_decommit_wrapper(tsdn_t *tsdn, arena_t *arena,
    extent_hooks_t **r_extent_hooks, extent_t *extent, size_t offset,
    size_t length) {
	witness_assert_depth_to_rank(tsdn_witness_tsdp_get(tsdn),
	    WITNESS_RANK_CORE, 0);

	extent_hooks_assure_initialized(arena, r_extent_hooks);

	if (*r_extent_hooks != &extent_hooks_default) {
		extent_hook_pre_reentrancy(tsdn, arena);
	}
	bool err = ((*r_extent_hooks)->decommit == NULL ||
	    (*r_extent_hooks)->decommit(*r_extent_hooks,
	    extent_base_get(extent), extent_size_get(extent), offset, length,
	    arena_ind_get(arena)));
	if (*r_extent_hooks != &extent_hooks_default) {
		extent_hook_post_reentrancy(tsdn);
	}
	extent_committed_set(extent, extent_committed_get(extent) && err);
	return err;
}

#ifdef PAGES_CAN_PURGE_LAZY
static bool
extent_purge_lazy_default(extent_hooks_t *extent_hooks, void *addr, size_t size,
    size_t offset, size_t length, unsigned arena_ind) {
	assert(addr != NULL);
	assert((offset & PAGE_MASK) == 0);
	assert(length != 0);
	assert((length & PAGE_MASK) == 0);

	return pages_purge_lazy((void *)((uintptr_t)addr + (uintptr_t)offset),
	    length);
}
#endif

static bool
extent_purge_lazy_impl(tsdn_t *tsdn, arena_t *arena,
    extent_hooks_t **r_extent_hooks, extent_t *extent, size_t offset,
    size_t length, bool growing_retained) {
	witness_assert_depth_to_rank(tsdn_witness_tsdp_get(tsdn),
	    WITNESS_RANK_CORE, growing_retained ? 1 : 0);

	extent_hooks_assure_initialized(arena, r_extent_hooks);

	if ((*r_extent_hooks)->purge_lazy == NULL) {
		return true;
	}
	if (*r_extent_hooks != &extent_hooks_default) {
		extent_hook_pre_reentrancy(tsdn, arena);
	}
	bool err = (*r_extent_hooks)->purge_lazy(*r_extent_hooks,
	    extent_base_get(extent), extent_size_get(extent), offset, length,
	    arena_ind_get(arena));
	if (*r_extent_hooks != &extent_hooks_default) {
		extent_hook_post_reentrancy(tsdn);
	}

	return err;
}

bool
extent_purge_lazy_wrapper(tsdn_t *tsdn, arena_t *arena,
    extent_hooks_t **r_extent_hooks, extent_t *extent, size_t offset,
    size_t length) {
	return extent_purge_lazy_impl(tsdn, arena, r_extent_hooks, extent,
	    offset, length, false);
}

#ifdef PAGES_CAN_PURGE_FORCED
static bool
extent_purge_forced_default(extent_hooks_t *extent_hooks, void *addr,
    size_t size, size_t offset, size_t length, unsigned arena_ind) {
	assert(addr != NULL);
	assert((offset & PAGE_MASK) == 0);
	assert(length != 0);
	assert((length & PAGE_MASK) == 0);

	return pages_purge_forced((void *)((uintptr_t)addr +
	    (uintptr_t)offset), length);
}
#endif

static bool
extent_purge_forced_impl(tsdn_t *tsdn, arena_t *arena,
    extent_hooks_t **r_extent_hooks, extent_t *extent, size_t offset,
    size_t length, bool growing_retained) {
	witness_assert_depth_to_rank(tsdn_witness_tsdp_get(tsdn),
	    WITNESS_RANK_CORE, growing_retained ? 1 : 0);

	extent_hooks_assure_initialized(arena, r_extent_hooks);

	if ((*r_extent_hooks)->purge_forced == NULL) {
		return true;
	}
	if (*r_extent_hooks != &extent_hooks_default) {
		extent_hook_pre_reentrancy(tsdn, arena);
	}
	bool err = (*r_extent_hooks)->purge_forced(*r_extent_hooks,
	    extent_base_get(extent), extent_size_get(extent), offset, length,
	    arena_ind_get(arena));
	if (*r_extent_hooks != &extent_hooks_default) {
		extent_hook_post_reentrancy(tsdn);
	}
	return err;
}

bool
extent_purge_forced_wrapper(tsdn_t *tsdn, arena_t *arena,
    extent_hooks_t **r_extent_hooks, extent_t *extent, size_t offset,
    size_t length) {
	return extent_purge_forced_impl(tsdn, arena, r_extent_hooks, extent,
	    offset, length, false);
}

#ifdef JEMALLOC_MAPS_COALESCE
static bool
extent_split_default(extent_hooks_t *extent_hooks, void *addr, size_t size,
    size_t size_a, size_t size_b, bool committed, unsigned arena_ind) {
	return !maps_coalesce;
}
#endif

static extent_t *
extent_split_impl(tsdn_t *tsdn, arena_t *arena,
    extent_hooks_t **r_extent_hooks, extent_t *extent, size_t size_a,
    szind_t szind_a, bool slab_a, size_t size_b, szind_t szind_b, bool slab_b,
    bool growing_retained) {
	assert(extent_size_get(extent) == size_a + size_b);
	witness_assert_depth_to_rank(tsdn_witness_tsdp_get(tsdn),
	    WITNESS_RANK_CORE, growing_retained ? 1 : 0);

	extent_hooks_assure_initialized(arena, r_extent_hooks);

	if ((*r_extent_hooks)->split == NULL) {
		return NULL;
	}

	extent_t *trail = extent_alloc(tsdn, arena);
	if (trail == NULL) {
		goto label_error_a;
	}

	extent_init(trail, arena, (void *)((uintptr_t)extent_base_get(extent) +
	    size_a), size_b, slab_b, szind_b, extent_sn_get(extent),
	    extent_state_get(extent), extent_zeroed_get(extent),
	    extent_committed_get(extent));

	rtree_ctx_t rtree_ctx_fallback;
	rtree_ctx_t *rtree_ctx = tsdn_rtree_ctx(tsdn, &rtree_ctx_fallback);
	rtree_leaf_elm_t *lead_elm_a, *lead_elm_b;
	{
		extent_t lead;

		extent_init(&lead, arena, extent_addr_get(extent), size_a,
		    slab_a, szind_a, extent_sn_get(extent),
		    extent_state_get(extent), extent_zeroed_get(extent),
		    extent_committed_get(extent));

		extent_rtree_leaf_elms_lookup(tsdn, rtree_ctx, &lead, false,
		    true, &lead_elm_a, &lead_elm_b);
	}
	rtree_leaf_elm_t *trail_elm_a, *trail_elm_b;
	extent_rtree_leaf_elms_lookup(tsdn, rtree_ctx, trail, false, true,
	    &trail_elm_a, &trail_elm_b);

	if (lead_elm_a == NULL || lead_elm_b == NULL || trail_elm_a == NULL
	    || trail_elm_b == NULL) {
		goto label_error_b;
	}

	extent_lock2(tsdn, extent, trail);

	if (*r_extent_hooks != &extent_hooks_default) {
		extent_hook_pre_reentrancy(tsdn, arena);
	}
	bool err = (*r_extent_hooks)->split(*r_extent_hooks, extent_base_get(extent),
	    size_a + size_b, size_a, size_b, extent_committed_get(extent),
	    arena_ind_get(arena));
	if (*r_extent_hooks != &extent_hooks_default) {
		extent_hook_post_reentrancy(tsdn);
	}
	if (err) {
		goto label_error_c;
	}

	extent_size_set(extent, size_a);
	extent_szind_set(extent, szind_a);

	extent_rtree_write_acquired(tsdn, lead_elm_a, lead_elm_b, extent,
	    szind_a, slab_a);
	extent_rtree_write_acquired(tsdn, trail_elm_a, trail_elm_b, trail,
	    szind_b, slab_b);

	extent_unlock2(tsdn, extent, trail);

	return trail;
label_error_c:
	extent_unlock2(tsdn, extent, trail);
label_error_b:
	extent_dalloc(tsdn, arena, trail);
label_error_a:
	return NULL;
}

extent_t *
extent_split_wrapper(tsdn_t *tsdn, arena_t *arena,
    extent_hooks_t **r_extent_hooks, extent_t *extent, size_t size_a,
    szind_t szind_a, bool slab_a, size_t size_b, szind_t szind_b, bool slab_b) {
	return extent_split_impl(tsdn, arena, r_extent_hooks, extent, size_a,
	    szind_a, slab_a, size_b, szind_b, slab_b, false);
}

static bool
extent_merge_default_impl(void *addr_a, void *addr_b) {
	if (!maps_coalesce) {
		return true;
	}
	if (have_dss && !extent_dss_mergeable(addr_a, addr_b)) {
		return true;
	}

	return false;
}

#ifdef JEMALLOC_MAPS_COALESCE
static bool
extent_merge_default(extent_hooks_t *extent_hooks, void *addr_a, size_t size_a,
    void *addr_b, size_t size_b, bool committed, unsigned arena_ind) {
	return extent_merge_default_impl(addr_a, addr_b);
}
#endif

static bool
extent_merge_impl(tsdn_t *tsdn, arena_t *arena,
    extent_hooks_t **r_extent_hooks, extent_t *a, extent_t *b,
    bool growing_retained) {
	witness_assert_depth_to_rank(tsdn_witness_tsdp_get(tsdn),
	    WITNESS_RANK_CORE, growing_retained ? 1 : 0);

	extent_hooks_assure_initialized(arena, r_extent_hooks);

	if ((*r_extent_hooks)->merge == NULL) {
		return true;
	}

	bool err;
	if (*r_extent_hooks == &extent_hooks_default) {
		/* Call directly to propagate tsdn. */
		err = extent_merge_default_impl(extent_base_get(a),
		    extent_base_get(b));
	} else {
		extent_hook_pre_reentrancy(tsdn, arena);
		err = (*r_extent_hooks)->merge(*r_extent_hooks,
		    extent_base_get(a), extent_size_get(a), extent_base_get(b),
		    extent_size_get(b), extent_committed_get(a),
		    arena_ind_get(arena));
		extent_hook_post_reentrancy(tsdn);
	}

	if (err) {
		return true;
	}

	/*
	 * The rtree writes must happen while all the relevant elements are
	 * owned, so the following code uses decomposed helper functions rather
	 * than extent_{,de}register() to do things in the right order.
	 */
	rtree_ctx_t rtree_ctx_fallback;
	rtree_ctx_t *rtree_ctx = tsdn_rtree_ctx(tsdn, &rtree_ctx_fallback);
	rtree_leaf_elm_t *a_elm_a, *a_elm_b, *b_elm_a, *b_elm_b;
	extent_rtree_leaf_elms_lookup(tsdn, rtree_ctx, a, true, false, &a_elm_a,
	    &a_elm_b);
	extent_rtree_leaf_elms_lookup(tsdn, rtree_ctx, b, true, false, &b_elm_a,
	    &b_elm_b);

	extent_lock2(tsdn, a, b);

	if (a_elm_b != NULL) {
		rtree_leaf_elm_write(tsdn, &extents_rtree, a_elm_b, NULL,
		    NSIZES, false);
	}
	if (b_elm_b != NULL) {
		rtree_leaf_elm_write(tsdn, &extents_rtree, b_elm_a, NULL,
		    NSIZES, false);
	} else {
		b_elm_b = b_elm_a;
	}

	extent_size_set(a, extent_size_get(a) + extent_size_get(b));
	extent_szind_set(a, NSIZES);
	extent_sn_set(a, (extent_sn_get(a) < extent_sn_get(b)) ?
	    extent_sn_get(a) : extent_sn_get(b));
	extent_zeroed_set(a, extent_zeroed_get(a) && extent_zeroed_get(b));

	extent_rtree_write_acquired(tsdn, a_elm_a, b_elm_b, a, NSIZES, false);

	extent_unlock2(tsdn, a, b);

	extent_dalloc(tsdn, extent_arena_get(b), b);

	return false;
}

bool
extent_merge_wrapper(tsdn_t *tsdn, arena_t *arena,
    extent_hooks_t **r_extent_hooks, extent_t *a, extent_t *b) {
	return extent_merge_impl(tsdn, arena, r_extent_hooks, a, b, false);
}

bool
extent_boot(void) {
	if (rtree_new(&extents_rtree, true)) {
		return true;
	}

	if (mutex_pool_init(&extent_mutex_pool, "extent_mutex_pool",
	    WITNESS_RANK_EXTENT_POOL)) {
		return true;
	}

	if (have_dss) {
		extent_dss_boot();
	}

	return false;
}

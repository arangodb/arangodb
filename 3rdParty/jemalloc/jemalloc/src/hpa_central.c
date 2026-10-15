#include "jemalloc/internal/jemalloc_preamble.h"
#include "jemalloc/internal/jemalloc_internal_includes.h"

#include "jemalloc/internal/hpa_central.h"
#include "jemalloc/internal/tsd.h"
#include "jemalloc/internal/witness.h"

#define HPA_EDEN_SIZE (128 * HUGEPAGE)

bool
hpa_central_init(
    hpa_central_t *central, base_t *base, const hpa_hooks_t *hooks) {
	/* malloc_conf processing should have filtered out these cases. */
	assert(hpa_supported());
	bool err;
	err = malloc_mutex_init(&central->grow_mtx, "hpa_central_grow",
	    WITNESS_RANK_HPA_CENTRAL_GROW, malloc_mutex_rank_exclusive);
	if (err) {
		return true;
	}

	central->base = base;
	central->eden = NULL;
	central->eden_len = 0;
	central->hooks = *hooks;
	return false;
}

static hpdata_t *
hpa_alloc_ps(tsdn_t *tsdn, hpa_central_t *central) {
	return (hpdata_t *)base_alloc(
	    tsdn, central->base, sizeof(hpdata_t), CACHELINE);
}

hpdata_t *
hpa_central_extract(tsdn_t *tsdn, hpa_central_t *central, size_t size,
    uint64_t age, bool hugify_eager, bool *oom) {
	/* Don't yet support big allocations; these should get filtered out. */
	assert(size <= HUGEPAGE);
	/*
	 * Should only try to extract from the central allocator if the local
	 * shard is exhausted.  We should hold the grow_mtx on that shard.
	 */
	witness_assert_positive_depth_to_rank(
	    tsdn_witness_tsdp_get(tsdn), WITNESS_RANK_HPA_SHARD_GROW);

	malloc_mutex_lock(tsdn, &central->grow_mtx);
	*oom = false;

	hpdata_t *ps = NULL;
	bool      start_as_huge = hugify_eager
	    || (init_system_thp_mode == system_thp_mode_always
	        && opt_experimental_hpa_start_huge_if_thp_always);

	/* Is eden a perfect fit? */
	if (central->eden != NULL && central->eden_len == HUGEPAGE) {
		ps = hpa_alloc_ps(tsdn, central);
		if (ps == NULL) {
			*oom = true;
			malloc_mutex_unlock(tsdn, &central->grow_mtx);
			return NULL;
		}
		hpdata_init(ps, central->eden, age, start_as_huge);
		central->eden = NULL;
		central->eden_len = 0;
		malloc_mutex_unlock(tsdn, &central->grow_mtx);
		return ps;
	}

	/*
	 * We're about to try to allocate from eden by splitting.  If eden is
	 * NULL, we have to allocate it too.  Otherwise, we just have to
	 * allocate an edata_t for the new psset.
	 */
	if (central->eden == NULL) {
		/* Allocate address space, bailing if we fail. */
		void *new_eden = central->hooks.map(HPA_EDEN_SIZE);
		if (new_eden == NULL) {
			*oom = true;
			malloc_mutex_unlock(tsdn, &central->grow_mtx);
			return NULL;
		}
		if (hugify_eager) {
			central->hooks.hugify(
			    new_eden, HPA_EDEN_SIZE, /* sync */ false);
		}
		ps = hpa_alloc_ps(tsdn, central);
		if (ps == NULL) {
			central->hooks.unmap(new_eden, HPA_EDEN_SIZE);
			*oom = true;
			malloc_mutex_unlock(tsdn, &central->grow_mtx);
			return NULL;
		}
		central->eden = new_eden;
		central->eden_len = HPA_EDEN_SIZE;
	} else {
		/* Eden is already nonempty; only need an edata for ps. */
		ps = hpa_alloc_ps(tsdn, central);
		if (ps == NULL) {
			*oom = true;
			malloc_mutex_unlock(tsdn, &central->grow_mtx);
			return NULL;
		}
	}
	assert(ps != NULL);
	assert(central->eden != NULL);
	assert(central->eden_len > HUGEPAGE);
	assert(central->eden_len % HUGEPAGE == 0);
	assert(HUGEPAGE_ADDR2BASE(central->eden) == central->eden);

	hpdata_init(ps, central->eden, age, start_as_huge);

	char *eden_char = (char *)central->eden;
	eden_char += HUGEPAGE;
	central->eden = (void *)eden_char;
	central->eden_len -= HUGEPAGE;

	malloc_mutex_unlock(tsdn, &central->grow_mtx);

	return ps;
}

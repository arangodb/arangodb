#include "jemalloc/internal/jemalloc_preamble.h"
#include "jemalloc/internal/jemalloc_internal_includes.h"

#include "jemalloc/internal/hpa.h"

static void
pa_nactive_add(pa_shard_t *shard, size_t add_pages) {
	atomic_fetch_add_zu(&shard->nactive, add_pages, ATOMIC_RELAXED);
}

static void
pa_nactive_sub(pa_shard_t *shard, size_t sub_pages) {
	assert(atomic_load_zu(&shard->nactive, ATOMIC_RELAXED) >= sub_pages);
	atomic_fetch_sub_zu(&shard->nactive, sub_pages, ATOMIC_RELAXED);
}

bool
pa_shard_init(tsdn_t *tsdn, pa_shard_t *shard, emap_t *emap, base_t *base,
    unsigned ind, pa_shard_stats_t *stats, malloc_mutex_t *stats_mtx,
    nstime_t *cur_time, size_t oversize_threshold, ssize_t dirty_decay_ms,
    ssize_t muzzy_decay_ms) {
	/* This will change eventually, but for now it should hold. */
	assert(base_ind_get(base) == ind);
	if (edata_cache_init(&shard->edata_cache, base)) {
		return true;
	}

	if (pac_init(tsdn, &shard->pac, base, emap, &shard->edata_cache,
	    cur_time, oversize_threshold, dirty_decay_ms, muzzy_decay_ms,
	    &stats->pac_stats, stats_mtx)) {
		return true;
	}

	shard->ind = ind;

	shard->ever_used_hpa = false;
	atomic_store_b(&shard->use_hpa, false, ATOMIC_RELAXED);

	atomic_store_zu(&shard->nactive, 0, ATOMIC_RELAXED);

	shard->stats_mtx = stats_mtx;
	shard->stats = stats;
	memset(shard->stats, 0, sizeof(*shard->stats));

	shard->emap = emap;
	shard->base = base;

	return false;
}

bool
pa_shard_enable_hpa(pa_shard_t *shard, const hpa_shard_opts_t *hpa_opts,
    const sec_opts_t *hpa_sec_opts) {
	if (hpa_shard_init(&shard->hpa_shard, shard->emap, shard->base,
	    &shard->edata_cache, shard->ind, hpa_opts)) {
		return true;
	}
	if (sec_init(&shard->hpa_sec, &shard->hpa_shard.pai, hpa_sec_opts)) {
		return true;
	}
	shard->ever_used_hpa = true;
	atomic_store_b(&shard->use_hpa, true, ATOMIC_RELAXED);

	return false;
}

void
pa_shard_disable_hpa(tsdn_t *tsdn, pa_shard_t *shard) {
	atomic_store_b(&shard->use_hpa, false, ATOMIC_RELAXED);
	if (shard->ever_used_hpa) {
		sec_disable(tsdn, &shard->hpa_sec);
		hpa_shard_disable(tsdn, &shard->hpa_shard);
	}
}

void
pa_shard_reset(tsdn_t *tsdn, pa_shard_t *shard) {
	atomic_store_zu(&shard->nactive, 0, ATOMIC_RELAXED);
	if (shard->ever_used_hpa) {
		sec_flush(tsdn, &shard->hpa_sec);
	}
}

void
pa_shard_destroy(tsdn_t *tsdn, pa_shard_t *shard) {
	pac_destroy(tsdn, &shard->pac);
	if (shard->ever_used_hpa) {
		sec_flush(tsdn, &shard->hpa_sec);
		hpa_shard_disable(tsdn, &shard->hpa_shard);
	}
}

static pai_t *
pa_get_pai(pa_shard_t *shard, edata_t *edata) {
	return (edata_pai_get(edata) == EXTENT_PAI_PAC
	    ? &shard->pac.pai : &shard->hpa_sec.pai);
}

edata_t *
pa_alloc(tsdn_t *tsdn, pa_shard_t *shard, size_t size, size_t alignment,
    bool slab, szind_t szind, bool zero) {
	witness_assert_depth_to_rank(tsdn_witness_tsdp_get(tsdn),
	    WITNESS_RANK_CORE, 0);

	edata_t *edata = NULL;
	if (atomic_load_b(&shard->use_hpa, ATOMIC_RELAXED)) {
		edata = pai_alloc(tsdn, &shard->hpa_sec.pai, size, alignment,
		    zero);
	}
	/*
	 * Fall back to the PAC if the HPA is off or couldn't serve the given
	 * allocation request.
	 */
	if (edata == NULL) {
		edata = pai_alloc(tsdn, &shard->pac.pai, size, alignment, zero);
	}

	if (edata != NULL) {
		pa_nactive_add(shard, size >> LG_PAGE);
		emap_remap(tsdn, shard->emap, edata, szind, slab);
		edata_szind_set(edata, szind);
		edata_slab_set(edata, slab);
		if (slab && (size > 2 * PAGE)) {
			emap_register_interior(tsdn, shard->emap, edata, szind);
		}
	}
	if (edata != NULL) {
		assert(edata_arena_ind_get(edata) == shard->ind);
	}
	return edata;
}

bool
pa_expand(tsdn_t *tsdn, pa_shard_t *shard, edata_t *edata, size_t old_size,
    size_t new_size, szind_t szind, bool zero) {
	assert(new_size > old_size);
	assert(edata_size_get(edata) == old_size);
	assert((new_size & PAGE_MASK) == 0);

	size_t expand_amount = new_size - old_size;

	pai_t *pai = pa_get_pai(shard, edata);

	bool error = pai_expand(tsdn, pai, edata, old_size, new_size, zero);
	if (error) {
		return true;
	}

	pa_nactive_add(shard, expand_amount >> LG_PAGE);
	edata_szind_set(edata, szind);
	emap_remap(tsdn, shard->emap, edata, szind, /* slab */ false);
	return false;
}

bool
pa_shrink(tsdn_t *tsdn, pa_shard_t *shard, edata_t *edata, size_t old_size,
    size_t new_size, szind_t szind, bool *generated_dirty) {
	assert(new_size < old_size);
	assert(edata_size_get(edata) == old_size);
	assert((new_size & PAGE_MASK) == 0);
	size_t shrink_amount = old_size - new_size;

	*generated_dirty = false;
	pai_t *pai = pa_get_pai(shard, edata);
	bool error = pai_shrink(tsdn, pai, edata, old_size, new_size);
	if (error) {
		return true;
	}
	pa_nactive_sub(shard, shrink_amount >> LG_PAGE);
	*generated_dirty = (edata_pai_get(edata) == EXTENT_PAI_PAC);

	edata_szind_set(edata, szind);
	emap_remap(tsdn, shard->emap, edata, szind, /* slab */ false);
	return false;
}

void
pa_dalloc(tsdn_t *tsdn, pa_shard_t *shard, edata_t *edata,
    bool *generated_dirty) {
	emap_remap(tsdn, shard->emap, edata, SC_NSIZES, /* slab */ false);
	if (edata_slab_get(edata)) {
		emap_deregister_interior(tsdn, shard->emap, edata);
		edata_slab_set(edata, false);
	}
	edata_addr_set(edata, edata_base_get(edata));
	edata_szind_set(edata, SC_NSIZES);
	pa_nactive_sub(shard, edata_size_get(edata) >> LG_PAGE);
	pai_t *pai = pa_get_pai(shard, edata);
	pai_dalloc(tsdn, pai, edata);
	*generated_dirty = (edata_pai_get(edata) == EXTENT_PAI_PAC);
}

bool
pa_shard_retain_grow_limit_get_set(tsdn_t *tsdn, pa_shard_t *shard,
    size_t *old_limit, size_t *new_limit) {
	return pac_retain_grow_limit_get_set(tsdn, &shard->pac, old_limit,
	    new_limit);
}

bool
pa_decay_ms_set(tsdn_t *tsdn, pa_shard_t *shard, extent_state_t state,
    ssize_t decay_ms, pac_purge_eagerness_t eagerness) {
	return pac_decay_ms_set(tsdn, &shard->pac, state, decay_ms, eagerness);
}

ssize_t
pa_decay_ms_get(pa_shard_t *shard, extent_state_t state) {
	return pac_decay_ms_get(&shard->pac, state);
}

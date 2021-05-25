#include "jemalloc/internal/jemalloc_preamble.h"
#include "jemalloc/internal/jemalloc_internal_includes.h"

#include "jemalloc/internal/hpa.h"

#include "jemalloc/internal/flat_bitmap.h"
#include "jemalloc/internal/witness.h"

#define HPA_EDEN_SIZE (128 * HUGEPAGE)

static edata_t *hpa_alloc(tsdn_t *tsdn, pai_t *self, size_t size,
    size_t alignment, bool zero);
static size_t hpa_alloc_batch(tsdn_t *tsdn, pai_t *self, size_t size,
    size_t nallocs, edata_list_active_t *results);
static bool hpa_expand(tsdn_t *tsdn, pai_t *self, edata_t *edata,
    size_t old_size, size_t new_size, bool zero);
static bool hpa_shrink(tsdn_t *tsdn, pai_t *self, edata_t *edata,
    size_t old_size, size_t new_size);
static void hpa_dalloc(tsdn_t *tsdn, pai_t *self, edata_t *edata);
static void hpa_dalloc_batch(tsdn_t *tsdn, pai_t *self,
    edata_list_active_t *list);

bool
hpa_supported() {
#ifdef _WIN32
	/*
	 * At least until the API and implementation is somewhat settled, we
	 * don't want to try to debug the VM subsystem on the hardest-to-test
	 * platform.
	 */
	return false;
#endif
	if (!pages_can_hugify) {
		return false;
	}
	/*
	 * We fundamentally rely on a address-space-hungry growth strategy for
	 * hugepages.
	 */
	if (LG_SIZEOF_PTR != 3) {
		return false;
	}
	/*
	 * If we couldn't detect the value of HUGEPAGE, HUGEPAGE_PAGES becomes
	 * this sentinel value -- see the comment in pages.h.
	 */
	if (HUGEPAGE_PAGES == 1) {
		return false;
	}
	return true;
}

bool
hpa_shard_init(hpa_shard_t *shard, emap_t *emap, base_t *base,
    edata_cache_t *edata_cache, unsigned ind, const hpa_shard_opts_t *opts) {
	/* malloc_conf processing should have filtered out these cases. */
	assert(hpa_supported());
	bool err;
	err = malloc_mutex_init(&shard->grow_mtx, "hpa_shard_grow",
	    WITNESS_RANK_HPA_SHARD_GROW, malloc_mutex_rank_exclusive);
	if (err) {
		return true;
	}
	err = malloc_mutex_init(&shard->mtx, "hpa_shard",
	    WITNESS_RANK_HPA_SHARD, malloc_mutex_rank_exclusive);
	if (err) {
		return true;
	}

	assert(edata_cache != NULL);
	shard->base = base;
	edata_cache_small_init(&shard->ecs, edata_cache);
	psset_init(&shard->psset);
	shard->age_counter = 0;
	shard->eden = NULL;
	shard->eden_len = 0;
	shard->ind = ind;
	shard->emap = emap;

	shard->opts = *opts;

	shard->npending_purge = 0;

	shard->stats.npurge_passes = 0;
	shard->stats.npurges = 0;
	shard->stats.nhugifies = 0;
	shard->stats.ndehugifies = 0;

	/*
	 * Fill these in last, so that if an hpa_shard gets used despite
	 * initialization failing, we'll at least crash instead of just
	 * operating on corrupted data.
	 */
	shard->pai.alloc = &hpa_alloc;
	shard->pai.alloc_batch = &pai_alloc_batch_default;
	shard->pai.expand = &hpa_expand;
	shard->pai.shrink = &hpa_shrink;
	shard->pai.dalloc = &hpa_dalloc;
	shard->pai.dalloc_batch = &hpa_dalloc_batch;

	return false;
}

/*
 * Note that the stats functions here follow the usual stats naming conventions;
 * "merge" obtains the stats from some live object of instance, while "accum"
 * only combines the stats from one stats objet to another.  Hence the lack of
 * locking here.
 */
static void
hpa_shard_nonderived_stats_accum(hpa_shard_nonderived_stats_t *dst,
    hpa_shard_nonderived_stats_t *src) {
	dst->npurge_passes += src->npurge_passes;
	dst->npurges += src->npurges;
	dst->nhugifies += src->nhugifies;
	dst->ndehugifies += src->ndehugifies;
}

void
hpa_shard_stats_accum(hpa_shard_stats_t *dst, hpa_shard_stats_t *src) {
	psset_stats_accum(&dst->psset_stats, &src->psset_stats);
	hpa_shard_nonderived_stats_accum(&dst->nonderived_stats,
	    &src->nonderived_stats);
}

void
hpa_shard_stats_merge(tsdn_t *tsdn, hpa_shard_t *shard,
    hpa_shard_stats_t *dst) {
	malloc_mutex_lock(tsdn, &shard->grow_mtx);
	malloc_mutex_lock(tsdn, &shard->mtx);
	psset_stats_accum(&dst->psset_stats, &shard->psset.stats);
	hpa_shard_nonderived_stats_accum(&dst->nonderived_stats, &shard->stats);
	malloc_mutex_unlock(tsdn, &shard->mtx);
	malloc_mutex_unlock(tsdn, &shard->grow_mtx);
}

static hpdata_t *
hpa_alloc_ps(tsdn_t *tsdn, hpa_shard_t *shard) {
	return (hpdata_t *)base_alloc(tsdn, shard->base, sizeof(hpdata_t),
	    CACHELINE);
}

static bool
hpa_good_hugification_candidate(hpa_shard_t *shard, hpdata_t *ps) {
	/*
	 * Note that this needs to be >= rather than just >, because of the
	 * important special case in which the hugification threshold is exactly
	 * HUGEPAGE.
	 */
	return hpdata_nactive_get(ps) * PAGE
	    >= shard->opts.hugification_threshold;
}

static size_t
hpa_adjusted_ndirty(tsdn_t *tsdn, hpa_shard_t *shard) {
	malloc_mutex_assert_owner(tsdn, &shard->mtx);
	return psset_ndirty(&shard->psset) - shard->npending_purge;
}

static size_t
hpa_ndirty_max(tsdn_t *tsdn, hpa_shard_t *shard) {
	malloc_mutex_assert_owner(tsdn, &shard->mtx);
	if (shard->opts.dirty_mult == (fxp_t)-1) {
		return (size_t)-1;
	}
	return fxp_mul_frac(psset_nactive(&shard->psset),
	    shard->opts.dirty_mult);
}

static bool
hpa_hugify_blocked_by_ndirty(tsdn_t *tsdn, hpa_shard_t *shard) {
	malloc_mutex_assert_owner(tsdn, &shard->mtx);
	hpdata_t *to_hugify = psset_pick_hugify(&shard->psset);
	if (to_hugify == NULL) {
		return false;
	}
	return hpa_adjusted_ndirty(tsdn, shard)
	    + hpdata_nretained_get(to_hugify) > hpa_ndirty_max(tsdn, shard);
}

static bool
hpa_should_purge(tsdn_t *tsdn, hpa_shard_t *shard) {
	malloc_mutex_assert_owner(tsdn, &shard->mtx);
	if (hpa_adjusted_ndirty(tsdn, shard) > hpa_ndirty_max(tsdn, shard)) {
		return true;
	}
	if (hpa_hugify_blocked_by_ndirty(tsdn, shard)) {
		return true;
	}
	return false;
}

static void
hpa_update_purge_hugify_eligibility(tsdn_t *tsdn, hpa_shard_t *shard,
    hpdata_t *ps) {
	malloc_mutex_assert_owner(tsdn, &shard->mtx);
	if (hpdata_changing_state_get(ps)) {
		hpdata_purge_allowed_set(ps, false);
		hpdata_hugify_allowed_set(ps, false);
		return;
	}
	/*
	 * Hugepages are distinctly costly to purge, so try to avoid it unless
	 * they're *particularly* full of dirty pages.  Eventually, we should
	 * use a smarter / more dynamic heuristic for situations where we have
	 * to manually hugify.
	 *
	 * In situations where we don't manually hugify, this problem is
	 * reduced.  The "bad" situation we're trying to avoid is one's that's
	 * common in some Linux configurations (where both enabled and defrag
	 * are set to madvise) that can lead to long latency spikes on the first
	 * access after a hugification.  The ideal policy in such configurations
	 * is probably time-based for both purging and hugifying; only hugify a
	 * hugepage if it's met the criteria for some extended period of time,
	 * and only dehugify it if it's failed to meet the criteria for an
	 * extended period of time.  When background threads are on, we should
	 * try to take this hit on one of them, as well.
	 *
	 * I think the ideal setting is THP always enabled, and defrag set to
	 * deferred; in that case we don't need any explicit calls on the
	 * allocator's end at all; we just try to pack allocations in a
	 * hugepage-friendly manner and let the OS hugify in the background.
	 */
	hpdata_purge_allowed_set(ps, hpdata_ndirty_get(ps) > 0);
	if (hpa_good_hugification_candidate(shard, ps)
	    && !hpdata_huge_get(ps)) {
		hpdata_hugify_allowed_set(ps, true);
	}
}

static hpdata_t *
hpa_grow(tsdn_t *tsdn, hpa_shard_t *shard) {
	malloc_mutex_assert_owner(tsdn, &shard->grow_mtx);
	hpdata_t *ps = NULL;

	/* Is eden a perfect fit? */
	if (shard->eden != NULL && shard->eden_len == HUGEPAGE) {
		ps = hpa_alloc_ps(tsdn, shard);
		if (ps == NULL) {
			return NULL;
		}
		hpdata_init(ps, shard->eden, shard->age_counter++);
		shard->eden = NULL;
		shard->eden_len = 0;
		return ps;
	}

	/*
	 * We're about to try to allocate from eden by splitting.  If eden is
	 * NULL, we have to allocate it too.  Otherwise, we just have to
	 * allocate an edata_t for the new psset.
	 */
	if (shard->eden == NULL) {
		/*
		 * During development, we're primarily concerned with systems
		 * with overcommit.  Eventually, we should be more careful here.
		 */
		bool commit = true;
		/* Allocate address space, bailing if we fail. */
		void *new_eden = pages_map(NULL, HPA_EDEN_SIZE, HUGEPAGE,
		    &commit);
		if (new_eden == NULL) {
			return NULL;
		}
		ps = hpa_alloc_ps(tsdn, shard);
		if (ps == NULL) {
			pages_unmap(new_eden, HPA_EDEN_SIZE);
			return NULL;
		}
		shard->eden = new_eden;
		shard->eden_len = HPA_EDEN_SIZE;
	} else {
		/* Eden is already nonempty; only need an edata for ps. */
		ps = hpa_alloc_ps(tsdn, shard);
		if (ps == NULL) {
			return NULL;
		}
	}
	assert(ps != NULL);
	assert(shard->eden != NULL);
	assert(shard->eden_len > HUGEPAGE);
	assert(shard->eden_len % HUGEPAGE == 0);
	assert(HUGEPAGE_ADDR2BASE(shard->eden) == shard->eden);

	hpdata_init(ps, shard->eden, shard->age_counter++);

	char *eden_char = (char *)shard->eden;
	eden_char += HUGEPAGE;
	shard->eden = (void *)eden_char;
	shard->eden_len -= HUGEPAGE;

	return ps;
}

/* Returns whether or not we purged anything. */
static bool
hpa_try_purge(tsdn_t *tsdn, hpa_shard_t *shard) {
	malloc_mutex_assert_owner(tsdn, &shard->mtx);

	hpdata_t *to_purge = psset_pick_purge(&shard->psset);
	if (to_purge == NULL) {
		return false;
	}
	assert(hpdata_purge_allowed_get(to_purge));
	assert(!hpdata_changing_state_get(to_purge));

	/*
	 * Don't let anyone else purge or hugify this page while
	 * we're purging it (allocations and deallocations are
	 * OK).
	 */
	psset_update_begin(&shard->psset, to_purge);
	assert(hpdata_alloc_allowed_get(to_purge));
	hpdata_mid_purge_set(to_purge, true);
	hpdata_purge_allowed_set(to_purge, false);
	hpdata_hugify_allowed_set(to_purge, false);
	/*
	 * Unlike with hugification (where concurrent
	 * allocations are allowed), concurrent allocation out
	 * of a hugepage being purged is unsafe; we might hand
	 * out an extent for an allocation and then purge it
	 * (clearing out user data).
	 */
	hpdata_alloc_allowed_set(to_purge, false);
	psset_update_end(&shard->psset, to_purge);

	/* Gather all the metadata we'll need during the purge. */
	bool dehugify = hpdata_huge_get(to_purge);
	hpdata_purge_state_t purge_state;
	size_t num_to_purge = hpdata_purge_begin(to_purge, &purge_state);

	shard->npending_purge += num_to_purge;

	malloc_mutex_unlock(tsdn, &shard->mtx);

	/* Actually do the purging, now that the lock is dropped. */
	if (dehugify) {
		pages_nohuge(hpdata_addr_get(to_purge), HUGEPAGE);
	}
	size_t total_purged = 0;
	uint64_t purges_this_pass = 0;
	void *purge_addr;
	size_t purge_size;
	while (hpdata_purge_next(to_purge, &purge_state, &purge_addr,
	    &purge_size)) {
		total_purged += purge_size;
		assert(total_purged <= HUGEPAGE);
		purges_this_pass++;
		pages_purge_forced(purge_addr, purge_size);
	}

	malloc_mutex_lock(tsdn, &shard->mtx);
	/* The shard updates */
	shard->npending_purge -= num_to_purge;
	shard->stats.npurge_passes++;
	shard->stats.npurges += purges_this_pass;
	if (dehugify) {
		shard->stats.ndehugifies++;
	}

	/* The hpdata updates. */
	psset_update_begin(&shard->psset, to_purge);
	if (dehugify) {
		hpdata_dehugify(to_purge);
	}
	hpdata_purge_end(to_purge, &purge_state);
	hpdata_mid_purge_set(to_purge, false);

	hpdata_alloc_allowed_set(to_purge, true);
	hpa_update_purge_hugify_eligibility(tsdn, shard, to_purge);

	psset_update_end(&shard->psset, to_purge);

	return true;
}

/* Returns whether or not we hugified anything. */
static bool
hpa_try_hugify(tsdn_t *tsdn, hpa_shard_t *shard) {
	malloc_mutex_assert_owner(tsdn, &shard->mtx);

	if (hpa_hugify_blocked_by_ndirty(tsdn, shard)) {
		return false;
	}

	hpdata_t *to_hugify = psset_pick_hugify(&shard->psset);
	if (to_hugify == NULL) {
		return false;
	}
	assert(hpdata_hugify_allowed_get(to_hugify));
	assert(!hpdata_changing_state_get(to_hugify));

	/*
	 * Don't let anyone else purge or hugify this page while
	 * we're hugifying it (allocations and deallocations are
	 * OK).
	 */
	psset_update_begin(&shard->psset, to_hugify);
	hpdata_mid_hugify_set(to_hugify, true);
	hpdata_purge_allowed_set(to_hugify, false);
	hpdata_hugify_allowed_set(to_hugify, false);
	assert(hpdata_alloc_allowed_get(to_hugify));
	psset_update_end(&shard->psset, to_hugify);

	malloc_mutex_unlock(tsdn, &shard->mtx);

	bool err = pages_huge(hpdata_addr_get(to_hugify),
	    HUGEPAGE);
	/*
	 * It's not clear what we could do in case of error; we
	 * might get into situations where we loop trying to
	 * hugify some page and failing over and over again.
	 * Just eat the error and pretend we were successful.
	 */
	(void)err;

	malloc_mutex_lock(tsdn, &shard->mtx);
	shard->stats.nhugifies++;

	psset_update_begin(&shard->psset, to_hugify);
	hpdata_hugify(to_hugify);
	hpdata_mid_hugify_set(to_hugify, false);
	hpa_update_purge_hugify_eligibility(tsdn, shard, to_hugify);
	psset_update_end(&shard->psset, to_hugify);

	return true;
}

static void
hpa_do_deferred_work(tsdn_t *tsdn, hpa_shard_t *shard) {
	bool hugified;
	bool purged;
	size_t nloop = 0;
	/* Just *some* bound, to impose a worst-case latency bound. */
	size_t maxloops = 100;;
	do {
		malloc_mutex_assert_owner(tsdn, &shard->mtx);
		hugified = hpa_try_hugify(tsdn, shard);

		purged = false;
		if (hpa_should_purge(tsdn, shard)) {
			purged = hpa_try_purge(tsdn, shard);
		}
		malloc_mutex_assert_owner(tsdn, &shard->mtx);
	} while ((hugified || purged) && nloop++ < maxloops);
}

static edata_t *
hpa_try_alloc_one_no_grow(tsdn_t *tsdn, hpa_shard_t *shard, size_t size,
    bool *oom) {
	bool err;
	edata_t *edata = edata_cache_small_get(tsdn, &shard->ecs);
	if (edata == NULL) {
		*oom = true;
		return NULL;
	}

	hpdata_t *ps = psset_pick_alloc(&shard->psset, size);
	if (ps == NULL) {
		edata_cache_small_put(tsdn, &shard->ecs, edata);
		return NULL;
	}

	psset_update_begin(&shard->psset, ps);

	if (hpdata_empty(ps)) {
		/*
		 * If the pageslab used to be empty, treat it as though it's
		 * brand new for fragmentation-avoidance purposes; what we're
		 * trying to approximate is the age of the allocations *in* that
		 * pageslab, and the allocations in the new pageslab are
		 * definitionally the youngest in this hpa shard.
		 */
		hpdata_age_set(ps, shard->age_counter++);
	}

	void *addr = hpdata_reserve_alloc(ps, size);
	edata_init(edata, shard->ind, addr, size, /* slab */ false,
	    SC_NSIZES, /* sn */ hpdata_age_get(ps), extent_state_active,
	    /* zeroed */ false, /* committed */ true, EXTENT_PAI_HPA,
	    EXTENT_NOT_HEAD);
	edata_ps_set(edata, ps);

	/*
	 * This could theoretically be moved outside of the critical section,
	 * but that introduces the potential for a race.  Without the lock, the
	 * (initially nonempty, since this is the reuse pathway) pageslab we
	 * allocated out of could become otherwise empty while the lock is
	 * dropped.  This would force us to deal with a pageslab eviction down
	 * the error pathway, which is a pain.
	 */
	err = emap_register_boundary(tsdn, shard->emap, edata,
	    SC_NSIZES, /* slab */ false);
	if (err) {
		hpdata_unreserve(ps, edata_addr_get(edata),
		    edata_size_get(edata));
		/*
		 * We should arguably reset dirty state here, but this would
		 * require some sort of prepare + commit functionality that's a
		 * little much to deal with for now.
		 *
		 * We don't have a do_deferred_work down this pathway, on the
		 * principle that we didn't *really* affect shard state (we
		 * tweaked the stats, but our tweaks weren't really accurate).
		 */
		psset_update_end(&shard->psset, ps);
		edata_cache_small_put(tsdn, &shard->ecs, edata);
		*oom = true;
		return NULL;
	}

	hpa_update_purge_hugify_eligibility(tsdn, shard, ps);
	psset_update_end(&shard->psset, ps);
	return edata;
}

static size_t
hpa_try_alloc_batch_no_grow(tsdn_t *tsdn, hpa_shard_t *shard, size_t size,
    bool *oom, size_t nallocs, edata_list_active_t *results) {
	malloc_mutex_lock(tsdn, &shard->mtx);
	size_t nsuccess = 0;
	for (; nsuccess < nallocs; nsuccess++) {
		edata_t *edata = hpa_try_alloc_one_no_grow(tsdn, shard, size,
		    oom);
		if (edata == NULL) {
			break;
		}
		edata_list_active_append(results, edata);
	}

	hpa_do_deferred_work(tsdn, shard);
	malloc_mutex_unlock(tsdn, &shard->mtx);
	return nsuccess;
}

static size_t
hpa_alloc_batch_psset(tsdn_t *tsdn, hpa_shard_t *shard, size_t size,
    size_t nallocs, edata_list_active_t *results) {
	assert(size <= shard->opts.slab_max_alloc);
	bool oom = false;

	size_t nsuccess = hpa_try_alloc_batch_no_grow(tsdn, shard, size, &oom,
	    nallocs, results);

	if (nsuccess == nallocs || oom) {
		return nsuccess;
	}

	/*
	 * We didn't OOM, but weren't able to fill everything requested of us;
	 * try to grow.
	 */
	malloc_mutex_lock(tsdn, &shard->grow_mtx);
	/*
	 * Check for grow races; maybe some earlier thread expanded the psset
	 * in between when we dropped the main mutex and grabbed the grow mutex.
	 */
	nsuccess += hpa_try_alloc_batch_no_grow(tsdn, shard, size, &oom,
	    nallocs - nsuccess, results);
	if (nsuccess == nallocs || oom) {
		malloc_mutex_unlock(tsdn, &shard->grow_mtx);
		return nsuccess;
	}

	/*
	 * Note that we don't hold shard->mtx here (while growing);
	 * deallocations (and allocations of smaller sizes) may still succeed
	 * while we're doing this potentially expensive system call.
	 */
	hpdata_t *ps = hpa_grow(tsdn, shard);
	if (ps == NULL) {
		malloc_mutex_unlock(tsdn, &shard->grow_mtx);
		return nsuccess;
	}

	/*
	 * We got the pageslab; allocate from it.  This does an unlock followed
	 * by a lock on the same mutex, and holds the grow mutex while doing
	 * deferred work, but this is an uncommon path; the simplicity is worth
	 * it.
	 */
	malloc_mutex_lock(tsdn, &shard->mtx);
	psset_insert(&shard->psset, ps);
	malloc_mutex_unlock(tsdn, &shard->mtx);

	nsuccess += hpa_try_alloc_batch_no_grow(tsdn, shard, size, &oom,
	    nallocs - nsuccess, results);
	/*
	 * Drop grow_mtx before doing deferred work; other threads blocked on it
	 * should be allowed to proceed while we're working.
	 */
	malloc_mutex_unlock(tsdn, &shard->grow_mtx);

	return nsuccess;
}

static hpa_shard_t *
hpa_from_pai(pai_t *self) {
	assert(self->alloc = &hpa_alloc);
	assert(self->expand = &hpa_expand);
	assert(self->shrink = &hpa_shrink);
	assert(self->dalloc = &hpa_dalloc);
	return (hpa_shard_t *)self;
}

static size_t
hpa_alloc_batch(tsdn_t *tsdn, pai_t *self, size_t size, size_t nallocs,
    edata_list_active_t *results) {
	assert(nallocs > 0);
	assert((size & PAGE_MASK) == 0);
	witness_assert_depth_to_rank(tsdn_witness_tsdp_get(tsdn),
	    WITNESS_RANK_CORE, 0);
	hpa_shard_t *shard = hpa_from_pai(self);

	if (size > shard->opts.slab_max_alloc) {
		return 0;
	}

	size_t nsuccess = hpa_alloc_batch_psset(tsdn, shard, size, nallocs,
	    results);

	witness_assert_depth_to_rank(tsdn_witness_tsdp_get(tsdn),
	    WITNESS_RANK_CORE, 0);

	edata_t *edata;
	ql_foreach(edata, &results->head, ql_link_active) {
		emap_assert_mapped(tsdn, shard->emap, edata);
		assert(edata_pai_get(edata) == EXTENT_PAI_HPA);
		assert(edata_state_get(edata) == extent_state_active);
		assert(edata_arena_ind_get(edata) == shard->ind);
		assert(edata_szind_get_maybe_invalid(edata) == SC_NSIZES);
		assert(!edata_slab_get(edata));
		assert(edata_committed_get(edata));
		assert(edata_base_get(edata) == edata_addr_get(edata));
		assert(edata_base_get(edata) != NULL);
	}
	return nsuccess;
}

static edata_t *
hpa_alloc(tsdn_t *tsdn, pai_t *self, size_t size, size_t alignment, bool zero) {
	assert((size & PAGE_MASK) == 0);
	witness_assert_depth_to_rank(tsdn_witness_tsdp_get(tsdn),
	    WITNESS_RANK_CORE, 0);

	/* We don't handle alignment or zeroing for now. */
	if (alignment > PAGE || zero) {
		return NULL;
	}
	/*
	 * An alloc with alignment == PAGE and zero == false is equivalent to a
	 * batch alloc of 1.  Just do that, so we can share code.
	 */
	edata_list_active_t results;
	edata_list_active_init(&results);
	size_t nallocs = hpa_alloc_batch(tsdn, self, size, /* nallocs */ 1,
	    &results);
	assert(nallocs == 0 || nallocs == 1);
	edata_t *edata = edata_list_active_first(&results);
	return edata;
}

static bool
hpa_expand(tsdn_t *tsdn, pai_t *self, edata_t *edata,
    size_t old_size, size_t new_size, bool zero) {
	/* Expand not yet supported. */
	return true;
}

static bool
hpa_shrink(tsdn_t *tsdn, pai_t *self, edata_t *edata,
    size_t old_size, size_t new_size) {
	/* Shrink not yet supported. */
	return true;
}

static void
hpa_dalloc_prepare_unlocked(tsdn_t *tsdn, hpa_shard_t *shard, edata_t *edata) {
	malloc_mutex_assert_not_owner(tsdn, &shard->mtx);

	assert(edata_pai_get(edata) == EXTENT_PAI_HPA);
	assert(edata_state_get(edata) == extent_state_active);
	assert(edata_arena_ind_get(edata) == shard->ind);
	assert(edata_szind_get_maybe_invalid(edata) == SC_NSIZES);
	assert(!edata_slab_get(edata));
	assert(edata_committed_get(edata));
	assert(edata_base_get(edata) != NULL);

	/*
	 * Another thread shouldn't be trying to touch the metadata of an
	 * allocation being freed.  The one exception is a merge attempt from a
	 * lower-addressed PAC extent; in this case we have a nominal race on
	 * the edata metadata bits, but in practice the fact that the PAI bits
	 * are different will prevent any further access.  The race is bad, but
	 * benign in practice, and the long term plan is to track enough state
	 * in the rtree to prevent these merge attempts in the first place.
	 */
	edata_addr_set(edata, edata_base_get(edata));
	edata_zeroed_set(edata, false);
	emap_deregister_boundary(tsdn, shard->emap, edata);
}

static void
hpa_dalloc_locked(tsdn_t *tsdn, hpa_shard_t *shard, edata_t *edata) {
	malloc_mutex_assert_owner(tsdn, &shard->mtx);

	/*
	 * Release the metadata early, to avoid having to remember to do it
	 * while we're also doing tricky purging logic.  First, we need to grab
	 * a few bits of metadata from it.
	 *
	 * Note that the shard mutex protects ps's metadata too; it wouldn't be
	 * correct to try to read most information out of it without the lock.
	 */
	hpdata_t *ps = edata_ps_get(edata);
	/* Currently, all edatas come from pageslabs. */
	assert(ps != NULL);
	void *unreserve_addr = edata_addr_get(edata);
	size_t unreserve_size = edata_size_get(edata);
	edata_cache_small_put(tsdn, &shard->ecs, edata);

	psset_update_begin(&shard->psset, ps);
	hpdata_unreserve(ps, unreserve_addr, unreserve_size);
	hpa_update_purge_hugify_eligibility(tsdn, shard, ps);
	psset_update_end(&shard->psset, ps);
	hpa_do_deferred_work(tsdn, shard);
}

static void
hpa_dalloc(tsdn_t *tsdn, pai_t *self, edata_t *edata) {
	hpa_shard_t *shard = hpa_from_pai(self);

	hpa_dalloc_prepare_unlocked(tsdn, shard, edata);
	malloc_mutex_lock(tsdn, &shard->mtx);
	hpa_dalloc_locked(tsdn, shard, edata);
	malloc_mutex_unlock(tsdn, &shard->mtx);
}

static void
hpa_dalloc_batch(tsdn_t *tsdn, pai_t *self, edata_list_active_t *list) {
	hpa_shard_t *shard = hpa_from_pai(self);

	edata_t *edata;
	ql_foreach(edata, &list->head, ql_link_active) {
		hpa_dalloc_prepare_unlocked(tsdn, shard, edata);
	}

	malloc_mutex_lock(tsdn, &shard->mtx);
	/* Now, remove from the list. */
	while ((edata = edata_list_active_first(list)) != NULL) {
		edata_list_active_remove(list, edata);
		hpa_dalloc_locked(tsdn, shard, edata);
	}
	malloc_mutex_unlock(tsdn, &shard->mtx);
}

void
hpa_shard_disable(tsdn_t *tsdn, hpa_shard_t *shard) {
	malloc_mutex_lock(tsdn, &shard->mtx);
	edata_cache_small_disable(tsdn, &shard->ecs);
	malloc_mutex_unlock(tsdn, &shard->mtx);
}

static void
hpa_shard_assert_stats_empty(psset_bin_stats_t *bin_stats) {
	assert(bin_stats->npageslabs == 0);
	assert(bin_stats->nactive == 0);
}

static void
hpa_assert_empty(tsdn_t *tsdn, hpa_shard_t *shard, psset_t *psset) {
	malloc_mutex_assert_owner(tsdn, &shard->mtx);
	for (int huge = 0; huge <= 1; huge++) {
		hpa_shard_assert_stats_empty(&psset->stats.full_slabs[huge]);
		for (pszind_t i = 0; i < PSSET_NPSIZES; i++) {
			hpa_shard_assert_stats_empty(
			    &psset->stats.nonfull_slabs[i][huge]);
		}
	}
}

void
hpa_shard_destroy(tsdn_t *tsdn, hpa_shard_t *shard) {
	/*
	 * By the time we're here, the arena code should have dalloc'd all the
	 * active extents, which means we should have eventually evicted
	 * everything from the psset, so it shouldn't be able to serve even a
	 * 1-page allocation.
	 */
	if (config_debug) {
		malloc_mutex_lock(tsdn, &shard->mtx);
		hpa_assert_empty(tsdn, shard, &shard->psset);
		malloc_mutex_unlock(tsdn, &shard->mtx);
	}
	hpdata_t *ps;
	while ((ps = psset_pick_alloc(&shard->psset, PAGE)) != NULL) {
		/* There should be no allocations anywhere. */
		assert(hpdata_empty(ps));
		psset_remove(&shard->psset, ps);
		pages_unmap(hpdata_addr_get(ps), HUGEPAGE);
	}
}

void
hpa_shard_prefork3(tsdn_t *tsdn, hpa_shard_t *shard) {
	malloc_mutex_prefork(tsdn, &shard->grow_mtx);
}

void
hpa_shard_prefork4(tsdn_t *tsdn, hpa_shard_t *shard) {
	malloc_mutex_prefork(tsdn, &shard->mtx);
}

void
hpa_shard_postfork_parent(tsdn_t *tsdn, hpa_shard_t *shard) {
	malloc_mutex_postfork_parent(tsdn, &shard->grow_mtx);
	malloc_mutex_postfork_parent(tsdn, &shard->mtx);
}

void
hpa_shard_postfork_child(tsdn_t *tsdn, hpa_shard_t *shard) {
	malloc_mutex_postfork_child(tsdn, &shard->grow_mtx);
	malloc_mutex_postfork_child(tsdn, &shard->mtx);
}

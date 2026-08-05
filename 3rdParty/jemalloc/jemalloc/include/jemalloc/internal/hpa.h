#ifndef JEMALLOC_INTERNAL_HPA_H
#define JEMALLOC_INTERNAL_HPA_H

#include "jemalloc/internal/jemalloc_preamble.h"
#include "jemalloc/internal/base.h"
#include "jemalloc/internal/edata_cache.h"
#include "jemalloc/internal/emap.h"
#include "jemalloc/internal/exp_grow.h"
#include "jemalloc/internal/hpa_central.h"
#include "jemalloc/internal/hpa_hooks.h"
#include "jemalloc/internal/hpa_opts.h"
#include "jemalloc/internal/mutex.h"
#include "jemalloc/internal/pai.h"
#include "jemalloc/internal/psset.h"
#include "jemalloc/internal/sec.h"

typedef struct hpa_shard_nonderived_stats_s hpa_shard_nonderived_stats_t;
struct hpa_shard_nonderived_stats_s {
	/*
	 * The number of times we've purged within a hugepage.
	 *
	 * Guarded by mtx.
	 */
	uint64_t npurge_passes;
	/*
	 * The number of individual purge calls we perform (which should always
	 * be bigger than npurge_passes, since each pass purges at least one
	 * extent within a hugepage.
	 *
	 * Guarded by mtx.
	 */
	uint64_t npurges;

	/*
	 * The number of times we've hugified a pageslab.
	 *
	 * Guarded by mtx.
	 */
	uint64_t nhugifies;

	/*
	 * The number of times we've tried to hugify a pageslab, but failed.
	 *
	 * Guarded by mtx.
	 */
	uint64_t nhugify_failures;

	/*
	 * The number of times we've dehugified a pageslab.
	 *
	 * Guarded by mtx.
	 */
	uint64_t ndehugifies;
};

/* Completely derived; only used by CTL. */
typedef struct hpa_shard_stats_s hpa_shard_stats_t;
struct hpa_shard_stats_s {
	psset_stats_t                psset_stats;
	hpa_shard_nonderived_stats_t nonderived_stats;
	sec_stats_t                  secstats;
};

typedef struct hpa_shard_s hpa_shard_t;
struct hpa_shard_s {
	/*
	 * pai must be the first member; we cast from a pointer to it to a
	 * pointer to the hpa_shard_t.
	 */
	pai_t pai;

	/* The central allocator we get our hugepages from. */
	hpa_central_t *central;

	/* Protects most of this shard's state. */
	malloc_mutex_t mtx;

	/*
	 * Guards the shard's access to the central allocator (preventing
	 * multiple threads operating on this shard from accessing the central
	 * allocator).
	 */
	malloc_mutex_t grow_mtx;

	/* The base metadata allocator. */
	base_t *base;

	/*
	 * This edata cache is the one we use when allocating a small extent
	 * from a pageslab.  The pageslab itself comes from the centralized
	 * allocator, and so will use its edata_cache.
	 */
	edata_cache_fast_t ecf;

	/* Small extent cache (not guarded by mtx) */
	JEMALLOC_ALIGNED(CACHELINE) sec_t sec;

	psset_t psset;

	/*
	 * How many grow operations have occurred.
	 *
	 * Guarded by grow_mtx.
	 */
	uint64_t age_counter;

	/* The arena ind we're associated with. */
	unsigned ind;

	/*
	 * Our emap.  This is just a cache of the emap pointer in the associated
	 * hpa_central.
	 */
	emap_t *emap;

	/* The configuration choices for this hpa shard. */
	hpa_shard_opts_t opts;

	/*
	 * How many pages have we started but not yet finished purging in this
	 * hpa shard.
	 */
	size_t npending_purge;

	/*
	 * Those stats which are copied directly into the CTL-centric hpa shard
	 * stats.
	 */
	hpa_shard_nonderived_stats_t stats;

	/*
	 * Last time we performed purge on this shard.
	 */
	nstime_t last_purge;

	/*
	 * Last time when we attempted work (purging or hugifying). If deferral
	 * of the work is allowed (we have background thread), this is the time
	 * when background thread checked if purging or hugifying needs to be
	 * done. If deferral is not allowed, this is the time of (hpa_alloc or
	 * hpa_dalloc) activity in the shard.
	 */
	nstime_t last_time_work_attempted;
};

bool hpa_hugepage_size_exceeds_limit(void);
/*
 * Whether or not the HPA can be used given the current configuration.  This is
 * is not necessarily a guarantee that it backs its allocations by hugepages,
 * just that it can function properly given the system it's running on.
 */
bool hpa_supported(void);
bool hpa_shard_init(tsdn_t *tsdn, hpa_shard_t *shard, hpa_central_t *central,
    emap_t *emap, base_t *base, edata_cache_t *edata_cache, unsigned ind,
    const hpa_shard_opts_t *opts, const sec_opts_t *sec_opts);

void hpa_shard_stats_accum(hpa_shard_stats_t *dst, hpa_shard_stats_t *src);
void hpa_shard_stats_merge(
    tsdn_t *tsdn, hpa_shard_t *shard, hpa_shard_stats_t *dst);

/*
 * Notify the shard that we won't use it for allocations much longer.  Due to
 * the possibility of races, we don't actually prevent allocations; just flush
 * and disable the embedded edata_cache_small.
 */
void hpa_shard_disable(tsdn_t *tsdn, hpa_shard_t *shard);
void hpa_shard_destroy(tsdn_t *tsdn, hpa_shard_t *shard);
/* Flush caches that shard may be using */
void hpa_shard_flush(tsdn_t *tsdn, hpa_shard_t *shard);

void hpa_shard_set_deferral_allowed(
    tsdn_t *tsdn, hpa_shard_t *shard, bool deferral_allowed);
void hpa_shard_do_deferred_work(tsdn_t *tsdn, hpa_shard_t *shard);

/*
 * We share the fork ordering with the PA and arena prefork handling; that's why
 * these are 2, 3 and 4 rather than 0 and 1.
 */
void hpa_shard_prefork2(tsdn_t *tsdn, hpa_shard_t *shard);
void hpa_shard_prefork3(tsdn_t *tsdn, hpa_shard_t *shard);
void hpa_shard_prefork4(tsdn_t *tsdn, hpa_shard_t *shard);
void hpa_shard_postfork_parent(tsdn_t *tsdn, hpa_shard_t *shard);
void hpa_shard_postfork_child(tsdn_t *tsdn, hpa_shard_t *shard);

#endif /* JEMALLOC_INTERNAL_HPA_H */

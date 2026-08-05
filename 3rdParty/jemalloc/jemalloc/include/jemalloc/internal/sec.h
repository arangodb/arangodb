#ifndef JEMALLOC_INTERNAL_SEC_H
#define JEMALLOC_INTERNAL_SEC_H

#include "jemalloc/internal/jemalloc_preamble.h"
#include "jemalloc/internal/base.h"
#include "jemalloc/internal/atomic.h"
#include "jemalloc/internal/mutex.h"
#include "jemalloc/internal/pai.h"
#include "jemalloc/internal/sec_opts.h"

/*
 * Small extent cache.
 *
 * This includes some utilities to cache small extents.  We have a per-pszind
 * bin with its own list of extents of that size.  We don't try to do any
 * coalescing of extents (since it would in general require cross-shard locks or
 * knowledge of the underlying PAI implementation).
 */

typedef struct sec_bin_stats_s sec_bin_stats_t;
struct sec_bin_stats_s {
	/* Number of alloc requests that did not find extent in this bin */
	size_t nmisses;
	/* Number of successful alloc requests. */
	size_t nhits;
	/* Number of dallocs causing the flush */
	size_t ndalloc_flush;
	/* Number of dallocs not causing the flush */
	size_t ndalloc_noflush;
	/* Number of fills that hit max_bytes */
	size_t noverfills;
};
typedef struct sec_stats_s sec_stats_t;
struct sec_stats_s {
	/* Sum of bytes_cur across all shards. */
	size_t bytes;

	/* Totals of bin_stats. */
	sec_bin_stats_t total;
};

static inline void
sec_bin_stats_init(sec_bin_stats_t *stats) {
	stats->ndalloc_flush = 0;
	stats->nmisses = 0;
	stats->nhits = 0;
	stats->ndalloc_noflush = 0;
	stats->noverfills = 0;
}

static inline void
sec_bin_stats_accum(sec_bin_stats_t *dst, sec_bin_stats_t *src) {
	dst->nmisses += src->nmisses;
	dst->nhits += src->nhits;
	dst->ndalloc_flush += src->ndalloc_flush;
	dst->ndalloc_noflush += src->ndalloc_noflush;
	dst->noverfills += src->noverfills;
}

static inline void
sec_stats_accum(sec_stats_t *dst, sec_stats_t *src) {
	dst->bytes += src->bytes;
	sec_bin_stats_accum(&dst->total, &src->total);
}

/* A collections of free extents, all of the same size. */
typedef struct sec_bin_s sec_bin_t;
struct sec_bin_s {
	/*
	 * Protects the data members of the bin.
	 */
	malloc_mutex_t mtx;

	/*
	 * Number of bytes in this particular bin.
	 */
	size_t              bytes_cur;
	edata_list_active_t freelist;
	sec_bin_stats_t     stats;
};

typedef struct sec_s sec_t;
struct sec_s {
	sec_opts_t opts;
	sec_bin_t *bins;
	pszind_t   npsizes;
};

static inline bool
sec_is_used(sec_t *sec) {
	return sec->opts.nshards != 0;
}

static inline bool
sec_size_supported(sec_t *sec, size_t size) {
	return sec_is_used(sec) && size <= sec->opts.max_alloc;
}

/* If sec does not have extent available, it will return NULL. */
edata_t *sec_alloc(tsdn_t *tsdn, sec_t *sec, size_t size);
void     sec_fill(tsdn_t *tsdn, sec_t *sec, size_t size,
        edata_list_active_t *result, size_t nallocs);

/*
 * Upon return dalloc_list may be empty if edata is consumed by sec or non-empty
 * if there are extents that need to be flushed from cache.  Please note, that
 * if we need to flush, extent(s) returned in the list to be deallocated
 * will almost certainly not contain the one being dalloc-ed (that one will be
 * considered "hot" and preserved in the cache, while "colder" ones are
 * returned).
 */
void sec_dalloc(tsdn_t *tsdn, sec_t *sec, edata_list_active_t *dalloc_list);

bool sec_init(tsdn_t *tsdn, sec_t *sec, base_t *base, const sec_opts_t *opts);

/* Fills to_flush with extents that need to be deallocated */
void sec_flush(tsdn_t *tsdn, sec_t *sec, edata_list_active_t *to_flush);

/*
 * Morally, these two stats methods probably ought to be a single one (and the
 * mutex_prof_data ought to live in the sec_stats_t.  But splitting them apart
 * lets them fit easily into the pa_shard stats framework (which also has this
 * split), which simplifies the stats management.
 */
void sec_stats_merge(tsdn_t *tsdn, sec_t *sec, sec_stats_t *stats);
void sec_mutex_stats_read(
    tsdn_t *tsdn, sec_t *sec, mutex_prof_data_t *mutex_prof_data);

/*
 * We use the arena lock ordering; these are acquired in phase 2 of forking, but
 * should be acquired before the underlying allocator mutexes.
 */
void sec_prefork2(tsdn_t *tsdn, sec_t *sec);
void sec_postfork_parent(tsdn_t *tsdn, sec_t *sec);
void sec_postfork_child(tsdn_t *tsdn, sec_t *sec);

#endif /* JEMALLOC_INTERNAL_SEC_H */

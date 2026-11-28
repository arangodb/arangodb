#ifndef JEMALLOC_INTERNAL_BIN_H
#define JEMALLOC_INTERNAL_BIN_H

#include "jemalloc/internal/jemalloc_preamble.h"
#include "jemalloc/internal/bin_info.h"
#include "jemalloc/internal/bin_stats.h"
#include "jemalloc/internal/bin_types.h"
#include "jemalloc/internal/edata.h"
#include "jemalloc/internal/mutex.h"
#include "jemalloc/internal/sc.h"

/*
 * A bin contains a set of extents that are currently being used for slab
 * allocations.
 */
typedef struct bin_s bin_t;
struct bin_s {
	/* All operations on bin_t fields require lock ownership. */
	malloc_mutex_t lock;

	/*
	 * Bin statistics.  These get touched every time the lock is acquired,
	 * so put them close by in the hopes of getting some cache locality.
	 */
	bin_stats_t stats;

	/*
	 * Current slab being used to service allocations of this bin's size
	 * class.  slabcur is independent of slabs_{nonfull,full}; whenever
	 * slabcur is reassigned, the previous slab must be deallocated or
	 * inserted into slabs_{nonfull,full}.
	 */
	edata_t *slabcur;

	/*
	 * Heap of non-full slabs.  This heap is used to assure that new
	 * allocations come from the non-full slab that is oldest/lowest in
	 * memory.
	 */
	edata_heap_t slabs_nonfull;

	/* List used to track full slabs. */
	edata_list_active_t slabs_full;
};

/* A set of sharded bins of the same size class. */
typedef struct bins_s bins_t;
struct bins_s {
	/* Sharded bins.  Dynamically sized. */
	bin_t *bin_shards;
};

void bin_shard_sizes_boot(unsigned bin_shard_sizes[SC_NBINS]);
bool bin_update_shard_size(unsigned bin_shards[SC_NBINS], size_t start_size,
    size_t end_size, size_t nshards);

/* Initializes a bin to empty.  Returns true on error. */
bool bin_init(bin_t *bin);

/* Forking. */
void bin_prefork(tsdn_t *tsdn, bin_t *bin);
void bin_postfork_parent(tsdn_t *tsdn, bin_t *bin);
void bin_postfork_child(tsdn_t *tsdn, bin_t *bin);

/* Slab region allocation. */
void *bin_slab_reg_alloc(edata_t *slab, const bin_info_t *bin_info);
void  bin_slab_reg_alloc_batch(
     edata_t *slab, const bin_info_t *bin_info, unsigned cnt, void **ptrs);

/* Slab list management. */
void     bin_slabs_nonfull_insert(bin_t *bin, edata_t *slab);
void     bin_slabs_nonfull_remove(bin_t *bin, edata_t *slab);
edata_t *bin_slabs_nonfull_tryget(bin_t *bin);
void     bin_slabs_full_insert(bool is_auto, bin_t *bin, edata_t *slab);
void     bin_slabs_full_remove(bool is_auto, bin_t *bin, edata_t *slab);

/* Slab association / demotion. */
void bin_dissociate_slab(bool is_auto, edata_t *slab, bin_t *bin);
void bin_lower_slab(tsdn_t *tsdn, bool is_auto, edata_t *slab, bin_t *bin);

/* Deallocation helpers (called under bin lock). */
void bin_dalloc_slab_prepare(tsdn_t *tsdn, edata_t *slab, bin_t *bin);
void bin_dalloc_locked_handle_newly_empty(
    tsdn_t *tsdn, bool is_auto, edata_t *slab, bin_t *bin);
void bin_dalloc_locked_handle_newly_nonempty(
    tsdn_t *tsdn, bool is_auto, edata_t *slab, bin_t *bin);

/* Slabcur refill and allocation. */
void  bin_refill_slabcur_with_fresh_slab(tsdn_t *tsdn, bin_t *bin,
    szind_t binind, edata_t *fresh_slab);
void *bin_malloc_with_fresh_slab(tsdn_t *tsdn, bin_t *bin,
    szind_t binind, edata_t *fresh_slab);
bool  bin_refill_slabcur_no_fresh_slab(tsdn_t *tsdn, bool is_auto,
    bin_t *bin);
void *bin_malloc_no_fresh_slab(tsdn_t *tsdn, bool is_auto, bin_t *bin,
    szind_t binind);

/* Bin selection. */
bin_t *bin_choose(tsdn_t *tsdn, arena_t *arena, szind_t binind,
    unsigned *binshard_p);

/* Stats. */
static inline void
bin_stats_merge(tsdn_t *tsdn, bin_stats_data_t *dst_bin_stats, bin_t *bin) {
	malloc_mutex_lock(tsdn, &bin->lock);
	malloc_mutex_prof_accum(tsdn, &dst_bin_stats->mutex_data, &bin->lock);
	bin_stats_t *stats = &dst_bin_stats->stats_data;
	stats->nmalloc += bin->stats.nmalloc;
	stats->ndalloc += bin->stats.ndalloc;
	stats->nrequests += bin->stats.nrequests;
	stats->curregs += bin->stats.curregs;
	stats->nfills += bin->stats.nfills;
	stats->nflushes += bin->stats.nflushes;
	stats->nslabs += bin->stats.nslabs;
	stats->reslabs += bin->stats.reslabs;
	stats->curslabs += bin->stats.curslabs;
	stats->nonfull_slabs += bin->stats.nonfull_slabs;
	malloc_mutex_unlock(tsdn, &bin->lock);
}

#endif /* JEMALLOC_INTERNAL_BIN_H */

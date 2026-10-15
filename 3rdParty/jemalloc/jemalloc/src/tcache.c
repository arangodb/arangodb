#include "jemalloc/internal/jemalloc_preamble.h"
#include "jemalloc/internal/jemalloc_internal_includes.h"

#include "jemalloc/internal/assert.h"
#include "jemalloc/internal/base.h"
#include "jemalloc/internal/mutex.h"
#include "jemalloc/internal/safety_check.h"
#include "jemalloc/internal/san.h"
#include "jemalloc/internal/sc.h"

/******************************************************************************/
/* Data. */

bool opt_tcache = true;

/* global_do_not_change_tcache_maxclass is set to 32KB by default. */
size_t opt_tcache_max = ((size_t)1) << 15;

/* Reasonable defaults for min and max values. */
unsigned opt_tcache_nslots_small_min = 20;
unsigned opt_tcache_nslots_small_max = 200;
unsigned opt_tcache_nslots_large = 20;

/*
 * We attempt to make the number of slots in a tcache bin for a given size class
 * equal to the number of objects in a slab times some multiplier.  By default,
 * the multiplier is 2 (i.e. we set the maximum number of objects in the tcache
 * to twice the number of objects in a slab).
 * This is bounded by some other constraints as well, like the fact that it
 * must be even, must be less than opt_tcache_nslots_small_max, etc..
 */
ssize_t opt_lg_tcache_nslots_mul = 1;

/*
 * Number of allocation bytes between tcache incremental GCs.  Again, this
 * default just seems to work well; more tuning is possible.
 */
size_t opt_tcache_gc_incr_bytes = 65536;

/*
 * With default settings, we may end up flushing small bins frequently with
 * small flush amounts.  To limit this tendency, we can set a number of bytes to
 * "delay" by.  If we try to flush N M-byte items, we decrease that size-class's
 * delay by N * M.  So, if delay is 1024 and we're looking at the 64-byte size
 * class, we won't do any flushing until we've been asked to flush 1024/64 == 16
 * items.  This can happen in any configuration (i.e. being asked to flush 16
 * items once, or 4 items 4 times).
 *
 * Practically, this is stored as a count of items in a uint8_t, so the
 * effective maximum value for a size class is 255 * sz.
 */
size_t opt_tcache_gc_delay_bytes = 0;

/*
 * When a cache bin is flushed because it's full, how much of it do we flush?
 * By default, we flush half the maximum number of items.
 */
unsigned opt_lg_tcache_flush_small_div = 1;
unsigned opt_lg_tcache_flush_large_div = 1;

/*
 * Number of cache bins enabled, including both large and small.  This value
 * is only used to initialize tcache_nbins in the per-thread tcache.
 * Directly modifying it will not affect threads already launched.
 */
unsigned global_do_not_change_tcache_nbins;
/*
 * Max size class to be cached (can be small or large). This value is only used
 * to initialize tcache_max in the per-thread tcache.   Directly modifying it
 * will not affect threads already launched.
 */
size_t global_do_not_change_tcache_maxclass;

/*
 * Default bin info for each bin.  Will be initialized in malloc_conf_init
 * and tcache_boot and should not be modified after that.
 */
static cache_bin_info_t opt_tcache_ncached_max[TCACHE_NBINS_MAX] = {{0}};
/*
 * Marks whether a bin's info is set already.  This is used in
 * tcache_bin_info_compute to avoid overwriting ncached_max specified by
 * malloc_conf.  It should be set only when parsing malloc_conf.
 */
static bool opt_tcache_ncached_max_set[TCACHE_NBINS_MAX] = {0};

tcaches_t *tcaches;

/* Index of first element within tcaches that has never been used. */
static unsigned tcaches_past;

/* Head of singly linked list tracking available tcaches elements. */
static tcaches_t *tcaches_avail;

/* Protects tcaches{,_past,_avail}. */
static malloc_mutex_t tcaches_mtx;

/******************************************************************************/

size_t
tcache_salloc(tsdn_t *tsdn, const void *ptr) {
	return arena_salloc(tsdn, ptr);
}

uint64_t
tcache_gc_new_event_wait(tsd_t *tsd) {
	return opt_tcache_gc_incr_bytes;
}

uint64_t
tcache_gc_postponed_event_wait(tsd_t *tsd) {
	return TE_MIN_START_WAIT;
}

static inline void
tcache_bin_fill_ctl_init(tcache_slow_t *tcache_slow, szind_t szind) {
	assert(szind < SC_NBINS);
	cache_bin_fill_ctl_t *ctl =
	    &tcache_slow->bin_fill_ctl_do_not_access_directly[szind];
	ctl->base = 1;
	ctl->offset = 0;
}

static inline cache_bin_fill_ctl_t *
tcache_bin_fill_ctl_get(tcache_slow_t *tcache_slow, szind_t szind) {
	assert(szind < SC_NBINS);
	cache_bin_fill_ctl_t *ctl =
	    &tcache_slow->bin_fill_ctl_do_not_access_directly[szind];
	assert(ctl->base > ctl->offset);
	return ctl;
}

/*
 * The number of items to be filled at a time for a given small bin is
 * calculated by (ncached_max >> lg_fill_div).
 * The actual ctl struct consists of two fields, i.e. base and offset,
 * and the difference between the two(base - offset) is the final lg_fill_div.
 * The base is adjusted during GC based on the traffic within a period of time,
 * while the offset is updated in real time to handle the immediate traffic.
 */
static inline uint8_t
tcache_nfill_small_lg_div_get(tcache_slow_t *tcache_slow, szind_t szind) {
	cache_bin_fill_ctl_t *ctl = tcache_bin_fill_ctl_get(tcache_slow, szind);
	return (ctl->base - (opt_experimental_tcache_gc ? ctl->offset : 0));
}

/*
 * When we want to fill more items to respond to burst load,
 * offset is increased so that (base - offset) is decreased,
 * which in return increases the number of items to be filled.
 */
static inline void
tcache_nfill_small_burst_prepare(tcache_slow_t *tcache_slow, szind_t szind) {
	cache_bin_fill_ctl_t *ctl = tcache_bin_fill_ctl_get(tcache_slow, szind);
	if (ctl->offset + 1 < ctl->base) {
		ctl->offset++;
	}
}

static inline void
tcache_nfill_small_burst_reset(tcache_slow_t *tcache_slow, szind_t szind) {
	cache_bin_fill_ctl_t *ctl = tcache_bin_fill_ctl_get(tcache_slow, szind);
	ctl->offset = 0;
}

/*
 * limit == 0: indicating that the fill count should be increased,
 * i.e. lg_div(base) should be decreased.
 *
 * limit != 0: limit is set to ncached_max, indicating that the fill
 * count should be decreased, i.e. lg_div(base) should be increased.
 */
static inline void
tcache_nfill_small_gc_update(
    tcache_slow_t *tcache_slow, szind_t szind, cache_bin_sz_t limit) {
	cache_bin_fill_ctl_t *ctl = tcache_bin_fill_ctl_get(tcache_slow, szind);
	if (!limit && ctl->base > 1) {
		/*
		 * Increase fill count by 2X for small bins.  Make sure
		 * lg_fill_div stays greater than 1.
		 */
		ctl->base--;
	} else if (limit && (limit >> ctl->base) > 1) {
		/*
		 * Reduce fill count by 2X.  Limit lg_fill_div such that
		 * the fill count is always at least 1.
		 */
		ctl->base++;
	}
	/* Reset the offset for the next GC period. */
	ctl->offset = 0;
}

static uint8_t
tcache_gc_item_delay_compute(szind_t szind) {
	assert(szind < SC_NBINS);
	size_t sz = sz_index2size(szind);
	size_t item_delay = opt_tcache_gc_delay_bytes / sz;
	size_t delay_max = ZU(1)
	    << (sizeof(((tcache_slow_t *)NULL)->bin_flush_delay_items[0]) * 8);
	if (item_delay >= delay_max) {
		item_delay = delay_max - 1;
	}
	return (uint8_t)item_delay;
}

static inline void *
tcache_gc_small_heuristic_addr_get(
    tsd_t *tsd, tcache_slow_t *tcache_slow, szind_t szind) {
	assert(szind < SC_NBINS);
	tsdn_t *tsdn = tsd_tsdn(tsd);
	bin_t  *bin = bin_choose(tsdn, tcache_slow->arena, szind, NULL);
	assert(bin != NULL);

	malloc_mutex_lock(tsdn, &bin->lock);
	edata_t *slab = (bin->slabcur == NULL)
	    ? edata_heap_first(&bin->slabs_nonfull)
	    : bin->slabcur;
	assert(slab != NULL || edata_heap_empty(&bin->slabs_nonfull));
	void *ret = (slab != NULL) ? edata_addr_get(slab) : NULL;
	assert(ret != NULL || slab == NULL);
	malloc_mutex_unlock(tsdn, &bin->lock);

	return ret;
}

static inline bool
tcache_gc_is_addr_remote(void *addr, uintptr_t min, uintptr_t max) {
	assert(addr != NULL);
	return ((uintptr_t)addr < min || (uintptr_t)addr >= max);
}

static inline cache_bin_sz_t
tcache_gc_small_nremote_get(cache_bin_t *cache_bin, void *addr,
    uintptr_t *addr_min, uintptr_t *addr_max, szind_t szind, size_t nflush) {
	assert(addr != NULL && addr_min != NULL && addr_max != NULL);
	/* The slab address range that the provided addr belongs to. */
	uintptr_t slab_min = (uintptr_t)addr;
	uintptr_t slab_max = slab_min + bin_infos[szind].slab_size;
	/*
	 * When growing retained virtual memory, it's increased exponentially,
	 * starting from 2M, so that the total number of disjoint virtual
	 * memory ranges retained by each shard is limited.
	 */
	uintptr_t neighbor_min = ((uintptr_t)addr > TCACHE_GC_NEIGHBOR_LIMIT)
	    ? ((uintptr_t)addr - TCACHE_GC_NEIGHBOR_LIMIT)
	    : 0;
	uintptr_t neighbor_max = ((uintptr_t)addr
	                             < (UINTPTR_MAX - TCACHE_GC_NEIGHBOR_LIMIT))
	    ? ((uintptr_t)addr + TCACHE_GC_NEIGHBOR_LIMIT)
	    : UINTPTR_MAX;

	/* Scan the entire bin to count the number of remote pointers. */
	void         **head = cache_bin->stack_head;
	cache_bin_sz_t n_remote_slab = 0, n_remote_neighbor = 0;
	cache_bin_sz_t ncached = cache_bin_ncached_get_local(cache_bin);
	for (void **cur = head; cur < head + ncached; cur++) {
		n_remote_slab += (cache_bin_sz_t)tcache_gc_is_addr_remote(
		    *cur, slab_min, slab_max);
		n_remote_neighbor += (cache_bin_sz_t)tcache_gc_is_addr_remote(
		    *cur, neighbor_min, neighbor_max);
	}
	/*
	 * Note: since slab size is dynamic and can be larger than 2M, i.e.
	 * TCACHE_GC_NEIGHBOR_LIMIT, there is no guarantee as to which of
	 * n_remote_slab and n_remote_neighbor is greater.
	 */
	assert(n_remote_slab <= ncached && n_remote_neighbor <= ncached);
	/*
	 * We first consider keeping ptrs from the neighboring addr range,
	 * since in most cases the range is greater than the slab range.
	 * So if the number of non-neighbor ptrs is more than the intended
	 * flush amount, we use it as the anchor for flushing.
	 */
	if (n_remote_neighbor >= nflush) {
		*addr_min = neighbor_min;
		*addr_max = neighbor_max;
		return n_remote_neighbor;
	}
	/*
	 * We then consider only keeping ptrs from the local slab, and in most
	 * cases this is stricter, assuming that slab < 2M is the common case.
	 */
	*addr_min = slab_min;
	*addr_max = slab_max;
	return n_remote_slab;
}

/* Shuffle the ptrs in the bin to put the remote pointers at the bottom. */
static inline void
tcache_gc_small_bin_shuffle(cache_bin_t *cache_bin, cache_bin_sz_t nremote,
    uintptr_t addr_min, uintptr_t addr_max) {
	void         **swap = NULL;
	cache_bin_sz_t ncached = cache_bin_ncached_get_local(cache_bin);
	cache_bin_sz_t ntop = ncached - nremote, cnt = 0;
	assert(ntop > 0 && ntop < ncached);
	/*
	 * Scan the [head, head + ntop) part of the cache bin, during which
	 * bubbling the non-remote ptrs to the top of the bin.
	 * After this, the [head, head + cnt) part of the bin contains only
	 * non-remote ptrs, and they're in the same relative order as before.
	 * While the [head + cnt, head + ntop) part contains only remote ptrs.
	 */
	void **head = cache_bin->stack_head;
	for (void **cur = head; cur < head + ntop; cur++) {
		if (!tcache_gc_is_addr_remote(*cur, addr_min, addr_max)) {
			/* Tracks the number of non-remote ptrs seen so far. */
			cnt++;
			/*
			 * There is remote ptr before the current non-remote ptr,
			 * swap the current non-remote ptr with the remote ptr,
			 * and increment the swap pointer so that it's still
			 * pointing to the top remote ptr in the bin.
			 */
			if (swap != NULL) {
				assert(swap < cur);
				assert(tcache_gc_is_addr_remote(
				    *swap, addr_min, addr_max));
				void *tmp = *cur;
				*cur = *swap;
				*swap = tmp;
				swap++;
				assert(swap <= cur);
				assert(tcache_gc_is_addr_remote(
				    *swap, addr_min, addr_max));
			}
			continue;
		} else if (swap == NULL) {
			/* Swap always points to the top remote ptr in the bin. */
			swap = cur;
		}
	}
	/*
	 * Scan the [head + ntop, head + ncached) part of the cache bin,
	 * after which it should only contain remote ptrs.
	 */
	for (void **cur = head + ntop; cur < head + ncached; cur++) {
		/* Early break if all non-remote ptrs have been moved. */
		if (cnt == ntop) {
			break;
		}
		if (!tcache_gc_is_addr_remote(*cur, addr_min, addr_max)) {
			assert(tcache_gc_is_addr_remote(
			    *(head + cnt), addr_min, addr_max));
			void *tmp = *cur;
			*cur = *(head + cnt);
			*(head + cnt) = tmp;
			cnt++;
		}
	}
	assert(cnt == ntop);
	/* Sanity check to make sure the shuffle is done correctly. */
	for (void **cur = head; cur < head + ncached; cur++) {
		assert(*cur != NULL);
		assert(
		    ((cur < head + ntop)
		        && !tcache_gc_is_addr_remote(*cur, addr_min, addr_max))
		    || ((cur >= head + ntop)
		        && tcache_gc_is_addr_remote(*cur, addr_min, addr_max)));
	}
}

static bool
tcache_gc_small(
    tsd_t *tsd, tcache_slow_t *tcache_slow, tcache_t *tcache, szind_t szind) {
	/*
	 * Aim to flush 3/4 of items below low-water, with remote pointers being
	 * prioritized for flushing.
	 */
	assert(szind < SC_NBINS);

	cache_bin_t *cache_bin = &tcache->bins[szind];
	assert(!tcache_bin_disabled(szind, cache_bin, tcache->tcache_slow));
	cache_bin_sz_t ncached = cache_bin_ncached_get_local(cache_bin);
	cache_bin_sz_t low_water = cache_bin_low_water_get(cache_bin);
	if (low_water > 0) {
		/*
		 * There is unused items within the GC period => reduce fill count.
		 * limit field != 0 is borrowed to indicate that the fill count
		 * should be reduced.
		 */
		tcache_nfill_small_gc_update(tcache_slow, szind,
		    /* limit */ cache_bin_ncached_max_get(cache_bin));
	} else if (tcache_slow->bin_refilled[szind]) {
		/*
		 * There has been refills within the GC period => increase fill count.
		 * limit field set to 0 is borrowed to indicate that the fill count
		 * should be increased.
		 */
		tcache_nfill_small_gc_update(tcache_slow, szind, /* limit */ 0);
		tcache_slow->bin_refilled[szind] = false;
	}
	assert(!tcache_slow->bin_refilled[szind]);

	cache_bin_sz_t nflush = low_water - (low_water >> 2);
	/*
	 * When the new tcache gc is not enabled, keep the flush delay logic,
	 * and directly flush the bottom nflush items if needed.
	 */
	if (!opt_experimental_tcache_gc) {
		if (nflush < tcache_slow->bin_flush_delay_items[szind]) {
			/* Workaround for a conversion warning. */
			uint8_t nflush_uint8 = (uint8_t)nflush;
			assert(sizeof(tcache_slow->bin_flush_delay_items[0])
			    == sizeof(nflush_uint8));
			tcache_slow->bin_flush_delay_items[szind] -=
			    nflush_uint8;
			return false;
		}

		tcache_slow->bin_flush_delay_items[szind] =
		    tcache_gc_item_delay_compute(szind);
		goto label_flush;
	}

	/* Directly goto the flush path when the entire bin needs to be flushed. */
	if (nflush == ncached) {
		goto label_flush;
	}

	/* Query arena binshard to get heuristic locality info. */
	void *addr = tcache_gc_small_heuristic_addr_get(
	    tsd, tcache_slow, szind);
	if (addr == NULL) {
		goto label_flush;
	}

	/*
	 * Use the queried addr above to get the number of remote ptrs in the
	 * bin, and the min/max of the local addr range.
	 */
	uintptr_t      addr_min, addr_max;
	cache_bin_sz_t nremote = tcache_gc_small_nremote_get(
	    cache_bin, addr, &addr_min, &addr_max, szind, nflush);

	/*
	 * Update the nflush to the larger value between the intended flush count
	 * and the number of remote ptrs.
	 */
	if (nremote > nflush) {
		nflush = nremote;
	}
	/*
	 * When entering the locality check, nflush should be less than ncached,
	 * otherwise the entire bin should be flushed regardless. The only case
	 * when nflush gets updated to ncached after locality check is, when all
	 * the items in the bin are remote, in which case the entire bin should
	 * also be flushed.
	 */
	assert(nflush < ncached || nremote == ncached);
	if (nremote == 0 || nremote == ncached) {
		goto label_flush;
	}

	/*
	 * Move the remote points to the bottom of the bin for flushing.
	 * As long as moved to the bottom, the order of these nremote ptrs
	 * does not matter, since they are going to be flushed anyway.
	 * The rest of the ptrs are moved to the top of the bin, and their
	 * relative order is maintained.
	 */
	tcache_gc_small_bin_shuffle(cache_bin, nremote, addr_min, addr_max);

label_flush:
	if (nflush == 0) {
		assert(low_water == 0);
		return false;
	}
	assert(nflush <= ncached);
	tcache_bin_flush_small(
	    tsd, tcache, cache_bin, szind, (unsigned)(ncached - nflush));
	return true;
}

static bool
tcache_gc_large(
    tsd_t *tsd, tcache_slow_t *tcache_slow, tcache_t *tcache, szind_t szind) {
	/*
	 * Like the small GC, flush 3/4 of untouched items. However, simply flush
	 * the bottom nflush items, without any locality check.
	 */
	assert(szind >= SC_NBINS);
	cache_bin_t *cache_bin = &tcache->bins[szind];
	assert(!tcache_bin_disabled(szind, cache_bin, tcache->tcache_slow));
	cache_bin_sz_t low_water = cache_bin_low_water_get(cache_bin);
	if (low_water == 0) {
		return false;
	}
	unsigned nrem = (unsigned)(cache_bin_ncached_get_local(cache_bin)
	    - low_water + (low_water >> 2));
	tcache_bin_flush_large(tsd, tcache, cache_bin, szind, nrem);
	return true;
}

/* Try to gc one bin by szind, return true if there is item flushed. */
static bool
tcache_try_gc_bin(
    tsd_t *tsd, tcache_slow_t *tcache_slow, tcache_t *tcache, szind_t szind) {
	assert(tcache != NULL);
	cache_bin_t *cache_bin = &tcache->bins[szind];
	if (tcache_bin_disabled(szind, cache_bin, tcache_slow)) {
		return false;
	}

	bool is_small = (szind < SC_NBINS);
	tcache_bin_flush_stashed(tsd, tcache, cache_bin, szind, is_small);
	bool ret = is_small ? tcache_gc_small(tsd, tcache_slow, tcache, szind)
	                    : tcache_gc_large(tsd, tcache_slow, tcache, szind);
	cache_bin_low_water_set(cache_bin);
	return ret;
}

static void
tcache_gc_event(tsd_t *tsd) {
	tcache_t *tcache = tcache_get(tsd);
	if (tcache == NULL) {
		return;
	}

	tcache_slow_t *tcache_slow = tsd_tcache_slowp_get(tsd);
	assert(tcache_slow != NULL);

	/* When the new tcache gc is not enabled, GC one bin at a time. */
	if (!opt_experimental_tcache_gc) {
		szind_t szind = tcache_slow->next_gc_bin;
		tcache_try_gc_bin(tsd, tcache_slow, tcache, szind);
		tcache_slow->next_gc_bin++;
		if (tcache_slow->next_gc_bin == tcache_nbins_get(tcache_slow)) {
			tcache_slow->next_gc_bin = 0;
		}
		return;
	}

	nstime_t now;
	nstime_copy(&now, &tcache_slow->last_gc_time);
	nstime_update(&now);
	assert(nstime_compare(&now, &tcache_slow->last_gc_time) >= 0);

	if (nstime_ns(&now) - nstime_ns(&tcache_slow->last_gc_time)
	    < TCACHE_GC_INTERVAL_NS) {
		// time interval is too short, skip this event.
		return;
	}
	/* Update last_gc_time to now. */
	nstime_copy(&tcache_slow->last_gc_time, &now);

	unsigned gc_small_nbins = 0, gc_large_nbins = 0;
	unsigned tcache_nbins = tcache_nbins_get(tcache_slow);
	unsigned small_nbins = tcache_nbins > SC_NBINS ? SC_NBINS
	                                               : tcache_nbins;
	szind_t  szind_small = tcache_slow->next_gc_bin_small;
	szind_t  szind_large = tcache_slow->next_gc_bin_large;

	/* Flush at most TCACHE_GC_SMALL_NBINS_MAX small bins at a time. */
	for (unsigned i = 0;
	     i < small_nbins && gc_small_nbins < TCACHE_GC_SMALL_NBINS_MAX;
	     i++) {
		assert(szind_small < SC_NBINS);
		if (tcache_try_gc_bin(tsd, tcache_slow, tcache, szind_small)) {
			gc_small_nbins++;
		}
		if (++szind_small == small_nbins) {
			szind_small = 0;
		}
	}
	tcache_slow->next_gc_bin_small = szind_small;

	if (tcache_nbins <= SC_NBINS) {
		return;
	}

	/* Flush at most TCACHE_GC_LARGE_NBINS_MAX large bins at a time. */
	for (unsigned i = SC_NBINS;
	     i < tcache_nbins && gc_large_nbins < TCACHE_GC_LARGE_NBINS_MAX;
	     i++) {
		assert(szind_large >= SC_NBINS && szind_large < tcache_nbins);
		if (tcache_try_gc_bin(tsd, tcache_slow, tcache, szind_large)) {
			gc_large_nbins++;
		}
		if (++szind_large == tcache_nbins) {
			szind_large = SC_NBINS;
		}
	}
	tcache_slow->next_gc_bin_large = szind_large;
}

void *
tcache_alloc_small_hard(tsdn_t *tsdn, arena_t *arena, tcache_t *tcache,
    cache_bin_t *cache_bin, szind_t binind, bool *tcache_success) {
	tcache_slow_t *tcache_slow = tcache->tcache_slow;
	void          *ret;

	assert(tcache_slow->arena != NULL);
	assert(!tcache_bin_disabled(binind, cache_bin, tcache_slow));
	assert(cache_bin_ncached_get_local(cache_bin) == 0);
	cache_bin_sz_t nfill = cache_bin_ncached_max_get(cache_bin)
	    >> tcache_nfill_small_lg_div_get(tcache_slow, binind);
	if (nfill == 0) {
		nfill = 1;
	}
	cache_bin_sz_t nfill_min = opt_experimental_tcache_gc
	    ? ((nfill >> 1) + 1)
	    : nfill;
	cache_bin_sz_t nfill_max = nfill;
	CACHE_BIN_PTR_ARRAY_DECLARE(ptrs, nfill_max);
	cache_bin_init_ptr_array_for_fill(cache_bin, &ptrs, nfill_max);

	cache_bin_sz_t filled = arena_ptr_array_fill_small(tsdn, arena, binind,
	    &ptrs, /* nfill_min */ nfill_min, /* nfill_max */ nfill_max,
	    cache_bin->tstats);
	cache_bin_finish_fill(cache_bin, &ptrs, filled);
	assert(filled >= nfill_min && filled <= nfill_max);
	assert(cache_bin_ncached_get_local(cache_bin) == filled);

	tcache_slow->bin_refilled[binind] = true;
	tcache_nfill_small_burst_prepare(tcache_slow, binind);
	ret = cache_bin_alloc(cache_bin, tcache_success);

	return ret;
}

JEMALLOC_ALWAYS_INLINE void
tcache_bin_flush_bottom(tsd_t *tsd, tcache_t *tcache, cache_bin_t *cache_bin,
    szind_t binind, unsigned rem, bool small) {
	assert(rem <= cache_bin_ncached_max_get(cache_bin));
	assert(!tcache_bin_disabled(binind, cache_bin, tcache->tcache_slow));
	cache_bin_sz_t orig_nstashed = cache_bin_nstashed_get_local(cache_bin);
	tcache_bin_flush_stashed(tsd, tcache, cache_bin, binind, small);

	cache_bin_sz_t ncached = cache_bin_ncached_get_local(cache_bin);
	assert((cache_bin_sz_t)rem <= ncached + orig_nstashed);
	if ((cache_bin_sz_t)rem > ncached) {
		/*
		 * The flush_stashed above could have done enough flushing, if
		 * there were many items stashed.  Validate that: 1) non zero
		 * stashed, and 2) bin stack has available space now.
		 */
		assert(orig_nstashed > 0);
		assert(ncached + cache_bin_nstashed_get_local(cache_bin)
		    < cache_bin_ncached_max_get(cache_bin));
		/* Still go through the flush logic for stats purpose only. */
		rem = ncached;
	}
	cache_bin_sz_t nflush = ncached - (cache_bin_sz_t)rem;

	CACHE_BIN_PTR_ARRAY_DECLARE(ptrs, nflush);
	cache_bin_init_ptr_array_for_flush(cache_bin, &ptrs, nflush);

	arena_ptr_array_flush(tsd, binind, &ptrs, nflush, small,
	    tcache->tcache_slow->arena, cache_bin->tstats);

	cache_bin_finish_flush(cache_bin, &ptrs, nflush);
}

void
tcache_bin_flush_small(tsd_t *tsd, tcache_t *tcache, cache_bin_t *cache_bin,
    szind_t binind, unsigned rem) {
	tcache_nfill_small_burst_reset(tcache->tcache_slow, binind);
	tcache_bin_flush_bottom(tsd, tcache, cache_bin, binind, rem,
	    /* small */ true);
}

void
tcache_bin_flush_large(tsd_t *tsd, tcache_t *tcache, cache_bin_t *cache_bin,
    szind_t binind, unsigned rem) {
	tcache_bin_flush_bottom(tsd, tcache, cache_bin, binind, rem,
	    /* small */ false);
}

/*
 * Flushing stashed happens when 1) tcache fill, 2) tcache flush, or 3) tcache
 * GC event.  This makes sure that the stashed items do not hold memory for too
 * long, and new buffers can only be allocated when nothing is stashed.
 *
 * The downside is, the time between stash and flush may be relatively short,
 * especially when the request rate is high.  It lowers the chance of detecting
 * write-after-free -- however that is a delayed detection anyway, and is less
 * of a focus than the memory overhead.
 */
void
tcache_bin_flush_stashed(tsd_t *tsd, tcache_t *tcache, cache_bin_t *cache_bin,
    szind_t binind, bool is_small) {
	assert(!tcache_bin_disabled(binind, cache_bin, tcache->tcache_slow));
	/*
	 * The two below are for assertion only.  The content of original cached
	 * items remain unchanged -- the stashed items reside on the other end
	 * of the stack.  Checking the stack head and ncached to verify.
	 */
	void          *head_content = *cache_bin->stack_head;
	cache_bin_sz_t orig_cached = cache_bin_ncached_get_local(cache_bin);

	cache_bin_sz_t nstashed = cache_bin_nstashed_get_local(cache_bin);
	assert(orig_cached + nstashed <= cache_bin_ncached_max_get(cache_bin));
	if (nstashed == 0) {
		return;
	}

	CACHE_BIN_PTR_ARRAY_DECLARE(ptrs, nstashed);
	cache_bin_init_ptr_array_for_stashed(
	    cache_bin, binind, &ptrs, nstashed);
	san_check_stashed_ptrs(ptrs.ptr, nstashed, sz_index2size(binind));
	arena_ptr_array_flush(tsd, binind, &ptrs, nstashed, is_small,
	    tcache->tcache_slow->arena, cache_bin->tstats);
	cache_bin_finish_flush_stashed(cache_bin);

	assert(cache_bin_nstashed_get_local(cache_bin) == 0);
	assert(cache_bin_ncached_get_local(cache_bin) == orig_cached);
	assert(head_content == *cache_bin->stack_head);
}

JET_EXTERN bool
tcache_get_default_ncached_max_set(szind_t ind) {
	return opt_tcache_ncached_max_set[ind];
}

JET_EXTERN const cache_bin_info_t *
tcache_get_default_ncached_max(void) {
	return opt_tcache_ncached_max;
}

bool
tcache_bin_ncached_max_read(
    tsd_t *tsd, size_t bin_size, cache_bin_sz_t *ncached_max) {
	if (bin_size > TCACHE_MAXCLASS_LIMIT) {
		return true;
	}

	if (!tcache_available(tsd)) {
		*ncached_max = 0;
		return false;
	}

	tcache_t *tcache = tsd_tcachep_get(tsd);
	assert(tcache != NULL);
	szind_t bin_ind = sz_size2index(bin_size);

	cache_bin_t *bin = &tcache->bins[bin_ind];
	*ncached_max = tcache_bin_disabled(bin_ind, bin, tcache->tcache_slow)
	    ? 0
	    : cache_bin_ncached_max_get(bin);
	return false;
}

void
tcache_arena_associate(tsdn_t *tsdn, tcache_slow_t *tcache_slow,
    tcache_t *tcache, arena_t *arena) {
	assert(tcache_slow->arena == NULL);
	tcache_slow->arena = arena;

	if (config_stats) {
		/* Link into list of extant tcaches. */
		malloc_mutex_lock(tsdn, &arena->tcache_ql_mtx);

		ql_elm_new(tcache_slow, link);
		ql_tail_insert(&arena->tcache_ql, tcache_slow, link);
		cache_bin_array_descriptor_init(
		    &tcache_slow->cache_bin_array_descriptor, tcache->bins);
		ql_tail_insert(&arena->cache_bin_array_descriptor_ql,
		    &tcache_slow->cache_bin_array_descriptor, link);

		malloc_mutex_unlock(tsdn, &arena->tcache_ql_mtx);
	}
}

static void
tcache_arena_dissociate(
    tsdn_t *tsdn, tcache_slow_t *tcache_slow, tcache_t *tcache) {
	arena_t *arena = tcache_slow->arena;
	assert(arena != NULL);
	if (config_stats) {
		/* Unlink from list of extant tcaches. */
		malloc_mutex_lock(tsdn, &arena->tcache_ql_mtx);
		if (config_debug) {
			bool           in_ql = false;
			tcache_slow_t *iter;
			ql_foreach (iter, &arena->tcache_ql, link) {
				if (iter == tcache_slow) {
					in_ql = true;
					break;
				}
			}
			assert(in_ql);
		}
		ql_remove(&arena->tcache_ql, tcache_slow, link);
		ql_remove(&arena->cache_bin_array_descriptor_ql,
		    &tcache_slow->cache_bin_array_descriptor, link);
		tcache_stats_merge(tsdn, tcache_slow->tcache, arena);
		malloc_mutex_unlock(tsdn, &arena->tcache_ql_mtx);
	}
	tcache_slow->arena = NULL;
}

void
tcache_arena_reassociate(tsdn_t *tsdn, tcache_slow_t *tcache_slow,
    tcache_t *tcache, arena_t *arena) {
	tcache_arena_dissociate(tsdn, tcache_slow, tcache);
	tcache_arena_associate(tsdn, tcache_slow, tcache, arena);
}

static void
tcache_default_settings_init(tcache_slow_t *tcache_slow) {
	assert(tcache_slow != NULL);
	assert(global_do_not_change_tcache_maxclass != 0);
	assert(global_do_not_change_tcache_nbins != 0);
	tcache_slow->tcache_nbins = global_do_not_change_tcache_nbins;
}

static void
tcache_init(tsd_t *tsd, tcache_slow_t *tcache_slow, tcache_t *tcache, void *mem,
    const cache_bin_info_t *tcache_bin_info) {
	tcache->tcache_slow = tcache_slow;
	tcache_slow->tcache = tcache;

	memset(&tcache_slow->link, 0, sizeof(ql_elm(tcache_t)));
	nstime_init_zero(&tcache_slow->last_gc_time);
	tcache_slow->next_gc_bin = 0;
	tcache_slow->next_gc_bin_small = 0;
	tcache_slow->next_gc_bin_large = SC_NBINS;
	tcache_slow->arena = NULL;
	tcache_slow->dyn_alloc = mem;

	/*
	 * We reserve cache bins for all small size classes, even if some may
	 * not get used (i.e. bins higher than tcache_nbins).  This allows
	 * the fast and common paths to access cache bin metadata safely w/o
	 * worrying about which ones are disabled.
	 */
	unsigned tcache_nbins = tcache_nbins_get(tcache_slow);
	size_t   cur_offset = 0;
	cache_bin_preincrement(tcache_bin_info, tcache_nbins, mem, &cur_offset);
	for (unsigned i = 0; i < tcache_nbins; i++) {
		if (i < SC_NBINS) {
			tcache_bin_fill_ctl_init(tcache_slow, i);
			tcache_slow->bin_refilled[i] = false;
			tcache_slow->bin_flush_delay_items[i] =
			    tcache_gc_item_delay_compute(i);
		}
		cache_bin_t *cache_bin = &tcache->bins[i];
		if (tcache_bin_info[i].ncached_max > 0) {
			cache_bin_init(
			    cache_bin, &tcache_bin_info[i], mem, &cur_offset);
		} else {
			cache_bin_init_disabled(
			    cache_bin, tcache_bin_info[i].ncached_max);
		}
	}
	/*
	 * Initialize all disabled bins to a state that can safely and
	 * efficiently fail all fastpath alloc / free, so that no additional
	 * check around tcache_nbins is needed on fastpath.  Yet we still
	 * store the ncached_max in the bin_info for future usage.
	 */
	for (unsigned i = tcache_nbins; i < TCACHE_NBINS_MAX; i++) {
		cache_bin_t *cache_bin = &tcache->bins[i];
		cache_bin_init_disabled(
		    cache_bin, tcache_bin_info[i].ncached_max);
		assert(tcache_bin_disabled(i, cache_bin, tcache->tcache_slow));
	}

	cache_bin_postincrement(mem, &cur_offset);
	if (config_debug) {
		/* Sanity check that the whole stack is used. */
		size_t size, alignment;
		cache_bin_info_compute_alloc(
		    tcache_bin_info, tcache_nbins, &size, &alignment);
		assert(cur_offset == size);
	}
}

static inline unsigned
tcache_ncached_max_compute(szind_t szind) {
	if (szind >= SC_NBINS) {
		return opt_tcache_nslots_large;
	}
	unsigned slab_nregs = bin_infos[szind].nregs;

	/* We may modify these values; start with the opt versions. */
	unsigned nslots_small_min = opt_tcache_nslots_small_min;
	unsigned nslots_small_max = opt_tcache_nslots_small_max;

	/*
	 * Clamp values to meet our constraints -- even, nonzero, min < max, and
	 * suitable for a cache bin size.
	 */
	if (opt_tcache_nslots_small_max > CACHE_BIN_NCACHED_MAX) {
		nslots_small_max = CACHE_BIN_NCACHED_MAX;
	}
	if (nslots_small_min % 2 != 0) {
		nslots_small_min++;
	}
	if (nslots_small_max % 2 != 0) {
		nslots_small_max--;
	}
	if (nslots_small_min < 2) {
		nslots_small_min = 2;
	}
	if (nslots_small_max < 2) {
		nslots_small_max = 2;
	}
	if (nslots_small_min > nslots_small_max) {
		nslots_small_min = nslots_small_max;
	}

	unsigned candidate;
	if (opt_lg_tcache_nslots_mul < 0) {
		candidate = slab_nregs >> (-opt_lg_tcache_nslots_mul);
	} else {
		candidate = slab_nregs << opt_lg_tcache_nslots_mul;
	}
	if (candidate % 2 != 0) {
		/*
		 * We need the candidate size to be even -- we assume that we
		 * can divide by two and get a positive number (e.g. when
		 * flushing).
		 */
		++candidate;
	}
	if (candidate <= nslots_small_min) {
		return nslots_small_min;
	} else if (candidate <= nslots_small_max) {
		return candidate;
	} else {
		return nslots_small_max;
	}
}

JET_EXTERN void
tcache_bin_info_compute(cache_bin_info_t tcache_bin_info[TCACHE_NBINS_MAX]) {
	/*
	 * Compute the values for each bin, but for bins with indices larger
	 * than tcache_nbins, no items will be cached.
	 */
	for (szind_t i = 0; i < TCACHE_NBINS_MAX; i++) {
		unsigned ncached_max = tcache_get_default_ncached_max_set(i)
		    ? (unsigned)tcache_get_default_ncached_max()[i].ncached_max
		    : tcache_ncached_max_compute(i);
		assert(ncached_max <= CACHE_BIN_NCACHED_MAX);
		cache_bin_info_init(
		    &tcache_bin_info[i], (cache_bin_sz_t)ncached_max);
	}
}

static void *
tcache_stack_alloc_impl(tsdn_t *tsdn, size_t size, size_t alignment) {
	if (cache_bin_stack_use_thp()) {
		/* Alignment is ignored since it comes from THP. */
		assert(alignment == QUANTUM);
		return b0_alloc_tcache_stack(tsdn, size);
	}
	size = sz_sa2u(size, alignment);
	return ipallocztm(tsdn, size, alignment, true, NULL,
	    true, arena_get(TSDN_NULL, 0, true));
}

void *(*JET_MUTABLE tcache_stack_alloc)(tsdn_t *tsdn, size_t size,
    size_t alignment) = tcache_stack_alloc_impl;

static bool
tsd_tcache_data_init_impl(
    tsd_t *tsd, arena_t *arena, const cache_bin_info_t *tcache_bin_info) {
	tcache_slow_t *tcache_slow = tsd_tcache_slowp_get_unsafe(tsd);
	tcache_t      *tcache = tsd_tcachep_get_unsafe(tsd);

	assert(cache_bin_still_zero_initialized(&tcache->bins[0]));
	unsigned tcache_nbins = tcache_nbins_get(tcache_slow);
	size_t   size, alignment;
	cache_bin_info_compute_alloc(
	    tcache_bin_info, tcache_nbins, &size, &alignment);

	void *mem = tcache_stack_alloc(tsd_tsdn(tsd), size, alignment);
	if (mem == NULL) {
		return true;
	}

	tcache_init(tsd, tcache_slow, tcache, mem, tcache_bin_info);
	/*
	 * Initialization is a bit tricky here.  After malloc init is done, all
	 * threads can rely on arena_choose and associate tcache accordingly.
	 * However, the thread that does actual malloc bootstrapping relies on
	 * functional tsd, and it can only rely on a0.  In that case, we
	 * associate its tcache to a0 temporarily, and later on
	 * arena_choose_hard() will re-associate properly.
	 */
	tcache_slow->arena = NULL;
	if (!malloc_initialized()) {
		/* If in initialization, assign to a0. */
		arena = arena_get(tsd_tsdn(tsd), 0, false);
		tcache_arena_associate(
		    tsd_tsdn(tsd), tcache_slow, tcache, arena);
	} else {
		if (arena == NULL) {
			arena = arena_choose(tsd, NULL);
		}
		/* This may happen if thread.tcache.enabled is used. */
		if (tcache_slow->arena == NULL) {
			tcache_arena_associate(
			    tsd_tsdn(tsd), tcache_slow, tcache, arena);
		}
	}
	assert(arena == tcache_slow->arena);

	return false;
}

/* Initialize auto tcache (embedded in TSD). */
static bool
tsd_tcache_data_init(tsd_t *tsd, arena_t *arena,
    const cache_bin_info_t tcache_bin_info[TCACHE_NBINS_MAX]) {
	assert(tcache_bin_info != NULL);
	bool err = tsd_tcache_data_init_impl(tsd, arena, tcache_bin_info);
	if (unlikely(err)) {
		/*
		 * Disable the tcache before calling malloc_write to
		 * avoid recursive allocations through libc hooks.
		 */
		tsd_tcache_enabled_set(tsd, false);
		tsd_slow_update(tsd);
		malloc_write("<jemalloc>: Failed to allocate tcache data\n");
		if (opt_abort) {
			abort();
		}
	}
	return err;
}

/* Created manual tcache for tcache.create mallctl. */
tcache_t *
tcache_create_explicit(tsd_t *tsd) {
	/*
	 * We place the cache bin stacks, then the tcache_t, then a pointer to
	 * the beginning of the whole allocation (for freeing).  The makes sure
	 * the cache bins have the requested alignment.
	 */
	unsigned tcache_nbins = global_do_not_change_tcache_nbins;
	size_t   tcache_size, alignment;
	cache_bin_info_compute_alloc(tcache_get_default_ncached_max(),
	    tcache_nbins, &tcache_size, &alignment);

	size_t size = tcache_size + sizeof(tcache_t) + sizeof(tcache_slow_t);
	/* Naturally align the pointer stacks. */
	size = PTR_CEILING(size);
	size = sz_sa2u(size, alignment);

	void *mem = ipallocztm(tsd_tsdn(tsd), size, alignment, true, NULL, true,
	    arena_get(TSDN_NULL, 0, true));
	if (mem == NULL) {
		return NULL;
	}
	tcache_t      *tcache = (void *)((byte_t *)mem + tcache_size);
	tcache_slow_t *tcache_slow = (void *)((byte_t *)mem + tcache_size
	    + sizeof(tcache_t));
	tcache_default_settings_init(tcache_slow);
	tcache_init(
	    tsd, tcache_slow, tcache, mem, tcache_get_default_ncached_max());

	tcache_arena_associate(
	    tsd_tsdn(tsd), tcache_slow, tcache, arena_ichoose(tsd, NULL));

	return tcache;
}

bool
tsd_tcache_enabled_data_init(tsd_t *tsd) {
	/* Called upon tsd initialization. */
	tsd_tcache_enabled_set(tsd, opt_tcache);
	/*
	 * tcache is not available yet, but we need to set up its tcache_nbins
	 * in advance.
	 */
	tcache_default_settings_init(tsd_tcache_slowp_get(tsd));
	tsd_slow_update(tsd);

	if (opt_tcache) {
		/* Trigger tcache init. */
		return tsd_tcache_data_init(
			tsd, NULL, tcache_get_default_ncached_max());
	}

	return false;
}

void
tcache_enabled_set(tsd_t *tsd, bool enabled) {
	bool was_enabled = tsd_tcache_enabled_get(tsd);

	if (!was_enabled && enabled) {
		if (tsd_tcache_data_init(
		    tsd, NULL, tcache_get_default_ncached_max())) {
			return;
		}
	} else if (was_enabled && !enabled) {
		tcache_cleanup(tsd);
	}
	/* Commit the state last.  Above calls check current state. */
	tsd_tcache_enabled_set(tsd, enabled);
	tsd_slow_update(tsd);
}

bool
thread_tcache_max_set(tsd_t *tsd, size_t tcache_max) {
	assert(tcache_max <= TCACHE_MAXCLASS_LIMIT);
	assert(tcache_max == sz_s2u(tcache_max));
	tcache_t        *tcache = tsd_tcachep_get(tsd);
	tcache_slow_t   *tcache_slow = tcache->tcache_slow;
	cache_bin_info_t tcache_bin_info[TCACHE_NBINS_MAX] = {{0}};
	bool             ret = false;
	assert(tcache != NULL && tcache_slow != NULL);

	bool                    enabled = tcache_available(tsd);
	arena_t *assigned_arena JEMALLOC_CLANG_ANALYZER_SILENCE_INIT(NULL);
	if (enabled) {
		assigned_arena = tcache_slow->arena;
		/* Carry over the bin settings during the reboot. */
		tcache_bin_settings_backup(tcache, tcache_bin_info);
		/* Shutdown and reboot the tcache for a clean slate. */
		tcache_cleanup(tsd);
	}

	/*
	* Still set tcache_nbins of the tcache even if the tcache is not
	* available yet because the values are stored in tsd_t and are
	* always available for changing.
	*/
	tcache_max_set(tcache_slow, tcache_max);

	if (enabled) {
		ret = tsd_tcache_data_init(tsd, assigned_arena, tcache_bin_info);
	}

	assert(tcache_nbins_get(tcache_slow) == sz_size2index(tcache_max) + 1);
	return ret;
}

static bool
tcache_bin_info_settings_parse(const char *bin_settings_segment_cur,
    size_t len_left, cache_bin_info_t tcache_bin_info[TCACHE_NBINS_MAX],
    bool bin_info_is_set[TCACHE_NBINS_MAX]) {
	do {
		size_t size_start, size_end;
		size_t ncached_max;
		bool   err = multi_setting_parse_next(&bin_settings_segment_cur,
		      &len_left, &size_start, &size_end, &ncached_max);
		if (err) {
			return true;
		}
		if (size_end > TCACHE_MAXCLASS_LIMIT) {
			size_end = TCACHE_MAXCLASS_LIMIT;
		}
		if (size_start > TCACHE_MAXCLASS_LIMIT
		    || size_start > size_end) {
			continue;
		}
		/* May get called before sz_init (during malloc_conf_init). */
		szind_t bin_start = sz_size2index_compute(size_start);
		szind_t bin_end = sz_size2index_compute(size_end);
		if (ncached_max > CACHE_BIN_NCACHED_MAX) {
			ncached_max = (size_t)CACHE_BIN_NCACHED_MAX;
		}
		for (szind_t i = bin_start; i <= bin_end; i++) {
			cache_bin_info_init(
			    &tcache_bin_info[i], (cache_bin_sz_t)ncached_max);
			if (bin_info_is_set != NULL) {
				bin_info_is_set[i] = true;
			}
		}
	} while (len_left > 0);

	return false;
}

bool
tcache_bin_info_default_init(
    const char *bin_settings_segment_cur, size_t len_left) {
	return tcache_bin_info_settings_parse(bin_settings_segment_cur,
	    len_left, opt_tcache_ncached_max, opt_tcache_ncached_max_set);
}

bool
tcache_bins_ncached_max_write(tsd_t *tsd, char *settings, size_t len) {
	assert(tcache_available(tsd));
	assert(len != 0);
	tcache_t *tcache = tsd_tcachep_get(tsd);
	assert(tcache != NULL);
	cache_bin_info_t tcache_bin_info[TCACHE_NBINS_MAX];
	tcache_bin_settings_backup(tcache, tcache_bin_info);

	if (tcache_bin_info_settings_parse(
	        settings, len, tcache_bin_info, NULL)) {
		return true;
	}

	arena_t *assigned_arena = tcache->tcache_slow->arena;
	tcache_cleanup(tsd);
	return tsd_tcache_data_init(tsd, assigned_arena, tcache_bin_info);
}

static void
tcache_flush_cache(tsd_t *tsd, tcache_t *tcache) {
	tcache_slow_t *tcache_slow = tcache->tcache_slow;
	assert(tcache_slow->arena != NULL);

	for (unsigned i = 0; i < tcache_nbins_get(tcache_slow); i++) {
		cache_bin_t *cache_bin = &tcache->bins[i];
		if (tcache_bin_disabled(i, cache_bin, tcache_slow)) {
			continue;
		}
		if (i < SC_NBINS) {
			tcache_bin_flush_small(tsd, tcache, cache_bin, i, 0);
		} else {
			tcache_bin_flush_large(tsd, tcache, cache_bin, i, 0);
		}
		if (config_stats) {
			assert(cache_bin->tstats.nrequests == 0);
		}
	}
}

void
tcache_flush(tsd_t *tsd) {
	assert(tcache_available(tsd));
	tcache_flush_cache(tsd, tsd_tcachep_get(tsd));
}

static void
tcache_destroy(tsd_t *tsd, tcache_t *tcache, bool tsd_tcache) {
	tcache_slow_t *tcache_slow = tcache->tcache_slow;
	tcache_flush_cache(tsd, tcache);
	arena_t *arena = tcache_slow->arena;
	tcache_arena_dissociate(tsd_tsdn(tsd), tcache_slow, tcache);

	if (tsd_tcache) {
		cache_bin_t *cache_bin = &tcache->bins[0];
		cache_bin_assert_empty(cache_bin);
	}
	if (tsd_tcache && cache_bin_stack_use_thp()) {
		b0_dalloc_tcache_stack(tsd_tsdn(tsd), tcache_slow->dyn_alloc);
	} else {
		idalloctm(tsd_tsdn(tsd), tcache_slow->dyn_alloc, NULL, NULL,
		    true, true);
	}

	/*
	 * The deallocation and tcache flush above may not trigger decay since
	 * we are on the tcache shutdown path (potentially with non-nominal
	 * tsd).  Manually trigger decay to avoid pathological cases.  Also
	 * include arena 0 because the tcache array is allocated from it.
	 */
	arena_decay(
	    tsd_tsdn(tsd), arena_get(tsd_tsdn(tsd), 0, false), false, false);

	if (arena_nthreads_get(arena, false) == 0
	    && !background_thread_enabled()) {
		/* Force purging when no threads assigned to the arena anymore. */
		arena_decay(tsd_tsdn(tsd), arena,
		    /* is_background_thread */ false, /* all */ true);
	} else {
		arena_decay(tsd_tsdn(tsd), arena,
		    /* is_background_thread */ false, /* all */ false);
	}
}

/* For auto tcache (embedded in TSD) only. */
void
tcache_cleanup(tsd_t *tsd) {
	tcache_t *tcache = tsd_tcachep_get(tsd);
	if (!tcache_available(tsd)) {
		assert(tsd_tcache_enabled_get(tsd) == false);
		assert(cache_bin_still_zero_initialized(&tcache->bins[0]));
		return;
	}
	assert(tsd_tcache_enabled_get(tsd));
	assert(!cache_bin_still_zero_initialized(&tcache->bins[0]));

	tcache_destroy(tsd, tcache, true);
	/* Make sure all bins used are reinitialized to the clean state. */
	memset(tcache->bins, 0, sizeof(cache_bin_t) * TCACHE_NBINS_MAX);
}

void
tcache_stats_merge(tsdn_t *tsdn, tcache_t *tcache, arena_t *arena) {
	cassert(config_stats);

	/* Merge and reset tcache stats. */
	for (unsigned i = 0; i < tcache_nbins_get(tcache->tcache_slow); i++) {
		cache_bin_t *cache_bin = &tcache->bins[i];
		if (tcache_bin_disabled(i, cache_bin, tcache->tcache_slow)) {
			continue;
		}
		if (i < SC_NBINS) {
			bin_t *bin = bin_choose(tsdn, arena, i, NULL);
			malloc_mutex_lock(tsdn, &bin->lock);
			bin->stats.nrequests += cache_bin->tstats.nrequests;
			malloc_mutex_unlock(tsdn, &bin->lock);
		} else {
			arena_stats_large_flush_nrequests_add(tsdn,
			    &arena->stats, i, cache_bin->tstats.nrequests);
		}
		cache_bin->tstats.nrequests = 0;
	}
}

static bool
tcaches_create_prep(tsd_t *tsd, base_t *base) {
	bool err;

	malloc_mutex_assert_owner(tsd_tsdn(tsd), &tcaches_mtx);

	if (tcaches == NULL) {
		tcaches = base_alloc(tsd_tsdn(tsd), base,
		    sizeof(tcache_t *) * (MALLOCX_TCACHE_MAX + 1), CACHELINE);
		if (tcaches == NULL) {
			err = true;
			goto label_return;
		}
	}

	if (tcaches_avail == NULL && tcaches_past > MALLOCX_TCACHE_MAX) {
		err = true;
		goto label_return;
	}

	err = false;
label_return:
	return err;
}

bool
tcaches_create(tsd_t *tsd, base_t *base, unsigned *r_ind) {
	witness_assert_depth(tsdn_witness_tsdp_get(tsd_tsdn(tsd)), 0);

	bool err;

	malloc_mutex_lock(tsd_tsdn(tsd), &tcaches_mtx);

	if (tcaches_create_prep(tsd, base)) {
		err = true;
		goto label_return;
	}

	tcache_t *tcache = tcache_create_explicit(tsd);
	if (tcache == NULL) {
		err = true;
		goto label_return;
	}

	tcaches_t *elm;
	if (tcaches_avail != NULL) {
		elm = tcaches_avail;
		tcaches_avail = tcaches_avail->next;
		elm->tcache = tcache;
		*r_ind = (unsigned)(elm - tcaches);
	} else {
		elm = &tcaches[tcaches_past];
		elm->tcache = tcache;
		*r_ind = tcaches_past;
		tcaches_past++;
	}

	err = false;
label_return:
	malloc_mutex_unlock(tsd_tsdn(tsd), &tcaches_mtx);
	witness_assert_depth(tsdn_witness_tsdp_get(tsd_tsdn(tsd)), 0);
	return err;
}

static tcache_t *
tcaches_elm_remove(tsd_t *tsd, tcaches_t *elm, bool allow_reinit) {
	malloc_mutex_assert_owner(tsd_tsdn(tsd), &tcaches_mtx);

	if (elm->tcache == NULL) {
		return NULL;
	}
	tcache_t *tcache = elm->tcache;
	if (allow_reinit) {
		elm->tcache = TCACHES_ELM_NEED_REINIT;
	} else {
		elm->tcache = NULL;
	}

	if (tcache == TCACHES_ELM_NEED_REINIT) {
		return NULL;
	}
	return tcache;
}

void
tcaches_flush(tsd_t *tsd, unsigned ind) {
	malloc_mutex_lock(tsd_tsdn(tsd), &tcaches_mtx);
	tcache_t *tcache = tcaches_elm_remove(tsd, &tcaches[ind], true);
	malloc_mutex_unlock(tsd_tsdn(tsd), &tcaches_mtx);
	if (tcache != NULL) {
		/* Destroy the tcache; recreate in tcaches_get() if needed. */
		tcache_destroy(tsd, tcache, false);
	}
}

void
tcaches_destroy(tsd_t *tsd, unsigned ind) {
	malloc_mutex_lock(tsd_tsdn(tsd), &tcaches_mtx);
	tcaches_t *elm = &tcaches[ind];
	tcache_t  *tcache = tcaches_elm_remove(tsd, elm, false);
	elm->next = tcaches_avail;
	tcaches_avail = elm;
	malloc_mutex_unlock(tsd_tsdn(tsd), &tcaches_mtx);
	if (tcache != NULL) {
		tcache_destroy(tsd, tcache, false);
	}
}

bool
tcache_boot(tsdn_t *tsdn, base_t *base) {
	global_do_not_change_tcache_maxclass = sz_s2u(opt_tcache_max);
	assert(global_do_not_change_tcache_maxclass <= TCACHE_MAXCLASS_LIMIT);
	global_do_not_change_tcache_nbins =
	    sz_size2index(global_do_not_change_tcache_maxclass) + 1;
	/*
	 * Pre-compute default bin info and store the results in
	 * opt_tcache_ncached_max. After the changes here,
	 * opt_tcache_ncached_max should not be modified and should always be
	 * accessed using tcache_get_default_ncached_max.
	 */
	tcache_bin_info_compute(opt_tcache_ncached_max);

	if (malloc_mutex_init(&tcaches_mtx, "tcaches", WITNESS_RANK_TCACHES,
	        malloc_mutex_rank_exclusive)) {
		return true;
	}

	return false;
}

void
tcache_prefork(tsdn_t *tsdn) {
	malloc_mutex_prefork(tsdn, &tcaches_mtx);
}

void
tcache_postfork_parent(tsdn_t *tsdn) {
	malloc_mutex_postfork_parent(tsdn, &tcaches_mtx);
}

void
tcache_postfork_child(tsdn_t *tsdn) {
	malloc_mutex_postfork_child(tsdn, &tcaches_mtx);
}

void
tcache_assert_initialized(tcache_t *tcache) {
	assert(!cache_bin_still_zero_initialized(&tcache->bins[0]));
}

static te_enabled_t
tcache_gc_enabled(void) {
	return (opt_tcache_gc_incr_bytes > 0) ? te_enabled_yes : te_enabled_no;
}

/* Handles alloc and dalloc the same way */
te_base_cb_t tcache_gc_te_handler = {
    .enabled = &tcache_gc_enabled,
    .new_event_wait = &tcache_gc_new_event_wait,
    .postponed_event_wait = &tcache_gc_postponed_event_wait,
    .event_handler = &tcache_gc_event,
};

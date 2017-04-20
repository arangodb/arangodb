#ifndef JEMALLOC_INTERNAL_TCACHE_INLINES_H
#define JEMALLOC_INTERNAL_TCACHE_INLINES_H

#ifndef JEMALLOC_ENABLE_INLINE
void	tcache_event(tsd_t *tsd, tcache_t *tcache);
void	tcache_flush(void);
bool	tcache_enabled_get(void);
tcache_t *tcache_get(tsd_t *tsd, bool create);
void	tcache_enabled_set(bool enabled);
void	*tcache_alloc_easy(tcache_bin_t *tbin, bool *tcache_success);
void	*tcache_alloc_small(tsd_t *tsd, arena_t *arena, tcache_t *tcache,
    size_t size, szind_t ind, bool zero, bool slow_path);
void	*tcache_alloc_large(tsd_t *tsd, arena_t *arena, tcache_t *tcache,
    size_t size, szind_t ind, bool zero, bool slow_path);
void	tcache_dalloc_small(tsd_t *tsd, tcache_t *tcache, void *ptr,
    szind_t binind, bool slow_path);
void	tcache_dalloc_large(tsd_t *tsd, tcache_t *tcache, void *ptr,
    size_t size, bool slow_path);
tcache_t	*tcaches_get(tsd_t *tsd, unsigned ind);
#endif

#if (defined(JEMALLOC_ENABLE_INLINE) || defined(JEMALLOC_TCACHE_C_))
JEMALLOC_INLINE void
tcache_flush(void) {
	tsd_t *tsd;

	cassert(config_tcache);

	tsd = tsd_fetch();
	tcache_cleanup(tsd);
}

JEMALLOC_INLINE bool
tcache_enabled_get(void) {
	tsd_t *tsd;
	tcache_enabled_t tcache_enabled;

	cassert(config_tcache);

	tsd = tsd_fetch();
	tcache_enabled = tsd_tcache_enabled_get(tsd);
	if (tcache_enabled == tcache_enabled_default) {
		tcache_enabled = (tcache_enabled_t)opt_tcache;
		tsd_tcache_enabled_set(tsd, tcache_enabled);
	}

	return (bool)tcache_enabled;
}

JEMALLOC_INLINE void
tcache_enabled_set(bool enabled) {
	tsd_t *tsd;
	tcache_enabled_t tcache_enabled;

	cassert(config_tcache);

	tsd = tsd_fetch();

	tcache_enabled = (tcache_enabled_t)enabled;
	tsd_tcache_enabled_set(tsd, tcache_enabled);

	if (!enabled) {
		tcache_cleanup(tsd);
	}
}

JEMALLOC_ALWAYS_INLINE tcache_t *
tcache_get(tsd_t *tsd, bool create) {
	tcache_t *tcache;

	if (!config_tcache) {
		return NULL;
	}

	tcache = tsd_tcache_get(tsd);
	if (!create) {
		return tcache;
	}
	if (unlikely(tcache == NULL) && tsd_nominal(tsd)) {
		tcache = tcache_get_hard(tsd);
		tsd_tcache_set(tsd, tcache);
	}

	return tcache;
}

JEMALLOC_ALWAYS_INLINE void
tcache_event(tsd_t *tsd, tcache_t *tcache) {
	if (TCACHE_GC_INCR == 0) {
		return;
	}

	if (unlikely(ticker_tick(&tcache->gc_ticker))) {
		tcache_event_hard(tsd, tcache);
	}
}

JEMALLOC_ALWAYS_INLINE void *
tcache_alloc_easy(tcache_bin_t *tbin, bool *tcache_success) {
	void *ret;

	if (unlikely(tbin->ncached == 0)) {
		tbin->low_water = -1;
		*tcache_success = false;
		return NULL;
	}
	/*
	 * tcache_success (instead of ret) should be checked upon the return of
	 * this function.  We avoid checking (ret == NULL) because there is
	 * never a null stored on the avail stack (which is unknown to the
	 * compiler), and eagerly checking ret would cause pipeline stall
	 * (waiting for the cacheline).
	 */
	*tcache_success = true;
	ret = *(tbin->avail - tbin->ncached);
	tbin->ncached--;

	if (unlikely((int)tbin->ncached < tbin->low_water)) {
		tbin->low_water = tbin->ncached;
	}

	return ret;
}

JEMALLOC_ALWAYS_INLINE void *
tcache_alloc_small(tsd_t *tsd, arena_t *arena, tcache_t *tcache, size_t size,
    szind_t binind, bool zero, bool slow_path) {
	void *ret;
	tcache_bin_t *tbin;
	bool tcache_success;
	size_t usize JEMALLOC_CC_SILENCE_INIT(0);

	assert(binind < NBINS);
	tbin = &tcache->tbins[binind];
	ret = tcache_alloc_easy(tbin, &tcache_success);
	assert(tcache_success == (ret != NULL));
	if (unlikely(!tcache_success)) {
		bool tcache_hard_success;
		arena = arena_choose(tsd, arena);
		if (unlikely(arena == NULL)) {
			return NULL;
		}

		ret = tcache_alloc_small_hard(tsd_tsdn(tsd), arena, tcache,
		    tbin, binind, &tcache_hard_success);
		if (tcache_hard_success == false) {
			return NULL;
		}
	}

	assert(ret);
	/*
	 * Only compute usize if required.  The checks in the following if
	 * statement are all static.
	 */
	if (config_prof || (slow_path && config_fill) || unlikely(zero)) {
		usize = index2size(binind);
		assert(tcache_salloc(tsd_tsdn(tsd), ret) == usize);
	}

	if (likely(!zero)) {
		if (slow_path && config_fill) {
			if (unlikely(opt_junk_alloc)) {
				arena_alloc_junk_small(ret,
				    &arena_bin_info[binind], false);
			} else if (unlikely(opt_zero)) {
				memset(ret, 0, usize);
			}
		}
	} else {
		if (slow_path && config_fill && unlikely(opt_junk_alloc)) {
			arena_alloc_junk_small(ret, &arena_bin_info[binind],
			    true);
		}
		memset(ret, 0, usize);
	}

	if (config_stats) {
		tbin->tstats.nrequests++;
	}
	if (config_prof) {
		tcache->prof_accumbytes += usize;
	}
	tcache_event(tsd, tcache);
	return ret;
}

JEMALLOC_ALWAYS_INLINE void *
tcache_alloc_large(tsd_t *tsd, arena_t *arena, tcache_t *tcache, size_t size,
    szind_t binind, bool zero, bool slow_path) {
	void *ret;
	tcache_bin_t *tbin;
	bool tcache_success;

	assert(binind < nhbins);
	tbin = &tcache->tbins[binind];
	ret = tcache_alloc_easy(tbin, &tcache_success);
	assert(tcache_success == (ret != NULL));
	if (unlikely(!tcache_success)) {
		/*
		 * Only allocate one large object at a time, because it's quite
		 * expensive to create one and not use it.
		 */
		arena = arena_choose(tsd, arena);
		if (unlikely(arena == NULL)) {
			return NULL;
		}

		ret = large_malloc(tsd_tsdn(tsd), arena, s2u(size), zero);
		if (ret == NULL) {
			return NULL;
		}
	} else {
		size_t usize JEMALLOC_CC_SILENCE_INIT(0);

		/* Only compute usize on demand */
		if (config_prof || (slow_path && config_fill) ||
		    unlikely(zero)) {
			usize = index2size(binind);
			assert(usize <= tcache_maxclass);
		}

		if (likely(!zero)) {
			if (slow_path && config_fill) {
				if (unlikely(opt_junk_alloc)) {
					memset(ret, JEMALLOC_ALLOC_JUNK,
					    usize);
				} else if (unlikely(opt_zero)) {
					memset(ret, 0, usize);
				}
			}
		} else {
			memset(ret, 0, usize);
		}

		if (config_stats) {
			tbin->tstats.nrequests++;
		}
		if (config_prof) {
			tcache->prof_accumbytes += usize;
		}
	}

	tcache_event(tsd, tcache);
	return ret;
}

JEMALLOC_ALWAYS_INLINE void
tcache_dalloc_small(tsd_t *tsd, tcache_t *tcache, void *ptr, szind_t binind,
    bool slow_path) {
	tcache_bin_t *tbin;
	tcache_bin_info_t *tbin_info;

	assert(tcache_salloc(tsd_tsdn(tsd), ptr) <= SMALL_MAXCLASS);

	if (slow_path && config_fill && unlikely(opt_junk_free)) {
		arena_dalloc_junk_small(ptr, &arena_bin_info[binind]);
	}

	tbin = &tcache->tbins[binind];
	tbin_info = &tcache_bin_info[binind];
	if (unlikely(tbin->ncached == tbin_info->ncached_max)) {
		tcache_bin_flush_small(tsd, tcache, tbin, binind,
		    (tbin_info->ncached_max >> 1));
	}
	assert(tbin->ncached < tbin_info->ncached_max);
	tbin->ncached++;
	*(tbin->avail - tbin->ncached) = ptr;

	tcache_event(tsd, tcache);
}

JEMALLOC_ALWAYS_INLINE void
tcache_dalloc_large(tsd_t *tsd, tcache_t *tcache, void *ptr, size_t size,
    bool slow_path) {
	szind_t binind;
	tcache_bin_t *tbin;
	tcache_bin_info_t *tbin_info;

	assert(tcache_salloc(tsd_tsdn(tsd), ptr) > SMALL_MAXCLASS);
	assert(tcache_salloc(tsd_tsdn(tsd), ptr) <= tcache_maxclass);

	binind = size2index(size);

	if (slow_path && config_fill && unlikely(opt_junk_free)) {
		large_dalloc_junk(ptr, size);
	}

	tbin = &tcache->tbins[binind];
	tbin_info = &tcache_bin_info[binind];
	if (unlikely(tbin->ncached == tbin_info->ncached_max)) {
		tcache_bin_flush_large(tsd, tbin, binind,
		    (tbin_info->ncached_max >> 1), tcache);
	}
	assert(tbin->ncached < tbin_info->ncached_max);
	tbin->ncached++;
	*(tbin->avail - tbin->ncached) = ptr;

	tcache_event(tsd, tcache);
}

JEMALLOC_ALWAYS_INLINE tcache_t *
tcaches_get(tsd_t *tsd, unsigned ind) {
	tcaches_t *elm = &tcaches[ind];
	if (unlikely(elm->tcache == NULL)) {
		elm->tcache = tcache_create(tsd_tsdn(tsd), arena_choose(tsd,
		    NULL));
	}
	return elm->tcache;
}
#endif

#endif /* JEMALLOC_INTERNAL_TCACHE_INLINES_H */

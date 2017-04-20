#ifndef JEMALLOC_INTERNAL_RTREE_INLINES_H
#define JEMALLOC_INTERNAL_RTREE_INLINES_H

#ifndef JEMALLOC_ENABLE_INLINE
uintptr_t rtree_leafkey(uintptr_t key);
uintptr_t rtree_subkey(uintptr_t key, unsigned level);
extent_t *rtree_elm_read(rtree_elm_t *elm, bool dependent);
void rtree_elm_write(rtree_elm_t *elm, const extent_t *extent);
rtree_elm_t *rtree_elm_lookup(tsdn_t *tsdn, rtree_t *rtree,
    rtree_ctx_t *rtree_ctx, uintptr_t key, bool dependent, bool init_missing);
bool rtree_write(tsdn_t *tsdn, rtree_t *rtree, rtree_ctx_t *rtree_ctx,
    uintptr_t key, const extent_t *extent);
extent_t *rtree_read(tsdn_t *tsdn, rtree_t *rtree, rtree_ctx_t *rtree_ctx,
    uintptr_t key, bool dependent);
rtree_elm_t *rtree_elm_acquire(tsdn_t *tsdn, rtree_t *rtree,
    rtree_ctx_t *rtree_ctx, uintptr_t key, bool dependent, bool init_missing);
extent_t *rtree_elm_read_acquired(tsdn_t *tsdn, const rtree_t *rtree,
    rtree_elm_t *elm);
void rtree_elm_write_acquired(tsdn_t *tsdn, const rtree_t *rtree,
    rtree_elm_t *elm, const extent_t *extent);
void rtree_elm_release(tsdn_t *tsdn, const rtree_t *rtree, rtree_elm_t *elm);
void rtree_clear(tsdn_t *tsdn, rtree_t *rtree, rtree_ctx_t *rtree_ctx,
    uintptr_t key);
#endif

#if (defined(JEMALLOC_ENABLE_INLINE) || defined(JEMALLOC_RTREE_C_))
JEMALLOC_ALWAYS_INLINE uintptr_t
rtree_leafkey(uintptr_t key) {
	unsigned ptrbits = ZU(1) << (LG_SIZEOF_PTR+3);
	unsigned cumbits = (rtree_levels[RTREE_HEIGHT-1].cumbits -
	    rtree_levels[RTREE_HEIGHT-1].bits);
	unsigned maskbits = ptrbits - cumbits;
	uintptr_t mask = ~((ZU(1) << maskbits) - 1);
	return (key & mask);
}

JEMALLOC_ALWAYS_INLINE uintptr_t
rtree_subkey(uintptr_t key, unsigned level) {
	unsigned ptrbits = ZU(1) << (LG_SIZEOF_PTR+3);
	unsigned cumbits = rtree_levels[level].cumbits;
	unsigned shiftbits = ptrbits - cumbits;
	unsigned maskbits = rtree_levels[level].bits;
	uintptr_t mask = (ZU(1) << maskbits) - 1;
	return ((key >> shiftbits) & mask);
}

JEMALLOC_ALWAYS_INLINE extent_t *
rtree_elm_read(rtree_elm_t *elm, bool dependent) {
	extent_t *extent;

	if (dependent) {
		/*
		 * Reading a value on behalf of a pointer to a valid allocation
		 * is guaranteed to be a clean read even without
		 * synchronization, because the rtree update became visible in
		 * memory before the pointer came into existence.
		 */
		extent = elm->extent;
	} else {
		/*
		 * An arbitrary read, e.g. on behalf of ivsalloc(), may not be
		 * dependent on a previous rtree write, which means a stale read
		 * could result if synchronization were omitted here.
		 */
		extent = (extent_t *)atomic_read_p(&elm->pun);
	}

	/* Mask the lock bit. */
	extent = (extent_t *)((uintptr_t)extent & ~((uintptr_t)0x1));

	return extent;
}

JEMALLOC_INLINE void
rtree_elm_write(rtree_elm_t *elm, const extent_t *extent) {
	atomic_write_p(&elm->pun, extent);
}

JEMALLOC_ALWAYS_INLINE rtree_elm_t *
rtree_elm_lookup(tsdn_t *tsdn, rtree_t *rtree, rtree_ctx_t *rtree_ctx,
    uintptr_t key, bool dependent, bool init_missing) {
	assert(key != 0);
	assert(!dependent || !init_missing);

	uintptr_t leafkey = rtree_leafkey(key);
#define RTREE_CACHE_CHECK(i) do {					\
	if (likely(rtree_ctx->cache[i].leafkey == leafkey)) {		\
		rtree_elm_t *leaf = rtree_ctx->cache[i].leaf;		\
		if (likely(leaf != NULL)) {				\
			/* Reorder. */					\
			memmove(&rtree_ctx->cache[1],			\
			    &rtree_ctx->cache[0],			\
			    sizeof(rtree_ctx_cache_elm_t) * i);		\
			rtree_ctx->cache[0].leafkey = leafkey;		\
			rtree_ctx->cache[0].leaf = leaf;		\
									\
			uintptr_t subkey = rtree_subkey(key,		\
			    RTREE_HEIGHT-1);				\
			return &leaf[subkey];				\
		}							\
	}								\
} while (0)
	/* Check the MRU cache entry. */
	RTREE_CACHE_CHECK(0);
	/*
	 * Search the remaining cache elements, and on success move the matching
	 * element to the front.  Unroll the first iteration to avoid calling
	 * memmove() (the compiler typically optimizes it into raw moves).
	 */
	if (RTREE_CTX_NCACHE > 1) {
		RTREE_CACHE_CHECK(1);
	}
	for (unsigned i = 2; i < RTREE_CTX_NCACHE; i++) {
		RTREE_CACHE_CHECK(i);
	}
#undef RTREE_CACHE_CHECK

	return rtree_elm_lookup_hard(tsdn, rtree, rtree_ctx, key, dependent,
	    init_missing);
}

JEMALLOC_INLINE bool
rtree_write(tsdn_t *tsdn, rtree_t *rtree, rtree_ctx_t *rtree_ctx, uintptr_t key,
    const extent_t *extent) {
	rtree_elm_t *elm;

	assert(extent != NULL); /* Use rtree_clear() for this case. */
	assert(((uintptr_t)extent & (uintptr_t)0x1) == (uintptr_t)0x0);

	elm = rtree_elm_lookup(tsdn, rtree, rtree_ctx, key, false, true);
	if (elm == NULL) {
		return true;
	}
	assert(rtree_elm_read(elm, false) == NULL);
	rtree_elm_write(elm, extent);

	return false;
}

JEMALLOC_ALWAYS_INLINE extent_t *
rtree_read(tsdn_t *tsdn, rtree_t *rtree, rtree_ctx_t *rtree_ctx, uintptr_t key,
    bool dependent) {
	rtree_elm_t *elm;

	elm = rtree_elm_lookup(tsdn, rtree, rtree_ctx, key, dependent, false);
	if (!dependent && elm == NULL) {
		return NULL;
	}

	return rtree_elm_read(elm, dependent);
}

JEMALLOC_INLINE rtree_elm_t *
rtree_elm_acquire(tsdn_t *tsdn, rtree_t *rtree, rtree_ctx_t *rtree_ctx,
    uintptr_t key, bool dependent, bool init_missing) {
	rtree_elm_t *elm = rtree_elm_lookup(tsdn, rtree, rtree_ctx, key,
	    dependent, init_missing);
	if (!dependent && elm == NULL) {
		return NULL;
	}

	spin_t spinner = SPIN_INITIALIZER;
	while (true) {
		extent_t *extent = rtree_elm_read(elm, false);
		/* The least significant bit serves as a lock. */
		void *s = (void *)((uintptr_t)extent | (uintptr_t)0x1);
		if (!atomic_cas_p(&elm->pun, (void *)extent, s)) {
			break;
		}
		spin_adaptive(&spinner);
	}

	if (config_debug) {
		rtree_elm_witness_acquire(tsdn, rtree, key, elm);
	}

	return elm;
}

JEMALLOC_INLINE extent_t *
rtree_elm_read_acquired(tsdn_t *tsdn, const rtree_t *rtree, rtree_elm_t *elm) {
	extent_t *extent;

	assert(((uintptr_t)elm->pun & (uintptr_t)0x1) == (uintptr_t)0x1);
	extent = (extent_t *)((uintptr_t)elm->pun & ~((uintptr_t)0x1));
	assert(((uintptr_t)extent & (uintptr_t)0x1) == (uintptr_t)0x0);

	if (config_debug) {
		rtree_elm_witness_access(tsdn, rtree, elm);
	}

	return extent;
}

JEMALLOC_INLINE void
rtree_elm_write_acquired(tsdn_t *tsdn, const rtree_t *rtree, rtree_elm_t *elm,
    const extent_t *extent) {
	assert(((uintptr_t)extent & (uintptr_t)0x1) == (uintptr_t)0x0);
	assert(((uintptr_t)elm->pun & (uintptr_t)0x1) == (uintptr_t)0x1);

	if (config_debug) {
		rtree_elm_witness_access(tsdn, rtree, elm);
	}

	elm->pun = (void *)((uintptr_t)extent | (uintptr_t)0x1);
	assert(rtree_elm_read_acquired(tsdn, rtree, elm) == extent);
}

JEMALLOC_INLINE void
rtree_elm_release(tsdn_t *tsdn, const rtree_t *rtree, rtree_elm_t *elm) {
	rtree_elm_write(elm, rtree_elm_read_acquired(tsdn, rtree, elm));
	if (config_debug) {
		rtree_elm_witness_release(tsdn, rtree, elm);
	}
}

JEMALLOC_INLINE void
rtree_clear(tsdn_t *tsdn, rtree_t *rtree, rtree_ctx_t *rtree_ctx,
    uintptr_t key) {
	rtree_elm_t *elm;

	elm = rtree_elm_acquire(tsdn, rtree, rtree_ctx, key, true, false);
	rtree_elm_write_acquired(tsdn, rtree, elm, NULL);
	rtree_elm_release(tsdn, rtree, elm);
}
#endif

#endif /* JEMALLOC_INTERNAL_RTREE_INLINES_H */

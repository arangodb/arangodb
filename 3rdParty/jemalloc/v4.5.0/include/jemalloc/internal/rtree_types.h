#ifndef JEMALLOC_INTERNAL_RTREE_TYPES_H
#define JEMALLOC_INTERNAL_RTREE_TYPES_H

/*
 * This radix tree implementation is tailored to the singular purpose of
 * associating metadata with extents that are currently owned by jemalloc.
 *
 *******************************************************************************
 */

typedef struct rtree_elm_s rtree_elm_t;
typedef struct rtree_elm_witness_s rtree_elm_witness_t;
typedef struct rtree_elm_witness_tsd_s rtree_elm_witness_tsd_t;
typedef struct rtree_level_s rtree_level_t;
typedef struct rtree_ctx_cache_elm_s rtree_ctx_cache_elm_t;
typedef struct rtree_ctx_s rtree_ctx_t;
typedef struct rtree_s rtree_t;

/* Number of high insignificant bits. */
#define RTREE_NHIB ((1U << (LG_SIZEOF_PTR+3)) - LG_VADDR)
/* Number of low insigificant bits. */
#define RTREE_NLIB LG_PAGE
/* Number of significant bits. */
#define RTREE_NSB (LG_VADDR - RTREE_NLIB)
/* Number of levels in radix tree. */
#define RTREE_HEIGHT (sizeof(rtree_levels)/sizeof(rtree_level_t))

/*
 * Number of leafkey/leaf pairs to cache.  Each entry supports an entire leaf,
 * so the cache hit rate is typically high even with a small number of entries.
 * In rare cases extent activity will straddle the boundary between two leaf
 * nodes.  Furthermore, an arena may use a combination of dss and mmap.  Four
 * entries covers both of these considerations as long as locality of reference
 * is high, and/or total memory usage doesn't exceed the range supported by
 * those entries.  Note that as memory usage grows past the amount that this
 * cache can directly cover, the cache will become less effective if locality of
 * reference is low, but the consequence is merely cache misses while traversing
 * the tree nodes, and the cache will itself suffer cache misses if made overly
 * large, not to mention the cost of linear search.
 */
#define RTREE_CTX_NCACHE 8

/* Static initializer for rtree_ctx_t. */
#define RTREE_CTX_INITIALIZER	{					\
	{{0, NULL} /* C initializes all trailing elements to NULL. */}	\
}

/*
 * Maximum number of concurrently acquired elements per thread.  This controls
 * how many witness_t structures are embedded in tsd.  Ideally rtree_elm_t would
 * have a witness_t directly embedded, but that would dramatically bloat the
 * tree.  This must contain enough entries to e.g. coalesce two extents.
 */
#define RTREE_ELM_ACQUIRE_MAX	4

/* Initializers for rtree_elm_witness_tsd_t. */
#define RTREE_ELM_WITNESS_INITIALIZER {					\
	NULL,								\
	WITNESS_INITIALIZER("rtree_elm", WITNESS_RANK_RTREE_ELM)	\
}

#define RTREE_ELM_WITNESS_TSD_INITIALIZER {				\
	{								\
		RTREE_ELM_WITNESS_INITIALIZER,				\
		RTREE_ELM_WITNESS_INITIALIZER,				\
		RTREE_ELM_WITNESS_INITIALIZER,				\
		RTREE_ELM_WITNESS_INITIALIZER				\
	}								\
}

#endif /* JEMALLOC_INTERNAL_RTREE_TYPES_H */

#ifndef JEMALLOC_INTERNAL_STATS_TSD_H
#define JEMALLOC_INTERNAL_STATS_TSD_H

typedef struct tcache_bin_stats_s {
	/*
	 * Number of allocation requests that corresponded to the size of this
	 * bin.
	 */
	uint64_t	nrequests;
} tcache_bin_stats_t;

#endif /* JEMALLOC_INTERNAL_STATS_TSD_H */

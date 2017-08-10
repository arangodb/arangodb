#ifndef JEMALLOC_INTERNAL_STATS_H
#define JEMALLOC_INTERNAL_STATS_H

#include "jemalloc/internal/atomic.h"
#include "jemalloc/internal/mutex_prof.h"
#include "jemalloc/internal/mutex.h"
#include "jemalloc/internal/size_classes.h"
#include "jemalloc/internal/stats_tsd.h"

/*  OPTION(opt,		var_name,	default,	set_value_to) */
#define STATS_PRINT_OPTIONS						\
    OPTION('J',		json,		false,		true)		\
    OPTION('g',		general,	true,		false)		\
    OPTION('m',		merged,		config_stats,	false)		\
    OPTION('d',		destroyed,	config_stats,	false)		\
    OPTION('a',		unmerged,	config_stats,	false)		\
    OPTION('b',		bins,		true,		false)		\
    OPTION('l',		large,		true,		false)		\
    OPTION('x',		mutex,		true,		false)

enum {
#define OPTION(o, v, d, s) stats_print_option_num_##v,
    STATS_PRINT_OPTIONS
#undef OPTION
    stats_print_tot_num_options
};

/* Options for stats_print. */
extern bool opt_stats_print;
extern char opt_stats_print_opts[stats_print_tot_num_options+1];

/* Implements je_malloc_stats_print. */
void stats_print(void (*write_cb)(void *, const char *), void *cbopaque,
    const char *opts);

/*
 * In those architectures that support 64-bit atomics, we use atomic updates for
 * our 64-bit values.  Otherwise, we use a plain uint64_t and synchronize
 * externally.
 */
#ifdef JEMALLOC_ATOMIC_U64
typedef atomic_u64_t arena_stats_u64_t;
#else
/* Must hold the arena stats mutex while reading atomically. */
typedef uint64_t arena_stats_u64_t;
#endif

typedef struct malloc_bin_stats_s {
	/*
	 * Total number of allocation/deallocation requests served directly by
	 * the bin.  Note that tcache may allocate an object, then recycle it
	 * many times, resulting many increments to nrequests, but only one
	 * each to nmalloc and ndalloc.
	 */
	uint64_t	nmalloc;
	uint64_t	ndalloc;

	/*
	 * Number of allocation requests that correspond to the size of this
	 * bin.  This includes requests served by tcache, though tcache only
	 * periodically merges into this counter.
	 */
	uint64_t	nrequests;

	/*
	 * Current number of regions of this size class, including regions
	 * currently cached by tcache.
	 */
	size_t		curregs;

	/* Number of tcache fills from this bin. */
	uint64_t	nfills;

	/* Number of tcache flushes to this bin. */
	uint64_t	nflushes;

	/* Total number of slabs created for this bin's size class. */
	uint64_t	nslabs;

	/*
	 * Total number of slabs reused by extracting them from the slabs heap
	 * for this bin's size class.
	 */
	uint64_t	reslabs;

	/* Current number of slabs in this bin. */
	size_t		curslabs;

	mutex_prof_data_t mutex_data;
} malloc_bin_stats_t;

typedef struct malloc_large_stats_s {
	/*
	 * Total number of allocation/deallocation requests served directly by
	 * the arena.
	 */
	arena_stats_u64_t	nmalloc;
	arena_stats_u64_t	ndalloc;

	/*
	 * Number of allocation requests that correspond to this size class.
	 * This includes requests served by tcache, though tcache only
	 * periodically merges into this counter.
	 */
	arena_stats_u64_t	nrequests; /* Partially derived. */

	/* Current number of allocations of this size class. */
	size_t		curlextents; /* Derived. */
} malloc_large_stats_t;

typedef struct decay_stats_s {
	/* Total number of purge sweeps. */
	arena_stats_u64_t	npurge;
	/* Total number of madvise calls made. */
	arena_stats_u64_t	nmadvise;
	/* Total number of pages purged. */
	arena_stats_u64_t	purged;
} decay_stats_t;

/*
 * Arena stats.  Note that fields marked "derived" are not directly maintained
 * within the arena code; rather their values are derived during stats merge
 * requests.
 */
typedef struct arena_stats_s {
#ifndef JEMALLOC_ATOMIC_U64
	malloc_mutex_t		mtx;
#endif

	/* Number of bytes currently mapped, excluding retained memory. */
	atomic_zu_t		mapped; /* Partially derived. */

	/*
	 * Number of unused virtual memory bytes currently retained.  Retained
	 * bytes are technically mapped (though always decommitted or purged),
	 * but they are excluded from the mapped statistic (above).
	 */
	atomic_zu_t		retained; /* Derived. */

	decay_stats_t		decay_dirty;
	decay_stats_t		decay_muzzy;

	atomic_zu_t		base; /* Derived. */
	atomic_zu_t		internal;
	atomic_zu_t		resident; /* Derived. */

	atomic_zu_t		allocated_large; /* Derived. */
	arena_stats_u64_t	nmalloc_large; /* Derived. */
	arena_stats_u64_t	ndalloc_large; /* Derived. */
	arena_stats_u64_t	nrequests_large; /* Derived. */

	/* Number of bytes cached in tcache associated with this arena. */
	atomic_zu_t		tcache_bytes; /* Derived. */

	mutex_prof_data_t mutex_prof_data[mutex_prof_num_arena_mutexes];

	/* One element for each large size class. */
	malloc_large_stats_t	lstats[NSIZES - NBINS];

	/* Arena uptime. */
	nstime_t		uptime;
} arena_stats_t;

#endif /* JEMALLOC_INTERNAL_STATS_H */

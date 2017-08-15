#ifndef JEMALLOC_INTERNAL_MUTEX_PROF_H
#define JEMALLOC_INTERNAL_MUTEX_PROF_H

#include "jemalloc/internal/atomic.h"
#include "jemalloc/internal/nstime.h"
#include "jemalloc/internal/tsd_types.h"

#define MUTEX_PROF_GLOBAL_MUTEXES					\
    OP(background_thread)						\
    OP(ctl)								\
    OP(prof)

typedef enum {
#define OP(mtx) global_prof_mutex_##mtx,
	MUTEX_PROF_GLOBAL_MUTEXES
#undef OP
	mutex_prof_num_global_mutexes
} mutex_prof_global_ind_t;

#define MUTEX_PROF_ARENA_MUTEXES					\
    OP(large)								\
    OP(extent_avail)							\
    OP(extents_dirty)							\
    OP(extents_muzzy)							\
    OP(extents_retained)						\
    OP(decay_dirty)							\
    OP(decay_muzzy)							\
    OP(base)								\
    OP(tcache_list)

typedef enum {
#define OP(mtx) arena_prof_mutex_##mtx,
	MUTEX_PROF_ARENA_MUTEXES
#undef OP
	mutex_prof_num_arena_mutexes
} mutex_prof_arena_ind_t;

#define MUTEX_PROF_COUNTERS						\
    OP(num_ops, uint64_t)						\
    OP(num_wait, uint64_t)						\
    OP(num_spin_acq, uint64_t)						\
    OP(num_owner_switch, uint64_t)					\
    OP(total_wait_time, uint64_t)					\
    OP(max_wait_time, uint64_t)						\
    OP(max_num_thds, uint32_t)

typedef enum {
#define OP(counter, type) mutex_counter_##counter,
	MUTEX_PROF_COUNTERS
#undef OP
	mutex_prof_num_counters
} mutex_prof_counter_ind_t;

typedef struct {
	/*
	 * Counters touched on the slow path, i.e. when there is lock
	 * contention.  We update them once we have the lock.
	 */
	/* Total time (in nano seconds) spent waiting on this mutex. */
	nstime_t		tot_wait_time;
	/* Max time (in nano seconds) spent on a single lock operation. */
	nstime_t		max_wait_time;
	/* # of times have to wait for this mutex (after spinning). */
	uint64_t		n_wait_times;
	/* # of times acquired the mutex through local spinning. */
	uint64_t		n_spin_acquired;
	/* Max # of threads waiting for the mutex at the same time. */
	uint32_t		max_n_thds;
	/* Current # of threads waiting on the lock.  Atomic synced. */
	atomic_u32_t		n_waiting_thds;

	/*
	 * Data touched on the fast path.  These are modified right after we
	 * grab the lock, so it's placed closest to the end (i.e. right before
	 * the lock) so that we have a higher chance of them being on the same
	 * cacheline.
	 */
	/* # of times the mutex holder is different than the previous one. */
	uint64_t		n_owner_switches;
	/* Previous mutex holder, to facilitate n_owner_switches. */
	tsdn_t			*prev_owner;
	/* # of lock() operations in total. */
	uint64_t		n_lock_ops;
} mutex_prof_data_t;

#endif /* JEMALLOC_INTERNAL_MUTEX_PROF_H */

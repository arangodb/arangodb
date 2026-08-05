#ifndef JEMALLOC_INTERNAL_THREAD_EVENT_REGISTRY_H
#define JEMALLOC_INTERNAL_THREAD_EVENT_REGISTRY_H

#include "jemalloc/internal/jemalloc_preamble.h"
#include "jemalloc/internal/tsd_types.h"

#define TE_MAX_USER_EVENTS 4

/* "te" is short for "thread_event" */
enum te_alloc_e {
#ifdef JEMALLOC_PROF
	te_alloc_prof_sample,
#endif
	te_alloc_stats_interval,
	te_alloc_tcache_gc,
#ifdef JEMALLOC_STATS
	te_alloc_peak,
#endif
	te_alloc_user0,
	te_alloc_user1,
	te_alloc_user2,
	te_alloc_user3,
	te_alloc_last = te_alloc_user3,
	te_alloc_count = te_alloc_last + 1
};
typedef enum te_alloc_e te_alloc_t;

enum te_dalloc_e {
	te_dalloc_tcache_gc,
#ifdef JEMALLOC_STATS
	te_dalloc_peak,
#endif
	te_dalloc_user0,
	te_dalloc_user1,
	te_dalloc_user2,
	te_dalloc_user3,
	te_dalloc_last = te_dalloc_user3,
	te_dalloc_count = te_dalloc_last + 1
};
typedef enum te_dalloc_e te_dalloc_t;

/* These will live in tsd */
typedef struct te_data_s te_data_t;
struct te_data_s {
	uint64_t alloc_wait[te_alloc_count];
	uint64_t dalloc_wait[te_dalloc_count];
};
#define TE_DATA_INITIALIZER                                                    \
	{                                                                      \
		{0}, {                                                         \
			0                                                      \
		}                                                              \
	}

/*
 * Check if user event is installed, installed and enabled, or not
 * installed.
 *
 */
enum te_enabled_e { te_enabled_not_installed, te_enabled_yes, te_enabled_no };
typedef enum te_enabled_e te_enabled_t;

typedef struct te_base_cb_s te_base_cb_t;
struct te_base_cb_s {
	te_enabled_t (*enabled)(void);
	uint64_t (*new_event_wait)(tsd_t *tsd);
	uint64_t (*postponed_event_wait)(tsd_t *tsd);
	void (*event_handler)(tsd_t *tsd);
};

extern te_base_cb_t *te_alloc_handlers[te_alloc_count];
extern te_base_cb_t *te_dalloc_handlers[te_dalloc_count];

bool experimental_thread_events_boot(void);

/*
 *  User callback for thread events
 *
 *  is_alloc - true if event is allocation, false if event is free
 *  tallocated  - number of bytes allocated on current thread so far
 *  tdallocated - number of bytes allocated on current thread so far
 */
typedef void (*user_event_cb_t)(
    bool is_alloc, uint64_t tallocated, uint64_t tdallocated);

typedef struct user_hook_object_s user_hook_object_t;
struct user_hook_object_s {
	user_event_cb_t callback;
	uint64_t        interval;
	bool            is_alloc_only;
};

/*
 * register user callback
 *
 * return zero if event was registered
 *
 * if interval is zero or callback is NULL, or
 * no more slots are available event will not be registered
 * and non-zero value will be returned
 *
 */
int te_register_user_handler(tsdn_t *tsdn, user_hook_object_t *te_uobj);

te_enabled_t te_user_event_enabled(size_t ue_idx, bool is_alloc);

#endif /* JEMALLOC_INTERNAL_THREAD_EVENT_REGISTRY_H */

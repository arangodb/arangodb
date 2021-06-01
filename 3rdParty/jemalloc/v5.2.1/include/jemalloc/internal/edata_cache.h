#ifndef JEMALLOC_INTERNAL_EDATA_CACHE_H
#define JEMALLOC_INTERNAL_EDATA_CACHE_H

#include "jemalloc/internal/base.h"

/*
 * Public for tests.  When we go to the fallback when the small cache is empty,
 * we grab up to 8 items (grabbing less only if the fallback is exhausted).
 * When we exceed 16, we flush.  This caps the maximum memory lost per cache to
 * 16 * sizeof(edata_t), a max of 2k on architectures where the edata_t is 128
 * bytes.
 */
#define EDATA_CACHE_SMALL_MAX 16
#define EDATA_CACHE_SMALL_FILL 8

/*
 * A cache of edata_t structures allocated via base_alloc_edata (as opposed to
 * the underlying extents they describe).  The contents of returned edata_t
 * objects are garbage and cannot be relied upon.
 */

typedef struct edata_cache_s edata_cache_t;
struct edata_cache_s {
	edata_avail_t avail;
	atomic_zu_t count;
	malloc_mutex_t mtx;
	base_t *base;
};

bool edata_cache_init(edata_cache_t *edata_cache, base_t *base);
edata_t *edata_cache_get(tsdn_t *tsdn, edata_cache_t *edata_cache);
void edata_cache_put(tsdn_t *tsdn, edata_cache_t *edata_cache, edata_t *edata);

void edata_cache_prefork(tsdn_t *tsdn, edata_cache_t *edata_cache);
void edata_cache_postfork_parent(tsdn_t *tsdn, edata_cache_t *edata_cache);
void edata_cache_postfork_child(tsdn_t *tsdn, edata_cache_t *edata_cache);

/*
 * An edata_cache_small is like an edata_cache, but it relies on external
 * synchronization and avoids first-fit strategies.
 */

typedef struct edata_cache_small_s edata_cache_small_t;
struct edata_cache_small_s {
	edata_list_inactive_t list;
	size_t count;
	edata_cache_t *fallback;
	bool disabled;
};

void edata_cache_small_init(edata_cache_small_t *ecs, edata_cache_t *fallback);
edata_t *edata_cache_small_get(tsdn_t *tsdn, edata_cache_small_t *ecs);
void edata_cache_small_put(tsdn_t *tsdn, edata_cache_small_t *ecs,
    edata_t *edata);
void edata_cache_small_disable(tsdn_t *tsdn, edata_cache_small_t *ecs);

#endif /* JEMALLOC_INTERNAL_EDATA_CACHE_H */

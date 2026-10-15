#ifndef JEMALLOC_INTERNAL_HPA_CENTRAL_H
#define JEMALLOC_INTERNAL_HPA_CENTRAL_H

#include "jemalloc/internal/jemalloc_preamble.h"
#include "jemalloc/internal/base.h"
#include "jemalloc/internal/hpa_hooks.h"
#include "jemalloc/internal/hpdata.h"
#include "jemalloc/internal/mutex.h"
#include "jemalloc/internal/tsd_types.h"

typedef struct hpa_central_s hpa_central_t;
struct hpa_central_s {
	/*
	 * Guards expansion of eden.  We separate this from the regular mutex so
	 * that cheaper operations can still continue while we're doing the OS
	 * call.
	 */
	malloc_mutex_t grow_mtx;
	/*
	 * Either NULL (if empty), or some integer multiple of a
	 * hugepage-aligned number of hugepages.  We carve them off one at a
	 * time to satisfy new pageslab requests.
	 *
	 * Guarded by grow_mtx.
	 */
	void  *eden;
	size_t eden_len;
	/* Source for metadata. */
	base_t *base;

	/* The HPA hooks. */
	hpa_hooks_t hooks;
};

bool hpa_central_init(
    hpa_central_t *central, base_t *base, const hpa_hooks_t *hooks);

hpdata_t *hpa_central_extract(tsdn_t *tsdn, hpa_central_t *central, size_t size,
    uint64_t age, bool hugify_eager, bool *oom);

#endif /* JEMALLOC_INTERNAL_HPA_CENTRAL_H */

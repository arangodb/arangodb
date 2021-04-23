#ifndef JEMALLOC_INTERNAL_HPA_CENTRAL_H
#define JEMALLOC_INTERNAL_HPA_CENTRAL_H

#include "jemalloc/internal/base.h"
#include "jemalloc/internal/emap.h"

typedef struct hpa_central_s hpa_central_t;
struct hpa_central_s {
	/* The emap we use for metadata operations. */
	emap_t *emap;

	edata_cache_small_t ecs;
	eset_t eset;

	size_t sn_next;
};

void hpa_central_init(hpa_central_t *central, edata_cache_t *edata_cache,
    emap_t *emap);
/*
 * Tries to satisfy the given allocation request with an extent already given to
 * central.
 */
edata_t *hpa_central_alloc_reuse(tsdn_t *tsdn, hpa_central_t *central,
    size_t size_min, size_t size_goal);
/*
 * Adds the given edata to the central allocator as a new allocation.  The
 * intent is that after a reuse attempt fails, the caller can allocate a new
 * extent using whatever growth policy it prefers and allocate from that, giving
 * the excess to the hpa_central_t (this is analogous to the
 * extent_grow_retained functionality; we can allocate address space in
 * exponentially growing chunks).
 *
 * The edata_t should come from the same base that this hpa was initialized
 * with.  Only complete extents should be added (i.e. those for which the head
 * bit is true, and for which their successor is either not owned by jemalloc
 * or also has a head bit of true).  It should be active, large enough to
 * satisfy the requested allocation, and not already in the emap.
 *
 * If this returns true, then we did not accept the extent, and took no action.
 * Otherwise, modifies *edata to satisfy the allocation.
 */
bool hpa_central_alloc_grow(tsdn_t *tsdn, hpa_central_t *central,
    size_t size, edata_t *to_add);
void hpa_central_dalloc(tsdn_t *tsdn, hpa_central_t *central, edata_t *edata);

#endif /* JEMALLOC_INTERNAL_HPA_CENTRAL_H */

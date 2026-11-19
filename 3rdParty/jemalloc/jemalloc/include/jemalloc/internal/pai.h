#ifndef JEMALLOC_INTERNAL_PAI_H
#define JEMALLOC_INTERNAL_PAI_H

#include "jemalloc/internal/jemalloc_preamble.h"
#include "jemalloc/internal/edata.h"
#include "jemalloc/internal/tsd_types.h"

/* An interface for page allocation. */

typedef struct pai_s pai_t;
struct pai_s {
	/* Returns NULL on failure. */
	edata_t *(*alloc)(tsdn_t *tsdn, pai_t *self, size_t size,
	    size_t alignment, bool zero, bool guarded, bool frequent_reuse,
	    bool *deferred_work_generated);
	bool (*expand)(tsdn_t *tsdn, pai_t *self, edata_t *edata,
	    size_t old_size, size_t new_size, bool zero,
	    bool *deferred_work_generated);
	bool (*shrink)(tsdn_t *tsdn, pai_t *self, edata_t *edata,
	    size_t old_size, size_t new_size, bool *deferred_work_generated);
	void (*dalloc)(tsdn_t *tsdn, pai_t *self, edata_t *edata,
	    bool *deferred_work_generated);
	uint64_t (*time_until_deferred_work)(tsdn_t *tsdn, pai_t *self);
};

/*
 * These are just simple convenience functions to avoid having to reference the
 * same pai_t twice on every invocation.
 */

static inline edata_t *
pai_alloc(tsdn_t *tsdn, pai_t *self, size_t size, size_t alignment, bool zero,
    bool guarded, bool frequent_reuse, bool *deferred_work_generated) {
	return self->alloc(tsdn, self, size, alignment, zero, guarded,
	    frequent_reuse, deferred_work_generated);
}

static inline bool
pai_expand(tsdn_t *tsdn, pai_t *self, edata_t *edata, size_t old_size,
    size_t new_size, bool zero, bool *deferred_work_generated) {
	return self->expand(tsdn, self, edata, old_size, new_size, zero,
	    deferred_work_generated);
}

static inline bool
pai_shrink(tsdn_t *tsdn, pai_t *self, edata_t *edata, size_t old_size,
    size_t new_size, bool *deferred_work_generated) {
	return self->shrink(
	    tsdn, self, edata, old_size, new_size, deferred_work_generated);
}

static inline void
pai_dalloc(
    tsdn_t *tsdn, pai_t *self, edata_t *edata, bool *deferred_work_generated) {
	self->dalloc(tsdn, self, edata, deferred_work_generated);
}

static inline uint64_t
pai_time_until_deferred_work(tsdn_t *tsdn, pai_t *self) {
	return self->time_until_deferred_work(tsdn, self);
}

#endif /* JEMALLOC_INTERNAL_PAI_H */

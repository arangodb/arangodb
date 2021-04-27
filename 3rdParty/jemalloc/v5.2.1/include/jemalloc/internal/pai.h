#ifndef JEMALLOC_INTERNAL_PAI_H
#define JEMALLOC_INTERNAL_PAI_H

/* An interface for page allocation. */

typedef struct pai_s pai_t;
struct pai_s {
	/* Returns NULL on failure. */
	edata_t *(*alloc)(tsdn_t *tsdn, pai_t *self, size_t size,
	    size_t alignment, bool zero);
	/*
	 * Returns the number of extents added to the list (which may be fewer
	 * than requested, in case of OOM).  The list should already be
	 * initialized.  The only alignment guarantee is page-alignment, and
	 * the results are not necessarily zeroed.
	 */
	size_t (*alloc_batch)(tsdn_t *tsdn, pai_t *self, size_t size,
	    size_t nallocs, edata_list_active_t *results);
	bool (*expand)(tsdn_t *tsdn, pai_t *self, edata_t *edata,
	    size_t old_size, size_t new_size, bool zero);
	bool (*shrink)(tsdn_t *tsdn, pai_t *self, edata_t *edata,
	    size_t old_size, size_t new_size);
	void (*dalloc)(tsdn_t *tsdn, pai_t *self, edata_t *edata);
	/* This function empties out list as a side-effect of being called. */
	void (*dalloc_batch)(tsdn_t *tsdn, pai_t *self,
	    edata_list_active_t *list);
};

/*
 * These are just simple convenience functions to avoid having to reference the
 * same pai_t twice on every invocation.
 */

static inline edata_t *
pai_alloc(tsdn_t *tsdn, pai_t *self, size_t size, size_t alignment, bool zero) {
	return self->alloc(tsdn, self, size, alignment, zero);
}

static inline size_t
pai_alloc_batch(tsdn_t *tsdn, pai_t *self, size_t size, size_t nallocs,
    edata_list_active_t *results) {
	return self->alloc_batch(tsdn, self, size, nallocs, results);
}

static inline bool
pai_expand(tsdn_t *tsdn, pai_t *self, edata_t *edata, size_t old_size,
    size_t new_size, bool zero) {
	return self->expand(tsdn, self, edata, old_size, new_size, zero);
}

static inline bool
pai_shrink(tsdn_t *tsdn, pai_t *self, edata_t *edata, size_t old_size,
    size_t new_size) {
	return self->shrink(tsdn, self, edata, old_size, new_size);
}

static inline void
pai_dalloc(tsdn_t *tsdn, pai_t *self, edata_t *edata) {
	self->dalloc(tsdn, self, edata);
}

static inline void
pai_dalloc_batch(tsdn_t *tsdn, pai_t *self, edata_list_active_t *list) {
	return self->dalloc_batch(tsdn, self, list);
}

/*
 * An implementation of batch allocation that simply calls alloc once for
 * each item in the list.
 */
size_t pai_alloc_batch_default(tsdn_t *tsdn, pai_t *self, size_t size,
    size_t nallocs, edata_list_active_t *results);
/* Ditto, for dalloc. */
void pai_dalloc_batch_default(tsdn_t *tsdn, pai_t *self,
    edata_list_active_t *list);

#endif /* JEMALLOC_INTERNAL_PAI_H */

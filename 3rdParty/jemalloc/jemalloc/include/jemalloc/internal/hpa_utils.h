#ifndef JEMALLOC_INTERNAL_HPA_UTILS_H
#define JEMALLOC_INTERNAL_HPA_UTILS_H

#include "jemalloc/internal/hpa.h"
#include "jemalloc/internal/extent.h"

#define HPA_MIN_VAR_VEC_SIZE 8
/*
 * This is used for jemalloc internal tuning and may change in the future based
 * on production traffic.
 *
 * This value protects two things:
 *    1. Stack size
 *    2. Number of huge pages that are being purged in a batch as we do not
 *       allow allocations while making madvise syscall.
 */
#define HPA_PURGE_BATCH_MAX 16

#ifdef JEMALLOC_HAVE_PROCESS_MADVISE
typedef struct iovec hpa_io_vector_t;
#else
typedef struct {
	void  *iov_base;
	size_t iov_len;
} hpa_io_vector_t;
#endif

static inline size_t
hpa_process_madvise_max_iovec_len(void) {
	assert(
	    opt_process_madvise_max_batch <= PROCESS_MADVISE_MAX_BATCH_LIMIT);
	return opt_process_madvise_max_batch == 0
	    ? HPA_MIN_VAR_VEC_SIZE
	    : opt_process_madvise_max_batch;
}

/* Actually invoke hooks. If we fail vectorized, use single purges */
static void
hpa_try_vectorized_purge(
    hpa_hooks_t *hooks, hpa_io_vector_t *vec, size_t vlen, size_t nbytes) {
	bool success = opt_process_madvise_max_batch > 0
	    && !hooks->vectorized_purge(vec, vlen, nbytes);
	if (!success) {
		/* On failure, it is safe to purge again (potential perf
		 * penalty) If kernel can tell exactly which regions
		 * failed, we could avoid that penalty.
		 */
		for (size_t i = 0; i < vlen; ++i) {
			hooks->purge(vec[i].iov_base, vec[i].iov_len);
		}
	}
}

/*
 * This structure accumulates the regions for process_madvise. It invokes the
 * hook when batch limit is reached.
 */
typedef struct {
	hpa_io_vector_t *vp;
	size_t           cur;
	size_t           total_bytes;
	size_t           capacity;
} hpa_range_accum_t;

static inline void
hpa_range_accum_init(hpa_range_accum_t *ra, hpa_io_vector_t *v, size_t sz) {
	ra->vp = v;
	ra->capacity = sz;
	ra->total_bytes = 0;
	ra->cur = 0;
}

static inline void
hpa_range_accum_flush(hpa_range_accum_t *ra, hpa_hooks_t *hooks) {
	assert(ra->total_bytes > 0 && ra->cur > 0);
	hpa_try_vectorized_purge(hooks, ra->vp, ra->cur, ra->total_bytes);
	ra->cur = 0;
	ra->total_bytes = 0;
}

static inline void
hpa_range_accum_add(
    hpa_range_accum_t *ra, void *addr, size_t sz, hpa_hooks_t *hooks) {
	assert(ra->cur < ra->capacity);

	ra->vp[ra->cur].iov_base = addr;
	ra->vp[ra->cur].iov_len = sz;
	ra->total_bytes += sz;
	ra->cur++;

	if (ra->cur == ra->capacity) {
		hpa_range_accum_flush(ra, hooks);
	}
}

static inline void
hpa_range_accum_finish(hpa_range_accum_t *ra, hpa_hooks_t *hooks) {
	if (ra->cur > 0) {
		hpa_range_accum_flush(ra, hooks);
	}
}

/*
 * For purging more than one page we use batch of these items
 */
typedef struct {
	hpdata_purge_state_t state;
	hpdata_t            *hp;
	bool                 dehugify;
} hpa_purge_item_t;

typedef struct hpa_purge_batch_s hpa_purge_batch_t;
struct hpa_purge_batch_s {
	hpa_purge_item_t *items;
	size_t            items_capacity;
	/* Number of huge pages to purge in current batch */
	size_t item_cnt;
	/* Number of ranges to purge in current batch */
	size_t nranges;
	/* Total number of dirty pages in current batch*/
	size_t ndirty_in_batch;

	/* Max number of huge pages to purge */
	size_t max_hp;
	/*
	 * Once we are above this watermark we should not add more pages
	 * to the same batch. This is because while we want to minimize
	 * number of madvise calls we also do not want to be preventing
	 * allocations from too many huge pages (which we have to do
	 * while they are being purged)
	 */
	size_t range_watermark;

	size_t npurged_hp_total;
};

static inline bool
hpa_batch_full(hpa_purge_batch_t *b) {
	/* It's okay for ranges to go above */
	return b->npurged_hp_total == b->max_hp
	    || b->item_cnt == b->items_capacity
	    || b->nranges >= b->range_watermark;
}

static inline void
hpa_batch_pass_start(hpa_purge_batch_t *b) {
	b->item_cnt = 0;
	b->nranges = 0;
	b->ndirty_in_batch = 0;
}

static inline bool
hpa_batch_empty(hpa_purge_batch_t *b) {
	return b->item_cnt == 0;
}

/* Purge pages in a batch using given hooks */
void hpa_purge_batch(
    hpa_hooks_t *hooks, hpa_purge_item_t *batch, size_t batch_sz);

#endif /* JEMALLOC_INTERNAL_HPA_UTILS_H */

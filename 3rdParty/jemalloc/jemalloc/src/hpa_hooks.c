#include "jemalloc/internal/jemalloc_preamble.h"
#include "jemalloc/internal/jemalloc_internal_includes.h"

#include "jemalloc/internal/hpa_hooks.h"
#include "jemalloc/internal/jemalloc_probe.h"

static void    *hpa_hooks_map(size_t size);
static void     hpa_hooks_unmap(void *ptr, size_t size);
static void     hpa_hooks_purge(void *ptr, size_t size);
static bool     hpa_hooks_hugify(void *ptr, size_t size, bool sync);
static void     hpa_hooks_dehugify(void *ptr, size_t size);
static void     hpa_hooks_curtime(nstime_t *r_nstime, bool first_reading);
static uint64_t hpa_hooks_ms_since(nstime_t *past_nstime);
static bool hpa_hooks_vectorized_purge(void *vec, size_t vlen, size_t nbytes);

const hpa_hooks_t hpa_hooks_default = {&hpa_hooks_map, &hpa_hooks_unmap,
    &hpa_hooks_purge, &hpa_hooks_hugify, &hpa_hooks_dehugify,
    &hpa_hooks_curtime, &hpa_hooks_ms_since, &hpa_hooks_vectorized_purge};

static void *
hpa_hooks_map(size_t size) {
	/*
	 * During development, we're primarily concerned with systems
	 * that overcommit.  Eventually, we should be more careful here.
	 */

	bool commit = true;
	assert((size & HUGEPAGE_MASK) == 0);
	void *ret = pages_map(NULL, size, HUGEPAGE, &commit);
	JE_USDT(hpa_map, 2, size, ret);
	return ret;
}

static void
hpa_hooks_unmap(void *ptr, size_t size) {
	JE_USDT(hpa_unmap, 2, size, ptr);
	pages_unmap(ptr, size);
}

static void
hpa_hooks_purge(void *ptr, size_t size) {
	JE_USDT(hpa_purge, 2, size, ptr);
	pages_purge_forced(ptr, size);
}

static bool
hpa_hooks_hugify(void *ptr, size_t size, bool sync) {
	/*
	 * We mark memory range as huge independently on which hugification
	 * technique is used (synchronous or asynchronous) to have correct
	 * VmFlags set for introspection and accounting purposes.  If
	 * synchronous hugification is enabled and pages_collapse call fails,
	 * then we hope memory range will be hugified asynchronously by
	 * khugepaged eventually.  Right now, 3 out of 4 error return codes of
	 * madvise(..., MADV_COLLAPSE) are retryable.  Instead of retrying, we
	 * just fallback to asynchronous khugepaged hugification to simplify
	 * implementation, even if we might know khugepaged fallback will not
	 * be successful (current madvise(..., MADV_COLLAPSE) implementation
	 * hints, when EINVAL is returned it is likely that khugepaged won't be
	 * able to collapse memory range into hugepage either).
	 */
	bool err = pages_huge(ptr, size);
	if (sync) {
		err = pages_collapse(ptr, size);
	}
	JE_USDT(hpa_hugify, 4, size, ptr, err, sync);
	return err;
}

static void
hpa_hooks_dehugify(void *ptr, size_t size) {
	bool err = pages_nohuge(ptr, size);
	JE_USDT(hpa_dehugify, 3, size, ptr, err);
	(void)err;
}

static void
hpa_hooks_curtime(nstime_t *r_nstime, bool first_reading) {
	if (first_reading) {
		nstime_init_zero(r_nstime);
	}
	nstime_update(r_nstime);
}

static uint64_t
hpa_hooks_ms_since(nstime_t *past_nstime) {
	return nstime_ms_since(past_nstime);
}

/* Return true if we did not purge all nbytes, or on some error */
static bool
hpa_hooks_vectorized_purge(void *vec, size_t vlen, size_t nbytes) {
#ifdef JEMALLOC_HAVE_PROCESS_MADVISE
	bool err = pages_purge_process_madvise(vec, vlen, nbytes);
	JE_USDT(hpa_vectorized_purge, 3, nbytes, vlen, err);
	return err;
#else
	return true;
#endif
}

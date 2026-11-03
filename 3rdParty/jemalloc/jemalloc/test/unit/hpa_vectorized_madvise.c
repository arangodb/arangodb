#include "test/jemalloc_test.h"

#include "jemalloc/internal/hpa.h"
#include "jemalloc/internal/nstime.h"

#define SHARD_IND 111

#define ALLOC_MAX (HUGEPAGE)

typedef struct test_data_s test_data_t;
struct test_data_s {
	/*
	 * Must be the first member -- we convert back and forth between the
	 * test_data_t and the hpa_shard_t;
	 */
	hpa_shard_t   shard;
	hpa_central_t central;
	base_t       *base;
	edata_cache_t shard_edata_cache;

	emap_t emap;
};

static hpa_shard_opts_t test_hpa_shard_opts_default = {
    /* slab_max_alloc */
    ALLOC_MAX,
    /* hugification_threshold */
    HUGEPAGE,
    /* dirty_mult */
    FXP_INIT_PERCENT(25),
    /* deferral_allowed */
    false,
    /* hugify_delay_ms */
    10 * 1000,
    /* hugify_sync */
    false,
    /* min_purge_interval_ms */
    5 * 1000,
    /* experimental_max_purge_nhp */
    -1,
    /* purge_threshold */
    1,
    /* purge_delay_ms */
    0,
    /* hugify_style */
    hpa_hugify_style_lazy};

static hpa_shard_t *
create_test_data(const hpa_hooks_t *hooks, hpa_shard_opts_t *opts) {
	bool    err;
	base_t *base = base_new(TSDN_NULL, /* ind */ SHARD_IND,
	    &ehooks_default_extent_hooks, /* metadata_use_hooks */ true);
	assert_ptr_not_null(base, "");

	test_data_t *test_data = malloc(sizeof(test_data_t));
	assert_ptr_not_null(test_data, "");

	test_data->base = base;

	err = edata_cache_init(&test_data->shard_edata_cache, base);
	assert_false(err, "");

	err = emap_init(&test_data->emap, test_data->base, /* zeroed */ false);
	assert_false(err, "");

	err = hpa_central_init(&test_data->central, test_data->base, hooks);
	assert_false(err, "");

	sec_opts_t sec_opts;
	sec_opts.nshards = 0;
	tsdn_t *tsdn = tsd_tsdn(tsd_fetch());
	err = hpa_shard_init(tsdn, &test_data->shard, &test_data->central,
	    &test_data->emap, test_data->base, &test_data->shard_edata_cache,
	    SHARD_IND, opts, &sec_opts);
	assert_false(err, "");

	return (hpa_shard_t *)test_data;
}

static void
destroy_test_data(hpa_shard_t *shard) {
	test_data_t *test_data = (test_data_t *)shard;
	base_delete(TSDN_NULL, test_data->base);
	free(test_data);
}

static uintptr_t defer_bump_ptr = HUGEPAGE * 123;
static void *
defer_test_map(size_t size) {
	void *result = (void *)defer_bump_ptr;
	defer_bump_ptr += size;
	return result;
}

static void
defer_test_unmap(void *ptr, size_t size) {
	(void)ptr;
	(void)size;
}

static size_t ndefer_purge_calls = 0;
static void
defer_test_purge(void *ptr, size_t size) {
	(void)ptr;
	(void)size;
	++ndefer_purge_calls;
}

static size_t ndefer_vec_purge_calls = 0;
static bool
defer_vectorized_purge(void *vec, size_t vlen, size_t nbytes) {
	(void)vec;
	(void)nbytes;
	++ndefer_vec_purge_calls;
	return false;
}

static bool defer_vec_purge_didfail = false;
static bool
defer_vectorized_purge_fail(void *vec, size_t vlen, size_t nbytes) {
	(void)vec;
	(void)vlen;
	(void)nbytes;
	defer_vec_purge_didfail = true;
	return true;
}

static size_t ndefer_hugify_calls = 0;
static bool
defer_test_hugify(void *ptr, size_t size, bool sync) {
	++ndefer_hugify_calls;
	return false;
}

static size_t ndefer_dehugify_calls = 0;
static void
defer_test_dehugify(void *ptr, size_t size) {
	++ndefer_dehugify_calls;
}

static nstime_t defer_curtime;
static void
defer_test_curtime(nstime_t *r_time, bool first_reading) {
	*r_time = defer_curtime;
}

static uint64_t
defer_test_ms_since(nstime_t *past_time) {
	return (nstime_ns(&defer_curtime) - nstime_ns(past_time)) / 1000 / 1000;
}

TEST_BEGIN(test_vectorized_failure_fallback) {
	test_skip_if(!hpa_supported() || (opt_process_madvise_max_batch == 0));

	hpa_hooks_t hooks;
	hooks.map = &defer_test_map;
	hooks.unmap = &defer_test_unmap;
	hooks.purge = &defer_test_purge;
	hooks.hugify = &defer_test_hugify;
	hooks.dehugify = &defer_test_dehugify;
	hooks.curtime = &defer_test_curtime;
	hooks.ms_since = &defer_test_ms_since;
	hooks.vectorized_purge = &defer_vectorized_purge_fail;
	defer_vec_purge_didfail = false;

	hpa_shard_opts_t opts = test_hpa_shard_opts_default;
	opts.deferral_allowed = true;
	opts.min_purge_interval_ms = 0;

	hpa_shard_t *shard = create_test_data(&hooks, &opts);

	bool deferred_work_generated = false;

	nstime_init(&defer_curtime, 0);
	tsdn_t *tsdn = tsd_tsdn(tsd_fetch());

	edata_t *edata = pai_alloc(tsdn, &shard->pai, PAGE, PAGE, false, false,
	    false, &deferred_work_generated);
	expect_ptr_not_null(edata, "Unexpected null edata");
	pai_dalloc(tsdn, &shard->pai, edata, &deferred_work_generated);
	hpa_shard_do_deferred_work(tsdn, shard);

	expect_true(defer_vec_purge_didfail, "Expect vec purge fail");
	expect_zu_eq(1, ndefer_purge_calls, "Expect non-vec purge");
	ndefer_purge_calls = 0;

	destroy_test_data(shard);
}
TEST_END

TEST_BEGIN(test_more_regions_purged_from_one_page) {
	test_skip_if(!hpa_supported() || (opt_process_madvise_max_batch == 0)
	    || HUGEPAGE_PAGES <= 4);

	hpa_hooks_t hooks;
	hooks.map = &defer_test_map;
	hooks.unmap = &defer_test_unmap;
	hooks.purge = &defer_test_purge;
	hooks.hugify = &defer_test_hugify;
	hooks.dehugify = &defer_test_dehugify;
	hooks.curtime = &defer_test_curtime;
	hooks.ms_since = &defer_test_ms_since;
	hooks.vectorized_purge = &defer_vectorized_purge;

	hpa_shard_opts_t opts = test_hpa_shard_opts_default;
	opts.deferral_allowed = true;
	opts.min_purge_interval_ms = 0;
	ndefer_vec_purge_calls = 0;
	ndefer_purge_calls = 0;

	hpa_shard_t *shard = create_test_data(&hooks, &opts);

	bool deferred_work_generated = false;

	nstime_init(&defer_curtime, 0);
	tsdn_t *tsdn = tsd_tsdn(tsd_fetch());

	enum { NALLOCS = 8 * HUGEPAGE_PAGES };
	edata_t *edatas[NALLOCS];
	for (int i = 0; i < NALLOCS; i++) {
		edatas[i] = pai_alloc(tsdn, &shard->pai, PAGE, PAGE, false,
		    false, false, &deferred_work_generated);
		expect_ptr_not_null(edatas[i], "Unexpected null edata");
	}
	/* Deallocate almost 3 pages out of 8, and to force batching
	 * leave the 2nd and 4th PAGE in the first 3 hugepages.
	 */
	for (int i = 0; i < 3 * (int)HUGEPAGE_PAGES; i++) {
		int j = i % HUGEPAGE_PAGES;
		if (j != 1 && j != 3) {
			pai_dalloc(tsdn, &shard->pai, edatas[i],
			    &deferred_work_generated);
		}
	}

	hpa_shard_do_deferred_work(tsdn, shard);

	/*
	 * Strict minimum purge interval is not set, we should purge as long as
	 * we have dirty pages.
	 */
	expect_zu_eq(0, ndefer_hugify_calls, "Hugified too early");
	expect_zu_eq(0, ndefer_dehugify_calls, "Dehugified too early");

	/* We purge from 2 huge pages, each one 3 dirty continous segments.
	 * For opt_process_madvise_max_batch = 2, that is
	 * 2 calls for first page, and 2 calls for second as we don't
	 * want to hold the lock on the second page while vectorized batch
	 * of size 2 is already filled with the first one.
	 */
	expect_zu_eq(4, ndefer_vec_purge_calls, "Expect purge");
	expect_zu_eq(0, ndefer_purge_calls, "Expect no non-vec purge");
	ndefer_vec_purge_calls = 0;

	destroy_test_data(shard);
}
TEST_END

int
main(void) {
	return test_no_reentrancy(test_vectorized_failure_fallback,
	    test_more_regions_purged_from_one_page);
}

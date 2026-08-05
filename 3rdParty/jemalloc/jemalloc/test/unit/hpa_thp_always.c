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

static hpa_shard_opts_t test_hpa_shard_opts_aggressive = {
    /* slab_max_alloc */
    HUGEPAGE,
    /* hugification_threshold */
    0.9 * HUGEPAGE,
    /* dirty_mult */
    FXP_INIT_PERCENT(11),
    /* deferral_allowed */
    true,
    /* hugify_delay_ms */
    0,
    /* hugify_sync */
    false,
    /* min_purge_interval_ms */
    5,
    /* experimental_max_purge_nhp */
    -1,
    /* purge_threshold */
    HUGEPAGE - 5 * PAGE,
    /* min_purge_delay_ms */
    10,
    /* hugify_style */
    hpa_hugify_style_eager};

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
static size_t npurge_size = 0;
static void
defer_test_purge(void *ptr, size_t size) {
	(void)ptr;
	npurge_size = size;
	++ndefer_purge_calls;
}

static bool defer_vectorized_purge_called = false;
static bool
defer_vectorized_purge(void *vec, size_t vlen, size_t nbytes) {
	(void)vec;
	(void)nbytes;
	++ndefer_purge_calls;
	defer_vectorized_purge_called = true;
	return false;
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

TEST_BEGIN(test_hpa_hugify_style_none_huge_no_syscall_thp_always) {
	test_skip_if(!hpa_supported() || (opt_process_madvise_max_batch != 0));

	hpa_hooks_t hooks;
	hooks.map = &defer_test_map;
	hooks.unmap = &defer_test_unmap;
	hooks.purge = &defer_test_purge;
	hooks.hugify = &defer_test_hugify;
	hooks.dehugify = &defer_test_dehugify;
	hooks.curtime = &defer_test_curtime;
	hooks.ms_since = &defer_test_ms_since;
	hooks.vectorized_purge = &defer_vectorized_purge;

	hpa_shard_opts_t opts = test_hpa_shard_opts_aggressive;
	opts.deferral_allowed = true;
	opts.purge_threshold = PAGE;
	opts.min_purge_delay_ms = 0;
	opts.hugification_threshold = HUGEPAGE * 0.25;
	opts.dirty_mult = FXP_INIT_PERCENT(10);
	opts.hugify_style = hpa_hugify_style_none;
	opts.min_purge_interval_ms = 0;
	opts.hugify_delay_ms = 0;

	hpa_shard_t *shard = create_test_data(&hooks, &opts);
	bool         deferred_work_generated = false;
	/* Current time = 10ms */
	nstime_init(&defer_curtime, 10 * 1000 * 1000);

	/* Fake that system is in thp_always mode */
	system_thp_mode_t old_mode = init_system_thp_mode;
	init_system_thp_mode = system_thp_mode_always;

	tsdn_t *tsdn = tsd_tsdn(tsd_fetch());
	enum { NALLOCS = HUGEPAGE_PAGES };
	edata_t *edatas[NALLOCS];
	ndefer_purge_calls = 0;
	for (int i = 0; i < NALLOCS / 2; i++) {
		edatas[i] = pai_alloc(tsdn, &shard->pai, PAGE, PAGE, false,
		    false, false, &deferred_work_generated);
		expect_ptr_not_null(edatas[i], "Unexpected null edata");
	}
	hpdata_t *ps = psset_pick_alloc(&shard->psset, PAGE);
	expect_true(hpdata_huge_get(ps),
	    "Page should be huge because thp=always and hugify_style is none");

	ndefer_hugify_calls = 0;
	ndefer_purge_calls = 0;
	hpa_shard_do_deferred_work(tsdn, shard);
	expect_zu_eq(ndefer_hugify_calls, 0, "style=none, no syscall");
	expect_zu_eq(ndefer_dehugify_calls, 0, "style=none, no syscall");
	expect_zu_eq(ndefer_purge_calls, 1, "purge should happen");

	destroy_test_data(shard);
	init_system_thp_mode = old_mode;
}
TEST_END

int
main(void) {
	return test_no_reentrancy(
	    test_hpa_hugify_style_none_huge_no_syscall_thp_always);
}

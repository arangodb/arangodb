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

static hpa_shard_opts_t test_hpa_shard_opts = {
    /* slab_max_alloc */
    HUGEPAGE,
    /* hugification_threshold */
    0.9 * HUGEPAGE,
    /* dirty_mult */
    FXP_INIT_PERCENT(10),
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
    PAGE,
    /* min_purge_delay_ms */
    10,
    /* hugify_style */
    hpa_hugify_style_lazy};

static hpa_shard_t *
create_test_data(const hpa_hooks_t *hooks, hpa_shard_opts_t *opts,
    const sec_opts_t *sec_opts) {
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
	tsdn_t *tsdn = tsd_tsdn(tsd_fetch());
	err = hpa_shard_init(tsdn, &test_data->shard, &test_data->central,
	    &test_data->emap, test_data->base, &test_data->shard_edata_cache,
	    SHARD_IND, opts, sec_opts);
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

// test that freed pages stay in SEC and hpa thinks they are active

TEST_BEGIN(test_hpa_sec) {
	test_skip_if(!hpa_supported());

	hpa_hooks_t hooks;
	hooks.map = &defer_test_map;
	hooks.unmap = &defer_test_unmap;
	hooks.purge = &defer_test_purge;
	hooks.hugify = &defer_test_hugify;
	hooks.dehugify = &defer_test_dehugify;
	hooks.curtime = &defer_test_curtime;
	hooks.ms_since = &defer_test_ms_since;
	hooks.vectorized_purge = &defer_vectorized_purge;

	hpa_shard_opts_t opts = test_hpa_shard_opts;

	enum { NALLOCS = 8 };
	sec_opts_t sec_opts;
	sec_opts.nshards = 1;
	sec_opts.max_alloc = 2 * PAGE;
	sec_opts.max_bytes = NALLOCS * PAGE;
	sec_opts.batch_fill_extra = 4;

	hpa_shard_t *shard = create_test_data(&hooks, &opts, &sec_opts);
	bool         deferred_work_generated = false;
	tsdn_t      *tsdn = tsd_tsdn(tsd_fetch());

	/* alloc 1 PAGE, confirm sec has fill_extra bytes. */
	edata_t *edata1 = pai_alloc(tsdn, &shard->pai, PAGE, PAGE, false, false,
	    false, &deferred_work_generated);
	expect_ptr_not_null(edata1, "Unexpected null edata");
	hpa_shard_stats_t hpa_stats;
	memset(&hpa_stats, 0, sizeof(hpa_shard_stats_t));
	hpa_shard_stats_merge(tsdn, shard, &hpa_stats);
	expect_zu_eq(hpa_stats.psset_stats.merged.nactive,
	    1 + sec_opts.batch_fill_extra, "");
	expect_zu_eq(hpa_stats.secstats.bytes, PAGE * sec_opts.batch_fill_extra,
	    "sec should have fill extra pages");

	/* Alloc/dealloc NALLOCS times and confirm extents are in sec. */
	edata_t *edatas[NALLOCS];
	for (int i = 0; i < NALLOCS; i++) {
		edatas[i] = pai_alloc(tsdn, &shard->pai, PAGE, PAGE, false,
		    false, false, &deferred_work_generated);
		expect_ptr_not_null(edatas[i], "Unexpected null edata");
	}
	memset(&hpa_stats, 0, sizeof(hpa_shard_stats_t));
	hpa_shard_stats_merge(tsdn, shard, &hpa_stats);
	expect_zu_eq(hpa_stats.psset_stats.merged.nactive, 2 + NALLOCS, "");
	expect_zu_eq(hpa_stats.secstats.bytes, PAGE, "2 refills (at 0 and 4)");

	for (int i = 0; i < NALLOCS - 1; i++) {
		pai_dalloc(
		    tsdn, &shard->pai, edatas[i], &deferred_work_generated);
	}
	memset(&hpa_stats, 0, sizeof(hpa_shard_stats_t));
	hpa_shard_stats_merge(tsdn, shard, &hpa_stats);
	expect_zu_eq(hpa_stats.psset_stats.merged.nactive, (2 + NALLOCS), "");
	expect_zu_eq(
	    hpa_stats.secstats.bytes, sec_opts.max_bytes, "sec should be full");

	/* this one should flush 1 + 0.25 * 8 = 3 extents */
	pai_dalloc(
	    tsdn, &shard->pai, edatas[NALLOCS - 1], &deferred_work_generated);
	memset(&hpa_stats, 0, sizeof(hpa_shard_stats_t));
	hpa_shard_stats_merge(tsdn, shard, &hpa_stats);
	expect_zu_eq(hpa_stats.psset_stats.merged.nactive, (NALLOCS - 1), "");
	expect_zu_eq(hpa_stats.psset_stats.merged.ndirty, 3, "");
	expect_zu_eq(hpa_stats.secstats.bytes, 0.75 * sec_opts.max_bytes,
	    "sec should be full");

	/* Next allocation should come from SEC and not increase active */
	edata_t *edata2 = pai_alloc(tsdn, &shard->pai, PAGE, PAGE, false, false,
	    false, &deferred_work_generated);
	expect_ptr_not_null(edata2, "Unexpected null edata");
	memset(&hpa_stats, 0, sizeof(hpa_shard_stats_t));
	hpa_shard_stats_merge(tsdn, shard, &hpa_stats);
	expect_zu_eq(hpa_stats.psset_stats.merged.nactive, NALLOCS - 1, "");
	expect_zu_eq(hpa_stats.secstats.bytes, 0.75 * sec_opts.max_bytes - PAGE,
	    "sec should have max_bytes minus one page that just came from it");

	/* We return this one and it stays in the cache */
	pai_dalloc(tsdn, &shard->pai, edata2, &deferred_work_generated);
	memset(&hpa_stats, 0, sizeof(hpa_shard_stats_t));
	hpa_shard_stats_merge(tsdn, shard, &hpa_stats);
	expect_zu_eq(hpa_stats.psset_stats.merged.nactive, NALLOCS - 1, "");
	expect_zu_eq(hpa_stats.psset_stats.merged.ndirty, 3, "");
	expect_zu_eq(hpa_stats.secstats.bytes, 0.75 * sec_opts.max_bytes, "");

	destroy_test_data(shard);
}
TEST_END

int
main(void) {
	return test_no_reentrancy(test_hpa_sec);
}

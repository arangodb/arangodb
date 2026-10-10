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
    /* min_purge_delay_ms */
    0,
    /* hugify_style */
    hpa_hugify_style_lazy};

static hpa_shard_opts_t test_hpa_shard_opts_purge = {
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
    5 * 1000,
    /* experimental_max_purge_nhp */
    -1,
    /* purge_threshold */
    1,
    /* min_purge_delay_ms */
    0,
    /* hugify_style */
    hpa_hugify_style_lazy};

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

TEST_BEGIN(test_alloc_max) {
	test_skip_if(!hpa_supported());

	hpa_shard_t *shard = create_test_data(
	    &hpa_hooks_default, &test_hpa_shard_opts_default);
	tsdn_t *tsdn = tsd_tsdn(tsd_fetch());

	edata_t *edata;

	/* Small max */
	bool deferred_work_generated = false;
	edata = pai_alloc(tsdn, &shard->pai, ALLOC_MAX, PAGE, false, false,
	    /* frequent_reuse */ false, &deferred_work_generated);
	expect_ptr_not_null(edata, "Allocation of small max failed");

	edata = pai_alloc(tsdn, &shard->pai, ALLOC_MAX + PAGE, PAGE, false,
	    false, /* frequent_reuse */ false, &deferred_work_generated);
	expect_ptr_null(edata, "Allocation of larger than small max succeeded");

	edata = pai_alloc(tsdn, &shard->pai, ALLOC_MAX, PAGE, false, false,
	    /* frequent_reuse */ true, &deferred_work_generated);
	expect_ptr_not_null(edata, "Allocation of frequent reused failed");

	edata = pai_alloc(tsdn, &shard->pai, HUGEPAGE, PAGE, false, false,
	    /* frequent_reuse */ true, &deferred_work_generated);
	expect_ptr_not_null(edata, "Allocation of frequent reused failed");

	edata = pai_alloc(tsdn, &shard->pai, HUGEPAGE + PAGE, PAGE, false,
	    false, /* frequent_reuse */ true, &deferred_work_generated);
	expect_ptr_null(edata, "Allocation of larger than hugepage succeeded");

	destroy_test_data(shard);
}
TEST_END

typedef struct mem_contents_s mem_contents_t;
struct mem_contents_s {
	uintptr_t my_addr;
	size_t    size;
	edata_t  *my_edata;
	rb_node(mem_contents_t) link;
};

static int
mem_contents_cmp(const mem_contents_t *a, const mem_contents_t *b) {
	return (a->my_addr > b->my_addr) - (a->my_addr < b->my_addr);
}

typedef rb_tree(mem_contents_t) mem_tree_t;
rb_gen(static, mem_tree_, mem_tree_t, mem_contents_t, link, mem_contents_cmp);

static void
node_assert_ordered(mem_contents_t *a, mem_contents_t *b) {
	assert_zu_lt(a->my_addr, a->my_addr + a->size, "Overflow");
	assert_zu_le(a->my_addr + a->size, b->my_addr, "");
}

static void
node_check(mem_tree_t *tree, mem_contents_t *contents) {
	edata_t *edata = contents->my_edata;
	assert_ptr_eq(contents, (void *)contents->my_addr, "");
	assert_ptr_eq(contents, edata_base_get(edata), "");
	assert_zu_eq(contents->size, edata_size_get(edata), "");
	assert_ptr_eq(contents->my_edata, edata, "");

	mem_contents_t *next = mem_tree_next(tree, contents);
	if (next != NULL) {
		node_assert_ordered(contents, next);
	}
	mem_contents_t *prev = mem_tree_prev(tree, contents);
	if (prev != NULL) {
		node_assert_ordered(prev, contents);
	}
}

static void
node_insert(mem_tree_t *tree, edata_t *edata, size_t npages) {
	mem_contents_t *contents = (mem_contents_t *)edata_base_get(edata);
	contents->my_addr = (uintptr_t)edata_base_get(edata);
	contents->size = edata_size_get(edata);
	contents->my_edata = edata;
	mem_tree_insert(tree, contents);
	node_check(tree, contents);
}

static void
node_remove(mem_tree_t *tree, edata_t *edata) {
	mem_contents_t *contents = (mem_contents_t *)edata_base_get(edata);
	node_check(tree, contents);
	mem_tree_remove(tree, contents);
}

TEST_BEGIN(test_stress) {
	test_skip_if(!hpa_supported());

	hpa_shard_t *shard = create_test_data(
	    &hpa_hooks_default, &test_hpa_shard_opts_default);

	tsdn_t *tsdn = tsd_tsdn(tsd_fetch());

	const size_t nlive_edatas_max = 500;
	size_t       nlive_edatas = 0;
	edata_t    **live_edatas = calloc(nlive_edatas_max, sizeof(edata_t *));
	/*
	 * Nothing special about this constant; we're only fixing it for
	 * consistency across runs.
	 */
	size_t prng_state = (size_t)0x76999ffb014df07c;

	mem_tree_t tree;
	mem_tree_new(&tree);

	bool deferred_work_generated = false;

	for (size_t i = 0; i < 100 * 1000; i++) {
		size_t operation = prng_range_zu(&prng_state, 2);
		if (operation == 0) {
			/* Alloc */
			if (nlive_edatas == nlive_edatas_max) {
				continue;
			}

			/*
			 * We make sure to get an even balance of small and
			 * large allocations.
			 */
			size_t npages_min = 1;
			size_t npages_max = ALLOC_MAX / PAGE;
			size_t npages = npages_min
			    + prng_range_zu(
			        &prng_state, npages_max - npages_min);
			edata_t *edata = pai_alloc(tsdn, &shard->pai,
			    npages * PAGE, PAGE, false, false, false,
			    &deferred_work_generated);
			assert_ptr_not_null(
			    edata, "Unexpected allocation failure");
			live_edatas[nlive_edatas] = edata;
			nlive_edatas++;
			node_insert(&tree, edata, npages);
		} else {
			/* Free. */
			if (nlive_edatas == 0) {
				continue;
			}
			size_t victim = prng_range_zu(
			    &prng_state, nlive_edatas);
			edata_t *to_free = live_edatas[victim];
			live_edatas[victim] = live_edatas[nlive_edatas - 1];
			nlive_edatas--;
			node_remove(&tree, to_free);
			pai_dalloc(tsdn, &shard->pai, to_free,
			    &deferred_work_generated);
		}
	}

	size_t ntreenodes = 0;
	for (mem_contents_t *contents = mem_tree_first(&tree); contents != NULL;
	    contents = mem_tree_next(&tree, contents)) {
		ntreenodes++;
		node_check(&tree, contents);
	}
	expect_zu_eq(ntreenodes, nlive_edatas, "");

	/*
	 * Test hpa_shard_destroy, which requires as a precondition that all its
	 * extents have been deallocated.
	 */
	for (size_t i = 0; i < nlive_edatas; i++) {
		edata_t *to_free = live_edatas[i];
		node_remove(&tree, to_free);
		pai_dalloc(
		    tsdn, &shard->pai, to_free, &deferred_work_generated);
	}
	hpa_shard_destroy(tsdn, shard);

	free(live_edatas);
	destroy_test_data(shard);
}
TEST_END

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

TEST_BEGIN(test_defer_time) {
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

	hpa_shard_opts_t opts = test_hpa_shard_opts_default;
	opts.deferral_allowed = true;

	hpa_shard_t *shard = create_test_data(&hooks, &opts);

	bool deferred_work_generated = false;

	nstime_init(&defer_curtime, 0);
	tsdn_t  *tsdn = tsd_tsdn(tsd_fetch());
	edata_t *edatas[HUGEPAGE_PAGES];
	for (int i = 0; i < (int)HUGEPAGE_PAGES; i++) {
		edatas[i] = pai_alloc(tsdn, &shard->pai, PAGE, PAGE, false,
		    false, false, &deferred_work_generated);
		expect_ptr_not_null(edatas[i], "Unexpected null edata");
	}
	hpa_shard_do_deferred_work(tsdn, shard);
	expect_zu_eq(0, ndefer_hugify_calls, "Hugified too early");

	/* Hugification delay is set to 10 seconds in options. */
	nstime_init2(&defer_curtime, 11, 0);
	hpa_shard_do_deferred_work(tsdn, shard);
	expect_zu_eq(1, ndefer_hugify_calls, "Failed to hugify");

	ndefer_hugify_calls = 0;

	/* Purge.  Recall that dirty_mult is .25. */
	for (int i = 0; i < (int)HUGEPAGE_PAGES / 2; i++) {
		pai_dalloc(
		    tsdn, &shard->pai, edatas[i], &deferred_work_generated);
	}

	hpa_shard_do_deferred_work(tsdn, shard);

	expect_zu_eq(0, ndefer_hugify_calls, "Hugified too early");
	expect_zu_eq(1, ndefer_dehugify_calls, "Should have dehugified");
	expect_zu_eq(1, ndefer_purge_calls, "Should have purged");
	ndefer_hugify_calls = 0;
	ndefer_dehugify_calls = 0;
	ndefer_purge_calls = 0;

	/*
	 * Refill the page.  We now meet the hugification threshold; we should
	 * be marked for pending hugify.
	 */
	for (int i = 0; i < (int)HUGEPAGE_PAGES / 2; i++) {
		edatas[i] = pai_alloc(tsdn, &shard->pai, PAGE, PAGE, false,
		    false, false, &deferred_work_generated);
		expect_ptr_not_null(edatas[i], "Unexpected null edata");
	}
	/*
	 * We would be ineligible for hugification, had we not already met the
	 * threshold before dipping below it.
	 */
	pai_dalloc(tsdn, &shard->pai, edatas[0], &deferred_work_generated);
	/* Wait for the threshold again. */
	nstime_init2(&defer_curtime, 22, 0);
	hpa_shard_do_deferred_work(tsdn, shard);
	expect_zu_eq(1, ndefer_hugify_calls, "Failed to hugify");
	expect_zu_eq(0, ndefer_dehugify_calls, "Unexpected dehugify");
	expect_zu_eq(0, ndefer_purge_calls, "Unexpected purge");
	ndefer_hugify_calls = 0;

	destroy_test_data(shard);
}
TEST_END

TEST_BEGIN(test_purge_no_infinite_loop) {
	test_skip_if(!hpa_supported());

	hpa_shard_t *shard = create_test_data(
	    &hpa_hooks_default, &test_hpa_shard_opts_purge);
	tsdn_t *tsdn = tsd_tsdn(tsd_fetch());

	/*
	 * This is not arbitrary value, it is chosen to met hugification
	 * criteria for huge page and at the same time do not allow hugify page
	 * without triggering a purge.
	 */
	const size_t npages = test_hpa_shard_opts_purge.hugification_threshold
	        / PAGE
	    + 1;
	const size_t size = npages * PAGE;

	bool     deferred_work_generated = false;
	edata_t *edata = pai_alloc(tsdn, &shard->pai, size, PAGE,
	    /* zero */ false, /* guarded */ false, /* frequent_reuse */ false,
	    &deferred_work_generated);
	expect_ptr_not_null(edata, "Unexpected alloc failure");

	hpa_shard_do_deferred_work(tsdn, shard);

	/* hpa_shard_do_deferred_work should not stuck in a purging loop */

	destroy_test_data(shard);
}
TEST_END

TEST_BEGIN(test_no_min_purge_interval) {
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

	/*
	 * Strict minimum purge interval is not set, we should purge as long as
	 * we have dirty pages.
	 */
	expect_zu_eq(0, ndefer_hugify_calls, "Hugified too early");
	expect_zu_eq(0, ndefer_dehugify_calls, "Dehugified too early");
	expect_zu_eq(1, ndefer_purge_calls, "Expect purge");
	ndefer_purge_calls = 0;

	destroy_test_data(shard);
}
TEST_END

TEST_BEGIN(test_min_purge_interval) {
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

	hpa_shard_opts_t opts = test_hpa_shard_opts_default;
	opts.deferral_allowed = true;

	hpa_shard_t *shard = create_test_data(&hooks, &opts);

	bool deferred_work_generated = false;

	nstime_init(&defer_curtime, 0);
	tsdn_t *tsdn = tsd_tsdn(tsd_fetch());

	edata_t *edata = pai_alloc(tsdn, &shard->pai, PAGE, PAGE, false, false,
	    false, &deferred_work_generated);
	expect_ptr_not_null(edata, "Unexpected null edata");
	pai_dalloc(tsdn, &shard->pai, edata, &deferred_work_generated);
	hpa_shard_do_deferred_work(tsdn, shard);

	/*
	 * We have a slab with dirty page and no active pages, but
	 * opt.min_purge_interval_ms didn't pass yet.
	 */
	expect_zu_eq(0, ndefer_hugify_calls, "Hugified too early");
	expect_zu_eq(0, ndefer_dehugify_calls, "Dehugified too early");
	expect_zu_eq(0, ndefer_purge_calls, "Purged too early");

	/* Minumum purge interval is set to 5 seconds in options. */
	nstime_init2(&defer_curtime, 6, 0);
	hpa_shard_do_deferred_work(tsdn, shard);

	/* Now we should purge, but nothing else. */
	expect_zu_eq(0, ndefer_hugify_calls, "Hugified too early");
	expect_zu_eq(0, ndefer_dehugify_calls, "Dehugified too early");
	expect_zu_eq(1, ndefer_purge_calls, "Expect purge");
	ndefer_purge_calls = 0;

	destroy_test_data(shard);
}
TEST_END

TEST_BEGIN(test_purge) {
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

	hpa_shard_opts_t opts = test_hpa_shard_opts_default;
	opts.deferral_allowed = true;

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
	/* Deallocate 3 hugepages out of 8. */
	for (int i = 0; i < 3 * (int)HUGEPAGE_PAGES; i++) {
		pai_dalloc(
		    tsdn, &shard->pai, edatas[i], &deferred_work_generated);
	}
	nstime_init2(&defer_curtime, 6, 0);
	hpa_shard_do_deferred_work(tsdn, shard);

	expect_zu_eq(0, ndefer_hugify_calls, "Hugified too early");
	expect_zu_eq(0, ndefer_dehugify_calls, "Dehugified too early");
	/*
	 * Expect only 2 purges, because opt.dirty_mult is set to 0.25 and we still
	 * have 5 active hugepages (1 / 5 = 0.2 < 0.25).
	 */
	expect_zu_eq(2, ndefer_purge_calls, "Expect purges");
	ndefer_purge_calls = 0;

	nstime_init2(&defer_curtime, 12, 0);
	hpa_shard_do_deferred_work(tsdn, shard);

	/*
	 * We are still having 5 active hugepages and now they are
	 * matching hugification criteria long enough to actually hugify them.
	 */
	expect_zu_eq(5, ndefer_hugify_calls, "Expect hugification");
	ndefer_hugify_calls = 0;
	expect_zu_eq(0, ndefer_dehugify_calls, "Dehugified too early");
	/*
	 * We still have completely dirty hugepage, but we are below
	 * opt.dirty_mult.
	 */
	expect_zu_eq(0, ndefer_purge_calls, "Purged too early");
	ndefer_purge_calls = 0;

	destroy_test_data(shard);
}
TEST_END

TEST_BEGIN(test_experimental_max_purge_nhp) {
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

	hpa_shard_opts_t opts = test_hpa_shard_opts_default;
	opts.deferral_allowed = true;
	opts.experimental_max_purge_nhp = 1;

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
	/* Deallocate 3 hugepages out of 8. */
	for (int i = 0; i < 3 * (int)HUGEPAGE_PAGES; i++) {
		pai_dalloc(
		    tsdn, &shard->pai, edatas[i], &deferred_work_generated);
	}
	nstime_init2(&defer_curtime, 6, 0);
	hpa_shard_do_deferred_work(tsdn, shard);

	expect_zu_eq(0, ndefer_hugify_calls, "Hugified too early");
	expect_zu_eq(0, ndefer_dehugify_calls, "Dehugified too early");
	/*
	 * Expect only one purge call, because opts.experimental_max_purge_nhp
	 * is set to 1.
	 */
	expect_zu_eq(1, ndefer_purge_calls, "Expect purges");
	ndefer_purge_calls = 0;

	nstime_init2(&defer_curtime, 12, 0);
	hpa_shard_do_deferred_work(tsdn, shard);

	expect_zu_eq(5, ndefer_hugify_calls, "Expect hugification");
	ndefer_hugify_calls = 0;
	expect_zu_eq(0, ndefer_dehugify_calls, "Dehugified too early");
	/* We still above the limit for dirty pages. */
	expect_zu_eq(1, ndefer_purge_calls, "Expect purge");
	ndefer_purge_calls = 0;

	nstime_init2(&defer_curtime, 18, 0);
	hpa_shard_do_deferred_work(tsdn, shard);

	expect_zu_eq(0, ndefer_hugify_calls, "Hugified too early");
	expect_zu_eq(0, ndefer_dehugify_calls, "Dehugified too early");
	/* Finally, we are below the limit, no purges are expected. */
	expect_zu_eq(0, ndefer_purge_calls, "Purged too early");

	destroy_test_data(shard);
}
TEST_END

TEST_BEGIN(test_vectorized_opt_eq_zero) {
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

	hpa_shard_opts_t opts = test_hpa_shard_opts_default;
	opts.deferral_allowed = true;
	opts.min_purge_interval_ms = 0;

	defer_vectorized_purge_called = false;
	ndefer_purge_calls = 0;

	hpa_shard_t *shard = create_test_data(&hooks, &opts);
	bool         deferred_work_generated = false;
	nstime_init(&defer_curtime, 0);
	tsdn_t  *tsdn = tsd_tsdn(tsd_fetch());
	edata_t *edata = pai_alloc(tsdn, &shard->pai, PAGE, PAGE, false, false,
	    false, &deferred_work_generated);
	expect_ptr_not_null(edata, "Unexpected null edata");
	pai_dalloc(tsdn, &shard->pai, edata, &deferred_work_generated);
	hpa_shard_do_deferred_work(tsdn, shard);

	expect_false(defer_vectorized_purge_called, "No vec purge");
	expect_zu_eq(1, ndefer_purge_calls, "Expect purge");

	destroy_test_data(shard);
}
TEST_END

TEST_BEGIN(test_starts_huge) {
	test_skip_if(!hpa_supported() || (opt_process_madvise_max_batch != 0)
	    || !config_stats);

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
	opts.min_purge_delay_ms = 10;
	opts.min_purge_interval_ms = 0;

	defer_vectorized_purge_called = false;
	ndefer_purge_calls = 0;

	hpa_shard_t *shard = create_test_data(&hooks, &opts);
	bool         deferred_work_generated = false;
	nstime_init2(&defer_curtime, 100, 0);

	tsdn_t *tsdn = tsd_tsdn(tsd_fetch());
	enum { NALLOCS = 2 * HUGEPAGE_PAGES };
	edata_t *edatas[NALLOCS];
	for (int i = 0; i < NALLOCS; i++) {
		edatas[i] = pai_alloc(tsdn, &shard->pai, PAGE, PAGE, false,
		    false, false, &deferred_work_generated);
		expect_ptr_not_null(edatas[i], "Unexpected null edata");
	}
	/* Deallocate 75%  */
	int pages_to_deallocate = (int)(0.75 * NALLOCS);
	for (int i = 0; i < pages_to_deallocate; i++) {
		pai_dalloc(
		    tsdn, &shard->pai, edatas[i], &deferred_work_generated);
	}

	/*
	 * While there is enough to purge as we have one empty page and that
	 * one meets the threshold,  we need to respect the delay, so no purging
	 * should happen yet.
	 */
	hpa_shard_do_deferred_work(tsdn, shard);
	expect_zu_eq(0, ndefer_purge_calls, "Purged too early, delay==10ms");

	nstime_iadd(&defer_curtime, opts.min_purge_delay_ms * 1000 * 1000);
	/* Now, enough time has passed, so we expect to purge */
	hpa_shard_do_deferred_work(tsdn, shard);
	expect_zu_eq(1, ndefer_purge_calls, "Expected purge");

	/*
	 * We purged one hugepage, so we expect to have one non-full page and it
	 * should have half of the other dirty.
	 */
	psset_stats_t *stat = &shard->psset.stats;
	expect_zu_eq(
	    stat->empty_slabs[1].npageslabs, 0, "Expected zero huge slabs");
	expect_zu_eq(stat->empty_slabs[0].npageslabs, 1, "Expected 1 nh slab");
	expect_zu_eq(stat->full_slabs[0].npageslabs, 0, "");
	expect_zu_eq(stat->full_slabs[1].npageslabs, 0, "");
	expect_zu_eq(
	    stat->merged.ndirty, HUGEPAGE_PAGES / 2, "One HP half dirty");

	/*
	 * We now allocate one more PAGE than a half the hugepage because we
	 * want to make sure that one more hugepage is needed.
	 */
	deferred_work_generated = false;
	const size_t HALF = HUGEPAGE_PAGES / 2;
	edatas[1] = pai_alloc(tsdn, &shard->pai, PAGE * (HALF + 1), PAGE, false,
	    false, false, &deferred_work_generated);
	expect_ptr_not_null(edatas[1], "Unexpected null edata");
	expect_false(deferred_work_generated, "No page is purgable");

	expect_zu_eq(stat->empty_slabs[1].npageslabs, 0, "");
	expect_zu_eq(stat->empty_slabs[0].npageslabs, 0, "");
	expect_zu_eq(stat->full_slabs[0].npageslabs, 0, "");
	expect_zu_eq(stat->full_slabs[1].npageslabs, 0, "");

	/*
	 * We expect that all inactive bytes on the second page are counted as
	 * dirty (this is because the page was huge and empty when we purged
	 * it, thus, it is assumed to come back as huge, thus all the bytes are
	 * counted as touched).
	 */
	expect_zu_eq(stat->merged.ndirty, 2 * HALF - 1,
	    "2nd page is huge because it was empty and huge when purged");
	expect_zu_eq(stat->merged.nactive, HALF + (HALF + 1), "1st + 2nd");

	nstime_iadd(&defer_curtime, opts.min_purge_delay_ms * 1000 * 1000);
	pai_dalloc(tsdn, &shard->pai, edatas[1], &deferred_work_generated);
	expect_true(deferred_work_generated, "");
	expect_zu_eq(stat->merged.ndirty, 3 * HALF, "1st + 2nd");

	/*
	 * Deallocate last allocation and confirm that page is empty again, and
	 * once new minimum delay is reached, page should be purged.
	 */
	ndefer_purge_calls = 0;
	nstime_iadd(&defer_curtime, opts.min_purge_delay_ms * 1000 * 1000);
	hpa_shard_do_deferred_work(tsdn, shard);
	expect_zu_eq(1, ndefer_purge_calls, "");
	expect_zu_eq(stat->merged.ndirty, HALF, "2nd cleared as it was empty");
	ndefer_purge_calls = 0;

	/* Deallocate all the rest, but leave only two active */
	for (int i = pages_to_deallocate; i < NALLOCS - 2; ++i) {
		pai_dalloc(
		    tsdn, &shard->pai, edatas[i], &deferred_work_generated);
	}

	/*
	 * With prior pai_dalloc our last page becomes purgable, however we
	 * still want to respect the delay.  Thus, it is not time to purge yet.
	 */
	hpa_shard_do_deferred_work(tsdn, shard);
	expect_true(deferred_work_generated, "Above limit, but not time yet");
	expect_zu_eq(0, ndefer_purge_calls, "");

	/*
	 * Finally, we move the time ahead, and we confirm that purge happens
	 * and that we have exactly two active base pages and none dirty.
	 */
	nstime_iadd(&defer_curtime, opts.min_purge_delay_ms * 1000 * 1000);
	hpa_shard_do_deferred_work(tsdn, shard);
	expect_true(deferred_work_generated, "Above limit, but not time yet");
	expect_zu_eq(1, ndefer_purge_calls, "");
	expect_zu_eq(stat->merged.ndirty, 0, "Purged all");
	expect_zu_eq(stat->merged.nactive, 2, "1st only");

	ndefer_purge_calls = 0;
	destroy_test_data(shard);
}
TEST_END

TEST_BEGIN(test_start_huge_purge_empty_only) {
	test_skip_if(!hpa_supported() || (opt_process_madvise_max_batch != 0)
	    || !config_stats);

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
	opts.purge_threshold = HUGEPAGE;
	opts.min_purge_delay_ms = 0;
	opts.hugify_style = hpa_hugify_style_eager;
	opts.min_purge_interval_ms = 0;

	ndefer_purge_calls = 0;
	npurge_size = 0;
	hpa_shard_t *shard = create_test_data(&hooks, &opts);
	bool         deferred_work_generated = false;
	nstime_init(&defer_curtime, 10 * 1000 * 1000);
	tsdn_t *tsdn = tsd_tsdn(tsd_fetch());
	enum { NALLOCS = 2 * HUGEPAGE_PAGES };
	edata_t *edatas[NALLOCS];
	for (int i = 0; i < NALLOCS; i++) {
		edatas[i] = pai_alloc(tsdn, &shard->pai, PAGE, PAGE, false,
		    false, false, &deferred_work_generated);
		expect_ptr_not_null(edatas[i], "Unexpected null edata");
	}
	/* Deallocate all from the first and one PAGE from the second HP. */
	for (int i = 0; i < NALLOCS / 2 + 1; i++) {
		pai_dalloc(
		    tsdn, &shard->pai, edatas[i], &deferred_work_generated);
	}
	hpa_shard_do_deferred_work(tsdn, shard);
	expect_true(deferred_work_generated, "");
	expect_zu_eq(1, ndefer_purge_calls, "Should purge, delay==0ms");
	expect_zu_eq(HUGEPAGE, npurge_size, "Purge whole folio");
	expect_zu_eq(shard->psset.stats.merged.ndirty, 1, "");
	expect_zu_eq(shard->psset.stats.merged.nactive, HUGEPAGE_PAGES - 1, "");

	ndefer_purge_calls = 0;
	npurge_size = 0;
	hpa_shard_do_deferred_work(tsdn, shard);
	expect_zu_eq(0, ndefer_purge_calls, "Should not purge anything");

	/* Allocate and free 2*PAGE so that it spills into second page again */
	edatas[0] = pai_alloc(tsdn, &shard->pai, 2 * PAGE, PAGE, false, false,
	    false, &deferred_work_generated);
	pai_dalloc(tsdn, &shard->pai, edatas[0], &deferred_work_generated);
	expect_true(deferred_work_generated, "");
	hpa_shard_do_deferred_work(tsdn, shard);
	expect_zu_eq(1, ndefer_purge_calls, "Should purge, delay==0ms");
	expect_zu_eq(HUGEPAGE, npurge_size, "Purge whole folio");

	ndefer_purge_calls = 0;
	destroy_test_data(shard);
}
TEST_END

TEST_BEGIN(test_assume_huge_purge_fully) {
	test_skip_if(!hpa_supported() || (opt_process_madvise_max_batch != 0)
	    || !config_stats);

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
	opts.hugification_threshold = HUGEPAGE;
	opts.min_purge_delay_ms = 0;
	opts.min_purge_interval_ms = 0;
	opts.hugify_style = hpa_hugify_style_eager;
	opts.dirty_mult = FXP_INIT_PERCENT(1);

	ndefer_purge_calls = 0;
	hpa_shard_t *shard = create_test_data(&hooks, &opts);
	bool         deferred_work_generated = false;
	nstime_init(&defer_curtime, 10 * 1000 * 1000);
	tsdn_t *tsdn = tsd_tsdn(tsd_fetch());
	enum { NALLOCS = HUGEPAGE_PAGES };
	edata_t *edatas[NALLOCS];
	for (int i = 0; i < NALLOCS; i++) {
		edatas[i] = pai_alloc(tsdn, &shard->pai, PAGE, PAGE, false,
		    false, false, &deferred_work_generated);
		expect_ptr_not_null(edatas[i], "Unexpected null edata");
	}
	/* Deallocate all */
	for (int i = 0; i < NALLOCS; i++) {
		pai_dalloc(
		    tsdn, &shard->pai, edatas[i], &deferred_work_generated);
	}
	hpa_shard_do_deferred_work(tsdn, shard);
	expect_true(deferred_work_generated, "");
	expect_zu_eq(1, ndefer_purge_calls, "Should purge, delay==0ms");

	/* Stats should say no active */
	expect_zu_eq(shard->psset.stats.merged.nactive, 0, "");
	expect_zu_eq(
	    shard->psset.stats.empty_slabs[0].npageslabs, 1, "Non huge");
	npurge_size = 0;
	edatas[0] = pai_alloc(tsdn, &shard->pai, PAGE, PAGE, false, false,
	    false, &deferred_work_generated);
	expect_ptr_not_null(edatas[0], "Unexpected null edata");
	expect_zu_eq(shard->psset.stats.merged.nactive, 1, "");
	expect_zu_eq(shard->psset.stats.slabs[1].npageslabs, 1, "Huge nonfull");
	pai_dalloc(tsdn, &shard->pai, edatas[0], &deferred_work_generated);
	expect_true(deferred_work_generated, "");
	ndefer_purge_calls = 0;
	npurge_size = 0;
	hpa_shard_do_deferred_work(tsdn, shard);
	expect_zu_eq(1, ndefer_purge_calls, "Should purge, delay==0ms");
	expect_zu_eq(HUGEPAGE, npurge_size, "Should purge full folio");

	/* Now allocate all, free 10%, alloc 5%, assert non-huge */
	for (int i = 0; i < NALLOCS; i++) {
		edatas[i] = pai_alloc(tsdn, &shard->pai, PAGE, PAGE, false,
		    false, false, &deferred_work_generated);
		expect_ptr_not_null(edatas[i], "Unexpected null edata");
	}
	int ten_pct = NALLOCS / 10;
	for (int i = 0; i < ten_pct; i++) {
		pai_dalloc(
		    tsdn, &shard->pai, edatas[i], &deferred_work_generated);
	}
	ndefer_purge_calls = 0;
	npurge_size = 0;
	hpa_shard_do_deferred_work(tsdn, shard);
	expect_zu_eq(1, ndefer_purge_calls, "Should purge, delay==0ms");
	expect_zu_eq(
	    ten_pct * PAGE, npurge_size, "Should purge 10 percent of pages");

	for (int i = 0; i < ten_pct / 2; i++) {
		edatas[i] = pai_alloc(tsdn, &shard->pai, PAGE, PAGE, false,
		    false, false, &deferred_work_generated);
		expect_ptr_not_null(edatas[i], "Unexpected null edata");
	}
	expect_zu_eq(
	    shard->psset.stats.slabs[0].npageslabs, 1, "Nonhuge nonfull");
	expect_zu_eq(shard->psset.stats.merged.ndirty, 0, "No dirty");

	npurge_size = 0;
	ndefer_purge_calls = 0;
	destroy_test_data(shard);
}
TEST_END

TEST_BEGIN(test_eager_with_purge_threshold) {
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

	const size_t     THRESHOLD = 10;
	hpa_shard_opts_t opts = test_hpa_shard_opts_aggressive;
	opts.deferral_allowed = true;
	opts.purge_threshold = THRESHOLD * PAGE;
	opts.min_purge_delay_ms = 0;
	opts.hugify_style = hpa_hugify_style_eager;
	opts.dirty_mult = FXP_INIT_PERCENT(0);

	ndefer_purge_calls = 0;
	hpa_shard_t *shard = create_test_data(&hooks, &opts);
	bool         deferred_work_generated = false;
	nstime_init(&defer_curtime, 10 * 1000 * 1000);
	tsdn_t *tsdn = tsd_tsdn(tsd_fetch());
	enum { NALLOCS = HUGEPAGE_PAGES };
	edata_t *edatas[NALLOCS];
	for (int i = 0; i < NALLOCS; i++) {
		edatas[i] = pai_alloc(tsdn, &shard->pai, PAGE, PAGE, false,
		    false, false, &deferred_work_generated);
		expect_ptr_not_null(edatas[i], "Unexpected null edata");
	}
	/* Deallocate less then threshold PAGEs. */
	for (size_t i = 0; i < THRESHOLD - 1; i++) {
		pai_dalloc(
		    tsdn, &shard->pai, edatas[i], &deferred_work_generated);
	}
	hpa_shard_do_deferred_work(tsdn, shard);
	expect_false(deferred_work_generated, "No page is purgable");
	expect_zu_eq(0, ndefer_purge_calls, "Should not purge yet");
	/* Deallocate one more page to meet the threshold */
	pai_dalloc(
	    tsdn, &shard->pai, edatas[THRESHOLD - 1], &deferred_work_generated);
	hpa_shard_do_deferred_work(tsdn, shard);
	expect_zu_eq(1, ndefer_purge_calls, "Should purge");
	expect_zu_eq(shard->psset.stats.merged.ndirty, 0, "");

	ndefer_purge_calls = 0;
	destroy_test_data(shard);
}
TEST_END

TEST_BEGIN(test_delay_when_not_allowed_deferral) {
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

	const uint64_t   DELAY_NS = 100 * 1000 * 1000;
	hpa_shard_opts_t opts = test_hpa_shard_opts_aggressive;
	opts.deferral_allowed = false;
	opts.purge_threshold = HUGEPAGE - 2 * PAGE;
	opts.min_purge_delay_ms = DELAY_NS / (1000 * 1000);
	opts.hugify_style = hpa_hugify_style_lazy;
	opts.min_purge_interval_ms = 0;

	hpa_shard_t *shard = create_test_data(&hooks, &opts);
	bool         deferred_work_generated = false;
	nstime_init2(&defer_curtime, 100, 0);
	tsdn_t *tsdn = tsd_tsdn(tsd_fetch());
	enum { NALLOCS = HUGEPAGE_PAGES };
	edata_t *edatas[NALLOCS];
	ndefer_purge_calls = 0;
	for (int i = 0; i < NALLOCS; i++) {
		edatas[i] = pai_alloc(tsdn, &shard->pai, PAGE, PAGE, false,
		    false, false, &deferred_work_generated);
		expect_ptr_not_null(edatas[i], "Unexpected null edata");
	}
	/* Deallocate all */
	for (int i = 0; i < NALLOCS; i++) {
		pai_dalloc(
		    tsdn, &shard->pai, edatas[i], &deferred_work_generated);
	}
	/* curtime = 100.0s */
	hpa_shard_do_deferred_work(tsdn, shard);
	expect_true(deferred_work_generated, "");
	expect_zu_eq(0, ndefer_purge_calls, "Too early");

	nstime_iadd(&defer_curtime, DELAY_NS - 1);
	/* This activity will take the curtime=100.1 and reset purgability */
	for (int i = 0; i < NALLOCS; i++) {
		edatas[i] = pai_alloc(tsdn, &shard->pai, PAGE, PAGE, false,
		    false, false, &deferred_work_generated);
		expect_ptr_not_null(edatas[i], "Unexpected null edata");
	}
	/* Dealloc all but 2 pages, purgable delay_ns later*/
	for (int i = 0; i < NALLOCS - 2; i++) {
		pai_dalloc(
		    tsdn, &shard->pai, edatas[i], &deferred_work_generated);
	}

	nstime_iadd(&defer_curtime, DELAY_NS);
	pai_dalloc(
	    tsdn, &shard->pai, edatas[NALLOCS - 1], &deferred_work_generated);
	expect_true(ndefer_purge_calls > 0, "Should have purged");

	ndefer_purge_calls = 0;
	destroy_test_data(shard);
}
TEST_END

TEST_BEGIN(test_deferred_until_time) {
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
	opts.min_purge_delay_ms = 1000;
	opts.hugification_threshold = HUGEPAGE / 2;
	opts.dirty_mult = FXP_INIT_PERCENT(10);
	opts.hugify_style = hpa_hugify_style_none;
	opts.min_purge_interval_ms = 500;
	opts.hugify_delay_ms = 3000;

	hpa_shard_t *shard = create_test_data(&hooks, &opts);
	bool         deferred_work_generated = false;
	/* Current time = 10ms */
	nstime_init(&defer_curtime, 10 * 1000 * 1000);

	/* Allocate one huge page */
	tsdn_t *tsdn = tsd_tsdn(tsd_fetch());
	enum { NALLOCS = HUGEPAGE_PAGES };
	edata_t *edatas[NALLOCS];
	ndefer_purge_calls = 0;
	for (int i = 0; i < NALLOCS; i++) {
		edatas[i] = pai_alloc(tsdn, &shard->pai, PAGE, PAGE, false,
		    false, false, &deferred_work_generated);
		expect_ptr_not_null(edatas[i], "Unexpected null edata");
	}
	/* Deallocate 25% */
	for (int i = 0; i < NALLOCS / 4; i++) {
		pai_dalloc(
		    tsdn, &shard->pai, edatas[i], &deferred_work_generated);
	}
	expect_true(deferred_work_generated, "We should hugify and purge");

	/* Current time = 300ms, purge_eligible at 300ms + 1000ms */
	nstime_init(&defer_curtime, 300UL * 1000 * 1000);
	for (int i = NALLOCS / 4; i < NALLOCS; i++) {
		pai_dalloc(
		    tsdn, &shard->pai, edatas[i], &deferred_work_generated);
	}
	expect_true(deferred_work_generated, "Purge work generated");
	hpa_shard_do_deferred_work(tsdn, shard);
	expect_zu_eq(0, ndefer_purge_calls, "not time for purging yet");

	/* Current time = 900ms, purge_eligible at 1300ms */
	nstime_init(&defer_curtime, 900UL * 1000 * 1000);
	uint64_t until_ns = pai_time_until_deferred_work(tsdn, &shard->pai);
	expect_u64_eq(until_ns, BACKGROUND_THREAD_DEFERRED_MIN,
	    "First pass did not happen");

	/* Fake that first pass happened more than min_purge_interval_ago */
	nstime_init(&shard->last_purge, 350UL * 1000 * 1000);
	shard->stats.npurge_passes = 1;
	until_ns = pai_time_until_deferred_work(tsdn, &shard->pai);
	expect_u64_eq(until_ns, BACKGROUND_THREAD_DEFERRED_MIN,
	    "No need to heck anything it is more than interval");

	nstime_init(&shard->last_purge, 900UL * 1000 * 1000);
	nstime_init(&defer_curtime, 1000UL * 1000 * 1000);
	/* Next purge expected at 900ms + min_purge_interval = 1400ms */
	uint64_t expected_ms = 1400 - 1000;
	until_ns = pai_time_until_deferred_work(tsdn, &shard->pai);
	expect_u64_eq(expected_ms, until_ns / (1000 * 1000), "Next in 400ms");
	destroy_test_data(shard);
}
TEST_END

TEST_BEGIN(test_eager_no_hugify_on_threshold) {
	test_skip_if(!hpa_supported() || (opt_process_madvise_max_batch != 0)
	    || !config_stats);

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
	opts.hugification_threshold = HUGEPAGE * 0.9;
	opts.dirty_mult = FXP_INIT_PERCENT(10);
	opts.hugify_style = hpa_hugify_style_eager;
	opts.min_purge_interval_ms = 0;
	opts.hugify_delay_ms = 0;

	hpa_shard_t *shard = create_test_data(&hooks, &opts);
	bool         deferred_work_generated = false;
	/* Current time = 10ms */
	nstime_init(&defer_curtime, 10 * 1000 * 1000);

	tsdn_t *tsdn = tsd_tsdn(tsd_fetch());
	/* First allocation makes the page huge */
	enum { NALLOCS = HUGEPAGE_PAGES };
	edata_t *edatas[NALLOCS];
	ndefer_purge_calls = 0;
	for (int i = 0; i < NALLOCS; i++) {
		edatas[i] = pai_alloc(tsdn, &shard->pai, PAGE, PAGE, false,
		    false, false, &deferred_work_generated);
		expect_ptr_not_null(edatas[i], "Unexpected null edata");
	}
	ndefer_hugify_calls = 0;
	hpa_shard_do_deferred_work(tsdn, shard);
	expect_zu_eq(ndefer_hugify_calls, 0, "No hugify needed - eager");
	expect_zu_eq(shard->psset.stats.full_slabs[1].npageslabs, 1,
	    "Page should be full-huge");

	/* Deallocate 25% */
	for (int i = 0; i < NALLOCS / 4; i++) {
		pai_dalloc(
		    tsdn, &shard->pai, edatas[i], &deferred_work_generated);
	}
	expect_true(deferred_work_generated, "purge is needed");
	ndefer_purge_calls = 0;
	hpa_shard_do_deferred_work(tsdn, shard);
	expect_zu_eq(ndefer_hugify_calls, 0, "No hugify needed - eager");
	expect_zu_eq(ndefer_purge_calls, 1, "Purge should have happened");

	/* Allocate 20% again, so that we are above hugification threshold */
	ndefer_purge_calls = 0;
	nstime_iadd(&defer_curtime, 800UL * 1000 * 1000);
	for (int i = 0; i < NALLOCS / 4 - 1; i++) {
		edatas[i] = pai_alloc(tsdn, &shard->pai, PAGE, PAGE, false,
		    false, false, &deferred_work_generated);
		expect_ptr_not_null(edatas[i], "Unexpected null edata");
	}
	hpa_shard_do_deferred_work(tsdn, shard);
	expect_zu_eq(0, ndefer_purge_calls, "no purging needed");
	expect_zu_eq(ndefer_hugify_calls, 0, "no hugify - eager");
	destroy_test_data(shard);
}
TEST_END

TEST_BEGIN(test_hpa_hugify_style_none_huge_no_syscall) {
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
	expect_false(
	    hpdata_huge_get(ps), "style=none, thp=madvise, should be non-huge");

	ndefer_hugify_calls = 0;
	ndefer_purge_calls = 0;
	hpa_shard_do_deferred_work(tsdn, shard);
	expect_zu_eq(ndefer_hugify_calls, 0, "Hugify none, no syscall");
	ps = psset_pick_alloc(&shard->psset, PAGE);
	expect_ptr_not_null(ps, "Unexpected null page");
	expect_false(
	    hpdata_huge_get(ps), "style=none, thp=madvise, should be non-huge");

	destroy_test_data(shard);
}
TEST_END

TEST_BEGIN(test_experimental_hpa_enforce_hugify) {
	test_skip_if(!hpa_supported() || (opt_process_madvise_max_batch != 0)
	    || !config_stats);

	bool old_opt_value = opt_experimental_hpa_enforce_hugify;
	opt_experimental_hpa_enforce_hugify = true;

	hpa_hooks_t hooks;
	hooks.map = &defer_test_map;
	hooks.unmap = &defer_test_unmap;
	hooks.purge = &defer_test_purge;
	hooks.hugify = &defer_test_hugify;
	hooks.dehugify = &defer_test_dehugify;
	hooks.curtime = &defer_test_curtime;
	hooks.ms_since = &defer_test_ms_since;
	hooks.vectorized_purge = &defer_vectorized_purge;

	/* Use eager so hugify would normally not be made on threshold */
	hpa_shard_opts_t opts = test_hpa_shard_opts_default;
	opts.hugify_style = hpa_hugify_style_eager;
	opts.deferral_allowed = true;
	opts.hugify_delay_ms = 0;
	opts.min_purge_interval_ms = 0;
	opts.hugification_threshold = 0.9 * HUGEPAGE;

	ndefer_hugify_calls = 0;
	ndefer_dehugify_calls = 0;
	ndefer_purge_calls = 0;

	hpa_shard_t *shard = create_test_data(&hooks, &opts);
	bool         deferred_work_generated = false;
	nstime_init2(&defer_curtime, 100, 0);

	tsdn_t *tsdn = tsd_tsdn(tsd_fetch());
	enum { NALLOCS = HUGEPAGE_PAGES * 95 / 100 };
	edata_t *edatas[NALLOCS];
	for (int i = 0; i < NALLOCS; i++) {
		edatas[i] = pai_alloc(tsdn, &shard->pai, PAGE, PAGE, false,
		    false, false, &deferred_work_generated);
		expect_ptr_not_null(edatas[i], "Unexpected null edata");
	}

	ndefer_hugify_calls = 0;
	hpa_shard_do_deferred_work(tsdn, shard);
	expect_zu_eq(ndefer_hugify_calls, 0, "Page was already huge");

	ndefer_hugify_calls = 0;
	ndefer_dehugify_calls = 0;
	ndefer_purge_calls = 0;

	/* Deallocate half to trigger purge */
	for (int i = 0; i < NALLOCS / 2; i++) {
		pai_dalloc(
		    tsdn, &shard->pai, edatas[i], &deferred_work_generated);
	}

	hpa_shard_do_deferred_work(tsdn, shard);
	/*
	 * Enforce hugify should have triggered dehugify syscall during purge
	 * when the page is huge and not empty.
	 */
	expect_zu_ge(ndefer_dehugify_calls, 1,
	    "Should have triggered dehugify syscall with eager style");

	for (int i = 0; i < NALLOCS / 2; i++) {
		edatas[i] = pai_alloc(tsdn, &shard->pai, PAGE, PAGE, false,
		    false, false, &deferred_work_generated);
		expect_ptr_not_null(edatas[i], "Unexpected null edata");
	}
	ndefer_hugify_calls = 0;
	hpa_shard_do_deferred_work(tsdn, shard);
	expect_zu_eq(ndefer_hugify_calls, 1, "");

	opt_experimental_hpa_enforce_hugify = old_opt_value;
	destroy_test_data(shard);
}
TEST_END

int
main(void) {
	/*
	 * These trigger unused-function warnings on CI runs, even if declared
	 * with static inline.
	 */
	(void)mem_tree_empty;
	(void)mem_tree_last;
	(void)mem_tree_search;
	(void)mem_tree_nsearch;
	(void)mem_tree_psearch;
	(void)mem_tree_iter;
	(void)mem_tree_reverse_iter;
	(void)mem_tree_destroy;
	return test_no_reentrancy(test_alloc_max, test_stress, test_defer_time,
	    test_purge_no_infinite_loop, test_no_min_purge_interval,
	    test_min_purge_interval, test_purge,
	    test_experimental_max_purge_nhp, test_vectorized_opt_eq_zero,
	    test_starts_huge, test_start_huge_purge_empty_only,
	    test_assume_huge_purge_fully, test_eager_with_purge_threshold,
	    test_delay_when_not_allowed_deferral, test_deferred_until_time,
	    test_eager_no_hugify_on_threshold,
	    test_hpa_hugify_style_none_huge_no_syscall,
	    test_experimental_hpa_enforce_hugify);
}

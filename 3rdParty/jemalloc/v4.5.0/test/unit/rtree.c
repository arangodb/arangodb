#include "test/jemalloc_test.h"

rtree_node_alloc_t *rtree_node_alloc_orig;
rtree_node_dalloc_t *rtree_node_dalloc_orig;

rtree_t *test_rtree;

static rtree_elm_t *
rtree_node_alloc_intercept(tsdn_t *tsdn, rtree_t *rtree, size_t nelms) {
	rtree_elm_t *node;

	if (rtree != test_rtree) {
		return rtree_node_alloc_orig(tsdn, rtree, nelms);
	}

	malloc_mutex_unlock(tsdn, &rtree->init_lock);
	node = (rtree_elm_t *)calloc(nelms, sizeof(rtree_elm_t));
	assert_ptr_not_null(node, "Unexpected calloc() failure");
	malloc_mutex_lock(tsdn, &rtree->init_lock);

	return node;
}

static void
rtree_node_dalloc_intercept(tsdn_t *tsdn, rtree_t *rtree, rtree_elm_t *node) {
	if (rtree != test_rtree) {
		rtree_node_dalloc_orig(tsdn, rtree, node);
		return;
	}

	free(node);
}

TEST_BEGIN(test_rtree_read_empty) {
	tsdn_t *tsdn;

	tsdn = tsdn_fetch();

	rtree_t rtree;
	rtree_ctx_t rtree_ctx = RTREE_CTX_INITIALIZER;
	test_rtree = &rtree;
	assert_false(rtree_new(&rtree), "Unexpected rtree_new() failure");
	assert_ptr_null(rtree_read(tsdn, &rtree, &rtree_ctx, PAGE, false),
	    "rtree_read() should return NULL for empty tree");
	rtree_delete(tsdn, &rtree);
	test_rtree = NULL;
}
TEST_END

#define NTHREADS	8
#define MAX_NBITS	30
#define NITERS		1000
#define SEED		42

typedef struct {
	rtree_t		rtree;
	uint32_t	seed;
} thd_start_arg_t;

static void *
thd_start(void *varg) {
	thd_start_arg_t *arg = (thd_start_arg_t *)varg;
	rtree_ctx_t rtree_ctx = RTREE_CTX_INITIALIZER;
	sfmt_t *sfmt;
	extent_t *extent;
	tsdn_t *tsdn;
	unsigned i;

	sfmt = init_gen_rand(arg->seed);
	extent = (extent_t *)malloc(sizeof(extent));
	assert_ptr_not_null(extent, "Unexpected malloc() failure");
	tsdn = tsdn_fetch();

	for (i = 0; i < NITERS; i++) {
		uintptr_t key = (uintptr_t)(gen_rand64(sfmt) & ((ZU(1) <<
		    MAX_NBITS) - ZU(1)));
		if (i % 2 == 0) {
			rtree_elm_t *elm;

			elm = rtree_elm_acquire(tsdn, &arg->rtree, &rtree_ctx,
			    key, false, true);
			assert_ptr_not_null(elm,
			    "Unexpected rtree_elm_acquire() failure");
			rtree_elm_write_acquired(tsdn, &arg->rtree, elm,
			    extent);
			rtree_elm_release(tsdn, &arg->rtree, elm);

			elm = rtree_elm_acquire(tsdn, &arg->rtree, &rtree_ctx,
			    key, true, false);
			assert_ptr_not_null(elm,
			    "Unexpected rtree_elm_acquire() failure");
			rtree_elm_read_acquired(tsdn, &arg->rtree, elm);
			rtree_elm_release(tsdn, &arg->rtree, elm);
		} else {
			rtree_read(tsdn, &arg->rtree, &rtree_ctx, key, false);
		}
	}

	free(extent);
	fini_gen_rand(sfmt);
	return NULL;
}

TEST_BEGIN(test_rtree_concurrent) {
	thd_start_arg_t arg;
	thd_t thds[NTHREADS];
	sfmt_t *sfmt;
	tsdn_t *tsdn;

	sfmt = init_gen_rand(SEED);
	tsdn = tsdn_fetch();
	test_rtree = &arg.rtree;
	assert_false(rtree_new(&arg.rtree), "Unexpected rtree_new() failure");
	arg.seed = gen_rand32(sfmt);
	for (unsigned i = 0; i < NTHREADS; i++) {
		thd_create(&thds[i], thd_start, (void *)&arg);
	}
	for (unsigned i = 0; i < NTHREADS; i++) {
		thd_join(thds[i], NULL);
	}
	rtree_delete(tsdn, &arg.rtree);
	test_rtree = NULL;
	fini_gen_rand(sfmt);
}
TEST_END

#undef NTHREADS
#undef NITERS
#undef SEED

TEST_BEGIN(test_rtree_extrema) {
	extent_t extent_a, extent_b;
	tsdn_t *tsdn;

	tsdn = tsdn_fetch();

	rtree_t rtree;
	rtree_ctx_t rtree_ctx = RTREE_CTX_INITIALIZER;
	test_rtree = &rtree;
	assert_false(rtree_new(&rtree), "Unexpected rtree_new() failure");

	assert_false(rtree_write(tsdn, &rtree, &rtree_ctx, PAGE, &extent_a),
	    "Unexpected rtree_write() failure");
	assert_ptr_eq(rtree_read(tsdn, &rtree, &rtree_ctx, PAGE, true),
	    &extent_a,
	    "rtree_read() should return previously set value");

	assert_false(rtree_write(tsdn, &rtree, &rtree_ctx, ~((uintptr_t)0),
	    &extent_b), "Unexpected rtree_write() failure");
	assert_ptr_eq(rtree_read(tsdn, &rtree, &rtree_ctx, ~((uintptr_t)0),
	    true), &extent_b,
	    "rtree_read() should return previously set value");

	rtree_delete(tsdn, &rtree);
	test_rtree = NULL;
}
TEST_END

TEST_BEGIN(test_rtree_bits) {
	tsdn_t *tsdn = tsdn_fetch();

	uintptr_t keys[] = {PAGE, PAGE + 1,
	    PAGE + (((uintptr_t)1) << LG_PAGE) - 1};

	extent_t extent;
	rtree_t rtree;
	rtree_ctx_t rtree_ctx = RTREE_CTX_INITIALIZER;

	test_rtree = &rtree;
	assert_false(rtree_new(&rtree),
	    "Unexpected rtree_new() failure");

	for (unsigned i = 0; i < sizeof(keys)/sizeof(uintptr_t); i++) {
		assert_false(rtree_write(tsdn, &rtree, &rtree_ctx, keys[i],
		    &extent), "Unexpected rtree_write() failure");
		for (unsigned j = 0; j < sizeof(keys)/sizeof(uintptr_t); j++) {
			assert_ptr_eq(rtree_read(tsdn, &rtree, &rtree_ctx,
			    keys[j], true), &extent,
			    "rtree_read() should return previously set "
			    "value and ignore insignificant key bits; "
			    "i=%u, j=%u, set key=%#"FMTxPTR", get "
			    "key=%#"FMTxPTR, i, j, keys[i], keys[j]);
		}
		assert_ptr_null(rtree_read(tsdn, &rtree, &rtree_ctx,
		    (((uintptr_t)2) << LG_PAGE), false),
		    "Only leftmost rtree leaf should be set; i=%u", i);
		rtree_clear(tsdn, &rtree, &rtree_ctx, keys[i]);
	}

	rtree_delete(tsdn, &rtree);
	test_rtree = NULL;
}
TEST_END

TEST_BEGIN(test_rtree_random) {
#define NSET 16
#define SEED 42
	sfmt_t *sfmt = init_gen_rand(SEED);
	tsdn_t *tsdn = tsdn_fetch();
	uintptr_t keys[NSET];
	extent_t extent;
	rtree_t rtree;
	rtree_ctx_t rtree_ctx = RTREE_CTX_INITIALIZER;
	rtree_elm_t *elm;

	test_rtree = &rtree;
	assert_false(rtree_new(&rtree), "Unexpected rtree_new() failure");

	for (unsigned i = 0; i < NSET; i++) {
		keys[i] = (uintptr_t)gen_rand64(sfmt);
		elm = rtree_elm_acquire(tsdn, &rtree, &rtree_ctx, keys[i],
		    false, true);
		assert_ptr_not_null(elm,
		    "Unexpected rtree_elm_acquire() failure");
		rtree_elm_write_acquired(tsdn, &rtree, elm, &extent);
		rtree_elm_release(tsdn, &rtree, elm);
		assert_ptr_eq(rtree_read(tsdn, &rtree, &rtree_ctx, keys[i],
		    true), &extent,
		    "rtree_read() should return previously set value");
	}
	for (unsigned i = 0; i < NSET; i++) {
		assert_ptr_eq(rtree_read(tsdn, &rtree, &rtree_ctx, keys[i],
		    true), &extent,
		    "rtree_read() should return previously set value, i=%u", i);
	}

	for (unsigned i = 0; i < NSET; i++) {
		rtree_clear(tsdn, &rtree, &rtree_ctx, keys[i]);
		assert_ptr_null(rtree_read(tsdn, &rtree, &rtree_ctx, keys[i],
		    true), "rtree_read() should return previously set value");
	}
	for (unsigned i = 0; i < NSET; i++) {
		assert_ptr_null(rtree_read(tsdn, &rtree, &rtree_ctx, keys[i],
		    true), "rtree_read() should return previously set value");
	}

	rtree_delete(tsdn, &rtree);
	test_rtree = NULL;
	fini_gen_rand(sfmt);
#undef NSET
#undef SEED
}
TEST_END

int
main(void) {
	rtree_node_alloc_orig = rtree_node_alloc;
	rtree_node_alloc = rtree_node_alloc_intercept;
	rtree_node_dalloc_orig = rtree_node_dalloc;
	rtree_node_dalloc = rtree_node_dalloc_intercept;
	test_rtree = NULL;

	return test(
	    test_rtree_read_empty,
	    test_rtree_concurrent,
	    test_rtree_extrema,
	    test_rtree_bits,
	    test_rtree_random);
}

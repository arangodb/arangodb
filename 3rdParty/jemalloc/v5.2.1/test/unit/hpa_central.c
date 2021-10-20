#include "test/jemalloc_test.h"

#include "jemalloc/internal/hpa_central.h"

typedef struct test_data_s test_data_t;
struct test_data_s {
	/*
	 * Must be the first member -- we convert back and forth between the
	 * test_data_t and the hpa_central_t;
	 */
	hpa_central_t central;
	base_t *base;
	edata_cache_t edata_cache;
	emap_t emap;
};

void
create_test_data(hpa_central_t **r_central, base_t **r_base) {
	bool err;
	base_t *base = base_new(TSDN_NULL, /* ind */ 111,
	    &ehooks_default_extent_hooks);
	assert_ptr_not_null(base, "");

	test_data_t *test_data = malloc(sizeof(test_data_t));
	assert_ptr_not_null(test_data, "");

	test_data->base = base;

	err = edata_cache_init(&test_data->edata_cache, base);
	assert_false(err, "");

	err = emap_init(&test_data->emap, test_data->base,
	    /* zeroed */ false);
	assert_false(err, "");

	hpa_central_init(&test_data->central, &test_data->edata_cache,
	    &test_data->emap);

	*r_central = (hpa_central_t *)test_data;
	*r_base = base;
}

static void
destroy_test_data(hpa_central_t *central) {
	test_data_t *test_data = (test_data_t *)central;
	base_delete(TSDN_NULL, test_data->base);
	free(test_data);
}

static edata_t *
test_edata(base_t *base, uintptr_t addr, size_t size) {
	edata_t *edata = base_alloc_edata(TSDN_NULL, base);
	assert_ptr_not_null(edata, "");
	edata_init(edata, base_ind_get(base), (void *)addr,
	    size, /* slab */ false, /* szind_t */ SC_NSIZES, /* sn */ 0,
	    extent_state_active, /* zeroed */ true, /* comitted */ true,
	    EXTENT_PAI_HPA, /* is_head */ true);
	return edata;
}

static void
edata_expect_alloc(base_t *base, edata_t *edata, uintptr_t addr, size_t size) {
	expect_ptr_not_null(edata, "Alloc should have succeeded");
	expect_u_eq(base_ind_get(base), edata_arena_ind_get(edata), "");
	expect_u_eq(SC_NSIZES, edata_szind_get_maybe_invalid(edata), "");
	expect_d_eq(extent_state_active, edata_state_get(edata), "");
	assert_ptr_eq((void *)addr, edata_base_get(edata), "");
	assert_zu_eq(size, edata_size_get(edata), "");
}


TEST_BEGIN(test_empty) {
	hpa_central_t *central;
	base_t *base;
	create_test_data(&central, &base);

	edata_t *edata;

	edata = hpa_central_alloc_reuse(TSDN_NULL, central, PAGE, PAGE);
	expect_ptr_null(edata, "Empty allocator succeed in its allocation");

	edata = hpa_central_alloc_reuse(TSDN_NULL, central, PAGE, 2 * PAGE);
	expect_ptr_null(edata, "Empty allocator succeed in its allocation");

	edata = hpa_central_alloc_reuse(TSDN_NULL, central, PAGE, 8 * PAGE);
	expect_ptr_null(edata, "Empty allocator succeed in its allocation");

	edata = hpa_central_alloc_reuse(TSDN_NULL, central, 4 * PAGE, 8 * PAGE);
	expect_ptr_null(edata, "Empty allocator succeed in its allocation");

	destroy_test_data(central);
}
TEST_END

TEST_BEGIN(test_first_fit_simple) {
	hpa_central_t *central;
	base_t *base;
	create_test_data(&central, &base);

	edata_t *edata1 = test_edata(base, 10 * PAGE, 10 * PAGE);
	bool err = hpa_central_alloc_grow(TSDN_NULL, central, PAGE, edata1);
	expect_false(err, "Unexpected grow failure");
	edata_expect_alloc(base, edata1, 10 * PAGE, PAGE);

	edata_t *edata2 = test_edata(base, 4 * PAGE, 1 * PAGE);
	err = hpa_central_alloc_grow(TSDN_NULL, central, PAGE, edata2);
	expect_false(err, "Unexpected grow failure");
	edata_expect_alloc(base, edata2, 4 * PAGE, PAGE);

	hpa_central_dalloc(TSDN_NULL, central, edata2);

	/*
	 * Even though there's a lower-addressed extent that a by-size search
	 * will find earlier, we should still pick the earlier one.
	 */
	edata_t *edata3 = hpa_central_alloc_reuse(TSDN_NULL, central, PAGE, PAGE);
	/*
	 * Recall there's still an active page at the beginning of the extent
	 * added at 10 * PAGE; the next allocation from it should be at 11 *
	 * PAGE.
	 */
	edata_expect_alloc(base, edata3, 11 * PAGE, PAGE);

	destroy_test_data(central);
}
TEST_END

TEST_BEGIN(test_first_fit_large_goal) {
	/*
	 * See the comment in hpa_central_alloc_reuse; we should prefer an
	 * earlier allocation over a later one, even if it means we fall short
	 * of the goal size.
	 */
	hpa_central_t *central;
	base_t *base;
	create_test_data(&central, &base);

	edata_t *edata1 = test_edata(base, 10 * PAGE, 10 * PAGE);
	bool err = hpa_central_alloc_grow(TSDN_NULL, central, 2 * PAGE, edata1);
	expect_false(err, "Unexpected grow failure");
	edata_expect_alloc(base, edata1, 10 * PAGE, 2 * PAGE);

	/* We need a page, but would like 2. */
	edata_t *edata2 = hpa_central_alloc_reuse(TSDN_NULL, central, PAGE,
	    2 * PAGE);
	edata_expect_alloc(base, edata2, 12 * PAGE, 2 * PAGE);

	hpa_central_dalloc(TSDN_NULL, central, edata1);

	/*
	 * Now, we have a 2-page inactive extent, then a 2-page active extent,
	 * then a 6-page inactive extent.  If our minimum size is 2 but the goal
	 * size is 4, we should still pick the first hole rather than the
	 * second.
	 */
	edata1 = hpa_central_alloc_reuse(TSDN_NULL, central, 2 * PAGE, 4 * PAGE);
	edata_expect_alloc(base, edata1, 10 * PAGE, 2 * PAGE);

	/*
	 * Make sure we didn't succeed only by forgetting about that last range
	 * or something.
	 */
	edata_t *edata3 = hpa_central_alloc_reuse(TSDN_NULL, central, 4 * PAGE,
	    4 * PAGE);
	edata_expect_alloc(base, edata3, 14 * PAGE, 4 * PAGE);

	destroy_test_data(central);
}
TEST_END

TEST_BEGIN(test_merging) {
	hpa_central_t *central;
	base_t *base;
	create_test_data(&central, &base);

	/* Test an exact match */
	bool err;
	edata_t *edata1 = test_edata(base, 10 * PAGE, PAGE);
	err = hpa_central_alloc_grow(TSDN_NULL, central, PAGE, edata1);
	expect_false(err, "Alloc should have succeeded");
	edata_expect_alloc(base, edata1, 10 * PAGE, PAGE);

	edata_t *edata2 = hpa_central_alloc_reuse(TSDN_NULL, central, PAGE,
	    PAGE);
	expect_ptr_null(edata2, "Allocation should have failed");

	/*
	 * Create two more regions; one immediately before the first and one
	 * immediately after.  The extents shouldn't get merged.
	 */
	edata2 = test_edata(base, 11 * PAGE, PAGE);
	err = hpa_central_alloc_grow(TSDN_NULL, central, PAGE, edata2);
	edata_expect_alloc(base, edata2, 11 * PAGE, PAGE);

	edata_t *edata3 = test_edata(base, 12 * PAGE, 20 * PAGE);
	err = hpa_central_alloc_grow(TSDN_NULL, central, PAGE, edata3);
	edata_expect_alloc(base, edata3, 12 * PAGE, PAGE);

	/*
	 * OK, we've got 3 contiguous ranges; [10, 11), [11, 12), and [12, 22).
	 * They shouldn't get merged though, even once freed.  We free the
	 * middle range last to test merging (or rather, the lack thereof) in
	 * both directions.
	 */
	hpa_central_dalloc(TSDN_NULL, central, edata1);
	hpa_central_dalloc(TSDN_NULL, central, edata3);
	hpa_central_dalloc(TSDN_NULL, central, edata2);

	/*
	 * A two-page range should only be satisfied by the third added region.
	 */
	edata_t *edata = hpa_central_alloc_reuse(TSDN_NULL, central, 2 * PAGE,
	    2 * PAGE);
	edata_expect_alloc(base, edata, 12 * PAGE, 2 * PAGE);
	hpa_central_dalloc(TSDN_NULL, central, edata);

	/* Same with a three-page range. */
	edata = hpa_central_alloc_reuse(TSDN_NULL, central, 3 * PAGE, 3 * PAGE);
	edata_expect_alloc(base, edata, 12 * PAGE, 3 * PAGE);
	hpa_central_dalloc(TSDN_NULL, central, edata);

	/* Let's try some cases that *should* get merged. */
	edata1 = hpa_central_alloc_reuse(TSDN_NULL, central, 2 * PAGE, 2 * PAGE);
	edata_expect_alloc(base, edata1, 12 * PAGE, 2 * PAGE);
	edata2 = hpa_central_alloc_reuse(TSDN_NULL, central, 2 * PAGE, 2 * PAGE);
	edata_expect_alloc(base, edata2, 14 * PAGE, 2 * PAGE);
	edata3 = hpa_central_alloc_reuse(TSDN_NULL, central, 2 * PAGE, 2 * PAGE);
	edata_expect_alloc(base, edata3, 16 * PAGE, 2 * PAGE);

	/* Merge with predecessor. */
	hpa_central_dalloc(TSDN_NULL, central, edata1);
	hpa_central_dalloc(TSDN_NULL, central, edata2);
	edata1 = hpa_central_alloc_reuse(TSDN_NULL, central, 4 * PAGE,
	    4 * PAGE);
	edata_expect_alloc(base, edata1, 12 * PAGE, 4 * PAGE);

	/* Merge with successor */
	hpa_central_dalloc(TSDN_NULL, central, edata3);
	hpa_central_dalloc(TSDN_NULL, central, edata1);
	edata1 = hpa_central_alloc_reuse(TSDN_NULL, central, 6 * PAGE,
	    6 * PAGE);
	edata_expect_alloc(base, edata1, 12 * PAGE, 6 * PAGE);
	hpa_central_dalloc(TSDN_NULL, central, edata1);

	/*
	 * Let's try merging with both.  We need to get three adjacent
	 * allocations again; do it the same way as before.
	 */
	edata1 = hpa_central_alloc_reuse(TSDN_NULL, central, 2 * PAGE, 2 * PAGE);
	edata_expect_alloc(base, edata1, 12 * PAGE, 2 * PAGE);
	edata2 = hpa_central_alloc_reuse(TSDN_NULL, central, 2 * PAGE, 2 * PAGE);
	edata_expect_alloc(base, edata2, 14 * PAGE, 2 * PAGE);
	edata3 = hpa_central_alloc_reuse(TSDN_NULL, central, 2 * PAGE, 2 * PAGE);
	edata_expect_alloc(base, edata3, 16 * PAGE, 2 * PAGE);

	hpa_central_dalloc(TSDN_NULL, central, edata1);
	hpa_central_dalloc(TSDN_NULL, central, edata3);
	hpa_central_dalloc(TSDN_NULL, central, edata2);

	edata1 = hpa_central_alloc_reuse(TSDN_NULL, central, 6 * PAGE,
	    6 * PAGE);
	edata_expect_alloc(base, edata1, 12 * PAGE, 6 * PAGE);

	destroy_test_data(central);
}
TEST_END

TEST_BEGIN(test_stress_simple) {
	hpa_central_t *central;
	base_t *base;
	create_test_data(&central, &base);

	enum {
		range_base = 1024 * PAGE,
		range_pages = 256,
		range_size = range_pages * PAGE
	};

	edata_t *edatas[range_pages];

	bool err;
	edata_t *range = test_edata(base, range_base, range_size);
	err = hpa_central_alloc_grow(TSDN_NULL, central, PAGE, range);
	expect_false(err, "Unexpected grow failure");
	hpa_central_dalloc(TSDN_NULL, central, range);

	for (size_t i = 0; i < range_pages; i++) {
		edatas[i] = hpa_central_alloc_reuse(TSDN_NULL, central, PAGE,
		    PAGE);
		edata_expect_alloc(base, edatas[i], range_base + i * PAGE,
		    PAGE);
	}
	/* Free up the odd indices. */
	for (size_t i = 0; i < range_pages; i++) {
		if (i % 2 == 0) {
			continue;
		}
		hpa_central_dalloc(TSDN_NULL, central, edatas[i]);
	}
	/*
	 * Reallocate them again.  Try it with a goal size that can't be
	 * satisfied.
	 */
	for (size_t i = 0; i < range_pages; i++) {
		if (i % 2 == 0) {
			continue;
		}
		edatas[i] = hpa_central_alloc_reuse(TSDN_NULL, central, PAGE,
		    PAGE);
		edata_expect_alloc(base, edatas[i], range_base + i * PAGE,
		    PAGE);
	}
	/*
	 * In each batch of 8, create a free range of 4 pages and a free range
	 * of 2 pages.
	 */
	for (size_t i = 0; i < range_pages; i += 8) {
		hpa_central_dalloc(TSDN_NULL, central, edatas[i + 1]);
		hpa_central_dalloc(TSDN_NULL, central, edatas[i + 2]);
		hpa_central_dalloc(TSDN_NULL, central, edatas[i + 3]);
		hpa_central_dalloc(TSDN_NULL, central, edatas[i + 4]);

		hpa_central_dalloc(TSDN_NULL, central, edatas[i + 6]);
		hpa_central_dalloc(TSDN_NULL, central, edatas[i + 7]);
	}

	/*
	 * And allocate 3 pages into the first, and 2 pages into the second.  To
	 * mix things up a little, lets get those amounts via goal sizes
	 * instead.
	 */
	for (size_t i = 0; i < range_pages; i += 8) {
		edatas[i + 1] = hpa_central_alloc_reuse(TSDN_NULL, central,
		    2 * PAGE, 3 * PAGE);
		edata_expect_alloc(base, edatas[i + 1],
		    range_base + (i + 1) * PAGE, 3 * PAGE);

		edatas[i + 6] = hpa_central_alloc_reuse(TSDN_NULL, central,
		    2 * PAGE, 4 * PAGE);
		edata_expect_alloc(base, edatas[i + 6],
		    range_base + (i + 6) * PAGE, 2 * PAGE);
	}

	edata_t *edata = hpa_central_alloc_reuse(TSDN_NULL, central, 2 * PAGE,
	    2 * PAGE);
	expect_ptr_null(edata, "Should be no free ranges of 2 pages");

	destroy_test_data(central);
}
TEST_END

TEST_BEGIN(test_stress_random) {
	const size_t range_length = 32 * PAGE;
	const size_t range_base = 100 * PAGE;
	const size_t size_max_pages = 16;

	hpa_central_t *central;
	base_t *base;
	create_test_data(&central, &base);

	/*
	 * We loop through this once per some operations, so we don't want it to
	 * get too big.
	 */
	const size_t nlive_edatas_max = 100;
	size_t nlive_edatas = 0;
	edata_t **live_edatas = calloc(nlive_edatas_max, sizeof(edata_t *));
	size_t nranges = 0;

	/*
	 * Nothing special about this constant; we're only fixing it for
	 * consistency across runs.
	 */
	size_t prng_state = (size_t)0x76999ffb014df07c;
	for (size_t i = 0; i < 100 * 1000; i++) {
		size_t operation = prng_range_zu(&prng_state, 2);
		if (operation == 0) {
			/* Do an alloc. */
			if (nlive_edatas == nlive_edatas_max) {
				continue;
			}
			size_t min_pages = 1 + prng_range_zu(
			    &prng_state, size_max_pages);
			size_t goal_pages = min_pages + prng_range_zu(
			    &prng_state, size_max_pages - min_pages + 1);
			edata_t *edata = hpa_central_alloc_reuse(TSDN_NULL,
			    central, min_pages * PAGE, goal_pages * PAGE);
			if (edata == NULL) {
				edata = test_edata(base,
				    range_base + range_length * nranges,
				    range_length);
				bool err = hpa_central_alloc_grow(TSDN_NULL,
				    central, goal_pages * PAGE, edata);
				assert_false(err, "Unexpected grow failure");
				nranges++;
			}
			uintptr_t begin = (uintptr_t)edata_base_get(edata);
			uintptr_t end = (uintptr_t)edata_last_get(edata);
			size_t range_begin = (begin - range_base) / range_length;
			size_t range_end = (end - range_base) / range_length;
			expect_zu_eq(range_begin, range_end,
			    "Should not have allocations spanning "
			    "multiple ranges");
			expect_zu_ge(begin, range_base,
			    "Gave back a pointer outside of the reserved "
			    "range");
			expect_zu_lt(end, range_base + range_length * nranges,
			    "Gave back a pointer outside of the reserved "
			    "range");
			for (size_t j = 0; j < nlive_edatas; j++) {
				edata_t *other = live_edatas[j];
				uintptr_t other_begin =
				    (uintptr_t)edata_base_get(other);
				uintptr_t other_end =
				    (uintptr_t)edata_last_get(other);
				expect_true(
				    (begin < other_begin && end < other_begin)
				    || (begin > other_end),
				    "Gave back two extents that overlap");
			}
			live_edatas[nlive_edatas] = edata;
			nlive_edatas++;
		} else {
			/* Do a free. */
			if (nlive_edatas == 0) {
				continue;
			}
			size_t victim = prng_range_zu(&prng_state,
			    nlive_edatas);
			edata_t *to_free = live_edatas[victim];
			live_edatas[victim] = live_edatas[nlive_edatas - 1];
			nlive_edatas--;
			hpa_central_dalloc(TSDN_NULL, central, to_free);
		}
	}

	free(live_edatas);
	destroy_test_data(central);
}
TEST_END

int main(void) {
	return test_no_reentrancy(
	    test_empty,
	    test_first_fit_simple,
	    test_first_fit_large_goal,
	    test_merging,
	    test_stress_simple,
	    test_stress_random);
}

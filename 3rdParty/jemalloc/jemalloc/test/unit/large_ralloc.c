#include "test/jemalloc_test.h"

/*
 * Test that large_ralloc_no_move causes a failure (returns true) when
 * in-place extent expansion cannot succeed for either usize_max or
 * usize_min.
 *
 * A previous bug omitted the ! negation on the second extent expansion
 * attempt (usize_min fallback), causing false success (return false) when
 * the expansion actually failed.
 */
TEST_BEGIN(test_large_ralloc_no_move_expand_fail) {
	/*
	 * Allocate two adjacent large objects in the same arena to block
	 * in-place expansion of the first one.
	 */
	unsigned arena_ind;
	size_t   sz = sizeof(arena_ind);
	expect_d_eq(mallctl("arenas.create", (void *)&arena_ind, &sz, NULL, 0),
	    0, "Unexpected mallctl() failure");

	int flags = MALLOCX_ARENA(arena_ind) | MALLOCX_TCACHE_NONE;

	size_t large_sz = SC_LARGE_MINCLASS;
	/* Allocate several blocks to prevent expansion of the first. */
	void *blocks[8];
	for (size_t i = 0; i < ARRAY_SIZE(blocks); i++) {
		blocks[i] = mallocx(large_sz, flags);
		expect_ptr_not_null(blocks[i], "Unexpected mallocx() failure");
	}

	/*
	 * Try to expand blocks[0] in place. Use usize_min < usize_max to
	 * exercise the fallback path.
	 */
	tsd_t   *tsd = tsd_fetch();
	edata_t *edata = emap_edata_lookup(
	    tsd_tsdn(tsd), &arena_emap_global, blocks[0]);
	expect_ptr_not_null(edata, "Unexpected edata lookup failure");

	size_t oldusize = edata_usize_get(edata);
	size_t usize_min = sz_s2u(oldusize + 1);
	size_t usize_max = sz_s2u(oldusize * 2);

	/* Ensure min and max are in different size classes. */
	if (usize_min == usize_max) {
		usize_max = sz_s2u(usize_min + 1);
	}

	bool ret = large_ralloc_no_move(
	    tsd_tsdn(tsd), edata, usize_min, usize_max, false);

	/*
	 * With adjacent allocations blocking expansion, this should fail.
	 * The bug caused ret == false (success) even when expansion failed.
	 */
	if (!ret) {
		/*
		 * Expansion might actually succeed if adjacent memory
		 * is free.  Verify the size actually changed.
		 */
		size_t newusize = edata_usize_get(edata);
		expect_zu_ge(newusize, usize_min,
		    "Expansion reported success but size didn't change");
	}

	for (size_t i = 0; i < ARRAY_SIZE(blocks); i++) {
		dallocx(blocks[i], flags);
	}
}
TEST_END

int
main(void) {
	return test_no_reentrancy(test_large_ralloc_no_move_expand_fail);
}

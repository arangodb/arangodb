#include "test/jemalloc_test.h"

static void assert_small_allocation_sampled(void *ptr, size_t size) {
	assert_ptr_not_null(ptr, "Unexpected malloc failure");
	assert_zu_le(size, SC_SMALL_MAXCLASS, "Unexpected large size class");
	edata_t *edata = emap_edata_lookup(TSDN_NULL, &arena_emap_global, ptr);
	assert_ptr_not_null(edata, "Unable to find edata for allocation");
	expect_false(edata_slab_get(edata),
	    "Sampled small allocations should not be placed on slabs");
	expect_ptr_eq(edata_base_get(edata), ptr,
	    "Sampled allocations should be page-aligned");
	expect_zu_eq(edata_usize_get(edata), size,
	    "Edata usize did not match requested size");
	expect_zu_eq(edata_size_get(edata), PAGE_CEILING(size) + sz_large_pad,
	    "Edata actual size was not a multiple of PAGE");
	prof_tctx_t *prof_tctx = edata_prof_tctx_get(edata);
	expect_ptr_not_null(prof_tctx, "Edata had null prof_tctx");
	expect_ptr_not_null(prof_tctx->tdata,
	    "Edata had null prof_tdata despite being sampled");
}

TEST_BEGIN(test_profile_small_allocations) {
	test_skip_if(!config_prof);

	for (szind_t index = 0; index < SC_NBINS; index++) {
		size_t size = sz_index2size(index);
		void *ptr = malloc(size);
		assert_small_allocation_sampled(ptr, size);
		free(ptr);
	}
}
TEST_END

TEST_BEGIN(test_profile_small_reallocations_growing) {
	test_skip_if(!config_prof);

	void *ptr = NULL;
	for (szind_t index = 0; index < SC_NBINS; index++) {
		size_t size = sz_index2size(index);
		ptr = realloc(ptr, size);
		assert_small_allocation_sampled(ptr, size);
	}
}
TEST_END

TEST_BEGIN(test_profile_small_reallocations_shrinking) {
	test_skip_if(!config_prof);

	void *ptr = NULL;
	for (szind_t index = SC_NBINS; index-- > 0;) {
		size_t size = sz_index2size(index);
		ptr = realloc(ptr, size);
		assert_small_allocation_sampled(ptr, size);
	}
}
TEST_END

TEST_BEGIN(test_profile_small_reallocations_same_size_class) {
	test_skip_if(!config_prof);

	for (szind_t index = 0; index < SC_NBINS; index++) {
		size_t size = sz_index2size(index);
		void *ptr = malloc(size);
		assert_small_allocation_sampled(ptr, size);
		ptr = realloc(ptr, size - 1);
		assert_small_allocation_sampled(ptr, size);
		free(ptr);
	}
}
TEST_END

int
main(void) {
	return test(test_profile_small_allocations,
	    test_profile_small_reallocations_growing,
	    test_profile_small_reallocations_shrinking,
	    test_profile_small_reallocations_same_size_class);
}

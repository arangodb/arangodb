#include "test/jemalloc_test.h"

#define INVALID_ARENA_IND ((1U << MALLOCX_ARENA_BITS) - 1)

/* Create a page-aligned mock slab with all regions free. */
static void
create_mock_slab(edata_t *slab, szind_t binind, uint64_t sn) {
	const bin_info_t *bin_info = &bin_infos[binind];
	void *addr;
	slab_data_t *slab_data;

	addr = mallocx(bin_info->slab_size, MALLOCX_LG_ALIGN(LG_PAGE));
	assert_ptr_not_null(addr, "Unexpected mallocx failure");

	memset(slab, 0, sizeof(edata_t));
	edata_init(slab, INVALID_ARENA_IND, addr, bin_info->slab_size,
	    true, binind, sn, extent_state_active, false, true,
	    EXTENT_PAI_PAC, EXTENT_NOT_HEAD);
	edata_nfree_set(slab, bin_info->nregs);

	/* Initialize bitmap to all regions free. */
	slab_data = edata_slab_data_get(slab);
	bitmap_init(slab_data->bitmap, &bin_info->bitmap_info, false);
}

/*
 * Test that bin_init produces a valid empty bin.
 */
TEST_BEGIN(test_bin_init) {
	bin_t bin;
	bool err;

	err = bin_init(&bin);
	expect_false(err, "bin_init should succeed");
	expect_ptr_null(bin.slabcur, "New bin should have NULL slabcur");
	expect_ptr_null(edata_heap_first(&bin.slabs_nonfull),
	    "New bin should have empty nonfull heap");
	expect_true(edata_list_active_empty(&bin.slabs_full),
	    "New bin should have empty full list");
	if (config_stats) {
		expect_u64_eq(bin.stats.nmalloc, 0,
		    "New bin should have zero nmalloc");
		expect_u64_eq(bin.stats.ndalloc, 0,
		    "New bin should have zero ndalloc");
		expect_zu_eq(bin.stats.curregs, 0,
		    "New bin should have zero curregs");
		expect_zu_eq(bin.stats.curslabs, 0,
		    "New bin should have zero curslabs");
	}
}
TEST_END

/*
 * Test single-region allocation from a slab.
 */
TEST_BEGIN(test_bin_slab_reg_alloc) {
	szind_t binind = 0;
	const bin_info_t *bin_info = &bin_infos[binind];
	edata_t slab;
	unsigned nregs;
	unsigned i;

	create_mock_slab(&slab, binind, 0);
	nregs = bin_info->nregs;

	for (i = 0; i < nregs; i++) {
		void *reg;

		expect_u_gt(edata_nfree_get(&slab), 0,
		    "Slab should have free regions");
		reg = bin_slab_reg_alloc(&slab, bin_info);
		expect_ptr_not_null(reg,
		    "bin_slab_reg_alloc should return non-NULL");
		/* Verify the pointer is within the slab. */
		expect_true(
		    (uintptr_t)reg >= (uintptr_t)edata_addr_get(&slab) &&
		    (uintptr_t)reg < (uintptr_t)edata_addr_get(&slab)
		    + bin_info->slab_size,
		    "Allocated region should be within slab bounds");
	}
	expect_u_eq(edata_nfree_get(&slab), 0,
	    "Slab should be full after allocating all regions");
	free(edata_addr_get(&slab));
}
TEST_END

/*
 * Test batch allocation from a slab.
 */
TEST_BEGIN(test_bin_slab_reg_alloc_batch) {
	szind_t binind = 0;
	const bin_info_t *bin_info = &bin_infos[binind];
	edata_t slab;
	unsigned nregs;
	void **ptrs;
	unsigned i;

	create_mock_slab(&slab, binind, 0);
	nregs = bin_info->nregs;
	ptrs = mallocx(nregs * sizeof(void *), 0);
	assert_ptr_not_null(ptrs, "Unexpected mallocx failure");

	bin_slab_reg_alloc_batch(&slab, bin_info, nregs, ptrs);
	expect_u_eq(edata_nfree_get(&slab), 0,
	    "Slab should be full after batch alloc of all regions");

	/* Verify all pointers are within the slab and distinct. */
	for (i = 0; i < nregs; i++) {
		unsigned j;

		expect_ptr_not_null(ptrs[i], "Batch pointer should be non-NULL");
		expect_true(
		    (uintptr_t)ptrs[i] >= (uintptr_t)edata_addr_get(&slab) &&
		    (uintptr_t)ptrs[i] < (uintptr_t)edata_addr_get(&slab)
		    + bin_info->slab_size,
		    "Batch pointer should be within slab bounds");
		for (j = 0; j < i; j++) {
			expect_ptr_ne(ptrs[i], ptrs[j],
			    "Batch pointers should be distinct");
		}
	}
	free(ptrs);
	free(edata_addr_get(&slab));
}
TEST_END

/*
 * Test partial batch allocation from a slab.
 */
TEST_BEGIN(test_bin_slab_reg_alloc_batch_partial) {
	szind_t binind = 0;
	const bin_info_t *bin_info = &bin_infos[binind];
	edata_t slab;
	unsigned nregs;
	unsigned half;
	void **ptrs;

	create_mock_slab(&slab, binind, 0);
	nregs = bin_info->nregs;

	/* Only allocate half. */
	half = nregs / 2;
	if (half == 0) {
		half = 1;
	}
	ptrs = mallocx(half * sizeof(void *), 0);
	assert_ptr_not_null(ptrs, "Unexpected mallocx failure");

	bin_slab_reg_alloc_batch(&slab, bin_info, half, ptrs);
	expect_u_eq(edata_nfree_get(&slab), nregs - half,
	    "Slab nfree should reflect partial batch alloc");

	free(ptrs);
	free(edata_addr_get(&slab));
}
TEST_END

/*
 * Test nonfull slab list insert, remove, and tryget.
 */
TEST_BEGIN(test_bin_slabs_nonfull) {
	bin_t bin;
	szind_t binind = 0;
	edata_t slab1, slab2;
	edata_t *got;
	edata_t *remaining;

	bin_init(&bin);

	/* Create two non-full slabs with different serial numbers. */
	create_mock_slab(&slab1, binind, 1);
	create_mock_slab(&slab2, binind, 2);

	/* Insert both into the nonfull heap. */
	bin_slabs_nonfull_insert(&bin, &slab1);
	expect_ptr_not_null(edata_heap_first(&bin.slabs_nonfull),
	    "Nonfull heap should be non-empty after insert");

	bin_slabs_nonfull_insert(&bin, &slab2);

	/* tryget should return a slab. */
	got = bin_slabs_nonfull_tryget(&bin);
	expect_ptr_not_null(got, "tryget should return a slab");

	/* Remove the remaining one explicitly. */
	remaining = edata_heap_first(&bin.slabs_nonfull);
	expect_ptr_not_null(remaining, "One slab should still remain");
	bin_slabs_nonfull_remove(&bin, remaining);
	expect_ptr_null(edata_heap_first(&bin.slabs_nonfull),
	    "Nonfull heap should be empty after removing both slabs");

	free(edata_addr_get(&slab1));
	free(edata_addr_get(&slab2));
}
TEST_END

/*
 * Test full slab list insert and remove (non-auto arena case).
 */
TEST_BEGIN(test_bin_slabs_full) {
	bin_t bin;
	szind_t binind = 0;
	const bin_info_t *bin_info = &bin_infos[binind];
	edata_t slab;
	unsigned i;

	bin_init(&bin);
	create_mock_slab(&slab, binind, 0);

	/* Consume all regions so the slab appears full. */
	for (i = 0; i < bin_info->nregs; i++) {
		bin_slab_reg_alloc(&slab, bin_info);
	}
	expect_u_eq(edata_nfree_get(&slab), 0, "Slab should be full");

	/* Insert into full list (is_auto=false to actually track). */
	bin_slabs_full_insert(false, &bin, &slab);
	expect_false(edata_list_active_empty(&bin.slabs_full),
	    "Full list should be non-empty after insert");

	/* Remove from full list. */
	bin_slabs_full_remove(false, &bin, &slab);
	expect_true(edata_list_active_empty(&bin.slabs_full),
	    "Full list should be empty after remove");

	free(edata_addr_get(&slab));
}
TEST_END

/*
 * Test that full slab insert/remove is a no-op for auto arenas.
 */
TEST_BEGIN(test_bin_slabs_full_auto) {
	bin_t bin;
	szind_t binind = 0;
	const bin_info_t *bin_info = &bin_infos[binind];
	edata_t slab;
	unsigned i;

	bin_init(&bin);
	create_mock_slab(&slab, binind, 0);
	for (i = 0; i < bin_info->nregs; i++) {
		bin_slab_reg_alloc(&slab, bin_info);
	}

	/* is_auto=true: insert should be a no-op. */
	bin_slabs_full_insert(true, &bin, &slab);
	expect_true(edata_list_active_empty(&bin.slabs_full),
	    "Full list should remain empty for auto arenas");

	/* Remove should also be a no-op without crashing. */
	bin_slabs_full_remove(true, &bin, &slab);

	free(edata_addr_get(&slab));
}
TEST_END

/*
 * Test dissociate_slab when the slab is slabcur.
 */
TEST_BEGIN(test_bin_dissociate_slabcur) {
	bin_t bin;
	szind_t binind = 0;
	edata_t slab;

	bin_init(&bin);
	create_mock_slab(&slab, binind, 0);

	bin.slabcur = &slab;
	bin_dissociate_slab(true, &slab, &bin);
	expect_ptr_null(bin.slabcur,
	    "Dissociating slabcur should NULL it out");

	free(edata_addr_get(&slab));
}
TEST_END

/*
 * Test dissociate_slab when the slab is in the nonfull heap.
 */
TEST_BEGIN(test_bin_dissociate_nonfull) {
	bin_t bin;
	szind_t binind = 0;
	const bin_info_t *bin_info = &bin_infos[binind];
	edata_t slab;

	bin_init(&bin);
	create_mock_slab(&slab, binind, 0);

	/*
	 * Only dissociate from nonfull when nregs > 1.  For nregs == 1,
	 * the slab goes directly to the full list, never nonfull.
	 */
	test_skip_if(bin_info->nregs == 1);

	bin_slabs_nonfull_insert(&bin, &slab);
	bin_dissociate_slab(true, &slab, &bin);
	expect_ptr_null(edata_heap_first(&bin.slabs_nonfull),
	    "Nonfull heap should be empty after dissociating the slab");

	free(edata_addr_get(&slab));
}
TEST_END

/*
 * Test refill slabcur with a fresh slab.
 */
TEST_BEGIN(test_bin_refill_slabcur_with_fresh_slab) {
	tsdn_t *tsdn = tsdn_fetch();
	bin_t bin;
	szind_t binind = 0;
	const bin_info_t *bin_info = &bin_infos[binind];
	edata_t fresh;

	bin_init(&bin);
	create_mock_slab(&fresh, binind, 0);

	malloc_mutex_lock(tsdn, &bin.lock);
	bin_refill_slabcur_with_fresh_slab(tsdn, &bin, binind, &fresh);
	expect_ptr_eq(bin.slabcur, &fresh,
	    "Fresh slab should become slabcur");
	if (config_stats) {
		expect_u64_eq(bin.stats.nslabs, 1,
		    "nslabs should be 1 after installing fresh slab");
		expect_zu_eq(bin.stats.curslabs, 1,
		    "curslabs should be 1 after installing fresh slab");
	}
	expect_u_eq(edata_nfree_get(bin.slabcur), bin_info->nregs,
	    "Fresh slab should have all regions free");
	malloc_mutex_unlock(tsdn, &bin.lock);

	free(edata_addr_get(&fresh));
}
TEST_END

/*
 * Test refill slabcur without a fresh slab (from the nonfull heap).
 */
TEST_BEGIN(test_bin_refill_slabcur_no_fresh_slab) {
	tsdn_t *tsdn = tsdn_fetch();
	bin_t bin;
	szind_t binind = 0;
	edata_t slab;
	bool empty;

	bin_init(&bin);
	create_mock_slab(&slab, binind, 0);

	malloc_mutex_lock(tsdn, &bin.lock);

	/* With no slabcur and empty nonfull heap, refill should fail. */
	empty = bin_refill_slabcur_no_fresh_slab(tsdn, true, &bin);
	expect_true(empty,
	    "Refill should fail when nonfull heap is empty");
	expect_ptr_null(bin.slabcur, "slabcur should remain NULL");

	/* Insert a slab into nonfull, then refill should succeed. */
	bin_slabs_nonfull_insert(&bin, &slab);
	empty = bin_refill_slabcur_no_fresh_slab(tsdn, true, &bin);
	expect_false(empty,
	    "Refill should succeed when nonfull heap has a slab");
	expect_ptr_eq(bin.slabcur, &slab,
	    "slabcur should be the slab from nonfull heap");

	malloc_mutex_unlock(tsdn, &bin.lock);
	free(edata_addr_get(&slab));
}
TEST_END

/*
 * Test that refill moves a full slabcur into the full list.
 */
TEST_BEGIN(test_bin_refill_slabcur_full_to_list) {
	tsdn_t *tsdn = tsdn_fetch();
	bin_t bin;
	szind_t binind = 0;
	const bin_info_t *bin_info = &bin_infos[binind];
	edata_t full_slab, nonfull_slab;
	unsigned i;
	bool empty;

	bin_init(&bin);
	create_mock_slab(&full_slab, binind, 0);
	create_mock_slab(&nonfull_slab, binind, 1);

	/* Make full_slab actually full. */
	for (i = 0; i < bin_info->nregs; i++) {
		bin_slab_reg_alloc(&full_slab, bin_info);
	}

	malloc_mutex_lock(tsdn, &bin.lock);
	bin.slabcur = &full_slab;
	bin_slabs_nonfull_insert(&bin, &nonfull_slab);

	/* Refill should move the full slabcur to full list and pick nonfull. */
	empty = bin_refill_slabcur_no_fresh_slab(tsdn, false, &bin);
	expect_false(empty, "Refill should succeed");
	expect_ptr_eq(bin.slabcur, &nonfull_slab,
	    "slabcur should now be the nonfull slab");
	expect_false(edata_list_active_empty(&bin.slabs_full),
	    "Old full slabcur should be in the full list");
	malloc_mutex_unlock(tsdn, &bin.lock);

	free(edata_addr_get(&full_slab));
	free(edata_addr_get(&nonfull_slab));
}
TEST_END

/*
 * Test malloc with a fresh slab.
 */
TEST_BEGIN(test_bin_malloc_with_fresh_slab) {
	tsdn_t *tsdn = tsdn_fetch();
	bin_t bin;
	szind_t binind = 0;
	const bin_info_t *bin_info = &bin_infos[binind];
	edata_t fresh;
	void *ptr;

	bin_init(&bin);
	create_mock_slab(&fresh, binind, 0);

	malloc_mutex_lock(tsdn, &bin.lock);
	ptr = bin_malloc_with_fresh_slab(tsdn, &bin, binind, &fresh);
	expect_ptr_not_null(ptr, "Should allocate from fresh slab");
	expect_ptr_eq(bin.slabcur, &fresh,
	    "Fresh slab should be installed as slabcur");
	expect_u_eq(edata_nfree_get(&fresh), bin_info->nregs - 1,
	    "One region should be consumed from fresh slab");
	if (config_stats) {
		expect_u64_eq(bin.stats.nslabs, 1, "nslabs should be 1");
		expect_zu_eq(bin.stats.curslabs, 1, "curslabs should be 1");
	}
	malloc_mutex_unlock(tsdn, &bin.lock);

	free(edata_addr_get(&fresh));
}
TEST_END

/*
 * Test malloc without a fresh slab (from existing slabcur).
 */
TEST_BEGIN(test_bin_malloc_no_fresh_slab) {
	tsdn_t *tsdn = tsdn_fetch();
	bin_t bin;
	szind_t binind = 0;
	const bin_info_t *bin_info = &bin_infos[binind];
	edata_t slab;
	void *ptr;

	bin_init(&bin);
	create_mock_slab(&slab, binind, 0);

	malloc_mutex_lock(tsdn, &bin.lock);

	/* With no slabcur and empty nonfull, should return NULL. */
	ptr = bin_malloc_no_fresh_slab(tsdn, true, &bin, binind);
	expect_ptr_null(ptr,
	    "Should return NULL when no slabs available");

	/* Set up a slabcur; malloc should succeed. */
	bin.slabcur = &slab;
	ptr = bin_malloc_no_fresh_slab(tsdn, true, &bin, binind);
	expect_ptr_not_null(ptr,
	    "Should allocate from slabcur");
	expect_u_eq(edata_nfree_get(&slab), bin_info->nregs - 1,
	    "One region should be consumed");
	malloc_mutex_unlock(tsdn, &bin.lock);

	free(edata_addr_get(&slab));
}
TEST_END

/*
 * Test the bin_dalloc_locked begin/step/finish sequence.
 */
TEST_BEGIN(test_bin_dalloc_locked) {
	tsdn_t *tsdn = tsdn_fetch();
	bin_t bin;
	szind_t binind = 0;
	const bin_info_t *bin_info = &bin_infos[binind];
	edata_t slab;
	unsigned nregs;
	void **ptrs;
	unsigned i;
	bin_dalloc_locked_info_t info;
	bool slab_empty;
	bool found_empty;

	bin_init(&bin);
	create_mock_slab(&slab, binind, 0);

	/* Allocate all regions from the slab. */
	nregs = bin_info->nregs;
	ptrs = mallocx(nregs * sizeof(void *), 0);
	assert_ptr_not_null(ptrs, "Unexpected mallocx failure");
	for (i = 0; i < nregs; i++) {
		ptrs[i] = bin_slab_reg_alloc(&slab, bin_info);
		assert_ptr_not_null(ptrs[i], "Alloc should succeed");
	}
	expect_u_eq(edata_nfree_get(&slab), 0, "Slab should be full");

	/* Set this slab as slabcur so dalloc steps work correctly. */
	bin.slabcur = &slab;
	if (config_stats) {
		bin.stats.nmalloc = nregs;
		bin.stats.curregs = nregs;
		bin.stats.nslabs = 1;
		bin.stats.curslabs = 1;
	}

	malloc_mutex_lock(tsdn, &bin.lock);

	/* Free one region and verify step returns false (not yet empty). */
	bin_dalloc_locked_begin(&info, binind);
	slab_empty = bin_dalloc_locked_step(
	    tsdn, true, &bin, &info, binind, &slab, ptrs[0]);
	if (nregs > 1) {
		expect_false(slab_empty,
		    "Slab should not be empty after freeing one region");
	}
	bin_dalloc_locked_finish(tsdn, &bin, &info);
	if (config_stats) {
		expect_zu_eq(bin.stats.curregs, nregs - 1,
		    "curregs should decrement by 1");
	}

	/* Free all remaining regions; the last one should empty the slab. */
	bin_dalloc_locked_begin(&info, binind);
	found_empty = false;
	for (i = 1; i < nregs; i++) {
		slab_empty = bin_dalloc_locked_step(
		    tsdn, true, &bin, &info, binind, &slab, ptrs[i]);
		if (slab_empty) {
			found_empty = true;
		}
	}
	bin_dalloc_locked_finish(tsdn, &bin, &info);
	expect_true(found_empty,
	    "Freeing all regions should produce an empty slab");
	expect_u_eq(edata_nfree_get(&slab), nregs,
	    "All regions should be free");
	if (config_stats) {
		expect_zu_eq(bin.stats.curregs, 0,
		    "curregs should be 0 after freeing all");
	}

	malloc_mutex_unlock(tsdn, &bin.lock);
	free(ptrs);
	free(edata_addr_get(&slab));
}
TEST_END

/*
 * Test that bin_lower_slab replaces slabcur when the new slab is older.
 */
TEST_BEGIN(test_bin_lower_slab_replaces_slabcur) {
	tsdn_t *tsdn = tsdn_fetch();
	bin_t bin;
	szind_t binind = 0;
	edata_t slab_old, slab_new;

	bin_init(&bin);

	/* slab_old has sn=0 (older), slab_new has sn=1 (newer). */
	create_mock_slab(&slab_old, binind, 0);
	create_mock_slab(&slab_new, binind, 1);

	/* Make slab_new the slabcur. */
	bin.slabcur = &slab_new;

	/*
	 * bin_lower_slab with the older slab should replace slabcur and move
	 * slab_new into either nonfull or full.
	 */
	malloc_mutex_lock(tsdn, &bin.lock);
	bin_lower_slab(tsdn, true, &slab_old, &bin);
	expect_ptr_eq(bin.slabcur, &slab_old,
	    "Older slab should replace slabcur");
	malloc_mutex_unlock(tsdn, &bin.lock);

	free(edata_addr_get(&slab_old));
	free(edata_addr_get(&slab_new));
}
TEST_END

/*
 * Test that bin_lower_slab inserts into the nonfull heap when the new slab
 * is newer than slabcur.
 */
TEST_BEGIN(test_bin_lower_slab_inserts_nonfull) {
	tsdn_t *tsdn = tsdn_fetch();
	bin_t bin;
	szind_t binind = 0;
	edata_t slab_old, slab_new;

	bin_init(&bin);
	create_mock_slab(&slab_old, binind, 0);
	create_mock_slab(&slab_new, binind, 1);

	/* Make slab_old the slabcur (older). */
	bin.slabcur = &slab_old;

	/* bin_lower_slab with the newer slab should insert into nonfull. */
	malloc_mutex_lock(tsdn, &bin.lock);
	bin_lower_slab(tsdn, true, &slab_new, &bin);
	expect_ptr_eq(bin.slabcur, &slab_old,
	    "Older slabcur should remain");
	expect_ptr_not_null(edata_heap_first(&bin.slabs_nonfull),
	    "Newer slab should be inserted into nonfull heap");
	malloc_mutex_unlock(tsdn, &bin.lock);

	free(edata_addr_get(&slab_old));
	free(edata_addr_get(&slab_new));
}
TEST_END

/*
 * Test bin_dalloc_slab_prepare updates stats.
 */
TEST_BEGIN(test_bin_dalloc_slab_prepare) {
	tsdn_t *tsdn = tsdn_fetch();
	bin_t bin;
	szind_t binind = 0;
	edata_t slab;

	bin_init(&bin);
	create_mock_slab(&slab, binind, 0);

	if (config_stats) {
		bin.stats.curslabs = 2;
	}

	/*
	 * bin_dalloc_slab_prepare requires the slab is not slabcur,
	 * so leave slabcur NULL.
	 */
	malloc_mutex_lock(tsdn, &bin.lock);
	bin_dalloc_slab_prepare(tsdn, &slab, &bin);
	if (config_stats) {
		expect_zu_eq(bin.stats.curslabs, 1,
		    "curslabs should decrement");
	}
	malloc_mutex_unlock(tsdn, &bin.lock);

	free(edata_addr_get(&slab));
}
TEST_END

/*
 * Test bin_shard_sizes_boot and bin_update_shard_size.
 */
TEST_BEGIN(test_bin_shard_sizes) {
	unsigned shard_sizes[SC_NBINS];
	unsigned i;
	bool err;
	szind_t ind1, ind2;

	/* Boot should set all to the default. */
	bin_shard_sizes_boot(shard_sizes);
	for (i = 0; i < SC_NBINS; i++) {
		expect_u_eq(shard_sizes[i], N_BIN_SHARDS_DEFAULT,
		    "Shard sizes should be default after boot");
	}

	/* Update with nshards=0 should fail (returns true). */
	err = bin_update_shard_size(shard_sizes, 1, 1, 0);
	expect_true(err, "nshards=0 should be an error");

	/* Update with nshards > BIN_SHARDS_MAX should fail. */
	err = bin_update_shard_size(shard_sizes, 1, 1, BIN_SHARDS_MAX + 1);
	expect_true(err, "nshards > BIN_SHARDS_MAX should be an error");

	/* Valid update: set a range to 4 shards. */
	err = bin_update_shard_size(shard_sizes, 1, 128, 4);
	expect_false(err, "Valid update should succeed");
	/* Verify the range was updated. */
	ind1 = sz_size2index_compute(1);
	ind2 = sz_size2index_compute(128);
	for (i = ind1; i <= ind2; i++) {
		expect_u_eq(shard_sizes[i], 4,
		    "Updated range should have nshards=4");
	}

	/* Update beyond SC_SMALL_MAXCLASS should be clamped, not fail. */
	err = bin_update_shard_size(shard_sizes,
	    SC_SMALL_MAXCLASS, SC_SMALL_MAXCLASS * 2, 2);
	expect_false(err,
	    "Update with end beyond SMALL_MAXCLASS should succeed");
}
TEST_END

/*
 * Test a full alloc-then-free cycle by allocating all regions from a bin
 * via bin_malloc_with_fresh_slab, then freeing them all via the
 * bin_dalloc_locked sequence.
 */
TEST_BEGIN(test_bin_alloc_free_cycle) {
	tsdn_t *tsdn = tsdn_fetch();
	bin_t bin;
	szind_t binind = 0;
	const bin_info_t *bin_info = &bin_infos[binind];
	unsigned nregs = bin_info->nregs;
	edata_t slab;
	void **ptrs;
	unsigned i;
	bin_dalloc_locked_info_t info;

	bin_init(&bin);
	create_mock_slab(&slab, binind, 0);

	ptrs = mallocx(nregs * sizeof(void *), 0);
	assert_ptr_not_null(ptrs, "Unexpected mallocx failure");

	malloc_mutex_lock(tsdn, &bin.lock);

	/* Allocate the first pointer via fresh slab path. */
	ptrs[0] = bin_malloc_with_fresh_slab(tsdn, &bin, binind, &slab);
	expect_ptr_not_null(ptrs[0], "First alloc should succeed");

	/* Allocate the rest from slabcur. */
	for (i = 1; i < nregs; i++) {
		ptrs[i] = bin_malloc_no_fresh_slab(tsdn, true, &bin, binind);
		expect_ptr_not_null(ptrs[i], "Alloc should succeed");
	}
	if (config_stats) {
		bin.stats.nmalloc += nregs;
		bin.stats.curregs += nregs;
	}

	expect_u_eq(edata_nfree_get(&slab), 0, "Slab should be full");

	/* Free all regions. */
	bin_dalloc_locked_begin(&info, binind);
	for (i = 0; i < nregs; i++) {
		bin_dalloc_locked_step(
		    tsdn, true, &bin, &info, binind, &slab, ptrs[i]);
	}
	bin_dalloc_locked_finish(tsdn, &bin, &info);

	expect_u_eq(edata_nfree_get(&slab), nregs,
	    "All regions should be free after full cycle");
	if (config_stats) {
		expect_zu_eq(bin.stats.curregs, 0,
		    "curregs should be 0 after full cycle");
	}

	malloc_mutex_unlock(tsdn, &bin.lock);
	free(ptrs);
	free(edata_addr_get(&slab));
}
TEST_END

/*
 * Test alloc/free cycle across multiple bin size classes.
 */
TEST_BEGIN(test_bin_multi_size_class) {
	tsdn_t *tsdn = tsdn_fetch();
	szind_t test_indices[] = {0, SC_NBINS / 2, SC_NBINS - 1};
	unsigned nindices = sizeof(test_indices) / sizeof(test_indices[0]);
	unsigned t;

	for (t = 0; t < nindices; t++) {
		szind_t binind = test_indices[t];
		const bin_info_t *bin_info = &bin_infos[binind];
		bin_t bin;
		edata_t slab;
		void *ptr;
		bin_dalloc_locked_info_t info;

		bin_init(&bin);
		create_mock_slab(&slab, binind, 0);

		malloc_mutex_lock(tsdn, &bin.lock);
		ptr = bin_malloc_with_fresh_slab(
		    tsdn, &bin, binind, &slab);
		expect_ptr_not_null(ptr,
		    "Alloc should succeed for binind %u", binind);
		expect_u_eq(edata_nfree_get(&slab), bin_info->nregs - 1,
		    "nfree should be nregs-1 for binind %u", binind);

		/* Free the allocated region. */
		if (config_stats) {
			bin.stats.nmalloc = 1;
			bin.stats.curregs = 1;
		}
		bin_dalloc_locked_begin(&info, binind);
		bin_dalloc_locked_step(
		    tsdn, true, &bin, &info, binind, &slab, ptr);
		bin_dalloc_locked_finish(tsdn, &bin, &info);

		expect_u_eq(edata_nfree_get(&slab), bin_info->nregs,
		    "All regions should be free for binind %u", binind);
		malloc_mutex_unlock(tsdn, &bin.lock);

		free(edata_addr_get(&slab));
	}
}
TEST_END

int
main(void) {
	return test(
	    test_bin_init,
	    test_bin_slab_reg_alloc,
	    test_bin_slab_reg_alloc_batch,
	    test_bin_slab_reg_alloc_batch_partial,
	    test_bin_slabs_nonfull,
	    test_bin_slabs_full,
	    test_bin_slabs_full_auto,
	    test_bin_dissociate_slabcur,
	    test_bin_dissociate_nonfull,
	    test_bin_refill_slabcur_with_fresh_slab,
	    test_bin_refill_slabcur_no_fresh_slab,
	    test_bin_refill_slabcur_full_to_list,
	    test_bin_malloc_with_fresh_slab,
	    test_bin_malloc_no_fresh_slab,
	    test_bin_dalloc_locked,
	    test_bin_lower_slab_replaces_slabcur,
	    test_bin_lower_slab_inserts_nonfull,
	    test_bin_dalloc_slab_prepare,
	    test_bin_shard_sizes,
	    test_bin_alloc_free_cycle,
	    test_bin_multi_size_class);
}

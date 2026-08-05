#include "test/jemalloc_test.h"

#include "jemalloc/internal/arena_structs.h"

#define STRINGIFY_HELPER(x) #x
#define STRINGIFY(x) STRINGIFY_HELPER(x)

TEST_BEGIN(test_stats_summary) {
	size_t sz, allocated, active, resident, mapped, metadata,
	    metadata_edata, metadata_rtree;
	int expected = config_stats ? 0 : ENOENT;

	sz = sizeof(size_t);
	expect_d_eq(
	    mallctl("stats.allocated", (void *)&allocated, &sz, NULL, 0),
	    expected, "Unexpected mallctl() result");
	expect_d_eq(mallctl("stats.active", (void *)&active, &sz, NULL, 0),
	    expected, "Unexpected mallctl() result");
	expect_d_eq(mallctl("stats.resident", (void *)&resident, &sz, NULL, 0),
	    expected, "Unexpected mallctl() result");
	expect_d_eq(mallctl("stats.mapped", (void *)&mapped, &sz, NULL, 0),
	    expected, "Unexpected mallctl() result");

	expect_d_eq(mallctl("stats.metadata", (void *)&metadata, &sz, NULL, 0),
	    expected, "Unexpected mallctl() result");
	expect_d_eq(mallctl("stats.metadata_edata", (void *)&metadata_edata,
	                &sz, NULL, 0),
	    expected, "Unexpected mallctl() result");
	expect_d_eq(mallctl("stats.metadata_rtree", (void *)&metadata_rtree,
	                &sz, NULL, 0),
	    expected, "Unexpected mallctl() result");

	if (config_stats) {
		expect_zu_le(allocated, active,
		    "allocated should be no larger than active");
		expect_zu_lt(
		    active, resident, "active should be less than resident");
		expect_zu_lt(
		    active, mapped, "active should be less than mapped");
		expect_zu_le(metadata_edata + metadata_rtree, metadata,
		    "the sum of metadata_edata and metadata_rtree "
		    "should be no larger than metadata");
	}
}
TEST_END

TEST_BEGIN(test_stats_large) {
	void    *p;
	uint64_t epoch;
	size_t   allocated;
	uint64_t nmalloc, ndalloc, nrequests;
	size_t   sz;
	int      expected = config_stats ? 0 : ENOENT;

	p = mallocx(SC_SMALL_MAXCLASS + 1, MALLOCX_ARENA(0));
	expect_ptr_not_null(p, "Unexpected mallocx() failure");

	expect_d_eq(mallctl("epoch", NULL, NULL, (void *)&epoch, sizeof(epoch)),
	    0, "Unexpected mallctl() failure");

	sz = sizeof(size_t);
	expect_d_eq(mallctl("stats.arenas.0.large.allocated",
	                (void *)&allocated, &sz, NULL, 0),
	    expected, "Unexpected mallctl() result");
	sz = sizeof(uint64_t);
	expect_d_eq(mallctl("stats.arenas.0.large.nmalloc", (void *)&nmalloc,
	                &sz, NULL, 0),
	    expected, "Unexpected mallctl() result");
	expect_d_eq(mallctl("stats.arenas.0.large.ndalloc", (void *)&ndalloc,
	                &sz, NULL, 0),
	    expected, "Unexpected mallctl() result");
	expect_d_eq(mallctl("stats.arenas.0.large.nrequests",
	                (void *)&nrequests, &sz, NULL, 0),
	    expected, "Unexpected mallctl() result");

	if (config_stats) {
		expect_zu_gt(
		    allocated, 0, "allocated should be greater than zero");
		expect_u64_ge(nmalloc, ndalloc,
		    "nmalloc should be at least as large as ndalloc");
		expect_u64_le(nmalloc, nrequests,
		    "nmalloc should no larger than nrequests");
	}

	dallocx(p, 0);
}
TEST_END

TEST_BEGIN(test_stats_arenas_summary) {
	void    *little, *large;
	uint64_t epoch;
	size_t   sz;
	int      expected = config_stats ? 0 : ENOENT;
	size_t   mapped;
	uint64_t dirty_npurge, dirty_nmadvise, dirty_purged;
	uint64_t muzzy_npurge, muzzy_nmadvise, muzzy_purged;

	little = mallocx(SC_SMALL_MAXCLASS, MALLOCX_ARENA(0));
	expect_ptr_not_null(little, "Unexpected mallocx() failure");
	large = mallocx((1U << SC_LG_LARGE_MINCLASS), MALLOCX_ARENA(0));
	expect_ptr_not_null(large, "Unexpected mallocx() failure");

	dallocx(little, 0);
	dallocx(large, 0);

	expect_d_eq(mallctl("thread.tcache.flush", NULL, NULL, NULL, 0),
	    opt_tcache ? 0 : EFAULT, "Unexpected mallctl() result");
	expect_d_eq(mallctl("arena.0.purge", NULL, NULL, NULL, 0), 0,
	    "Unexpected mallctl() failure");

	expect_d_eq(mallctl("epoch", NULL, NULL, (void *)&epoch, sizeof(epoch)),
	    0, "Unexpected mallctl() failure");

	sz = sizeof(size_t);
	expect_d_eq(
	    mallctl("stats.arenas.0.mapped", (void *)&mapped, &sz, NULL, 0),
	    expected, "Unexepected mallctl() result");

	sz = sizeof(uint64_t);
	expect_d_eq(mallctl("stats.arenas.0.dirty_npurge",
	                (void *)&dirty_npurge, &sz, NULL, 0),
	    expected, "Unexepected mallctl() result");
	expect_d_eq(mallctl("stats.arenas.0.dirty_nmadvise",
	                (void *)&dirty_nmadvise, &sz, NULL, 0),
	    expected, "Unexepected mallctl() result");
	expect_d_eq(mallctl("stats.arenas.0.dirty_purged",
	                (void *)&dirty_purged, &sz, NULL, 0),
	    expected, "Unexepected mallctl() result");
	expect_d_eq(mallctl("stats.arenas.0.muzzy_npurge",
	                (void *)&muzzy_npurge, &sz, NULL, 0),
	    expected, "Unexepected mallctl() result");
	expect_d_eq(mallctl("stats.arenas.0.muzzy_nmadvise",
	                (void *)&muzzy_nmadvise, &sz, NULL, 0),
	    expected, "Unexepected mallctl() result");
	expect_d_eq(mallctl("stats.arenas.0.muzzy_purged",
	                (void *)&muzzy_purged, &sz, NULL, 0),
	    expected, "Unexepected mallctl() result");

	if (config_stats) {
		if (!is_background_thread_enabled() && !opt_hpa) {
			expect_u64_gt(dirty_npurge + muzzy_npurge, 0,
			    "At least one purge should have occurred");
		}
		expect_u64_le(dirty_nmadvise, dirty_purged,
		    "dirty_nmadvise should be no greater than dirty_purged");
		expect_u64_le(muzzy_nmadvise, muzzy_purged,
		    "muzzy_nmadvise should be no greater than muzzy_purged");
	}
}
TEST_END

void *
thd_start(void *arg) {
	return NULL;
}

static void
no_lazy_lock(void) {
	thd_t thd;

	thd_create(&thd, thd_start, NULL);
	thd_join(thd, NULL);
}

TEST_BEGIN(test_stats_arenas_small) {
	void    *p;
	size_t   sz, allocated;
	uint64_t epoch, nmalloc, ndalloc, nrequests;
	int      expected = config_stats ? 0 : ENOENT;

	no_lazy_lock(); /* Lazy locking would dodge tcache testing. */

	p = mallocx(SC_SMALL_MAXCLASS, MALLOCX_ARENA(0));
	expect_ptr_not_null(p, "Unexpected mallocx() failure");

	expect_d_eq(mallctl("thread.tcache.flush", NULL, NULL, NULL, 0),
	    opt_tcache ? 0 : EFAULT, "Unexpected mallctl() result");

	expect_d_eq(mallctl("epoch", NULL, NULL, (void *)&epoch, sizeof(epoch)),
	    0, "Unexpected mallctl() failure");

	sz = sizeof(size_t);
	expect_d_eq(mallctl("stats.arenas.0.small.allocated",
	                (void *)&allocated, &sz, NULL, 0),
	    expected, "Unexpected mallctl() result");
	sz = sizeof(uint64_t);
	expect_d_eq(mallctl("stats.arenas.0.small.nmalloc", (void *)&nmalloc,
	                &sz, NULL, 0),
	    expected, "Unexpected mallctl() result");
	expect_d_eq(mallctl("stats.arenas.0.small.ndalloc", (void *)&ndalloc,
	                &sz, NULL, 0),
	    expected, "Unexpected mallctl() result");
	expect_d_eq(mallctl("stats.arenas.0.small.nrequests",
	                (void *)&nrequests, &sz, NULL, 0),
	    expected, "Unexpected mallctl() result");

	if (config_stats) {
		expect_zu_gt(
		    allocated, 0, "allocated should be greater than zero");
		expect_u64_gt(
		    nmalloc, 0, "nmalloc should be no greater than zero");
		expect_u64_ge(nmalloc, ndalloc,
		    "nmalloc should be at least as large as ndalloc");
		expect_u64_gt(
		    nrequests, 0, "nrequests should be greater than zero");
	}

	dallocx(p, 0);
}
TEST_END

TEST_BEGIN(test_stats_arenas_large) {
	void    *p;
	size_t   sz, allocated, allocated_before;
	uint64_t epoch, nmalloc, ndalloc;
	size_t   malloc_size = (1U << (SC_LG_LARGE_MINCLASS + 1)) + 1;
	int      expected = config_stats ? 0 : ENOENT;

	sz = sizeof(size_t);
	expect_d_eq(mallctl("stats.arenas.0.large.allocated",
	                (void *)&allocated_before, &sz, NULL, 0),
	    expected, "Unexpected mallctl() result");

	p = mallocx(malloc_size, MALLOCX_ARENA(0));
	expect_ptr_not_null(p, "Unexpected mallocx() failure");

	expect_d_eq(mallctl("epoch", NULL, NULL, (void *)&epoch, sizeof(epoch)),
	    0, "Unexpected mallctl() failure");

	expect_d_eq(mallctl("stats.arenas.0.large.allocated",
	                (void *)&allocated, &sz, NULL, 0),
	    expected, "Unexpected mallctl() result");
	sz = sizeof(uint64_t);
	expect_d_eq(mallctl("stats.arenas.0.large.nmalloc", (void *)&nmalloc,
	                &sz, NULL, 0),
	    expected, "Unexpected mallctl() result");
	expect_d_eq(mallctl("stats.arenas.0.large.ndalloc", (void *)&ndalloc,
	                &sz, NULL, 0),
	    expected, "Unexpected mallctl() result");

	if (config_stats) {
		expect_zu_ge(allocated_before, 0,
		    "allocated should be greater than zero");
		expect_zu_ge(allocated - allocated_before, sz_s2u(malloc_size),
		    "the diff between allocated should be greater than the allocation made");
		expect_u64_gt(
		    nmalloc, 0, "nmalloc should be greater than zero");
		expect_u64_ge(nmalloc, ndalloc,
		    "nmalloc should be at least as large as ndalloc");
	}

	dallocx(p, 0);
}
TEST_END

static void
gen_mallctl_str(char *cmd, char *name, unsigned arena_ind) {
	sprintf(cmd, "stats.arenas.%u.bins.0.%s", arena_ind, name);
}

TEST_BEGIN(test_stats_arenas_bins) {
	void    *p;
	size_t   sz, curslabs, curregs, nonfull_slabs;
	uint64_t epoch, nmalloc, ndalloc, nrequests, nfills, nflushes;
	uint64_t nslabs, nreslabs;
	int      expected = config_stats ? 0 : ENOENT;

	/* Make sure allocation below isn't satisfied by tcache. */
	expect_d_eq(mallctl("thread.tcache.flush", NULL, NULL, NULL, 0),
	    opt_tcache ? 0 : EFAULT, "Unexpected mallctl() result");

	unsigned arena_ind, old_arena_ind;
	sz = sizeof(unsigned);
	expect_d_eq(mallctl("arenas.create", (void *)&arena_ind, &sz, NULL, 0),
	    0, "Arena creation failure");
	sz = sizeof(arena_ind);
	expect_d_eq(mallctl("thread.arena", (void *)&old_arena_ind, &sz,
	                (void *)&arena_ind, sizeof(arena_ind)),
	    0, "Unexpected mallctl() failure");

	p = malloc(bin_infos[0].reg_size);
	expect_ptr_not_null(p, "Unexpected malloc() failure");

	expect_d_eq(mallctl("thread.tcache.flush", NULL, NULL, NULL, 0),
	    opt_tcache ? 0 : EFAULT, "Unexpected mallctl() result");

	expect_d_eq(mallctl("epoch", NULL, NULL, (void *)&epoch, sizeof(epoch)),
	    0, "Unexpected mallctl() failure");

	char cmd[128];
	sz = sizeof(uint64_t);
	gen_mallctl_str(cmd, "nmalloc", arena_ind);
	expect_d_eq(mallctl(cmd, (void *)&nmalloc, &sz, NULL, 0), expected,
	    "Unexpected mallctl() result");
	gen_mallctl_str(cmd, "ndalloc", arena_ind);
	expect_d_eq(mallctl(cmd, (void *)&ndalloc, &sz, NULL, 0), expected,
	    "Unexpected mallctl() result");
	gen_mallctl_str(cmd, "nrequests", arena_ind);
	expect_d_eq(mallctl(cmd, (void *)&nrequests, &sz, NULL, 0), expected,
	    "Unexpected mallctl() result");
	sz = sizeof(size_t);
	gen_mallctl_str(cmd, "curregs", arena_ind);
	expect_d_eq(mallctl(cmd, (void *)&curregs, &sz, NULL, 0), expected,
	    "Unexpected mallctl() result");

	sz = sizeof(uint64_t);
	gen_mallctl_str(cmd, "nfills", arena_ind);
	expect_d_eq(mallctl(cmd, (void *)&nfills, &sz, NULL, 0), expected,
	    "Unexpected mallctl() result");
	gen_mallctl_str(cmd, "nflushes", arena_ind);
	expect_d_eq(mallctl(cmd, (void *)&nflushes, &sz, NULL, 0), expected,
	    "Unexpected mallctl() result");

	gen_mallctl_str(cmd, "nslabs", arena_ind);
	expect_d_eq(mallctl(cmd, (void *)&nslabs, &sz, NULL, 0), expected,
	    "Unexpected mallctl() result");
	gen_mallctl_str(cmd, "nreslabs", arena_ind);
	expect_d_eq(mallctl(cmd, (void *)&nreslabs, &sz, NULL, 0), expected,
	    "Unexpected mallctl() result");
	sz = sizeof(size_t);
	gen_mallctl_str(cmd, "curslabs", arena_ind);
	expect_d_eq(mallctl(cmd, (void *)&curslabs, &sz, NULL, 0), expected,
	    "Unexpected mallctl() result");
	gen_mallctl_str(cmd, "nonfull_slabs", arena_ind);
	expect_d_eq(mallctl(cmd, (void *)&nonfull_slabs, &sz, NULL, 0),
	    expected, "Unexpected mallctl() result");

	if (config_stats) {
		expect_u64_gt(
		    nmalloc, 0, "nmalloc should be greater than zero");
		expect_u64_ge(nmalloc, ndalloc,
		    "nmalloc should be at least as large as ndalloc");
		expect_u64_gt(
		    nrequests, 0, "nrequests should be greater than zero");
		expect_zu_gt(
		    curregs, 0, "allocated should be greater than zero");
		if (opt_tcache) {
			expect_u64_gt(nfills, 0,
			    "At least one fill should have occurred");
			expect_u64_gt(nflushes, 0,
			    "At least one flush should have occurred");
		}
		expect_u64_gt(
		    nslabs, 0, "At least one slab should have been allocated");
		expect_zu_gt(curslabs, 0,
		    "At least one slab should be currently allocated");
		expect_zu_eq(nonfull_slabs, 0, "slabs_nonfull should be empty");
	}

	dallocx(p, 0);
}
TEST_END

TEST_BEGIN(test_stats_arenas_lextents) {
	void    *p;
	uint64_t epoch, nmalloc, ndalloc;
	size_t   curlextents, sz, hsize;
	int      expected = config_stats ? 0 : ENOENT;

	sz = sizeof(size_t);
	expect_d_eq(
	    mallctl("arenas.lextent.0.size", (void *)&hsize, &sz, NULL, 0), 0,
	    "Unexpected mallctl() failure");

	p = mallocx(hsize, MALLOCX_ARENA(0));
	expect_ptr_not_null(p, "Unexpected mallocx() failure");

	expect_d_eq(mallctl("epoch", NULL, NULL, (void *)&epoch, sizeof(epoch)),
	    0, "Unexpected mallctl() failure");

	sz = sizeof(uint64_t);
	expect_d_eq(mallctl("stats.arenas.0.lextents.0.nmalloc",
	                (void *)&nmalloc, &sz, NULL, 0),
	    expected, "Unexpected mallctl() result");
	expect_d_eq(mallctl("stats.arenas.0.lextents.0.ndalloc",
	                (void *)&ndalloc, &sz, NULL, 0),
	    expected, "Unexpected mallctl() result");
	sz = sizeof(size_t);
	expect_d_eq(mallctl("stats.arenas.0.lextents.0.curlextents",
	                (void *)&curlextents, &sz, NULL, 0),
	    expected, "Unexpected mallctl() result");

	if (config_stats) {
		expect_u64_gt(
		    nmalloc, 0, "nmalloc should be greater than zero");
		expect_u64_ge(nmalloc, ndalloc,
		    "nmalloc should be at least as large as ndalloc");
		expect_u64_gt(curlextents, 0,
		    "At least one extent should be currently allocated");
	}

	dallocx(p, 0);
}
TEST_END

static void
test_tcache_bytes_for_usize(size_t usize) {
	uint64_t epoch;
	size_t   tcache_bytes, tcache_stashed_bytes;
	size_t   sz = sizeof(tcache_bytes);

	void *ptr = mallocx(usize, 0);

	expect_d_eq(mallctl("epoch", NULL, NULL, (void *)&epoch, sizeof(epoch)),
	    0, "Unexpected mallctl() failure");
	assert_d_eq(mallctl("stats.arenas." STRINGIFY(
	                        MALLCTL_ARENAS_ALL) ".tcache_bytes",
	                &tcache_bytes, &sz, NULL, 0),
	    0, "Unexpected mallctl failure");
	assert_d_eq(mallctl("stats.arenas." STRINGIFY(
	                        MALLCTL_ARENAS_ALL) ".tcache_stashed_bytes",
	                &tcache_stashed_bytes, &sz, NULL, 0),
	    0, "Unexpected mallctl failure");
	size_t tcache_bytes_before = tcache_bytes + tcache_stashed_bytes;
	dallocx(ptr, 0);

	expect_d_eq(mallctl("epoch", NULL, NULL, (void *)&epoch, sizeof(epoch)),
	    0, "Unexpected mallctl() failure");
	assert_d_eq(mallctl("stats.arenas." STRINGIFY(
	                        MALLCTL_ARENAS_ALL) ".tcache_bytes",
	                &tcache_bytes, &sz, NULL, 0),
	    0, "Unexpected mallctl failure");
	assert_d_eq(mallctl("stats.arenas." STRINGIFY(
	                        MALLCTL_ARENAS_ALL) ".tcache_stashed_bytes",
	                &tcache_stashed_bytes, &sz, NULL, 0),
	    0, "Unexpected mallctl failure");
	size_t tcache_bytes_after = tcache_bytes + tcache_stashed_bytes;
	assert_zu_eq(tcache_bytes_after - tcache_bytes_before, usize,
	    "Incorrectly attributed a free");
}

TEST_BEGIN(test_stats_tcache_bytes_small) {
	test_skip_if(!config_stats);
	test_skip_if(!opt_tcache);
	test_skip_if(opt_tcache_max < SC_SMALL_MAXCLASS);

	test_tcache_bytes_for_usize(SC_SMALL_MAXCLASS);
}
TEST_END

TEST_BEGIN(test_stats_tcache_bytes_large) {
	test_skip_if(!config_stats);
	test_skip_if(!opt_tcache);
	test_skip_if(opt_tcache_max < SC_LARGE_MINCLASS);

	test_tcache_bytes_for_usize(SC_LARGE_MINCLASS);
}
TEST_END

TEST_BEGIN(test_approximate_stats_active) {
	/*
	 * Test 1: create a manual arena that we exclusively control and use it
	 * to verify the values returned by pa_shard_nactive() is accurate.
	 * This also helps verify the correctness of approximate_stats.active
	 * since it simply sums the pa_shard_nactive() of all arenas.
	 */
	tsdn_t  *tsdn = tsdn_fetch();
	unsigned arena_ind;
	size_t   sz = sizeof(unsigned);
	expect_d_eq(mallctl("arenas.create", (void *)&arena_ind, &sz, NULL, 0),
	    0, "Arena creation failed");

	arena_t *arena = arena_get(tsdn, arena_ind, false);
	expect_ptr_not_null(arena, "Failed to get arena");

	size_t nactive_initial = pa_shard_nactive(&arena->pa_shard);

	/*
	 * Allocate a small size from this arena.  Use MALLOCX_TCACHE_NONE
	 * to bypass tcache and ensure the allocation goes directly to the
	 * arena's pa_shard.
	 */
	size_t small_alloc_size = 128;
	void  *p_small = mallocx(
            small_alloc_size, MALLOCX_ARENA(arena_ind) | MALLOCX_TCACHE_NONE);
	expect_ptr_not_null(p_small, "Unexpected mallocx() failure for small");

	size_t nactive_after_small = pa_shard_nactive(&arena->pa_shard);
	/*
	 * For small allocations, jemalloc allocates a slab.  The slab size can
	 * be looked up via bin_infos[szind].slab_size.  The assertion allows
	 * for extra overhead from profiling, HPA, or sanitizer guard pages.
	 */
	size_t small_usize = nallocx(
	    small_alloc_size, MALLOCX_ARENA(arena_ind) | MALLOCX_TCACHE_NONE);
	szind_t small_szind = sz_size2index(small_usize);
	size_t  expected_small_pages = bin_infos[small_szind].slab_size / PAGE;
	expect_zu_ge(nactive_after_small - nactive_initial,
	    expected_small_pages,
	    "nactive increase should be at least the slab size in pages");

	/*
	 * Allocate a large size from this arena.
	 */
	size_t large_alloc_size = SC_LARGE_MINCLASS;
	void  *p_large = mallocx(
            large_alloc_size, MALLOCX_ARENA(arena_ind) | MALLOCX_TCACHE_NONE);
	expect_ptr_not_null(p_large, "Unexpected mallocx() failure for large");

	size_t nactive_after_large = pa_shard_nactive(&arena->pa_shard);
	/*
	 * For large allocations, the increase in pa_shard_nactive should be at
	 * least the allocation size in pages with sz_large_pad considered.
	 * The assertion allows for extra overhead from profiling, HPA, or
	 * sanitizer guard pages.
	 */
	size_t large_usize = nallocx(
	    large_alloc_size, MALLOCX_ARENA(arena_ind) | MALLOCX_TCACHE_NONE);
	size_t expected_large_pages = (large_usize + sz_large_pad) / PAGE;
	expect_zu_ge(nactive_after_large - nactive_after_small,
	    expected_large_pages,
	    "nactive increase should be at least the large allocation size in pages");

	/*
	 * Deallocate both allocations and verify nactive returns to the
	 * original value.
	 */
	dallocx(p_small, MALLOCX_TCACHE_NONE);
	dallocx(p_large, MALLOCX_TCACHE_NONE);

	size_t nactive_final = pa_shard_nactive(&arena->pa_shard);
	expect_zu_ge(nactive_final - nactive_after_large,
	    expected_small_pages + expected_large_pages,
	    "nactive should return to original value after deallocation");

	/*
	 * Test 2: allocate a large allocation in the auto arena and confirm
	 * that approximate_stats.active increases.  Since there may be other
	 * allocs/dallocs going on, cannot make more accurate assertions like
	 * Test 1.
	 */
	size_t approximate_active_before = 0;
	size_t approximate_active_after = 0;
	sz = sizeof(size_t);
	expect_d_eq(mallctl("approximate_stats.active",
	                (void *)&approximate_active_before, &sz, NULL, 0),
	    0, "Unexpected mallctl() result");

	void *p0 = mallocx(4 * SC_SMALL_MAXCLASS, MALLOCX_TCACHE_NONE);
	expect_ptr_not_null(p0, "Unexpected mallocx() failure");

	expect_d_eq(mallctl("approximate_stats.active",
	                (void *)&approximate_active_after, &sz, NULL, 0),
	    0, "Unexpected mallctl() result");
	expect_zu_gt(approximate_active_after, approximate_active_before,
	    "approximate_stats.active should increase after the allocation");

	free(p0);
}
TEST_END

int
main(void) {
	return test_no_reentrancy(test_stats_summary, test_stats_large,
	    test_stats_arenas_summary, test_stats_arenas_small,
	    test_stats_arenas_large, test_stats_arenas_bins,
	    test_stats_arenas_lextents, test_stats_tcache_bytes_small,
	    test_stats_tcache_bytes_large, test_approximate_stats_active);
}

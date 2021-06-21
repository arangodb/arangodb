#include "test/jemalloc_test.h"

#include "jemalloc/internal/ticker.h"

static nstime_monotonic_t *nstime_monotonic_orig;
static nstime_update_t *nstime_update_orig;

static unsigned nupdates_mock;
static nstime_t time_mock;
static bool monotonic_mock;

static bool
check_background_thread_enabled(void) {
	bool enabled;
	size_t sz = sizeof(bool);
	int ret = mallctl("background_thread", (void *)&enabled, &sz, NULL,0);
	if (ret == ENOENT) {
		return false;
	}
	expect_d_eq(ret, 0, "Unexpected mallctl error");
	return enabled;
}

static bool
nstime_monotonic_mock(void) {
	return monotonic_mock;
}

static void
nstime_update_mock(nstime_t *time) {
	nupdates_mock++;
	if (monotonic_mock) {
		nstime_copy(time, &time_mock);
	}
}

static unsigned
do_arena_create(ssize_t dirty_decay_ms, ssize_t muzzy_decay_ms) {
	unsigned arena_ind;
	size_t sz = sizeof(unsigned);
	expect_d_eq(mallctl("arenas.create", (void *)&arena_ind, &sz, NULL, 0),
	    0, "Unexpected mallctl() failure");
	size_t mib[3];
	size_t miblen = sizeof(mib)/sizeof(size_t);

	expect_d_eq(mallctlnametomib("arena.0.dirty_decay_ms", mib, &miblen),
	    0, "Unexpected mallctlnametomib() failure");
	mib[1] = (size_t)arena_ind;
	expect_d_eq(mallctlbymib(mib, miblen, NULL, NULL,
	    (void *)&dirty_decay_ms, sizeof(dirty_decay_ms)), 0,
	    "Unexpected mallctlbymib() failure");

	expect_d_eq(mallctlnametomib("arena.0.muzzy_decay_ms", mib, &miblen),
	    0, "Unexpected mallctlnametomib() failure");
	mib[1] = (size_t)arena_ind;
	expect_d_eq(mallctlbymib(mib, miblen, NULL, NULL,
	    (void *)&muzzy_decay_ms, sizeof(muzzy_decay_ms)), 0,
	    "Unexpected mallctlbymib() failure");

	return arena_ind;
}

static void
do_arena_destroy(unsigned arena_ind) {
	size_t mib[3];
	size_t miblen = sizeof(mib)/sizeof(size_t);
	expect_d_eq(mallctlnametomib("arena.0.destroy", mib, &miblen), 0,
	    "Unexpected mallctlnametomib() failure");
	mib[1] = (size_t)arena_ind;
	expect_d_eq(mallctlbymib(mib, miblen, NULL, NULL, NULL, 0), 0,
	    "Unexpected mallctlbymib() failure");
}

void
do_epoch(void) {
	uint64_t epoch = 1;
	expect_d_eq(mallctl("epoch", NULL, NULL, (void *)&epoch, sizeof(epoch)),
	    0, "Unexpected mallctl() failure");
}

void
do_purge(unsigned arena_ind) {
	size_t mib[3];
	size_t miblen = sizeof(mib)/sizeof(size_t);
	expect_d_eq(mallctlnametomib("arena.0.purge", mib, &miblen), 0,
	    "Unexpected mallctlnametomib() failure");
	mib[1] = (size_t)arena_ind;
	expect_d_eq(mallctlbymib(mib, miblen, NULL, NULL, NULL, 0), 0,
	    "Unexpected mallctlbymib() failure");
}

void
do_decay(unsigned arena_ind) {
	size_t mib[3];
	size_t miblen = sizeof(mib)/sizeof(size_t);
	expect_d_eq(mallctlnametomib("arena.0.decay", mib, &miblen), 0,
	    "Unexpected mallctlnametomib() failure");
	mib[1] = (size_t)arena_ind;
	expect_d_eq(mallctlbymib(mib, miblen, NULL, NULL, NULL, 0), 0,
	    "Unexpected mallctlbymib() failure");
}

static uint64_t
get_arena_npurge_impl(const char *mibname, unsigned arena_ind) {
	size_t mib[4];
	size_t miblen = sizeof(mib)/sizeof(size_t);
	expect_d_eq(mallctlnametomib(mibname, mib, &miblen), 0,
	    "Unexpected mallctlnametomib() failure");
	mib[2] = (size_t)arena_ind;
	uint64_t npurge = 0;
	size_t sz = sizeof(npurge);
	expect_d_eq(mallctlbymib(mib, miblen, (void *)&npurge, &sz, NULL, 0),
	    config_stats ? 0 : ENOENT, "Unexpected mallctlbymib() failure");
	return npurge;
}

static uint64_t
get_arena_dirty_npurge(unsigned arena_ind) {
	do_epoch();
	return get_arena_npurge_impl("stats.arenas.0.dirty_npurge", arena_ind);
}

static uint64_t
get_arena_dirty_purged(unsigned arena_ind) {
	do_epoch();
	return get_arena_npurge_impl("stats.arenas.0.dirty_purged", arena_ind);
}

static uint64_t
get_arena_muzzy_npurge(unsigned arena_ind) {
	do_epoch();
	return get_arena_npurge_impl("stats.arenas.0.muzzy_npurge", arena_ind);
}

static uint64_t
get_arena_npurge(unsigned arena_ind) {
	do_epoch();
	return get_arena_npurge_impl("stats.arenas.0.dirty_npurge", arena_ind) +
	    get_arena_npurge_impl("stats.arenas.0.muzzy_npurge", arena_ind);
}

static size_t
get_arena_pdirty(unsigned arena_ind) {
	do_epoch();
	size_t mib[4];
	size_t miblen = sizeof(mib)/sizeof(size_t);
	expect_d_eq(mallctlnametomib("stats.arenas.0.pdirty", mib, &miblen), 0,
	    "Unexpected mallctlnametomib() failure");
	mib[2] = (size_t)arena_ind;
	size_t pdirty;
	size_t sz = sizeof(pdirty);
	expect_d_eq(mallctlbymib(mib, miblen, (void *)&pdirty, &sz, NULL, 0), 0,
	    "Unexpected mallctlbymib() failure");
	return pdirty;
}

static size_t
get_arena_pmuzzy(unsigned arena_ind) {
	do_epoch();
	size_t mib[4];
	size_t miblen = sizeof(mib)/sizeof(size_t);
	expect_d_eq(mallctlnametomib("stats.arenas.0.pmuzzy", mib, &miblen), 0,
	    "Unexpected mallctlnametomib() failure");
	mib[2] = (size_t)arena_ind;
	size_t pmuzzy;
	size_t sz = sizeof(pmuzzy);
	expect_d_eq(mallctlbymib(mib, miblen, (void *)&pmuzzy, &sz, NULL, 0), 0,
	    "Unexpected mallctlbymib() failure");
	return pmuzzy;
}

static void *
do_mallocx(size_t size, int flags) {
	void *p = mallocx(size, flags);
	expect_ptr_not_null(p, "Unexpected mallocx() failure");
	return p;
}

static void
generate_dirty(unsigned arena_ind, size_t size) {
	int flags = MALLOCX_ARENA(arena_ind) | MALLOCX_TCACHE_NONE;
	void *p = do_mallocx(size, flags);
	dallocx(p, flags);
}

TEST_BEGIN(test_decay_ticks) {
	test_skip_if(check_background_thread_enabled());
	test_skip_if(opt_hpa);

	ticker_geom_t *decay_ticker;
	unsigned tick0, tick1, arena_ind;
	size_t sz, large0;
	void *p;

	sz = sizeof(size_t);
	expect_d_eq(mallctl("arenas.lextent.0.size", (void *)&large0, &sz, NULL,
	    0), 0, "Unexpected mallctl failure");

	/* Set up a manually managed arena for test. */
	arena_ind = do_arena_create(0, 0);

	/* Migrate to the new arena, and get the ticker. */
	unsigned old_arena_ind;
	size_t sz_arena_ind = sizeof(old_arena_ind);
	expect_d_eq(mallctl("thread.arena", (void *)&old_arena_ind,
	    &sz_arena_ind, (void *)&arena_ind, sizeof(arena_ind)), 0,
	    "Unexpected mallctl() failure");
	decay_ticker = tsd_arena_decay_tickerp_get(tsd_fetch());
	expect_ptr_not_null(decay_ticker,
	    "Unexpected failure getting decay ticker");

	/*
	 * Test the standard APIs using a large size class, since we can't
	 * control tcache interactions for small size classes (except by
	 * completely disabling tcache for the entire test program).
	 */

	/* malloc(). */
	tick0 = ticker_geom_read(decay_ticker);
	p = malloc(large0);
	expect_ptr_not_null(p, "Unexpected malloc() failure");
	tick1 = ticker_geom_read(decay_ticker);
	expect_u32_ne(tick1, tick0, "Expected ticker to tick during malloc()");
	/* free(). */
	tick0 = ticker_geom_read(decay_ticker);
	free(p);
	tick1 = ticker_geom_read(decay_ticker);
	expect_u32_ne(tick1, tick0, "Expected ticker to tick during free()");

	/* calloc(). */
	tick0 = ticker_geom_read(decay_ticker);
	p = calloc(1, large0);
	expect_ptr_not_null(p, "Unexpected calloc() failure");
	tick1 = ticker_geom_read(decay_ticker);
	expect_u32_ne(tick1, tick0, "Expected ticker to tick during calloc()");
	free(p);

	/* posix_memalign(). */
	tick0 = ticker_geom_read(decay_ticker);
	expect_d_eq(posix_memalign(&p, sizeof(size_t), large0), 0,
	    "Unexpected posix_memalign() failure");
	tick1 = ticker_geom_read(decay_ticker);
	expect_u32_ne(tick1, tick0,
	    "Expected ticker to tick during posix_memalign()");
	free(p);

	/* aligned_alloc(). */
	tick0 = ticker_geom_read(decay_ticker);
	p = aligned_alloc(sizeof(size_t), large0);
	expect_ptr_not_null(p, "Unexpected aligned_alloc() failure");
	tick1 = ticker_geom_read(decay_ticker);
	expect_u32_ne(tick1, tick0,
	    "Expected ticker to tick during aligned_alloc()");
	free(p);

	/* realloc(). */
	/* Allocate. */
	tick0 = ticker_geom_read(decay_ticker);
	p = realloc(NULL, large0);
	expect_ptr_not_null(p, "Unexpected realloc() failure");
	tick1 = ticker_geom_read(decay_ticker);
	expect_u32_ne(tick1, tick0, "Expected ticker to tick during realloc()");
	/* Reallocate. */
	tick0 = ticker_geom_read(decay_ticker);
	p = realloc(p, large0);
	expect_ptr_not_null(p, "Unexpected realloc() failure");
	tick1 = ticker_geom_read(decay_ticker);
	expect_u32_ne(tick1, tick0, "Expected ticker to tick during realloc()");
	/* Deallocate. */
	tick0 = ticker_geom_read(decay_ticker);
	realloc(p, 0);
	tick1 = ticker_geom_read(decay_ticker);
	expect_u32_ne(tick1, tick0, "Expected ticker to tick during realloc()");

	/*
	 * Test the *allocx() APIs using large and small size classes, with
	 * tcache explicitly disabled.
	 */
	{
		unsigned i;
		size_t allocx_sizes[2];
		allocx_sizes[0] = large0;
		allocx_sizes[1] = 1;

		for (i = 0; i < sizeof(allocx_sizes) / sizeof(size_t); i++) {
			sz = allocx_sizes[i];

			/* mallocx(). */
			tick0 = ticker_geom_read(decay_ticker);
			p = mallocx(sz, MALLOCX_TCACHE_NONE);
			expect_ptr_not_null(p, "Unexpected mallocx() failure");
			tick1 = ticker_geom_read(decay_ticker);
			expect_u32_ne(tick1, tick0,
			    "Expected ticker to tick during mallocx() (sz=%zu)",
			    sz);
			/* rallocx(). */
			tick0 = ticker_geom_read(decay_ticker);
			p = rallocx(p, sz, MALLOCX_TCACHE_NONE);
			expect_ptr_not_null(p, "Unexpected rallocx() failure");
			tick1 = ticker_geom_read(decay_ticker);
			expect_u32_ne(tick1, tick0,
			    "Expected ticker to tick during rallocx() (sz=%zu)",
			    sz);
			/* xallocx(). */
			tick0 = ticker_geom_read(decay_ticker);
			xallocx(p, sz, 0, MALLOCX_TCACHE_NONE);
			tick1 = ticker_geom_read(decay_ticker);
			expect_u32_ne(tick1, tick0,
			    "Expected ticker to tick during xallocx() (sz=%zu)",
			    sz);
			/* dallocx(). */
			tick0 = ticker_geom_read(decay_ticker);
			dallocx(p, MALLOCX_TCACHE_NONE);
			tick1 = ticker_geom_read(decay_ticker);
			expect_u32_ne(tick1, tick0,
			    "Expected ticker to tick during dallocx() (sz=%zu)",
			    sz);
			/* sdallocx(). */
			p = mallocx(sz, MALLOCX_TCACHE_NONE);
			expect_ptr_not_null(p, "Unexpected mallocx() failure");
			tick0 = ticker_geom_read(decay_ticker);
			sdallocx(p, sz, MALLOCX_TCACHE_NONE);
			tick1 = ticker_geom_read(decay_ticker);
			expect_u32_ne(tick1, tick0,
			    "Expected ticker to tick during sdallocx() "
			    "(sz=%zu)", sz);
		}
	}

	/*
	 * Test tcache fill/flush interactions for large and small size classes,
	 * using an explicit tcache.
	 */
	unsigned tcache_ind, i;
	size_t tcache_sizes[2];
	tcache_sizes[0] = large0;
	tcache_sizes[1] = 1;

	size_t tcache_max, sz_tcache_max;
	sz_tcache_max = sizeof(tcache_max);
	expect_d_eq(mallctl("arenas.tcache_max", (void *)&tcache_max,
	    &sz_tcache_max, NULL, 0), 0, "Unexpected mallctl() failure");

	sz = sizeof(unsigned);
	expect_d_eq(mallctl("tcache.create", (void *)&tcache_ind, &sz,
	    NULL, 0), 0, "Unexpected mallctl failure");

	for (i = 0; i < sizeof(tcache_sizes) / sizeof(size_t); i++) {
		sz = tcache_sizes[i];

		/* tcache fill. */
		tick0 = ticker_geom_read(decay_ticker);
		p = mallocx(sz, MALLOCX_TCACHE(tcache_ind));
		expect_ptr_not_null(p, "Unexpected mallocx() failure");
		tick1 = ticker_geom_read(decay_ticker);
		expect_u32_ne(tick1, tick0,
		    "Expected ticker to tick during tcache fill "
		    "(sz=%zu)", sz);
		/* tcache flush. */
		dallocx(p, MALLOCX_TCACHE(tcache_ind));
		tick0 = ticker_geom_read(decay_ticker);
		expect_d_eq(mallctl("tcache.flush", NULL, NULL,
		    (void *)&tcache_ind, sizeof(unsigned)), 0,
		    "Unexpected mallctl failure");
		tick1 = ticker_geom_read(decay_ticker);

		/* Will only tick if it's in tcache. */
		expect_u32_ne(tick1, tick0,
		    "Expected ticker to tick during tcache flush (sz=%zu)", sz);
	}
}
TEST_END

static void
decay_ticker_helper(unsigned arena_ind, int flags, bool dirty, ssize_t dt,
    uint64_t dirty_npurge0, uint64_t muzzy_npurge0, bool terminate_asap) {
#define NINTERVALS 101
	nstime_t time, update_interval, decay_ms, deadline;

	nstime_init_update(&time);

	nstime_init2(&decay_ms, dt, 0);
	nstime_copy(&deadline, &time);
	nstime_add(&deadline, &decay_ms);

	nstime_init2(&update_interval, dt, 0);
	nstime_idivide(&update_interval, NINTERVALS);

	/*
	 * Keep q's slab from being deallocated during the looping below.  If a
	 * cached slab were to repeatedly come and go during looping, it could
	 * prevent the decay backlog ever becoming empty.
	 */
	void *p = do_mallocx(1, flags);
	uint64_t dirty_npurge1, muzzy_npurge1;
	do {
		for (unsigned i = 0; i < ARENA_DECAY_NTICKS_PER_UPDATE / 2;
		    i++) {
			void *q = do_mallocx(1, flags);
			dallocx(q, flags);
		}
		dirty_npurge1 = get_arena_dirty_npurge(arena_ind);
		muzzy_npurge1 = get_arena_muzzy_npurge(arena_ind);

		nstime_add(&time_mock, &update_interval);
		nstime_update(&time);
	} while (nstime_compare(&time, &deadline) <= 0 && ((dirty_npurge1 ==
	    dirty_npurge0 && muzzy_npurge1 == muzzy_npurge0) ||
	    !terminate_asap));
	dallocx(p, flags);

	if (config_stats) {
		expect_u64_gt(dirty_npurge1 + muzzy_npurge1, dirty_npurge0 +
		    muzzy_npurge0, "Expected purging to occur");
	}
#undef NINTERVALS
}

TEST_BEGIN(test_decay_ticker) {
	test_skip_if(check_background_thread_enabled());
	test_skip_if(opt_hpa);
#define NPS 2048
	ssize_t ddt = opt_dirty_decay_ms;
	ssize_t mdt = opt_muzzy_decay_ms;
	unsigned arena_ind = do_arena_create(ddt, mdt);
	int flags = (MALLOCX_ARENA(arena_ind) | MALLOCX_TCACHE_NONE);
	void *ps[NPS];

	/*
	 * Allocate a bunch of large objects, pause the clock, deallocate every
	 * other object (to fragment virtual memory), restore the clock, then
	 * [md]allocx() in a tight loop while advancing time rapidly to verify
	 * the ticker triggers purging.
	 */
	size_t large;
	size_t sz = sizeof(size_t);
	expect_d_eq(mallctl("arenas.lextent.0.size", (void *)&large, &sz, NULL,
	    0), 0, "Unexpected mallctl failure");

	do_purge(arena_ind);
	uint64_t dirty_npurge0 = get_arena_dirty_npurge(arena_ind);
	uint64_t muzzy_npurge0 = get_arena_muzzy_npurge(arena_ind);

	for (unsigned i = 0; i < NPS; i++) {
		ps[i] = do_mallocx(large, flags);
	}

	nupdates_mock = 0;
	nstime_init_update(&time_mock);
	monotonic_mock = true;

	nstime_monotonic_orig = nstime_monotonic;
	nstime_update_orig = nstime_update;
	nstime_monotonic = nstime_monotonic_mock;
	nstime_update = nstime_update_mock;

	for (unsigned i = 0; i < NPS; i += 2) {
		dallocx(ps[i], flags);
		unsigned nupdates0 = nupdates_mock;
		do_decay(arena_ind);
		expect_u_gt(nupdates_mock, nupdates0,
		    "Expected nstime_update() to be called");
	}

	decay_ticker_helper(arena_ind, flags, true, ddt, dirty_npurge0,
	    muzzy_npurge0, true);
	decay_ticker_helper(arena_ind, flags, false, ddt+mdt, dirty_npurge0,
	    muzzy_npurge0, false);

	do_arena_destroy(arena_ind);

	nstime_monotonic = nstime_monotonic_orig;
	nstime_update = nstime_update_orig;
#undef NPS
}
TEST_END

TEST_BEGIN(test_decay_nonmonotonic) {
	test_skip_if(check_background_thread_enabled());
	test_skip_if(opt_hpa);
#define NPS (SMOOTHSTEP_NSTEPS + 1)
	int flags = (MALLOCX_ARENA(0) | MALLOCX_TCACHE_NONE);
	void *ps[NPS];
	uint64_t npurge0 = 0;
	uint64_t npurge1 = 0;
	size_t sz, large0;
	unsigned i, nupdates0;

	sz = sizeof(size_t);
	expect_d_eq(mallctl("arenas.lextent.0.size", (void *)&large0, &sz, NULL,
	    0), 0, "Unexpected mallctl failure");

	expect_d_eq(mallctl("arena.0.purge", NULL, NULL, NULL, 0), 0,
	    "Unexpected mallctl failure");
	do_epoch();
	sz = sizeof(uint64_t);
	npurge0 = get_arena_npurge(0);

	nupdates_mock = 0;
	nstime_init_update(&time_mock);
	monotonic_mock = false;

	nstime_monotonic_orig = nstime_monotonic;
	nstime_update_orig = nstime_update;
	nstime_monotonic = nstime_monotonic_mock;
	nstime_update = nstime_update_mock;

	for (i = 0; i < NPS; i++) {
		ps[i] = mallocx(large0, flags);
		expect_ptr_not_null(ps[i], "Unexpected mallocx() failure");
	}

	for (i = 0; i < NPS; i++) {
		dallocx(ps[i], flags);
		nupdates0 = nupdates_mock;
		expect_d_eq(mallctl("arena.0.decay", NULL, NULL, NULL, 0), 0,
		    "Unexpected arena.0.decay failure");
		expect_u_gt(nupdates_mock, nupdates0,
		    "Expected nstime_update() to be called");
	}

	do_epoch();
	sz = sizeof(uint64_t);
	npurge1 = get_arena_npurge(0);

	if (config_stats) {
		expect_u64_eq(npurge0, npurge1, "Unexpected purging occurred");
	}

	nstime_monotonic = nstime_monotonic_orig;
	nstime_update = nstime_update_orig;
#undef NPS
}
TEST_END

TEST_BEGIN(test_decay_now) {
	test_skip_if(check_background_thread_enabled());
	test_skip_if(opt_hpa);

	unsigned arena_ind = do_arena_create(0, 0);
	expect_zu_eq(get_arena_pdirty(arena_ind), 0, "Unexpected dirty pages");
	expect_zu_eq(get_arena_pmuzzy(arena_ind), 0, "Unexpected muzzy pages");
	size_t sizes[] = {16, PAGE<<2, HUGEPAGE<<2};
	/* Verify that dirty/muzzy pages never linger after deallocation. */
	for (unsigned i = 0; i < sizeof(sizes)/sizeof(size_t); i++) {
		size_t size = sizes[i];
		generate_dirty(arena_ind, size);
		expect_zu_eq(get_arena_pdirty(arena_ind), 0,
		    "Unexpected dirty pages");
		expect_zu_eq(get_arena_pmuzzy(arena_ind), 0,
		    "Unexpected muzzy pages");
	}
	do_arena_destroy(arena_ind);
}
TEST_END

TEST_BEGIN(test_decay_never) {
	test_skip_if(check_background_thread_enabled() || !config_stats);
	test_skip_if(opt_hpa);

	unsigned arena_ind = do_arena_create(-1, -1);
	int flags = MALLOCX_ARENA(arena_ind) | MALLOCX_TCACHE_NONE;
	expect_zu_eq(get_arena_pdirty(arena_ind), 0, "Unexpected dirty pages");
	expect_zu_eq(get_arena_pmuzzy(arena_ind), 0, "Unexpected muzzy pages");
	size_t sizes[] = {16, PAGE<<2, HUGEPAGE<<2};
	void *ptrs[sizeof(sizes)/sizeof(size_t)];
	for (unsigned i = 0; i < sizeof(sizes)/sizeof(size_t); i++) {
		ptrs[i] = do_mallocx(sizes[i], flags);
	}
	/* Verify that each deallocation generates additional dirty pages. */
	size_t pdirty_prev = get_arena_pdirty(arena_ind);
	size_t pmuzzy_prev = get_arena_pmuzzy(arena_ind);
	expect_zu_eq(pdirty_prev, 0, "Unexpected dirty pages");
	expect_zu_eq(pmuzzy_prev, 0, "Unexpected muzzy pages");
	for (unsigned i = 0; i < sizeof(sizes)/sizeof(size_t); i++) {
		dallocx(ptrs[i], flags);
		size_t pdirty = get_arena_pdirty(arena_ind);
		size_t pmuzzy = get_arena_pmuzzy(arena_ind);
		expect_zu_gt(pdirty + (size_t)get_arena_dirty_purged(arena_ind),
		    pdirty_prev, "Expected dirty pages to increase.");
		expect_zu_eq(pmuzzy, 0, "Unexpected muzzy pages");
		pdirty_prev = pdirty;
	}
	do_arena_destroy(arena_ind);
}
TEST_END

int
main(void) {
	return test(
	    test_decay_ticks,
	    test_decay_ticker,
	    test_decay_nonmonotonic,
	    test_decay_now,
	    test_decay_never);
}

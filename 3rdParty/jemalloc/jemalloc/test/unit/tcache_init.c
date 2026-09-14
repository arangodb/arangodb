#include "test/jemalloc_test.h"

static void *
tcache_stack_alloc_fail(tsdn_t *tsdn, size_t size, size_t alignment) {
	return NULL;
}

TEST_BEGIN(test_tcache_data_init_oom) {
	bool orig_opt_abort = opt_abort;
	void *(*orig_tcache_stack_alloc)(tsdn_t *, size_t, size_t) =
	    tcache_stack_alloc;

	opt_abort = false;
	tcache_stack_alloc = tcache_stack_alloc_fail;

	/*
	 * Trigger init through tcache_enabled_set by enabling and
	 * disabling the tcache.
	 */
	bool e0, e1;
	size_t bool_sz = sizeof(bool);

	/* Disable the tcache. */
	e1 = false;
	expect_d_eq(mallctl("thread.tcache.enabled", (void *)&e0, &bool_sz,
	    (void *)&e1, bool_sz), 0, "Unexpected mallctl failure");

	/* Try to enable the tcache.  Initialization should fail. */
	e1 = true;
	expect_d_eq(mallctl("thread.tcache.enabled", (void *)&e0, &bool_sz,
	    (void *)&e1, bool_sz), 0, "Unexpected mallctl failure");

	/* The tcache should be disabled. */
	tsd_t *tsd = tsd_fetch();
	expect_false(tsd_tcache_enabled_get(tsd),
	    "tcache should be disabled after init failure");

	/* Allocations should go to the arena. */
	void *p = malloc(64);
	expect_ptr_not_null(p, "malloc should succeed without tcache");
	free(p);

	/* Restore the original values */
	tcache_stack_alloc = orig_tcache_stack_alloc;
	opt_abort = orig_opt_abort;

	/*
	 * Try to enable the tcache again.  This time initialization
	 * should succeed.
	 */
	e1 = true;
	expect_d_eq(mallctl("thread.tcache.enabled", (void *)&e0, &bool_sz,
	    (void *)&e1, bool_sz), 0, "Unexpected mallctl failure");
}
TEST_END

TEST_BEGIN(test_tcache_reinit_oom) {
	bool orig_opt_abort = opt_abort;
	void *(*orig_tcache_stack_alloc)(tsdn_t *, size_t, size_t) =
	    tcache_stack_alloc;

	/* Read current tcache max. */
	size_t old_tcache_max, sz;
	sz = sizeof(old_tcache_max);
	expect_d_eq(mallctl("thread.tcache.max", (void *)&old_tcache_max, &sz,
	    NULL, 0), 0, "Unexpected mallctl failure");

	opt_abort = false;
	tcache_stack_alloc = tcache_stack_alloc_fail;

	/*
	 * Setting thread.tcache.max causes a reinitialization.  With
	 * the thread_stack_alloc override reinitialization should
	 * fail and disable tcache.
	 */
	size_t new_tcache_max = 1024;
	new_tcache_max = sz_s2u(new_tcache_max);
	expect_d_eq(mallctl("thread.tcache.max", NULL, NULL,
	    (void *)&new_tcache_max, sizeof(new_tcache_max)), 0,
	    "Unexpected mallctl failure");

	/* Check that the tcache was disabled. */
	tsd_t *tsd = tsd_fetch();
	expect_false(tsd_tcache_enabled_get(tsd),
	    "tcache should be disabled after reinit failure");

	/* Allocations should go to the arena. */
	void *p = malloc(64);
	expect_ptr_not_null(p, "malloc should succeed without tcache");
	free(p);

	/* Restore the original values */
	tcache_stack_alloc = orig_tcache_stack_alloc;
	opt_abort = orig_opt_abort;

	/*
	 * Try to enable the tcache again.  This time initialization
	 * should succeed.
	 */
	bool e0, e1;
	size_t bool_sz = sizeof(bool);
	e1 = true;
	expect_d_eq(mallctl("thread.tcache.enabled", (void *)&e0, &bool_sz,
	    (void *)&e1, bool_sz), 0, "Unexpected mallctl failure");

	/* Restore the original tcache max. */
	expect_d_eq(mallctl("thread.tcache.max", NULL, NULL,
	    (void *)&old_tcache_max, sizeof(old_tcache_max)), 0,
	    "Unexpected mallctl failure");
}
TEST_END

int
main(void) {
	return test(test_tcache_data_init_oom, test_tcache_reinit_oom);
}

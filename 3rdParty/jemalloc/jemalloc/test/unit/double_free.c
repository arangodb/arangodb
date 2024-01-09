#include "test/jemalloc_test.h"
#include "test/san.h"

#include "jemalloc/internal/safety_check.h"

bool fake_abort_called;
void fake_abort(const char *message) {
	(void)message;
	fake_abort_called = true;
}

static void
test_double_free_pre(void) {
	safety_check_set_abort(&fake_abort);
	fake_abort_called = false;
}

static void
test_double_free_post(void) {
	expect_b_eq(fake_abort_called, true, "Double-free check didn't fire.");
	safety_check_set_abort(NULL);
}

static bool
tcache_enabled(void) {
	bool enabled;
	size_t sz = sizeof(enabled);
	assert_d_eq(
	    mallctl("thread.tcache.enabled", &enabled, &sz, NULL, 0), 0,
	    "Unexpected mallctl failure");
	return enabled;
}

TEST_BEGIN(test_large_double_free_tcache) {
	test_skip_if(!config_opt_safety_checks);
	/*
	 * Skip debug builds, since too many assertions will be triggered with
	 * double-free before hitting the one we are interested in.
	 */
	test_skip_if(config_debug);

	test_double_free_pre();
	char *ptr = malloc(SC_LARGE_MINCLASS);
	bool guarded = extent_is_guarded(tsdn_fetch(), ptr);
	free(ptr);
	if (!guarded) {
		free(ptr);
	} else {
		/*
		 * Skip because guarded extents may unguard immediately on
		 * deallocation, in which case the second free will crash before
		 * reaching the intended safety check.
		 */
		fake_abort_called = true;
	}
	mallctl("thread.tcache.flush", NULL, NULL, NULL, 0);
	test_double_free_post();
}
TEST_END

TEST_BEGIN(test_large_double_free_no_tcache) {
	test_skip_if(!config_opt_safety_checks);
	test_skip_if(config_debug);

	test_double_free_pre();
	char *ptr = mallocx(SC_LARGE_MINCLASS, MALLOCX_TCACHE_NONE);
	bool guarded = extent_is_guarded(tsdn_fetch(), ptr);
	dallocx(ptr, MALLOCX_TCACHE_NONE);
	if (!guarded) {
		dallocx(ptr, MALLOCX_TCACHE_NONE);
	} else {
		/*
		 * Skip because guarded extents may unguard immediately on
		 * deallocation, in which case the second free will crash before
		 * reaching the intended safety check.
		 */
		fake_abort_called = true;
	}
	test_double_free_post();
}
TEST_END

TEST_BEGIN(test_small_double_free_tcache) {
	test_skip_if(!config_debug);
	test_skip_if(opt_debug_double_free_max_scan == 0);
	test_skip_if(!tcache_enabled());

	test_double_free_pre();
	char *ptr = malloc(1);
	bool guarded = extent_is_guarded(tsdn_fetch(), ptr);
	free(ptr);
	if (!guarded) {
		free(ptr);
	} else {
		/*
		 * Skip because guarded extents may unguard immediately on
		 * deallocation, in which case the second free will crash before
		 * reaching the intended safety check.
		 */
		fake_abort_called = true;
	}
	mallctl("thread.tcache.flush", NULL, NULL, NULL, 0);
	test_double_free_post();
}
TEST_END

TEST_BEGIN(test_small_double_free_arena) {
	test_skip_if(!config_debug);
	test_skip_if(!tcache_enabled());

	test_double_free_pre();
	/*
	 * Allocate one more pointer to keep the slab partially used after
	 * flushing the cache.
	 */
	char *ptr1 = malloc(1);
	char *ptr = malloc(1);
	bool guarded = extent_is_guarded(tsdn_fetch(), ptr);
	free(ptr);
	if (!guarded) {
		mallctl("thread.tcache.flush", NULL, NULL, NULL, 0);
		free(ptr);
	} else {
		/*
		 * Skip because guarded extents may unguard immediately on
		 * deallocation, in which case the second free will crash before
		 * reaching the intended safety check.
		 */
		fake_abort_called = true;
	}
	test_double_free_post();
	free(ptr1);
}
TEST_END

int
main(void) {
	return test(
	    test_large_double_free_no_tcache,
	    test_large_double_free_tcache,
	    test_small_double_free_tcache,
	    test_small_double_free_arena);
}

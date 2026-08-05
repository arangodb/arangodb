#include "test/jemalloc_test.h"

const char *malloc_conf = "dirty_decay_ms:1000,muzzy_decay_ms:2000";
const char *malloc_conf_2_conf_harder = "dirty_decay_ms:1234";

TEST_BEGIN(test_malloc_conf_2) {
#ifdef _WIN32
	bool windows = true;
#else
	bool windows = false;
#endif
	/* Windows doesn't support weak symbol linker trickery. */
	test_skip_if(windows);

	ssize_t dirty_decay_ms;
	size_t  sz = sizeof(dirty_decay_ms);

	int err = mallctl("opt.dirty_decay_ms", &dirty_decay_ms, &sz, NULL, 0);
	assert_d_eq(err, 0, "Unexpected mallctl failure");
	expect_zd_eq(
	    dirty_decay_ms, 1234, "malloc_conf_2 setting didn't take effect");
}
TEST_END

TEST_BEGIN(test_mallctl_global_var) {
#ifdef _WIN32
	bool windows = true;
#else
	bool windows = false;
#endif
	/* Windows doesn't support weak symbol linker trickery. */
	test_skip_if(windows);

	const char *mc;
	size_t      sz = sizeof(mc);
	expect_d_eq(
	    mallctl("opt.malloc_conf.global_var", (void *)&mc, &sz, NULL, 0), 0,
	    "Unexpected mallctl() failure");
	expect_str_eq(mc, malloc_conf,
	    "Unexpected value for the global variable "
	    "malloc_conf");

	expect_d_eq(mallctl("opt.malloc_conf.global_var_2_conf_harder",
	                (void *)&mc, &sz, NULL, 0),
	    0, "Unexpected mallctl() failure");
	expect_str_eq(mc, malloc_conf_2_conf_harder,
	    "Unexpected value for the "
	    "global variable malloc_conf_2_conf_harder");
}
TEST_END

TEST_BEGIN(test_non_conflicting_var) {
#ifdef _WIN32
	bool windows = true;
#else
	bool windows = false;
#endif
	/* Windows doesn't support weak symbol linker trickery. */
	test_skip_if(windows);

	ssize_t muzzy_decay_ms;
	size_t  sz = sizeof(muzzy_decay_ms);

	int err = mallctl("opt.muzzy_decay_ms", &muzzy_decay_ms, &sz, NULL, 0);
	assert_d_eq(err, 0, "Unexpected mallctl failure");
	expect_zd_eq(muzzy_decay_ms, 2000,
	    "Non-conflicting option from malloc_conf should pass through");
}
TEST_END

int
main(void) {
	return test(test_malloc_conf_2, test_mallctl_global_var,
	    test_non_conflicting_var);
}

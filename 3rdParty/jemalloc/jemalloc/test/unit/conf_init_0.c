#include "test/jemalloc_test.h"

TEST_BEGIN(test_default_dirty_decay_ms) {
#ifdef _WIN32
	test_skip("not supported on win32");
#endif

	ssize_t dirty_decay_ms;
	size_t sz = sizeof(dirty_decay_ms);

	int err = mallctl("opt.dirty_decay_ms", &dirty_decay_ms, &sz, NULL, 0);
	assert_d_eq(err, 0, "Unexpected mallctl failure");
	expect_zd_eq(dirty_decay_ms, 10000,
	    "dirty_decay_ms should be the default (10000)"
	    " when no global variables are set");
}
TEST_END

int
main(void) {
	return test(test_default_dirty_decay_ms);
}

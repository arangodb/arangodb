#include "test/jemalloc_test.h"

const char *malloc_conf = "dirty_decay_ms:1234,confirm_conf:true";

TEST_BEGIN(test_confirm_conf_two_pass) {
#ifdef _WIN32
	test_skip("not supported on win32");
#endif

	bool confirm_conf;
	size_t sz = sizeof(confirm_conf);

	int err = mallctl("opt.confirm_conf", &confirm_conf, &sz, NULL, 0);
	assert_d_eq(err, 0, "Unexpected mallctl failure");
	expect_true(confirm_conf,
	    "confirm_conf should be true (processed in pass 1)");
}
TEST_END

TEST_BEGIN(test_conf_option_applied_in_second_pass) {
#ifdef _WIN32
	test_skip("not supported on win32");
#endif

	ssize_t dirty_decay_ms;
	size_t sz = sizeof(dirty_decay_ms);

	int err = mallctl("opt.dirty_decay_ms", &dirty_decay_ms, &sz, NULL, 0);
	assert_d_eq(err, 0, "Unexpected mallctl failure");
	expect_zd_eq(dirty_decay_ms, 1234,
	    "dirty_decay_ms should be 1234 (processed in pass 2)");
}
TEST_END

int
main(void) {
	return test(test_confirm_conf_two_pass,
	    test_conf_option_applied_in_second_pass);
}

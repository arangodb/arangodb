#include "test/jemalloc_test.h"

#include "jemalloc/internal/conf.h"

TEST_BEGIN(test_conf_handle_bool_true) {
	bool result = false;
	bool err = conf_handle_bool("true", sizeof("true") - 1, &result);
	expect_false(err, "conf_handle_bool should succeed for \"true\"");
	expect_true(result, "result should be true");
}
TEST_END

TEST_BEGIN(test_conf_handle_bool_false) {
	bool result = true;
	bool err = conf_handle_bool("false", sizeof("false") - 1, &result);
	expect_false(err, "conf_handle_bool should succeed for \"false\"");
	expect_false(result, "result should be false");
}
TEST_END

TEST_BEGIN(test_conf_handle_bool_invalid) {
	bool result = false;
	bool err = conf_handle_bool("yes", sizeof("yes") - 1, &result);
	expect_true(err, "conf_handle_bool should fail for \"yes\"");
}
TEST_END

TEST_BEGIN(test_conf_handle_signed_valid) {
	intmax_t result = 0;
	bool     err = conf_handle_signed("5000", sizeof("5000") - 1, -1,
	        INTMAX_MAX, true, false, false, &result);
	expect_false(err, "Should succeed for valid value");
	expect_d64_eq((int64_t)result, 5000, "result should be 5000");
}
TEST_END

TEST_BEGIN(test_conf_handle_signed_negative) {
	intmax_t result = 0;
	bool err = conf_handle_signed("-1", sizeof("-1") - 1, -1, INTMAX_MAX,
	    true, false, false, &result);
	expect_false(err, "Should succeed for -1");
	expect_d64_eq((int64_t)result, -1, "result should be -1");
}
TEST_END

TEST_BEGIN(test_conf_handle_signed_out_of_range) {
	intmax_t result = 0;
	bool     err = conf_handle_signed(
            "5000", sizeof("5000") - 1, -1, 4999, true, true, false, &result);
	expect_true(err, "Should fail for out-of-range value");
}
TEST_END

TEST_BEGIN(test_conf_handle_char_p) {
	char buf[8];
	bool err;

	/* Normal copy. */
	err = conf_handle_char_p(
	    "hello", sizeof("hello") - 1, buf, sizeof(buf));
	expect_false(err, "Should succeed");
	expect_str_eq(buf, "hello", "Should copy string");

	/* Truncation. */
	err = conf_handle_char_p(
	    "longstring", sizeof("longstring") - 1, buf, sizeof(buf));
	expect_false(err, "Should succeed even when truncating");
	expect_str_eq(buf, "longstr", "Should truncate to dest_sz - 1");
}
TEST_END

TEST_BEGIN(test_conf_handle_char_p_zero_dest_sz) {
	char buf[4] = {'X', 'Y', 'Z', '\0'};
	bool err;

	err = conf_handle_char_p("abc", sizeof("abc") - 1, buf, 0);
	expect_false(err, "Should succeed for zero-sized destination");
	expect_c_eq(buf[0], 'X', "Zero-sized destination must not be modified");
}
TEST_END

int
main(void) {
	return test(test_conf_handle_bool_true, test_conf_handle_bool_false,
	    test_conf_handle_bool_invalid, test_conf_handle_signed_valid,
	    test_conf_handle_signed_negative,
	    test_conf_handle_signed_out_of_range, test_conf_handle_char_p,
	    test_conf_handle_char_p_zero_dest_sz);
}

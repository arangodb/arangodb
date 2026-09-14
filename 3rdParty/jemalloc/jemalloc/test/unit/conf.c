#include "test/jemalloc_test.h"

#include "jemalloc/internal/conf.h"

TEST_BEGIN(test_conf_next_simple) {
	const char *opts = "key:value";
	const char *k;
	size_t      klen;
	const char *v;
	size_t      vlen;

	had_conf_error = false;
	bool end = conf_next(&opts, &k, &klen, &v, &vlen);
	expect_false(end, "Should not be at end");
	expect_zu_eq(klen, 3, "Key length should be 3");
	expect_false(strncmp(k, "key", klen), "Key should be \"key\"");
	expect_zu_eq(vlen, 5, "Value length should be 5");
	expect_false(strncmp(v, "value", vlen), "Value should be \"value\"");
	expect_false(had_conf_error, "Should not have had an error");
}
TEST_END

TEST_BEGIN(test_conf_next_multi) {
	const char *opts = "k1:v1,k2:v2";
	const char *k;
	size_t      klen;
	const char *v;
	size_t      vlen;
	bool        end;

	had_conf_error = false;

	end = conf_next(&opts, &k, &klen, &v, &vlen);
	expect_false(end, "Should not be at end after first pair");
	expect_zu_eq(klen, 2, "First key length should be 2");
	expect_false(strncmp(k, "k1", klen), "First key should be \"k1\"");
	expect_zu_eq(vlen, 2, "First value length should be 2");
	expect_false(strncmp(v, "v1", vlen), "First value should be \"v1\"");

	end = conf_next(&opts, &k, &klen, &v, &vlen);
	expect_false(end, "Should not be at end after second pair");
	expect_zu_eq(klen, 2, "Second key length should be 2");
	expect_false(strncmp(k, "k2", klen), "Second key should be \"k2\"");
	expect_zu_eq(vlen, 2, "Second value length should be 2");
	expect_false(strncmp(v, "v2", vlen), "Second value should be \"v2\"");

	expect_false(had_conf_error, "Should not have had an error");
}
TEST_END

TEST_BEGIN(test_conf_next_empty) {
	const char *opts = "";
	const char *k;
	size_t      klen;
	const char *v;
	size_t      vlen;

	had_conf_error = false;
	bool end = conf_next(&opts, &k, &klen, &v, &vlen);
	expect_true(end, "Empty string should return true (end)");
	expect_false(had_conf_error, "Empty string should not set error");
}
TEST_END

TEST_BEGIN(test_conf_next_missing_value) {
	const char *opts = "key_only";
	const char *k;
	size_t      klen;
	const char *v;
	size_t      vlen;

	had_conf_error = false;
	bool end = conf_next(&opts, &k, &klen, &v, &vlen);
	expect_true(end, "Key without value should return true (end)");
	expect_true(had_conf_error, "Key without value should set error");
}
TEST_END

TEST_BEGIN(test_conf_next_malformed) {
	const char *opts = "bad!key:val";
	const char *k;
	size_t      klen;
	const char *v;
	size_t      vlen;

	had_conf_error = false;
	bool end = conf_next(&opts, &k, &klen, &v, &vlen);
	expect_true(end, "Malformed key should return true (end)");
	expect_true(had_conf_error, "Malformed key should set error");
}
TEST_END

TEST_BEGIN(test_conf_next_trailing_comma) {
	const char *opts = "k:v,";
	const char *k;
	size_t      klen;
	const char *v;
	size_t      vlen;

	had_conf_error = false;
	bool end = conf_next(&opts, &k, &klen, &v, &vlen);
	expect_false(end, "Should parse the first pair successfully");
	expect_true(had_conf_error,
	    "Trailing comma should set error");
}
TEST_END

int
main(void) {
	return test(test_conf_next_simple, test_conf_next_multi,
	    test_conf_next_empty, test_conf_next_missing_value,
	    test_conf_next_malformed, test_conf_next_trailing_comma);
}

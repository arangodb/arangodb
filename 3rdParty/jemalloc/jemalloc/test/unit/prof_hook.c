#include "test/jemalloc_test.h"

/*
 * The MALLOC_CONF of this test has lg_prof_sample:0, meaning that every single
 * allocation will be sampled (and trigger relevant hooks).
 */

const char *dump_filename = "/dev/null";

prof_backtrace_hook_t default_bt_hook;

bool mock_bt_hook_called = false;
bool mock_dump_hook_called = false;
bool mock_prof_sample_hook_called = false;
bool mock_prof_sample_free_hook_called = false;

void *sampled_ptr = NULL;
size_t sampled_ptr_sz = 0;
void *free_sampled_ptr = NULL;
size_t free_sampled_ptr_sz = 0;

void
mock_bt_hook(void **vec, unsigned *len, unsigned max_len) {
	*len = max_len;
	for (unsigned i = 0; i < max_len; ++i) {
		vec[i] = (void *)((uintptr_t)i);
	}
	mock_bt_hook_called = true;
}

void
mock_bt_augmenting_hook(void **vec, unsigned *len, unsigned max_len) {
	default_bt_hook(vec, len, max_len);
	expect_u_gt(*len, 0, "Default backtrace hook returned empty backtrace");
	expect_u_lt(*len, max_len,
	    "Default backtrace hook returned too large backtrace");

	/* Add a separator between default frames and augmented */
	vec[*len] = (void *)0x030303030;
	(*len)++;

	/* Add more stack frames */
	for (unsigned i = 0; i < 3; ++i) {
		if (*len == max_len) {
			break;
		}
		vec[*len] = (void *)((uintptr_t)i);
		(*len)++;
	}


	mock_bt_hook_called = true;
}

void
mock_dump_hook(const char *filename) {
	mock_dump_hook_called = true;
	expect_str_eq(filename, dump_filename,
	    "Incorrect file name passed to the dump hook");
}

void
mock_prof_sample_hook(const void *ptr, size_t sz, void **vec, unsigned len) {
	mock_prof_sample_hook_called = true;
	sampled_ptr = (void *)ptr;
	sampled_ptr_sz = sz;
	for (unsigned i = 0; i < len; i++) {
		expect_ptr_not_null((void **)vec[i],
		    "Backtrace should not contain NULL");
	}
}

void
mock_prof_sample_free_hook(const void *ptr, size_t sz) {
	mock_prof_sample_free_hook_called = true;
	free_sampled_ptr = (void *)ptr;
	free_sampled_ptr_sz = sz;
}

TEST_BEGIN(test_prof_backtrace_hook_replace) {

	test_skip_if(!config_prof);

	mock_bt_hook_called = false;

	void *p0 = mallocx(1, 0);
	assert_ptr_not_null(p0, "Failed to allocate");

	expect_false(mock_bt_hook_called, "Called mock hook before it's set");

	prof_backtrace_hook_t null_hook = NULL;
	expect_d_eq(mallctl("experimental.hooks.prof_backtrace",
	    NULL, 0, (void *)&null_hook,  sizeof(null_hook)),
		EINVAL, "Incorrectly allowed NULL backtrace hook");

	size_t default_bt_hook_sz = sizeof(prof_backtrace_hook_t);
	prof_backtrace_hook_t hook = &mock_bt_hook;
	expect_d_eq(mallctl("experimental.hooks.prof_backtrace",
	    (void *)&default_bt_hook, &default_bt_hook_sz, (void *)&hook,
	    sizeof(hook)), 0, "Unexpected mallctl failure setting hook");

	void *p1 = mallocx(1, 0);
	assert_ptr_not_null(p1, "Failed to allocate");

	expect_true(mock_bt_hook_called, "Didn't call mock hook");

	prof_backtrace_hook_t current_hook;
	size_t current_hook_sz = sizeof(prof_backtrace_hook_t);
	expect_d_eq(mallctl("experimental.hooks.prof_backtrace",
	    (void *)&current_hook, &current_hook_sz, (void *)&default_bt_hook,
	    sizeof(default_bt_hook)), 0,
	    "Unexpected mallctl failure resetting hook to default");

	expect_ptr_eq(current_hook, hook,
	    "Hook returned by mallctl is not equal to mock hook");

	dallocx(p1, 0);
	dallocx(p0, 0);
}
TEST_END

TEST_BEGIN(test_prof_backtrace_hook_augment) {

	test_skip_if(!config_prof);

	mock_bt_hook_called = false;

	void *p0 = mallocx(1, 0);
	assert_ptr_not_null(p0, "Failed to allocate");

	expect_false(mock_bt_hook_called, "Called mock hook before it's set");

	size_t default_bt_hook_sz = sizeof(prof_backtrace_hook_t);
	prof_backtrace_hook_t hook = &mock_bt_augmenting_hook;
	expect_d_eq(mallctl("experimental.hooks.prof_backtrace",
	    (void *)&default_bt_hook, &default_bt_hook_sz, (void *)&hook,
	    sizeof(hook)), 0, "Unexpected mallctl failure setting hook");

	void *p1 = mallocx(1, 0);
	assert_ptr_not_null(p1, "Failed to allocate");

	expect_true(mock_bt_hook_called, "Didn't call mock hook");

	prof_backtrace_hook_t current_hook;
	size_t current_hook_sz = sizeof(prof_backtrace_hook_t);
	expect_d_eq(mallctl("experimental.hooks.prof_backtrace",
	    (void *)&current_hook, &current_hook_sz, (void *)&default_bt_hook,
	    sizeof(default_bt_hook)), 0,
	    "Unexpected mallctl failure resetting hook to default");

	expect_ptr_eq(current_hook, hook,
	    "Hook returned by mallctl is not equal to mock hook");

	dallocx(p1, 0);
	dallocx(p0, 0);
}
TEST_END

TEST_BEGIN(test_prof_dump_hook) {

	test_skip_if(!config_prof);
	expect_u_eq(opt_prof_bt_max, 200, "Unexpected backtrace stack depth");

	mock_dump_hook_called = false;

	expect_d_eq(mallctl("prof.dump", NULL, NULL, (void *)&dump_filename,
	    sizeof(dump_filename)), 0, "Failed to dump heap profile");

	expect_false(mock_dump_hook_called, "Called dump hook before it's set");

	size_t default_bt_hook_sz = sizeof(prof_dump_hook_t);
	prof_dump_hook_t hook = &mock_dump_hook;
	expect_d_eq(mallctl("experimental.hooks.prof_dump",
	    (void *)&default_bt_hook, &default_bt_hook_sz, (void *)&hook,
	    sizeof(hook)), 0, "Unexpected mallctl failure setting hook");

	expect_d_eq(mallctl("prof.dump", NULL, NULL, (void *)&dump_filename,
	    sizeof(dump_filename)), 0, "Failed to dump heap profile");

	expect_true(mock_dump_hook_called, "Didn't call mock hook");

	prof_dump_hook_t current_hook;
	size_t current_hook_sz = sizeof(prof_dump_hook_t);
	expect_d_eq(mallctl("experimental.hooks.prof_dump",
	    (void *)&current_hook, &current_hook_sz, (void *)&default_bt_hook,
	    sizeof(default_bt_hook)), 0,
	    "Unexpected mallctl failure resetting hook to default");

	expect_ptr_eq(current_hook, hook,
	    "Hook returned by mallctl is not equal to mock hook");
}
TEST_END

/* Need the do_write flag because NULL is a valid to_write value. */
static void
read_write_prof_sample_hook(prof_sample_hook_t *to_read, bool do_write,
    prof_sample_hook_t to_write) {
	size_t hook_sz = sizeof(prof_sample_hook_t);
	expect_d_eq(mallctl("experimental.hooks.prof_sample",
	    (void *)to_read, &hook_sz, do_write ? &to_write : NULL, hook_sz), 0,
	    "Unexpected prof_sample_hook mallctl failure");
}

static void
write_prof_sample_hook(prof_sample_hook_t new_hook) {
	read_write_prof_sample_hook(NULL, true, new_hook);
}

static prof_sample_hook_t
read_prof_sample_hook(void) {
	prof_sample_hook_t curr_hook;
	read_write_prof_sample_hook(&curr_hook, false, NULL);

	return curr_hook;
}

static void
read_write_prof_sample_free_hook(prof_sample_free_hook_t *to_read,
    bool do_write, prof_sample_free_hook_t to_write) {
	size_t hook_sz = sizeof(prof_sample_free_hook_t);
	expect_d_eq(mallctl("experimental.hooks.prof_sample_free",
	    (void *)to_read, &hook_sz, do_write ? &to_write : NULL, hook_sz), 0,
	    "Unexpected prof_sample_free_hook mallctl failure");
}

static void
write_prof_sample_free_hook(prof_sample_free_hook_t new_hook) {
	read_write_prof_sample_free_hook(NULL, true, new_hook);
}

static prof_sample_free_hook_t
read_prof_sample_free_hook(void) {
	prof_sample_free_hook_t curr_hook;
	read_write_prof_sample_free_hook(&curr_hook, false, NULL);

	return curr_hook;
}

static void
check_prof_sample_hooks(bool sample_hook_set, bool sample_free_hook_set) {
	expect_false(mock_prof_sample_hook_called,
	    "Should not have called prof_sample hook");
	expect_false(mock_prof_sample_free_hook_called,
	    "Should not have called prof_sample_free hook");
	expect_ptr_null(sampled_ptr, "Unexpected sampled ptr");
	expect_zu_eq(sampled_ptr_sz, 0, "Unexpected sampled ptr size");
	expect_ptr_null(free_sampled_ptr, "Unexpected free sampled ptr");
	expect_zu_eq(free_sampled_ptr_sz, 0,
	    "Unexpected free sampled ptr size");

	prof_sample_hook_t curr_hook = read_prof_sample_hook();
	expect_ptr_eq(curr_hook, sample_hook_set ? mock_prof_sample_hook : NULL,
	    "Unexpected non NULL default hook");

	prof_sample_free_hook_t curr_free_hook = read_prof_sample_free_hook();
	expect_ptr_eq(curr_free_hook, sample_free_hook_set ?
	    mock_prof_sample_free_hook : NULL,
	    "Unexpected non NULL default hook");

	size_t alloc_sz = 10;
	void *p = mallocx(alloc_sz, 0);
	expect_ptr_not_null(p, "Failed to allocate");
	expect_true(mock_prof_sample_hook_called == sample_hook_set,
	   "Incorrect prof_sample hook usage");
	if (sample_hook_set) {
		expect_ptr_eq(p, sampled_ptr, "Unexpected sampled ptr");
		expect_zu_eq(alloc_sz, sampled_ptr_sz,
		    "Unexpected sampled usize");
	}

	dallocx(p, 0);
	expect_true(mock_prof_sample_free_hook_called == sample_free_hook_set,
	   "Incorrect prof_sample_free hook usage");
	if (sample_free_hook_set) {
		size_t usz = sz_s2u(alloc_sz);
		expect_ptr_eq(p, free_sampled_ptr, "Unexpected sampled ptr");
		expect_zu_eq(usz, free_sampled_ptr_sz, "Unexpected sampled usize");
	}

	sampled_ptr = free_sampled_ptr = NULL;
	sampled_ptr_sz = free_sampled_ptr_sz = 0;
	mock_prof_sample_hook_called = false;
	mock_prof_sample_free_hook_called = false;
}

TEST_BEGIN(test_prof_sample_hooks) {
	test_skip_if(!config_prof);

	check_prof_sample_hooks(false, false);

	write_prof_sample_hook(mock_prof_sample_hook);
	check_prof_sample_hooks(true, false);

	write_prof_sample_free_hook(mock_prof_sample_free_hook);
	check_prof_sample_hooks(true, true);

	write_prof_sample_hook(NULL);
	check_prof_sample_hooks(false, true);

	write_prof_sample_free_hook(NULL);
	check_prof_sample_hooks(false, false);

	/* Test read+write together. */
	prof_sample_hook_t sample_hook;
	read_write_prof_sample_hook(&sample_hook, true, mock_prof_sample_hook);
	expect_ptr_null(sample_hook, "Unexpected non NULL default hook");
	check_prof_sample_hooks(true, false);

	prof_sample_free_hook_t sample_free_hook;
	read_write_prof_sample_free_hook(&sample_free_hook, true,
	    mock_prof_sample_free_hook);
	expect_ptr_null(sample_free_hook, "Unexpected non NULL default hook");
	check_prof_sample_hooks(true, true);

	read_write_prof_sample_hook(&sample_hook, true, NULL);
	expect_ptr_eq(sample_hook, mock_prof_sample_hook,
	    "Unexpected prof_sample hook");
	check_prof_sample_hooks(false, true);

	read_write_prof_sample_free_hook(&sample_free_hook, true, NULL);
	expect_ptr_eq(sample_free_hook, mock_prof_sample_free_hook,
	    "Unexpected prof_sample_free hook");
	check_prof_sample_hooks(false, false);
}
TEST_END

int
main(void) {
	return test(
	    test_prof_backtrace_hook_replace,
	    test_prof_backtrace_hook_augment,
	    test_prof_dump_hook,
	    test_prof_sample_hooks);
}

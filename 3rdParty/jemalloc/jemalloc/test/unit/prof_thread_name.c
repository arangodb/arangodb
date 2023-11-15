#include "test/jemalloc_test.h"

static void
mallctl_thread_name_get_impl(const char *thread_name_expected, const char *func,
    int line) {
	const char *thread_name_old;
	size_t sz;

	sz = sizeof(thread_name_old);
	expect_d_eq(mallctl("thread.prof.name", (void *)&thread_name_old, &sz,
	    NULL, 0), 0,
	    "%s():%d: Unexpected mallctl failure reading thread.prof.name",
	    func, line);
	expect_str_eq(thread_name_old, thread_name_expected,
	    "%s():%d: Unexpected thread.prof.name value", func, line);
}

static void
mallctl_thread_name_set_impl(const char *thread_name, const char *func,
    int line) {
	expect_d_eq(mallctl("thread.prof.name", NULL, NULL,
	    (void *)&thread_name, sizeof(thread_name)), 0,
	    "%s():%d: Unexpected mallctl failure writing thread.prof.name",
	    func, line);
	mallctl_thread_name_get_impl(thread_name, func, line);
}

#define mallctl_thread_name_get(a)					\
	mallctl_thread_name_get_impl(a, __func__, __LINE__)

#define mallctl_thread_name_set(a)					\
	mallctl_thread_name_set_impl(a, __func__, __LINE__)

TEST_BEGIN(test_prof_thread_name_validation) {
	test_skip_if(!config_prof);
	test_skip_if(opt_prof_sys_thread_name);

	mallctl_thread_name_get("");

	const char *test_name1 = "test case1";
	mallctl_thread_name_set(test_name1);

	/* Test name longer than the max len. */
	char long_name[] =
	    "test case longer than expected; test case longer than expected";
	expect_zu_gt(strlen(long_name), PROF_THREAD_NAME_MAX_LEN,
	   "Long test name not long enough");
	const char *test_name_long = long_name;
	expect_d_eq(mallctl("thread.prof.name", NULL, NULL,
	    (void *)&test_name_long, sizeof(test_name_long)), 0,
	    "Unexpected mallctl failure from thread.prof.name");
	/* Long name cut to match. */
	long_name[PROF_THREAD_NAME_MAX_LEN - 1] = '\0';
	mallctl_thread_name_get(test_name_long);

	/* NULL input shouldn't be allowed. */
	const char *test_name2 = NULL;
	expect_d_eq(mallctl("thread.prof.name", NULL, NULL,
	    (void *)&test_name2, sizeof(test_name2)), EINVAL,
	    "Unexpected mallctl result writing to thread.prof.name");

	/* '\n' shouldn't be allowed. */
	const char *test_name3 = "test\ncase";
	expect_d_eq(mallctl("thread.prof.name", NULL, NULL,
	    (void *)&test_name3, sizeof(test_name3)), EINVAL,
	    "Unexpected mallctl result writing \"%s\" to thread.prof.name",
	    test_name3);

	/* Simultaneous read/write shouldn't be allowed. */
	const char *thread_name_old;
	size_t sz = sizeof(thread_name_old);
	expect_d_eq(mallctl("thread.prof.name", (void *)&thread_name_old, &sz,
	    (void *)&test_name1, sizeof(test_name1)), EPERM,
	    "Unexpected mallctl result from thread.prof.name");

	mallctl_thread_name_set("");
}
TEST_END

static void *
thd_start(void *varg) {
	unsigned thd_ind = *(unsigned *)varg;
	char thread_name[16] = "";
	unsigned i;

	malloc_snprintf(thread_name, sizeof(thread_name), "thread %u", thd_ind);

	mallctl_thread_name_get("");
	mallctl_thread_name_set(thread_name);

#define NRESET 25
	for (i = 0; i < NRESET; i++) {
		expect_d_eq(mallctl("prof.reset", NULL, NULL, NULL, 0), 0,
		    "Unexpected error while resetting heap profile data");
		mallctl_thread_name_get(thread_name);
	}

	mallctl_thread_name_set(thread_name);
	mallctl_thread_name_set("");

	return NULL;
#undef NRESET
}

TEST_BEGIN(test_prof_thread_name_threaded) {
	test_skip_if(!config_prof);
	test_skip_if(opt_prof_sys_thread_name);

#define NTHREADS 4
	thd_t thds[NTHREADS];
	unsigned thd_args[NTHREADS];
	unsigned i;

	for (i = 0; i < NTHREADS; i++) {
		thd_args[i] = i;
		thd_create(&thds[i], thd_start, (void *)&thd_args[i]);
	}
	for (i = 0; i < NTHREADS; i++) {
		thd_join(thds[i], NULL);
	}
#undef NTHREADS
}
TEST_END

int
main(void) {
	return test(
	    test_prof_thread_name_validation,
	    test_prof_thread_name_threaded);
}

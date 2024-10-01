#include "test/jemalloc_test.h"

#include "jemalloc/internal/prof_sys.h"

static const char *test_thread_name = "test_name";
static const char *dump_filename = "/dev/null";

static int
test_prof_sys_thread_name_read_error(char *buf, size_t limit) {
	return ENOSYS;
}

static int
test_prof_sys_thread_name_read(char *buf, size_t limit) {
	assert(strlen(test_thread_name) < limit);
	strncpy(buf, test_thread_name, limit);
	return 0;
}

static int
test_prof_sys_thread_name_read_clear(char *buf, size_t limit) {
	assert(limit > 0);
	buf[0] = '\0';
	return 0;
}

TEST_BEGIN(test_prof_sys_thread_name) {
	test_skip_if(!config_prof);
	test_skip_if(!opt_prof_sys_thread_name);

	bool oldval;
	size_t sz = sizeof(oldval);
	assert_d_eq(mallctl("opt.prof_sys_thread_name", &oldval, &sz, NULL, 0),
	    0, "mallctl failed");
	assert_true(oldval, "option was not set correctly");

	const char *thread_name;
	sz = sizeof(thread_name);
	assert_d_eq(mallctl("thread.prof.name", &thread_name, &sz, NULL, 0), 0,
	    "mallctl read for thread name should not fail");
	expect_str_eq(thread_name, "", "Initial thread name should be empty");

	thread_name = test_thread_name;
	assert_d_eq(mallctl("thread.prof.name", NULL, NULL, &thread_name, sz),
	    ENOENT, "mallctl write for thread name should fail");
	assert_ptr_eq(thread_name, test_thread_name,
	    "Thread name should not be touched");

	prof_sys_thread_name_read_t *orig_prof_sys_thread_name_read =
	    prof_sys_thread_name_read;
	prof_sys_thread_name_read = test_prof_sys_thread_name_read_error;
	void *p = malloc(1);
	free(p);
	assert_d_eq(mallctl("thread.prof.name", &thread_name, &sz, NULL, 0), 0,
	    "mallctl read for thread name should not fail");
	assert_str_eq(thread_name, "",
	    "Thread name should stay the same if the system call fails");

	prof_sys_thread_name_read = test_prof_sys_thread_name_read;
	p = malloc(1);
	free(p);
	assert_d_eq(mallctl("thread.prof.name", &thread_name, &sz, NULL, 0), 0,
	    "mallctl read for thread name should not fail");
	assert_str_eq(thread_name, test_thread_name,
	    "Thread name should be changed if the system call succeeds");

	prof_sys_thread_name_read = test_prof_sys_thread_name_read_clear;
	p = malloc(1);
	free(p);
	assert_d_eq(mallctl("thread.prof.name", &thread_name, &sz, NULL, 0), 0,
	    "mallctl read for thread name should not fail");
	expect_str_eq(thread_name, "", "Thread name should be updated if the "
	    "system call returns a different name");

	prof_sys_thread_name_read = orig_prof_sys_thread_name_read;
}
TEST_END

#define ITER (16*1024)
static void *
thd_start(void *unused) {
	/* Triggering samples which loads thread names. */
	for (unsigned i = 0; i < ITER; i++) {
		void *p = mallocx(4096, 0);
		assert_ptr_not_null(p, "Unexpected mallocx() failure");
		dallocx(p, 0);
	}

	return NULL;
}

TEST_BEGIN(test_prof_sys_thread_name_mt) {
	test_skip_if(!config_prof);
	test_skip_if(!opt_prof_sys_thread_name);

#define NTHREADS 4
	thd_t thds[NTHREADS];
	unsigned thd_args[NTHREADS];
	unsigned i;

	for (i = 0; i < NTHREADS; i++) {
		thd_args[i] = i;
		thd_create(&thds[i], thd_start, (void *)&thd_args[i]);
	}
	/* Prof dump which reads the thread names. */
	for (i = 0; i < ITER; i++) {
		expect_d_eq(mallctl("prof.dump", NULL, NULL,
		    (void *)&dump_filename, sizeof(dump_filename)), 0,
		    "Unexpected mallctl failure while dumping");
	}

	for (i = 0; i < NTHREADS; i++) {
		thd_join(thds[i], NULL);
	}
}
#undef NTHREADS
#undef ITER
TEST_END

int
main(void) {
	return test(
	    test_prof_sys_thread_name,
	    test_prof_sys_thread_name_mt);
}

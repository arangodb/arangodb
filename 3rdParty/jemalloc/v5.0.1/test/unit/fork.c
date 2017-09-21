#include "test/jemalloc_test.h"

#ifndef _WIN32
#include <sys/wait.h>
#endif

TEST_BEGIN(test_fork) {
#ifndef _WIN32
	void *p;
	pid_t pid;

	/* Set up a manually managed arena for test. */
	unsigned arena_ind;
	size_t sz = sizeof(unsigned);
	assert_d_eq(mallctl("arenas.create", (void *)&arena_ind, &sz, NULL, 0),
	    0, "Unexpected mallctl() failure");

	/* Migrate to the new arena. */
	unsigned old_arena_ind;
	sz = sizeof(old_arena_ind);
	assert_d_eq(mallctl("thread.arena", (void *)&old_arena_ind, &sz,
	    (void *)&arena_ind, sizeof(arena_ind)), 0,
	    "Unexpected mallctl() failure");

	p = malloc(1);
	assert_ptr_not_null(p, "Unexpected malloc() failure");

	pid = fork();

	free(p);

	p = malloc(64);
	assert_ptr_not_null(p, "Unexpected malloc() failure");
	free(p);

	if (pid == -1) {
		/* Error. */
		test_fail("Unexpected fork() failure");
	} else if (pid == 0) {
		/* Child. */
		_exit(0);
	} else {
		int status;

		/* Parent. */
		while (true) {
			if (waitpid(pid, &status, 0) == -1) {
				test_fail("Unexpected waitpid() failure");
			}
			if (WIFSIGNALED(status)) {
				test_fail("Unexpected child termination due to "
				    "signal %d", WTERMSIG(status));
				break;
			}
			if (WIFEXITED(status)) {
				if (WEXITSTATUS(status) != 0) {
					test_fail(
					    "Unexpected child exit value %d",
					    WEXITSTATUS(status));
				}
				break;
			}
		}
	}
#else
	test_skip("fork(2) is irrelevant to Windows");
#endif
}
TEST_END

int
main(void) {
	return test(
	    test_fork);
}

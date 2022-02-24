#include <stdio.h>

/*
 * We can't test C++ in unit tests, and we can't change the safety check failure
 * hook in integration tests.  So we check that we *actually* abort on failure,
 * by forking and checking the child process exit code.
 */

/* It's a unix system? */
#ifdef __unix__
/* I know this! */
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
static const bool can_fork = true;
#else
static const bool can_fork = false;
#endif

#include "test/jemalloc_test.h"

TEST_BEGIN(test_failing_alloc) {
	test_skip_if(!can_fork);
#ifdef __unix__
	pid_t pid = fork();
	expect_d_ne(pid, -1, "Unexpected fork failure");
	if (pid == 0) {
		/*
		 * In the child, we'll print an error message to stderr before
		 * exiting.  Close stderr to avoid spamming output for this
		 * expected failure.
		 */
		fclose(stderr);
		try {
			/* Too big of an allocation to succeed. */
			void *volatile ptr = ::operator new((size_t)-1);
			(void)ptr;
		} catch (...) {
			/*
			 * Swallow the exception; remember, we expect this to
			 * fail via an abort within new, not because an
			 * exception didn't get caught.
			 */
		}
	} else {
		int status;
		pid_t err = waitpid(pid, &status, 0);
		expect_d_ne(-1, err, "waitpid failure");
		expect_false(WIFEXITED(status),
		    "Should have seen an abnormal failure");
	}
#endif
}
TEST_END

int
main(void) {
	return test(
	    test_failing_alloc);
}


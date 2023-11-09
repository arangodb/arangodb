#include "test/jemalloc_test.h"

static bool abort_called = false;
static void (*default_malloc_message)(void *, const char *);

static void
mock_invalid_conf_abort(void) {
	abort_called = true;
}

static void
null_malloc_message(void *_1, const char* _2) {
}

TEST_BEGIN(test_hpa_validate_conf) {
	test_skip_if(!hpa_supported());
	void *ptr = malloc(4096);
	/* Need to restore this here to see any possible assert messages */
	malloc_message = default_malloc_message;
	assert_true(abort_called,
	     "Should have aborted due to invalid values for hpa_dirty_mult and "
	     "hpa_hugification_threshold_ratio");
	free(ptr);
}
TEST_END

/*
 * We have to set `abort_conf:true` here and not via the `MALLOC_CONF`
 * environment variable in the associated shell script for this test. This is
 * because when testing on FreeBSD (where Jemalloc is the system allocator) in
 * CI configs where HPA is not supported, setting `abort_conf:true` there would
 * result in the system Jemalloc picking this up and aborting before we could
 * ever even launch the test.
 */
const char *malloc_conf = "abort_conf:true";

int
main(void) {
	/*
	 * OK, this is a sort of nasty hack.  We don't want to add *another*
	 * config option for HPA (the intent is that it becomes available on
	 * more platforms over time, and we're trying to prune back config
	 * options generally.  But we'll get initialization errors on other
	 * platforms if we set hpa:true in the MALLOC_CONF (even if we set
	 * abort_conf:false as well).  So we reach into the internals and set
	 * them directly, but only if we know that we're actually going to do
	 * something nontrivial in the tests.
	 */
	if (hpa_supported()) {
		default_malloc_message = malloc_message;
		malloc_message = null_malloc_message;
		opt_hpa = true;
		invalid_conf_abort = mock_invalid_conf_abort;
	}
	return test_no_reentrancy(test_hpa_validate_conf);
}

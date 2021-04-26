#include "test/jemalloc_test.h"

#include "jemalloc/internal/decay.h"

/*
 * Honestly, this is mostly a stub for now.  Eventually, we should beef up
 * testing here.
 */

TEST_BEGIN(test_decay_empty) {
	/* If we never have any decaying pages, npages_limit should be 0. */
	decay_t decay;
	memset(&decay, 0, sizeof(decay));

	nstime_t curtime;
	nstime_init(&curtime, 0);

	uint64_t decay_ms = 1000;
	uint64_t decay_ns = decay_ms * 1000 * 1000;

	bool err = decay_init(&decay, &curtime, (ssize_t)decay_ms);
	assert_false(err, "");

	uint64_t time_between_calls = decay_epoch_duration_ns(&decay) / 5;
	int nepochs = 0;
	for (uint64_t i = 0; i < decay_ns / time_between_calls * 10; i++) {
		size_t dirty_pages = 0;
		nstime_init(&curtime, i * time_between_calls);
		bool epoch_advanced = decay_maybe_advance_epoch(&decay,
		    &curtime, dirty_pages);
		if (epoch_advanced) {
			nepochs++;
			assert_zu_eq(decay_npages_limit_get(&decay), 0,
			    "Should not increase the limit arbitrarily");
		}
	}
	assert_d_gt(nepochs, 0, "Should have advanced epochs");
}
TEST_END

int
main(void) {
	return test(
	    test_decay_empty);
}

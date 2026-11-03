#include "test/jemalloc_test.h"

static uint32_t nuser_hook_calls;
static bool     is_registered = false;
static void
test_cb(bool is_alloc, uint64_t tallocated, uint64_t tdallocated) {
	++nuser_hook_calls;
}

static user_hook_object_t tobj = {
    .callback = &test_cb, .interval = 10, .is_alloc_only = false};

TEST_BEGIN(test_next_event_fast) {
	tsd_t   *tsd = tsd_fetch();
	te_ctx_t ctx;
	te_ctx_get(tsd, &ctx, true);

	te_ctx_last_event_set(&ctx, 0);
	te_ctx_current_bytes_set(&ctx, TE_NEXT_EVENT_FAST_MAX - 8U);
	te_ctx_next_event_set(tsd, &ctx, TE_NEXT_EVENT_FAST_MAX);

	if (!is_registered) {
		is_registered = 0
		    == te_register_user_handler(tsd_tsdn(tsd), &tobj);
	}
	assert_true(is_registered || !config_stats, "Register user handler");
	nuser_hook_calls = 0;

	uint64_t *waits = tsd_te_datap_get_unsafe(tsd)->alloc_wait;
	for (size_t i = 0; i < te_alloc_count; i++) {
		waits[i] = TE_NEXT_EVENT_FAST_MAX;
	}

	/* Test next_event_fast rolling back to 0. */
	void *p = malloc(16U);
	assert_true(
	    nuser_hook_calls == 1 || !config_stats, "Expected alloc call");
	assert_ptr_not_null(p, "malloc() failed");
	free(p);

	/* Test next_event_fast resuming to be equal to next_event. */
	void *q = malloc(SC_LOOKUP_MAXCLASS);
	assert_ptr_not_null(q, "malloc() failed");
	free(q);
}
TEST_END

int
main(void) {
	return test(test_next_event_fast);
}

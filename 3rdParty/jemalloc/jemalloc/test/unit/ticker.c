#include "test/jemalloc_test.h"

#include "jemalloc/internal/ticker.h"

TEST_BEGIN(test_ticker_tick) {
#define NREPS 2
#define NTICKS 3
	ticker_t ticker;
	int32_t i, j;

	ticker_init(&ticker, NTICKS);
	for (i = 0; i < NREPS; i++) {
		for (j = 0; j < NTICKS; j++) {
			expect_u_eq(ticker_read(&ticker), NTICKS - j,
			    "Unexpected ticker value (i=%d, j=%d)", i, j);
			expect_false(ticker_tick(&ticker, false),
			    "Unexpected ticker fire (i=%d, j=%d)", i, j);
		}
		expect_u32_eq(ticker_read(&ticker), 0,
		    "Expected ticker depletion");
		expect_true(ticker_tick(&ticker, false),
		    "Expected ticker fire (i=%d)", i);
		expect_u32_eq(ticker_read(&ticker), NTICKS,
		    "Expected ticker reset");
	}
#undef NTICKS
}
TEST_END

TEST_BEGIN(test_ticker_ticks) {
#define NTICKS 3
	ticker_t ticker;

	ticker_init(&ticker, NTICKS);

	expect_u_eq(ticker_read(&ticker), NTICKS, "Unexpected ticker value");
	expect_false(ticker_ticks(&ticker, NTICKS, false),
	    "Unexpected ticker fire");
	expect_u_eq(ticker_read(&ticker), 0, "Unexpected ticker value");
	expect_true(ticker_ticks(&ticker, NTICKS, false),
	    "Expected ticker fire");
	expect_u_eq(ticker_read(&ticker), NTICKS, "Unexpected ticker value");

	expect_true(ticker_ticks(&ticker, NTICKS + 1, false),
	    "Expected ticker fire");
	expect_u_eq(ticker_read(&ticker), NTICKS, "Unexpected ticker value");
#undef NTICKS
}
TEST_END

TEST_BEGIN(test_ticker_copy) {
#define NTICKS 3
	ticker_t ta, tb;

	ticker_init(&ta, NTICKS);
	ticker_copy(&tb, &ta);
	expect_u_eq(ticker_read(&tb), NTICKS, "Unexpected ticker value");
	expect_true(ticker_ticks(&tb, NTICKS + 1, false),
	    "Expected ticker fire");
	expect_u_eq(ticker_read(&tb), NTICKS, "Unexpected ticker value");

	ticker_tick(&ta, false);
	ticker_copy(&tb, &ta);
	expect_u_eq(ticker_read(&tb), NTICKS - 1, "Unexpected ticker value");
	expect_true(ticker_ticks(&tb, NTICKS, false), "Expected ticker fire");
	expect_u_eq(ticker_read(&tb), NTICKS, "Unexpected ticker value");
#undef NTICKS
}
TEST_END

TEST_BEGIN(test_ticker_geom) {
	const int32_t ticks = 100;
	const uint64_t niters = 100 * 1000;

	ticker_geom_t ticker;
	ticker_geom_init(&ticker, ticks);
	uint64_t total_ticks = 0;
	/* Just some random constant. */
	uint64_t prng_state = 0x343219f93496db9fULL;
	for (uint64_t i = 0; i < niters; i++) {
		while(!ticker_geom_tick(&ticker, &prng_state, false)) {
			total_ticks++;
		}
	}
	/*
	 * In fact, with this choice of random seed and the PRNG implementation
	 * used at the time this was tested, total_ticks is 95.1% of the
	 * expected ticks.
	 */
	expect_u64_ge(total_ticks , niters * ticks * 9 / 10,
	    "Mean off by > 10%%");
	expect_u64_le(total_ticks , niters * ticks * 11 / 10,
	    "Mean off by > 10%%");
}
TEST_END

TEST_BEGIN(test_ticker_delay) {
	const int32_t ticks = 1000;
	const uint64_t niters = 10000;

	ticker_t t1;
	ticker_init(&t1, ticks);

	ticker_geom_t t2;
	/* Just some random constant. */
	uint64_t prng_state = 0x43219f93496db9f3ULL;
	ticker_geom_init(&t2, ticks);

	bool delay = false;
	expect_false(ticker_ticks(&t1, ticks, delay), "Unexpected ticker fire");
	expect_false(ticker_geom_ticks(&t2, &prng_state, ticks, delay),
	    "Unexpected ticker fire");
	expect_d_eq(ticker_read(&t1), 0, "Unexpected ticker value");
	expect_d_eq(ticker_geom_read(&t2), 0, "Unexpected ticker value");

	delay = true;
	/* Not allowed to fire when delay is set to true. */
	for (unsigned i = 0; i < niters; i++) {
		expect_false(ticker_tick(&t1, delay), "Unexpected ticker fire");
		expect_false(ticker_geom_tick(&t2, &prng_state, delay),
		    "Unexpected ticker fire");
		expect_d_eq(ticker_read(&t1), 0, "Unexpected ticker value");
		expect_d_eq(ticker_geom_read(&t2), 0, "Unexpected ticker value");
	}

	delay = false;
	expect_true(ticker_tick(&t1, delay), "Expected ticker fire");
	expect_true(ticker_geom_tick(&t2, &prng_state, delay),
	    "Expected ticker fire");
}
TEST_END

int
main(void) {
	return test(
	    test_ticker_tick,
	    test_ticker_ticks,
	    test_ticker_copy,
	    test_ticker_geom,
	    test_ticker_delay);
}

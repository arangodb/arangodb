#include "test/jemalloc_test.h"

/*
 * Test to verify that background thread initialization has no race conditions.
 *
 * See https://github.com/facebook/jemalloc/pull/68
 */

#ifdef JEMALLOC_BACKGROUND_THREAD
const char *malloc_conf = "background_thread:true,percpu_arena:percpu";
#else
const char *malloc_conf = "";
#endif

#define N_INIT_THREADS 32
#define N_ITERATIONS 10

static mtx_t barrier_mtx;
static atomic_u32_t n_waiting;
static unsigned n_threads;
static atomic_b_t release;

/*
 * Simple spin barrier - all threads wait until everyone arrives,
 * then they all proceed to call malloc() simultaneously.
 */
static void
barrier_wait(void) {
	mtx_lock(&barrier_mtx);
	uint32_t waiting = atomic_load_u32(&n_waiting, ATOMIC_RELAXED) + 1;
	atomic_store_u32(&n_waiting, waiting, ATOMIC_RELAXED);
	bool should_release = (waiting == n_threads);
	mtx_unlock(&barrier_mtx);

	if (should_release) {
		atomic_store_b(&release, true, ATOMIC_RELEASE);
	}

	while (!atomic_load_b(&release, ATOMIC_ACQUIRE)) {
		/* Spin until released. */
	}
}

static void
barrier_reset(void) {
	atomic_store_u32(&n_waiting, 0, ATOMIC_RELAXED);
	atomic_store_b(&release, false, ATOMIC_RELAXED);
}

static void *
thd_start(void *arg) {
	barrier_wait();

	/*
	 * All threads race to malloc simultaneously.
	 * This triggers concurrent arena initialization with percpu_arena.
	 */
	void *p = malloc(64);
	expect_ptr_not_null(p, "malloc failed");
	free(p);

	return NULL;
}

TEST_BEGIN(test_mt_background_thread_init) {
	test_skip_if(!have_background_thread);
	test_skip_if(!have_percpu_arena ||
	    !PERCPU_ARENA_ENABLED(opt_percpu_arena));

	thd_t thds[N_INIT_THREADS];

	expect_false(mtx_init(&barrier_mtx), "mtx_init failed");
	n_threads = N_INIT_THREADS;
	barrier_reset();

	/* Create threads that will all race to call malloc(). */
	for (unsigned i = 0; i < N_INIT_THREADS; i++) {
		thd_create(&thds[i], thd_start, NULL);
	}

	/* Wait for all threads to complete. */
	for (unsigned i = 0; i < N_INIT_THREADS; i++) {
		thd_join(thds[i], NULL);
	}

	mtx_fini(&barrier_mtx);

	/*
	 * Verify background threads are properly running. Before the fix,
	 * the race could leave Thread 0 marked as "started" without an
	 * actual pthread behind it.
	 */
#ifdef JEMALLOC_BACKGROUND_THREAD
	tsd_t *tsd = tsd_fetch();
	background_thread_info_t *t0 = &background_thread_info[0];

	malloc_mutex_lock(tsd_tsdn(tsd), &t0->mtx);
	expect_d_eq(t0->state, background_thread_started,
	    "Thread 0 should be in started state");
	malloc_mutex_unlock(tsd_tsdn(tsd), &t0->mtx);

	expect_zu_gt(n_background_threads, 0,
	    "At least one background thread should be running");
#endif
}
TEST_END

TEST_BEGIN(test_mt_background_thread_init_stress) {
	test_skip_if(!have_background_thread);
	test_skip_if(!config_stats);

	thd_t thds[N_INIT_THREADS];

	expect_false(mtx_init(&barrier_mtx), "mtx_init failed");
	n_threads = N_INIT_THREADS;

	/*
	 * Run multiple iterations to increase the chance of hitting
	 * any race conditions. Each iteration creates new threads that
	 * perform allocations concurrently.
	 */
	for (unsigned iter = 0; iter < N_ITERATIONS; iter++) {
		barrier_reset();

		for (unsigned i = 0; i < N_INIT_THREADS; i++) {
			thd_create(&thds[i], thd_start, NULL);
		}

		for (unsigned i = 0; i < N_INIT_THREADS; i++) {
			thd_join(thds[i], NULL);
		}
	}

	mtx_fini(&barrier_mtx);

#ifdef JEMALLOC_BACKGROUND_THREAD
	/*
	 * Verify Thread 0 is actually running by checking it has done work.
	 * Wait up to a few seconds for the background thread to run.
	 */
	tsd_t *tsd = tsd_fetch();
	background_thread_info_t *t0 = &background_thread_info[0];

	nstime_t start;
	nstime_init_update(&start);

	bool ran = false;
	while (!ran) {
		malloc_mutex_lock(tsd_tsdn(tsd), &t0->mtx);
		if (t0->tot_n_runs > 0) {
			ran = true;
		}
		malloc_mutex_unlock(tsd_tsdn(tsd), &t0->mtx);

		if (ran) {
			break;
		}

		nstime_t now;
		nstime_init_update(&now);
		nstime_subtract(&now, &start);
		if (nstime_sec(&now) > 10) {
			/*
			 * If Thread 0 hasn't run after 10 seconds, it's
			 * likely not actually running (the bug condition).
			 */
			expect_true(false,
			    "Thread 0 did not run within 10 seconds - "
			    "possible initialization race");
			break;
		}
		sleep_ns(100 * 1000 * 1000); /* 100ms */
	}
#endif
}
TEST_END

int
main(void) {
	return test_no_reentrancy(
	    test_mt_background_thread_init,
	    test_mt_background_thread_init_stress);
}

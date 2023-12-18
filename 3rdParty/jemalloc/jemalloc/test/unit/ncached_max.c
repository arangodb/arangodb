#include "test/jemalloc_test.h"
#include "test/san.h"

const char *malloc_conf =
"tcache_ncached_max:256-1024:1001|2048-2048:0|8192-8192:1,tcache_max:4096";
extern void tcache_bin_info_compute(
    cache_bin_info_t tcache_bin_info[TCACHE_NBINS_MAX]);
extern bool tcache_get_default_ncached_max_set(szind_t ind);
extern const cache_bin_info_t *tcache_get_default_ncached_max(void);

static void
check_bins_info(cache_bin_info_t tcache_bin_info[TCACHE_NBINS_MAX]) {
	size_t mib_get[4], mib_get_len;
	mib_get_len = sizeof(mib_get) / sizeof(size_t);
	const char *get_name = "thread.tcache.ncached_max.read_sizeclass";
	size_t ncached_max;
	size_t sz = sizeof(size_t);
	expect_d_eq(mallctlnametomib(get_name, mib_get, &mib_get_len), 0,
	    "Unexpected mallctlnametomib() failure");

	for (szind_t i = 0; i < TCACHE_NBINS_MAX; i++) {
		size_t bin_size = sz_index2size(i);
		expect_d_eq(mallctlbymib(mib_get, mib_get_len,
		    (void *)&ncached_max, &sz,
		    (void *)&bin_size, sizeof(size_t)), 0,
		    "Unexpected mallctlbymib() failure");
		expect_zu_eq(ncached_max, tcache_bin_info[i].ncached_max,
		    "Unexpected ncached_max for bin %d", i);
		/* Check ncached_max returned under a non-bin size. */
		bin_size--;
		size_t temp_ncached_max = 0;
		expect_d_eq(mallctlbymib(mib_get, mib_get_len,
		    (void *)&temp_ncached_max, &sz,
		    (void *)&bin_size, sizeof(size_t)), 0,
		    "Unexpected mallctlbymib() failure");
		expect_zu_eq(temp_ncached_max, ncached_max,
		    "Unexpected ncached_max for inaccurate bin size.");
	}
}

static void *
ncached_max_check(void* args) {
	cache_bin_info_t tcache_bin_info[TCACHE_NBINS_MAX];
	cache_bin_info_t tcache_bin_info_backup[TCACHE_NBINS_MAX];
	tsd_t *tsd = tsd_fetch();
	tcache_t *tcache = tsd_tcachep_get(tsd);
	assert(tcache != NULL);
	tcache_slow_t *tcache_slow = tcache->tcache_slow;


	tcache_bin_info_compute(tcache_bin_info);
	memcpy(tcache_bin_info_backup, tcache_bin_info,
	    sizeof(tcache_bin_info));
	/* Check ncached_max set by malloc_conf. */
	for (szind_t i = 0; i < TCACHE_NBINS_MAX; i++) {
		bool first_range = (i >= sz_size2index(256) &&
		    i <= sz_size2index(1024));
		bool second_range = (i == sz_size2index(2048));
		bool third_range = (i == sz_size2index(8192));
		cache_bin_sz_t target_ncached_max = 0;
		if (first_range || second_range || third_range) {
			target_ncached_max = first_range ? 1001:
			    (second_range ? 0: 1);
			expect_true(tcache_get_default_ncached_max_set(i),
			    "Unexpected state for bin %u", i);
			expect_zu_eq(target_ncached_max,
			    tcache_bin_info[i].ncached_max,
			    "Unexpected generated ncached_max for bin %u", i);
			expect_zu_eq(target_ncached_max,
			    tcache_get_default_ncached_max()[i].ncached_max,
			    "Unexpected pre-set ncached_max for bin %u", i);
		} else {
			expect_false(tcache_get_default_ncached_max_set(i),
			    "Unexpected state for bin %u", i);
		}
	}
	unsigned nbins = tcache_nbins_get(tcache_slow);
	for (szind_t i = nbins; i < TCACHE_NBINS_MAX; i++) {
		cache_bin_info_init(&tcache_bin_info[i], 0);
	}
	/* Check the initial bin settings. */
	check_bins_info(tcache_bin_info);

	size_t mib_set[4], mib_set_len;
	mib_set_len = sizeof(mib_set) / sizeof(size_t);
	const char *set_name = "thread.tcache.ncached_max.write";
	expect_d_eq(mallctlnametomib(set_name, mib_set, &mib_set_len), 0,
	    "Unexpected mallctlnametomib() failure");

	/* Test the ncached_max set with tcache on. */
	char inputs[100] = "8-128:1|160-160:11|170-320:22|224-8388609:0";
	char *inputp = inputs;
	expect_d_eq(mallctlbymib(mib_set, mib_set_len, NULL, NULL,
	    (void *)&inputp, sizeof(char *)), 0,
	    "Unexpected mallctlbymib() failure");
	for (szind_t i = 0; i < TCACHE_NBINS_MAX; i++) {
		if (i >= sz_size2index(8) &&i <= sz_size2index(128)) {
			cache_bin_info_init(&tcache_bin_info[i], 1);
		}
		if (i == sz_size2index(160)) {
			cache_bin_info_init(&tcache_bin_info[i], 11);
		}
		if (i >= sz_size2index(170) && i <= sz_size2index(320)) {
			cache_bin_info_init(&tcache_bin_info[i], 22);
		}
		if (i >= sz_size2index(224)) {
			cache_bin_info_init(&tcache_bin_info[i], 0);
		}
		if (i >= nbins) {
			cache_bin_info_init(&tcache_bin_info[i], 0);
		}
	}
	check_bins_info(tcache_bin_info);

	/*
	 * Close the tcache and set ncached_max of some bins.  It will be
	 * set properly but thread.tcache.ncached_max.read still returns 0
	 * since the bin is not available yet.  After enabling the tcache,
	 * the new setting will not be carried on.  Instead, the default
	 * settings will be applied.
	 */
	bool e0 = false, e1;
	size_t bool_sz = sizeof(bool);
	expect_d_eq(mallctl("thread.tcache.enabled", (void *)&e1, &bool_sz,
	    (void *)&e0, bool_sz), 0, "Unexpected mallctl() error");
	expect_true(e1, "Unexpected previous tcache state");
	strcpy(inputs, "0-112:8");
	/* Setting returns ENOENT when the tcache is disabled. */
	expect_d_eq(mallctlbymib(mib_set, mib_set_len, NULL, NULL,
	    (void *)&inputp, sizeof(char *)), ENOENT,
	    "Unexpected mallctlbymib() failure");
	/* All ncached_max should return 0 once tcache is disabled. */
	for (szind_t i = 0; i < TCACHE_NBINS_MAX; i++) {
		cache_bin_info_init(&tcache_bin_info[i], 0);
	}
	check_bins_info(tcache_bin_info);

	e0 = true;
	expect_d_eq(mallctl("thread.tcache.enabled", (void *)&e1, &bool_sz,
	    (void *)&e0, bool_sz), 0, "Unexpected mallctl() error");
	expect_false(e1, "Unexpected previous tcache state");
	memcpy(tcache_bin_info, tcache_bin_info_backup,
	    sizeof(tcache_bin_info_backup));
	for (szind_t i = tcache_nbins_get(tcache_slow); i < TCACHE_NBINS_MAX;
	    i++) {
		cache_bin_info_init(&tcache_bin_info[i], 0);
	}
	check_bins_info(tcache_bin_info);

	/*
	 * Set ncached_max of bins not enabled yet.  Then, enable them by
	 * resetting tcache_max.  The ncached_max changes should stay.
	 */
	size_t tcache_max = 1024;
	assert_d_eq(mallctl("thread.tcache.max",
	    NULL, NULL, (void *)&tcache_max, sizeof(size_t)),.0,
	    "Unexpected.mallctl().failure");
	for (szind_t i = sz_size2index(1024) + 1; i < TCACHE_NBINS_MAX; i++) {
		cache_bin_info_init(&tcache_bin_info[i], 0);
	}
	strcpy(inputs, "2048-6144:123");
	expect_d_eq(mallctlbymib(mib_set, mib_set_len, NULL, NULL,
	    (void *)&inputp, sizeof(char *)), 0,
	    "Unexpected mallctlbymib() failure");
	check_bins_info(tcache_bin_info);

	tcache_max = 6144;
	assert_d_eq(mallctl("thread.tcache.max",
	    NULL, NULL, (void *)&tcache_max, sizeof(size_t)),.0,
	    "Unexpected.mallctl().failure");
	memcpy(tcache_bin_info, tcache_bin_info_backup,
	    sizeof(tcache_bin_info_backup));
	for (szind_t i = sz_size2index(2048); i < TCACHE_NBINS_MAX; i++) {
		if (i <= sz_size2index(6144)) {
			cache_bin_info_init(&tcache_bin_info[i], 123);
		} else if (i > sz_size2index(6144)) {
			cache_bin_info_init(&tcache_bin_info[i], 0);
		}
	}
	check_bins_info(tcache_bin_info);

	/* Test an empty input, it should do nothing. */
	strcpy(inputs, "");
	expect_d_eq(mallctlbymib(mib_set, mib_set_len, NULL, NULL,
	    (void *)&inputp, sizeof(char *)), 0,
	    "Unexpected mallctlbymib() failure");
	check_bins_info(tcache_bin_info);

	/* Test a half-done string, it should return EINVAL and do nothing. */
	strcpy(inputs, "4-1024:7|256-1024");
	expect_d_eq(mallctlbymib(mib_set, mib_set_len, NULL, NULL,
	    (void *)&inputp, sizeof(char *)), EINVAL,
	    "Unexpected mallctlbymib() failure");
	check_bins_info(tcache_bin_info);

	/*
	 * Test an invalid string with start size larger than end size.  It
	 * should return success but do nothing.
	 */
	strcpy(inputs, "1024-256:7");
	expect_d_eq(mallctlbymib(mib_set, mib_set_len, NULL, NULL,
	    (void *)&inputp, sizeof(char *)), 0,
	    "Unexpected mallctlbymib() failure");
	check_bins_info(tcache_bin_info);

	/*
	 * Test a string exceeding the length limit, it should return EINVAL
	 * and do nothing.
	 */
	char *long_inputs = (char *)malloc(10000 * sizeof(char));
	expect_true(long_inputs != NULL, "Unexpected allocation failure.");
	for (int i = 0; i < 200; i++) {
		memcpy(long_inputs + i * 9, "4-1024:3|", 9);
	}
	memcpy(long_inputs + 200 * 9, "4-1024:3", 8);
	long_inputs[200 * 9 + 8] = '\0';
	inputp = long_inputs;
	expect_d_eq(mallctlbymib(mib_set, mib_set_len, NULL, NULL,
	    (void *)&inputp, sizeof(char *)), EINVAL,
	    "Unexpected mallctlbymib() failure");
	check_bins_info(tcache_bin_info);
	free(long_inputs);

	/*
	 * Test a string with invalid characters, it should return EINVAL
	 * and do nothing.
	 */
	strcpy(inputs, "k8-1024:77p");
	inputp = inputs;
	expect_d_eq(mallctlbymib(mib_set, mib_set_len, NULL, NULL,
	    (void *)&inputp, sizeof(char *)), EINVAL,
	    "Unexpected mallctlbymib() failure");
	check_bins_info(tcache_bin_info);

	/* Test large ncached_max, it should return success but capped. */
	strcpy(inputs, "1024-1024:65540");
	expect_d_eq(mallctlbymib(mib_set, mib_set_len, NULL, NULL,
	    (void *)&inputp, sizeof(char *)), 0,
	    "Unexpected mallctlbymib() failure");
	cache_bin_info_init(&tcache_bin_info[sz_size2index(1024)],
	    CACHE_BIN_NCACHED_MAX);
	check_bins_info(tcache_bin_info);

	return NULL;
}

TEST_BEGIN(test_ncached_max) {
	test_skip_if(!config_stats);
	test_skip_if(!opt_tcache);
	test_skip_if(san_uaf_detection_enabled());
	/* TODO: change nthreads to 8 to reduce CI loads. */
	unsigned nthreads = 108;
	VARIABLE_ARRAY(thd_t, threads, nthreads);
	for (unsigned i = 0; i < nthreads; i++) {
		thd_create(&threads[i], ncached_max_check, NULL);
	}
	for (unsigned i = 0; i < nthreads; i++) {
		thd_join(threads[i], NULL);
	}
}
TEST_END

int
main(void) {
	return test(
	    test_ncached_max);
}


#include "test/jemalloc_test.h"

#include "jemalloc/internal/sec.h"

typedef struct test_data_s test_data_t;
struct test_data_s {
	/*
	 * Must be the first member -- we convert back and forth between the
	 * test_data_t and the sec_t;
	 */
	sec_t   sec;
	base_t *base;
};

static void
test_data_init(tsdn_t *tsdn, test_data_t *tdata, const sec_opts_t *opts) {
	tdata->base = base_new(TSDN_NULL, /* ind */ 123,
	    &ehooks_default_extent_hooks, /* metadata_use_hooks */ true);

	bool err = sec_init(tsdn, &tdata->sec, tdata->base, opts);
	assert_false(err, "Unexpected initialization failure");
	if (tdata->sec.opts.nshards > 0) {
		assert_u_ge(tdata->sec.npsizes, 0,
		    "Zero size classes allowed for caching");
	}
}

static void
destroy_test_data(tsdn_t *tsdn, test_data_t *tdata) {
	/* There is no destroy sec to delete the bins ?! */
	base_delete(tsdn, tdata->base);
}

TEST_BEGIN(test_max_nshards_option_zero) {
	test_data_t tdata;
	sec_opts_t  opts;
	opts.nshards = 0;
	opts.max_alloc = PAGE;
	opts.max_bytes = 512 * PAGE;

	tsdn_t *tsdn = tsd_tsdn(tsd_fetch());
	test_data_init(tsdn, &tdata, &opts);

	edata_t *edata = sec_alloc(tsdn, &tdata.sec, PAGE);
	expect_ptr_null(edata, "SEC should be disabled when nshards==0");
	destroy_test_data(tsdn, &tdata);
}
TEST_END

TEST_BEGIN(test_max_alloc_option_too_small) {
	test_data_t tdata;
	sec_opts_t  opts;
	opts.nshards = 1;
	opts.max_alloc = 2 * PAGE;
	opts.max_bytes = 512 * PAGE;

	tsdn_t *tsdn = tsd_tsdn(tsd_fetch());
	test_data_init(tsdn, &tdata, &opts);

	edata_t *edata = sec_alloc(tsdn, &tdata.sec, 3 * PAGE);
	expect_ptr_null(edata, "max_alloc is 2*PAGE, should not alloc 3*PAGE");
	destroy_test_data(tsdn, &tdata);
}
TEST_END

TEST_BEGIN(test_sec_fill) {
	test_data_t tdata;
	sec_opts_t  opts;
	opts.nshards = 1;
	opts.max_alloc = 2 * PAGE;
	opts.max_bytes = 4 * PAGE;
	opts.batch_fill_extra = 2;

	tsdn_t *tsdn = tsd_tsdn(tsd_fetch());
	test_data_init(tsdn, &tdata, &opts);

	/* Fill the cache with two extents */
	sec_stats_t         stats = {0};
	edata_list_active_t allocs;
	edata_list_active_init(&allocs);
	edata_t edata1, edata2;
	edata_size_set(&edata1, PAGE);
	edata_size_set(&edata2, PAGE);
	edata_list_active_append(&allocs, &edata1);
	edata_list_active_append(&allocs, &edata2);
	sec_fill(tsdn, &tdata.sec, PAGE, &allocs, 2);
	sec_stats_merge(tsdn, &tdata.sec, &stats);
	expect_zu_eq(stats.bytes, 2 * PAGE, "SEC should have what we filled");
	expect_true(edata_list_active_empty(&allocs),
	    "extents should be consumed by sec");

	/* Try to overfill and confirm that max_bytes is respected. */
	stats.bytes = 0;
	edata_t edata5, edata4, edata3;
	edata_size_set(&edata3, PAGE);
	edata_size_set(&edata4, PAGE);
	edata_size_set(&edata5, PAGE);
	edata_list_active_append(&allocs, &edata3);
	edata_list_active_append(&allocs, &edata4);
	edata_list_active_append(&allocs, &edata5);
	sec_fill(tsdn, &tdata.sec, PAGE, &allocs, 3);
	sec_stats_merge(tsdn, &tdata.sec, &stats);
	expect_zu_eq(
	    stats.bytes, opts.max_bytes, "SEC can't have more than max_bytes");
	expect_false(edata_list_active_empty(&allocs), "Not all should fit");
	expect_zu_eq(stats.total.noverfills, 1, "Expected one overfill");
	destroy_test_data(tsdn, &tdata);
}
TEST_END

TEST_BEGIN(test_sec_alloc) {
	test_data_t tdata;
	sec_opts_t  opts;
	opts.nshards = 1;
	opts.max_alloc = 2 * PAGE;
	opts.max_bytes = 4 * PAGE;
	opts.batch_fill_extra = 1;

	tsdn_t *tsdn = tsd_tsdn(tsd_fetch());
	test_data_init(tsdn, &tdata, &opts);

	/* Alloc from empty cache returns NULL */
	edata_t *edata = sec_alloc(tsdn, &tdata.sec, PAGE);
	expect_ptr_null(edata, "SEC is empty");

	/* Place two extents into the sec */
	edata_list_active_t allocs;
	edata_list_active_init(&allocs);
	edata_t edata1, edata2;
	edata_size_set(&edata1, PAGE);
	edata_list_active_append(&allocs, &edata1);
	sec_dalloc(tsdn, &tdata.sec, &allocs);
	expect_true(edata_list_active_empty(&allocs), "");
	edata_size_set(&edata2, PAGE);
	edata_list_active_append(&allocs, &edata2);
	sec_dalloc(tsdn, &tdata.sec, &allocs);
	expect_true(edata_list_active_empty(&allocs), "");

	sec_stats_t stats = {0};
	sec_stats_merge(tsdn, &tdata.sec, &stats);
	expect_zu_eq(stats.bytes, 2 * PAGE,
	    "After fill bytes should reflect what is in the cache");
	stats.bytes = 0;

	/* Most recently cached extent should be used on alloc */
	edata = sec_alloc(tsdn, &tdata.sec, PAGE);
	expect_ptr_eq(edata, &edata2, "edata2 is most recently used");
	sec_stats_merge(tsdn, &tdata.sec, &stats);
	expect_zu_eq(stats.bytes, PAGE, "One more item left in the cache");
	stats.bytes = 0;

	/* Alloc can still get extents from cache */
	edata = sec_alloc(tsdn, &tdata.sec, PAGE);
	expect_ptr_eq(edata, &edata1, "SEC is not empty");
	sec_stats_merge(tsdn, &tdata.sec, &stats);
	expect_zu_eq(stats.bytes, 0, "No more items after last one is popped");

	/* And cache is empty again */
	edata = sec_alloc(tsdn, &tdata.sec, PAGE);
	expect_ptr_null(edata, "SEC is empty");
	destroy_test_data(tsdn, &tdata);
}
TEST_END

TEST_BEGIN(test_sec_dalloc) {
	test_data_t tdata;
	sec_opts_t  opts;
	opts.nshards = 1;
	opts.max_alloc = PAGE;
	opts.max_bytes = 2 * PAGE;

	tsdn_t *tsdn = tsd_tsdn(tsd_fetch());
	test_data_init(tsdn, &tdata, &opts);

	/* Return one extent into the cache */
	edata_list_active_t allocs;
	edata_list_active_init(&allocs);
	edata_t edata1;
	edata_size_set(&edata1, PAGE);
	edata_list_active_append(&allocs, &edata1);

	/* SEC is empty, we return one pointer to it */
	sec_dalloc(tsdn, &tdata.sec, &allocs);
	expect_true(
	    edata_list_active_empty(&allocs), "extents should be consumed");

	/* Return one more extent, so that we are at the limit */
	edata_t edata2;
	edata_size_set(&edata2, PAGE);
	edata_list_active_append(&allocs, &edata2);
	/* Sec can take one more as well and we will be exactly at max_bytes */
	sec_dalloc(tsdn, &tdata.sec, &allocs);
	expect_true(
	    edata_list_active_empty(&allocs), "extents should be consumed");

	sec_stats_t stats = {0};
	sec_stats_merge(tsdn, &tdata.sec, &stats);
	expect_zu_eq(stats.bytes, opts.max_bytes, "Size should match deallocs");
	stats.bytes = 0;

	/*
	 * We are at max_bytes.  Now, we dalloc one more pointer and we go above
	 * the limit.  This will force flush to 3/4 of max_bytes and given that
	 * we have max of 2 pages, we will have to flush two. We will not flush
	 * the one given in the input as it is the most recently used.
	 */
	edata_t edata3;
	edata_size_set(&edata3, PAGE);
	edata_list_active_append(&allocs, &edata3);
	sec_dalloc(tsdn, &tdata.sec, &allocs);
	expect_false(
	    edata_list_active_empty(&allocs), "extents should NOT be consumed");
	expect_ptr_ne(
	    edata_list_active_first(&allocs), &edata3, "edata3 is MRU");
	expect_ptr_ne(
	    edata_list_active_last(&allocs), &edata3, "edata3 is MRU");
	sec_stats_merge(tsdn, &tdata.sec, &stats);
	expect_zu_eq(PAGE, stats.bytes, "Should have flushed");
	destroy_test_data(tsdn, &tdata);
}
TEST_END

TEST_BEGIN(test_max_bytes_too_low) {
	test_data_t tdata;
	sec_opts_t  opts;
	opts.nshards = 1;
	opts.max_alloc = 4 * PAGE;
	opts.max_bytes = 2 * PAGE;

	tsdn_t *tsdn = tsd_tsdn(tsd_fetch());
	test_data_init(tsdn, &tdata, &opts);

	/* Return one extent into the cache. Item is too big */
	edata_list_active_t allocs;
	edata_list_active_init(&allocs);
	edata_t edata1;
	edata_size_set(&edata1, 3 * PAGE);
	edata_list_active_append(&allocs, &edata1);

	/* SEC is empty, we return one pointer to it */
	sec_dalloc(tsdn, &tdata.sec, &allocs);
	expect_false(
	    edata_list_active_empty(&allocs), "extents should not be consumed");
	destroy_test_data(tsdn, &tdata);
}
TEST_END

TEST_BEGIN(test_sec_flush) {
	test_data_t tdata;
	sec_opts_t  opts;
	opts.nshards = 1;
	opts.max_alloc = 4 * PAGE;
	opts.max_bytes = 1024 * PAGE;

	tsdn_t *tsdn = tsd_tsdn(tsd_fetch());
	test_data_init(tsdn, &tdata, &opts);

	/* We put in 10 one-page extents, and 10 four-page extents */
	edata_list_active_t allocs1;
	edata_list_active_t allocs4;
	edata_list_active_init(&allocs1);
	edata_list_active_init(&allocs4);
	enum { NALLOCS = 10 };
	edata_t edata1[NALLOCS];
	edata_t edata4[NALLOCS];
	for (int i = 0; i < NALLOCS; i++) {
		edata_size_set(&edata1[i], PAGE);
		edata_size_set(&edata4[i], 4 * PAGE);

		edata_list_active_append(&allocs1, &edata1[i]);
		sec_dalloc(tsdn, &tdata.sec, &allocs1);
		edata_list_active_append(&allocs4, &edata4[i]);
		sec_dalloc(tsdn, &tdata.sec, &allocs4);
	}

	sec_stats_t stats = {0};
	sec_stats_merge(tsdn, &tdata.sec, &stats);
	expect_zu_eq(
	    stats.bytes, 10 * 5 * PAGE, "SEC should have what we filled");
	stats.bytes = 0;

	expect_true(edata_list_active_empty(&allocs1), "");
	sec_flush(tsdn, &tdata.sec, &allocs1);
	expect_false(edata_list_active_empty(&allocs1), "");

	sec_stats_merge(tsdn, &tdata.sec, &stats);
	expect_zu_eq(stats.bytes, 0, "SEC should be empty");
	stats.bytes = 0;
	destroy_test_data(tsdn, &tdata);
}
TEST_END

TEST_BEGIN(test_sec_stats) {
	test_data_t tdata;
	sec_opts_t  opts;
	opts.nshards = 1;
	opts.max_alloc = PAGE;
	opts.max_bytes = 2 * PAGE;

	tsdn_t *tsdn = tsd_tsdn(tsd_fetch());
	test_data_init(tsdn, &tdata, &opts);

	edata_list_active_t allocs;
	edata_list_active_init(&allocs);
	edata_t edata1;
	edata_size_set(&edata1, PAGE);
	edata_list_active_append(&allocs, &edata1);

	/* SEC is empty alloc fails. nmisses==1 */
	edata_t *edata = sec_alloc(tsdn, &tdata.sec, PAGE);
	expect_ptr_null(edata, "SEC should be empty");

	/* SEC is empty, we return one pointer to it. ndalloc_noflush=1 */
	sec_dalloc(tsdn, &tdata.sec, &allocs);
	expect_true(
	    edata_list_active_empty(&allocs), "extents should be consumed");

	edata_t edata2;
	edata_size_set(&edata2, PAGE);
	edata_list_active_append(&allocs, &edata2);
	/* Sec can take one more, so ndalloc_noflush=2 */
	sec_dalloc(tsdn, &tdata.sec, &allocs);
	expect_true(
	    edata_list_active_empty(&allocs), "extents should be consumed");

	sec_stats_t stats;
	memset(&stats, 0, sizeof(sec_stats_t));
	sec_stats_merge(tsdn, &tdata.sec, &stats);
	expect_zu_eq(stats.bytes, opts.max_bytes, "Size should match deallocs");
	expect_zu_eq(stats.total.ndalloc_noflush, 2, "");
	expect_zu_eq(stats.total.nmisses, 1, "");

	memset(&stats, 0, sizeof(sec_stats_t));

	/*
	 * We are at max_bytes.  Now, we dalloc one more pointer and we go above
	 * the limit.  This will force flush, so ndalloc_flush = 1.
	 */
	edata_t edata3;
	edata_size_set(&edata3, PAGE);
	edata_list_active_append(&allocs, &edata3);
	sec_dalloc(tsdn, &tdata.sec, &allocs);
	expect_false(
	    edata_list_active_empty(&allocs), "extents should NOT be consumed");
	sec_stats_merge(tsdn, &tdata.sec, &stats);
	expect_zu_eq(PAGE, stats.bytes, "Should have flushed");
	expect_zu_eq(stats.total.ndalloc_flush, 1, "");
	memset(&stats, 0, sizeof(sec_stats_t));
	destroy_test_data(tsdn, &tdata);
}
TEST_END

#define NOPS_PER_THREAD 100
#define NPREFILL 32

static void
edata_init_test(edata_t *edata) {
	memset(edata, 0, sizeof(*edata));
}

typedef struct {
	sec_t              *sec;
	uint8_t             preferred_shard;
	size_t              nallocs;
	size_t              nallocs_fail;
	size_t              ndallocs;
	size_t              ndallocs_fail;
	edata_list_active_t fill_list;
	size_t              fill_list_sz;
	edata_t            *edata[NOPS_PER_THREAD];
} trylock_test_arg_t;

static void *
thd_trylock_test(void *varg) {
	trylock_test_arg_t *arg = (trylock_test_arg_t *)varg;
	tsd_t              *tsd = tsd_fetch();
	tsdn_t             *tsdn = tsd_tsdn(tsd);

	/* Set the preferred shard for this thread */
	uint8_t *shard_idx = tsd_sec_shardp_get(tsd);
	*shard_idx = arg->preferred_shard;

	/* Fill the shard with some extents */
	sec_fill(tsdn, arg->sec, PAGE, &arg->fill_list, arg->fill_list_sz);
	expect_true(edata_list_active_empty(&arg->fill_list), "");

	for (unsigned i = 0; i < NOPS_PER_THREAD; i++) {
		/* Try to allocate from SEC */
		arg->edata[i] = sec_alloc(tsdn, arg->sec, PAGE);
		if (arg->edata[i] != NULL) {
			expect_zu_eq(edata_size_get(arg->edata[i]), PAGE, "");
		}
	}

	for (unsigned i = 0; i < NOPS_PER_THREAD; i++) {
		if (arg->edata[i] != NULL) {
			edata_list_active_t list;
			edata_list_active_init(&list);
			arg->nallocs++;
			edata_list_active_append(&list, arg->edata[i]);
			expect_zu_eq(edata_size_get(arg->edata[i]), PAGE, "");
			sec_dalloc(tsdn, arg->sec, &list);
			if (edata_list_active_empty(&list)) {
				arg->ndallocs++;
			} else {
				arg->ndallocs_fail++;
			}
		} else {
			arg->nallocs_fail++;
		}
	}

	return NULL;
}

TEST_BEGIN(test_sec_multishard) {
	test_data_t tdata;
	sec_opts_t  opts;
	enum { NSHARDS = 2 };
	enum { NTHREADS = NSHARDS * 16 };
	opts.nshards = NSHARDS;
	opts.max_alloc = 2 * PAGE;
	opts.max_bytes = 64 * NTHREADS * PAGE;

	tsdn_t *tsdn = tsd_tsdn(tsd_fetch());
	test_data_init(tsdn, &tdata, &opts);

	/* Create threads with different preferred shards */
	thd_t              thds[NTHREADS];
	trylock_test_arg_t args[NTHREADS];

	edata_t all_edatas[NPREFILL * NTHREADS];

	for (unsigned i = 0; i < NTHREADS; i++) {
		edata_list_active_init(&args[i].fill_list);
		for (unsigned j = 0; j < NPREFILL; ++j) {
			size_t ind = i * NPREFILL + j;
			edata_init_test(&all_edatas[ind]);
			edata_size_set(&all_edatas[ind], PAGE);
			edata_list_active_append(
			    &args[i].fill_list, &all_edatas[ind]);
		}
		args[i].fill_list_sz = NPREFILL;
		args[i].sec = &tdata.sec;
		args[i].preferred_shard = i % opts.nshards;
		args[i].nallocs = 0;
		args[i].nallocs_fail = 0;
		args[i].ndallocs = 0;
		args[i].ndallocs_fail = 0;
		memset(
		    &args[i].edata[0], 0, NOPS_PER_THREAD * sizeof(edata_t *));
		thd_create(&thds[i], thd_trylock_test, &args[i]);
	}

	for (unsigned i = 0; i < NTHREADS; i++) {
		thd_join(thds[i], NULL);
	}

	/* Wait for all threads to complete */
	size_t total_allocs = 0;
	size_t total_dallocs = 0;
	size_t total_allocs_fail = 0;
	for (unsigned i = 0; i < NTHREADS; i++) {
		total_allocs += args[i].nallocs;
		total_dallocs += args[i].ndallocs;
		total_allocs_fail += args[i].nallocs_fail;
	}

	/* We must have at least some hits */
	expect_zu_gt(total_allocs, 0, "");
	/*
	 * We must have at least some successful dallocs by design (max_bytes is
	 * big enough).
	 */
	expect_zu_gt(total_dallocs, 0, "");

	/* Get final stats to verify that hits and misses are accurate */
	sec_stats_t stats = {0};
	memset(&stats, 0, sizeof(sec_stats_t));
	sec_stats_merge(tsdn, &tdata.sec, &stats);
	expect_zu_eq(stats.total.nhits, total_allocs, "");
	expect_zu_eq(stats.total.nmisses, total_allocs_fail, "");

	destroy_test_data(tsdn, &tdata);
}
TEST_END

int
main(void) {
	return test(test_max_nshards_option_zero,
	    test_max_alloc_option_too_small, test_sec_fill, test_sec_alloc,
	    test_sec_dalloc, test_max_bytes_too_low, test_sec_flush,
	    test_sec_stats, test_sec_multishard);
}

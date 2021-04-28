#include "test/jemalloc_test.h"

#include "jemalloc/internal/hpa.h"

#define SHARD_IND 111

#define ALLOC_MAX (HUGEPAGE / 4)

typedef struct test_data_s test_data_t;
struct test_data_s {
	/*
	 * Must be the first member -- we convert back and forth between the
	 * test_data_t and the hpa_shard_t;
	 */
	hpa_shard_t shard;
	base_t *base;
	edata_cache_t shard_edata_cache;

	emap_t emap;
};

static hpa_shard_t *
create_test_data() {
	bool err;
	base_t *base = base_new(TSDN_NULL, /* ind */ SHARD_IND,
	    &ehooks_default_extent_hooks);
	assert_ptr_not_null(base, "");

	test_data_t *test_data = malloc(sizeof(test_data_t));
	assert_ptr_not_null(test_data, "");

	test_data->base = base;

	err = edata_cache_init(&test_data->shard_edata_cache, base);
	assert_false(err, "");

	err = emap_init(&test_data->emap, test_data->base, /* zeroed */ false);
	assert_false(err, "");

	hpa_shard_opts_t opts = HPA_SHARD_OPTS_DEFAULT;
	opts.slab_max_alloc = ALLOC_MAX;

	err = hpa_shard_init(&test_data->shard, &test_data->emap,
	    test_data->base, &test_data->shard_edata_cache, SHARD_IND,
	    &opts);
	assert_false(err, "");

	return (hpa_shard_t *)test_data;
}

static void
destroy_test_data(hpa_shard_t *shard) {
	test_data_t *test_data = (test_data_t *)shard;
	base_delete(TSDN_NULL, test_data->base);
	free(test_data);
}

TEST_BEGIN(test_alloc_max) {
	test_skip_if(!hpa_supported());

	hpa_shard_t *shard = create_test_data();
	tsdn_t *tsdn = tsd_tsdn(tsd_fetch());

	edata_t *edata;

	/* Small max */
	edata = pai_alloc(tsdn, &shard->pai, ALLOC_MAX, PAGE, false);
	expect_ptr_not_null(edata, "Allocation of small max failed");
	edata = pai_alloc(tsdn, &shard->pai, ALLOC_MAX + PAGE, PAGE, false);
	expect_ptr_null(edata, "Allocation of larger than small max succeeded");

	destroy_test_data(shard);
}
TEST_END

typedef struct mem_contents_s mem_contents_t;
struct mem_contents_s {
	uintptr_t my_addr;
	size_t size;
	edata_t *my_edata;
	rb_node(mem_contents_t) link;
};

static int
mem_contents_cmp(const mem_contents_t *a, const mem_contents_t *b) {
	return (a->my_addr > b->my_addr) - (a->my_addr < b->my_addr);
}

typedef rb_tree(mem_contents_t) mem_tree_t;
rb_gen(static, mem_tree_, mem_tree_t, mem_contents_t, link,
    mem_contents_cmp);

static void
node_assert_ordered(mem_contents_t *a, mem_contents_t *b) {
	assert_zu_lt(a->my_addr, a->my_addr + a->size, "Overflow");
	assert_zu_le(a->my_addr + a->size, b->my_addr, "");
}

static void
node_check(mem_tree_t *tree, mem_contents_t *contents) {
	edata_t *edata = contents->my_edata;
	assert_ptr_eq(contents, (void *)contents->my_addr, "");
	assert_ptr_eq(contents, edata_base_get(edata), "");
	assert_zu_eq(contents->size, edata_size_get(edata), "");
	assert_ptr_eq(contents->my_edata, edata, "");

	mem_contents_t *next = mem_tree_next(tree, contents);
	if (next != NULL) {
		node_assert_ordered(contents, next);
	}
	mem_contents_t *prev = mem_tree_prev(tree, contents);
	if (prev != NULL) {
		node_assert_ordered(prev, contents);
	}
}

static void
node_insert(mem_tree_t *tree, edata_t *edata, size_t npages) {
	mem_contents_t *contents = (mem_contents_t *)edata_base_get(edata);
	contents->my_addr = (uintptr_t)edata_base_get(edata);
	contents->size = edata_size_get(edata);
	contents->my_edata = edata;
	mem_tree_insert(tree, contents);
	node_check(tree, contents);
}

static void
node_remove(mem_tree_t *tree, edata_t *edata) {
	mem_contents_t *contents = (mem_contents_t *)edata_base_get(edata);
	node_check(tree, contents);
	mem_tree_remove(tree, contents);
}

TEST_BEGIN(test_stress) {
	test_skip_if(!hpa_supported());

	hpa_shard_t *shard = create_test_data();

	tsdn_t *tsdn = tsd_tsdn(tsd_fetch());

	const size_t nlive_edatas_max = 500;
	size_t nlive_edatas = 0;
	edata_t **live_edatas = calloc(nlive_edatas_max, sizeof(edata_t *));
	/*
	 * Nothing special about this constant; we're only fixing it for
	 * consistency across runs.
	 */
	size_t prng_state = (size_t)0x76999ffb014df07c;

	mem_tree_t tree;
	mem_tree_new(&tree);

	for (size_t i = 0; i < 100 * 1000; i++) {
		size_t operation = prng_range_zu(&prng_state, 2);
		if (operation == 0) {
			/* Alloc */
			if (nlive_edatas == nlive_edatas_max) {
				continue;
			}

			/*
			 * We make sure to get an even balance of small and
			 * large allocations.
			 */
			size_t npages_min = 1;
			size_t npages_max = ALLOC_MAX / PAGE;
			size_t npages = npages_min + prng_range_zu(&prng_state,
			    npages_max - npages_min);
			edata_t *edata = pai_alloc(tsdn, &shard->pai,
			    npages * PAGE, PAGE, false);
			assert_ptr_not_null(edata,
			    "Unexpected allocation failure");
			live_edatas[nlive_edatas] = edata;
			nlive_edatas++;
			node_insert(&tree, edata, npages);
		} else {
			/* Free. */
			if (nlive_edatas == 0) {
				continue;
			}
			size_t victim = prng_range_zu(&prng_state, nlive_edatas);
			edata_t *to_free = live_edatas[victim];
			live_edatas[victim] = live_edatas[nlive_edatas - 1];
			nlive_edatas--;
			node_remove(&tree, to_free);
			pai_dalloc(tsdn, &shard->pai, to_free);
		}
	}

	size_t ntreenodes = 0;
	for (mem_contents_t *contents = mem_tree_first(&tree); contents != NULL;
	    contents = mem_tree_next(&tree, contents)) {
		ntreenodes++;
		node_check(&tree, contents);
	}
	expect_zu_eq(ntreenodes, nlive_edatas, "");

	/*
	 * Test hpa_shard_destroy, which requires as a precondition that all its
	 * extents have been deallocated.
	 */
	for (size_t i = 0; i < nlive_edatas; i++) {
		edata_t *to_free = live_edatas[i];
		node_remove(&tree, to_free);
		pai_dalloc(tsdn, &shard->pai, to_free);
	}
	hpa_shard_destroy(tsdn, shard);

	free(live_edatas);
	destroy_test_data(shard);
}
TEST_END

static void
expect_contiguous(edata_t **edatas, size_t nedatas) {
	for (size_t i = 0; i < nedatas; i++) {
		size_t expected = (size_t)edata_base_get(edatas[0])
		    + i * PAGE;
		expect_zu_eq(expected, (size_t)edata_base_get(edatas[i]),
		    "Mismatch at index %zu", i);
	}
}

TEST_BEGIN(test_alloc_dalloc_batch) {
	test_skip_if(!hpa_supported());

	hpa_shard_t *shard = create_test_data();
	tsdn_t *tsdn = tsd_tsdn(tsd_fetch());

	enum {NALLOCS = 8};

	edata_t *allocs[NALLOCS];
	/*
	 * Allocate a mix of ways; first half from regular alloc, second half
	 * from alloc_batch.
	 */
	for (size_t i = 0; i < NALLOCS / 2; i++) {
		allocs[i] = pai_alloc(tsdn, &shard->pai, PAGE, PAGE,
		    /* zero */ false);
		expect_ptr_not_null(allocs[i], "Unexpected alloc failure");
	}
	edata_list_active_t allocs_list;
	edata_list_active_init(&allocs_list);
	size_t nsuccess = pai_alloc_batch(tsdn, &shard->pai, PAGE, NALLOCS / 2,
	    &allocs_list);
	expect_zu_eq(NALLOCS / 2, nsuccess, "Unexpected oom");
	for (size_t i = NALLOCS / 2; i < NALLOCS; i++) {
		allocs[i] = edata_list_active_first(&allocs_list);
		edata_list_active_remove(&allocs_list, allocs[i]);
	}

	/*
	 * Should have allocated them contiguously, despite the differing
	 * methods used.
	 */
	void *orig_base = edata_base_get(allocs[0]);
	expect_contiguous(allocs, NALLOCS);

	/*
	 * Batch dalloc the first half, individually deallocate the second half.
	 */
	for (size_t i = 0; i < NALLOCS / 2; i++) {
		edata_list_active_append(&allocs_list, allocs[i]);
	}
	pai_dalloc_batch(tsdn, &shard->pai, &allocs_list);
	for (size_t i = NALLOCS / 2; i < NALLOCS; i++) {
		pai_dalloc(tsdn, &shard->pai, allocs[i]);
	}

	/* Reallocate (individually), and ensure reuse and contiguity. */
	for (size_t i = 0; i < NALLOCS; i++) {
		allocs[i] = pai_alloc(tsdn, &shard->pai, PAGE, PAGE,
		    /* zero */ false);
		expect_ptr_not_null(allocs[i], "Unexpected alloc failure.");
	}
	void *new_base = edata_base_get(allocs[0]);
	expect_ptr_eq(orig_base, new_base,
	    "Failed to reuse the allocated memory.");
	expect_contiguous(allocs, NALLOCS);

	destroy_test_data(shard);
}
TEST_END

int
main(void) {
	/*
	 * These trigger unused-function warnings on CI runs, even if declared
	 * with static inline.
	 */
	(void)mem_tree_empty;
	(void)mem_tree_last;
	(void)mem_tree_search;
	(void)mem_tree_nsearch;
	(void)mem_tree_psearch;
	(void)mem_tree_iter;
	(void)mem_tree_reverse_iter;
	(void)mem_tree_destroy;
	return test_no_reentrancy(
	    test_alloc_max,
	    test_stress,
	    test_alloc_dalloc_batch);
}

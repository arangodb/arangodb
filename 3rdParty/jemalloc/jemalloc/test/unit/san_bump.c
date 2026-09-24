#include "test/jemalloc_test.h"
#include "test/arena_util.h"

#include "jemalloc/internal/arena_structs.h"
#include "jemalloc/internal/san_bump.h"

static extent_hooks_t *san_bump_default_hooks;
static extent_hooks_t  san_bump_hooks;
static bool            fail_retained_alloc;
static unsigned        retained_alloc_fail_calls;

static void *
san_bump_fail_alloc_hook(extent_hooks_t *UNUSED extent_hooks, void *new_addr,
    size_t size, size_t alignment, bool *zero, bool *commit,
    unsigned arena_ind) {
	if (fail_retained_alloc && new_addr == NULL
	    && size >= SBA_RETAINED_ALLOC_SIZE) {
		retained_alloc_fail_calls++;
		return NULL;
	}
	return san_bump_default_hooks->alloc(san_bump_default_hooks, new_addr,
	    size, alignment, zero, commit, arena_ind);
}

static void
install_san_bump_fail_alloc_hooks(unsigned arena_ind) {
	size_t          hooks_mib[3];
	size_t          hooks_miblen = sizeof(hooks_mib) / sizeof(size_t);
	size_t          old_size = sizeof(extent_hooks_t *);
	size_t          new_size = sizeof(extent_hooks_t *);
	extent_hooks_t *new_hooks;
	extent_hooks_t *old_hooks;

	expect_d_eq(
	    mallctlnametomib("arena.0.extent_hooks", hooks_mib, &hooks_miblen),
	    0, "Unexpected mallctlnametomib() failure");
	hooks_mib[1] = (size_t)arena_ind;
	expect_d_eq(mallctlbymib(hooks_mib, hooks_miblen, (void *)&old_hooks,
	                &old_size, NULL, 0),
	    0, "Unexpected extent_hooks error");

	san_bump_default_hooks = old_hooks;
	san_bump_hooks = *old_hooks;
	san_bump_hooks.alloc = san_bump_fail_alloc_hook;
	new_hooks = &san_bump_hooks;
	expect_d_eq(mallctlbymib(hooks_mib, hooks_miblen, NULL, NULL,
	                (void *)&new_hooks, new_size),
	    0, "Unexpected extent_hooks install failure");
}

TEST_BEGIN(test_san_bump_alloc) {
	test_skip_if(!maps_coalesce || !opt_retain);

	tsdn_t *tsdn = tsdn_fetch();

	san_bump_alloc_t sba;
	san_bump_alloc_init(&sba);

	unsigned arena_ind = do_arena_create(0, 0);
	assert_u_ne(arena_ind, UINT_MAX, "Failed to create an arena");

	arena_t *arena = arena_get(tsdn, arena_ind, false);
	pac_t   *pac = &arena->pa_shard.pac;

	size_t   alloc_size = PAGE * 16;
	size_t   alloc_n = alloc_size / sizeof(unsigned);
	edata_t *edata = san_bump_alloc(
	    tsdn, &sba, pac, pac_ehooks_get(pac), alloc_size, /* zero */ false);

	expect_ptr_not_null(edata, "Failed to allocate edata");
	expect_u_eq(edata_arena_ind_get(edata), arena_ind,
	    "Edata was assigned an incorrect arena id");
	expect_zu_eq(edata_size_get(edata), alloc_size,
	    "Allocated edata of incorrect size");
	expect_false(edata_slab_get(edata),
	    "Bump allocator incorrectly assigned 'slab' to true");
	expect_true(edata_committed_get(edata), "Edata is not committed");

	void *ptr = edata_addr_get(edata);
	expect_ptr_not_null(ptr, "Edata was assigned an invalid address");
	/* Test that memory is allocated; no guard pages are misplaced */
	for (unsigned i = 0; i < alloc_n; ++i) {
		((unsigned *)ptr)[i] = 1;
	}

	size_t   alloc_size2 = PAGE * 28;
	size_t   alloc_n2 = alloc_size / sizeof(unsigned);
	edata_t *edata2 = san_bump_alloc(
	    tsdn, &sba, pac, pac_ehooks_get(pac), alloc_size2, /* zero */ true);

	expect_ptr_not_null(edata2, "Failed to allocate edata");
	expect_u_eq(edata_arena_ind_get(edata2), arena_ind,
	    "Edata was assigned an incorrect arena id");
	expect_zu_eq(edata_size_get(edata2), alloc_size2,
	    "Allocated edata of incorrect size");
	expect_false(edata_slab_get(edata2),
	    "Bump allocator incorrectly assigned 'slab' to true");
	expect_true(edata_committed_get(edata2), "Edata is not committed");

	void *ptr2 = edata_addr_get(edata2);
	expect_ptr_not_null(ptr, "Edata was assigned an invalid address");

	uintptr_t ptrdiff = ptr2 > ptr ? (uintptr_t)ptr2 - (uintptr_t)ptr
	                               : (uintptr_t)ptr - (uintptr_t)ptr2;
	size_t    between_allocs = (size_t)ptrdiff - alloc_size;

	expect_zu_ge(
	    between_allocs, PAGE, "Guard page between allocs is missing");

	for (unsigned i = 0; i < alloc_n2; ++i) {
		expect_u_eq(((unsigned *)ptr2)[i], 0, "Memory is not zeroed");
	}
}
TEST_END

TEST_BEGIN(test_failed_grow_preserves_curr_reg) {
	test_skip_if(!maps_coalesce || !opt_retain);

	tsdn_t *tsdn = tsdn_fetch();

	san_bump_alloc_t sba;
	san_bump_alloc_init(&sba);

	unsigned arena_ind = do_arena_create(0, 0);
	assert_u_ne(arena_ind, UINT_MAX, "Failed to create an arena");
	install_san_bump_fail_alloc_hooks(arena_ind);

	arena_t *arena = arena_get(tsdn, arena_ind, false);
	pac_t   *pac = &arena->pa_shard.pac;

	size_t   small_alloc_size = PAGE * 16;
	edata_t *edata = san_bump_alloc(tsdn, &sba, pac, pac_ehooks_get(pac),
	    small_alloc_size, /* zero */ false);
	expect_ptr_not_null(edata, "Initial san_bump allocation failed");
	expect_ptr_not_null(sba.curr_reg,
	    "Expected retained region remainder after initial allocation");

	fail_retained_alloc = true;
	retained_alloc_fail_calls = 0;

	edata_t *failed = san_bump_alloc(tsdn, &sba, pac, pac_ehooks_get(pac),
	    SBA_RETAINED_ALLOC_SIZE, /* zero */ false);
	expect_ptr_null(failed, "Expected retained grow allocation failure");
	expect_u_eq(retained_alloc_fail_calls, 1,
	    "Expected exactly one failed retained allocation attempt");

	edata_t *reused = san_bump_alloc(tsdn, &sba, pac, pac_ehooks_get(pac),
	    small_alloc_size, /* zero */ false);
	expect_ptr_not_null(
	    reused, "Expected allocator to reuse preexisting current region");
	expect_u_eq(retained_alloc_fail_calls, 1,
	    "Reuse path should not attempt another retained grow allocation");

	fail_retained_alloc = false;
}
TEST_END

TEST_BEGIN(test_large_alloc_size) {
	test_skip_if(!maps_coalesce || !opt_retain);

	tsdn_t *tsdn = tsdn_fetch();

	san_bump_alloc_t sba;
	san_bump_alloc_init(&sba);

	unsigned arena_ind = do_arena_create(0, 0);
	assert_u_ne(arena_ind, UINT_MAX, "Failed to create an arena");

	arena_t *arena = arena_get(tsdn, arena_ind, false);
	pac_t   *pac = &arena->pa_shard.pac;

	size_t   alloc_size = SBA_RETAINED_ALLOC_SIZE * 2;
	edata_t *edata = san_bump_alloc(
	    tsdn, &sba, pac, pac_ehooks_get(pac), alloc_size, /* zero */ false);
	expect_u_eq(edata_arena_ind_get(edata), arena_ind,
	    "Edata was assigned an incorrect arena id");
	expect_zu_eq(edata_size_get(edata), alloc_size,
	    "Allocated edata of incorrect size");
	expect_false(edata_slab_get(edata),
	    "Bump allocator incorrectly assigned 'slab' to true");
	expect_true(edata_committed_get(edata), "Edata is not committed");

	void *ptr = edata_addr_get(edata);
	expect_ptr_not_null(ptr, "Edata was assigned an invalid address");
	/* Test that memory is allocated; no guard pages are misplaced */
	for (unsigned i = 0; i < alloc_size / PAGE; ++i) {
		*((char *)ptr + PAGE * i) = 1;
	}
}
TEST_END

int
main(void) {
	return test(test_san_bump_alloc, test_failed_grow_preserves_curr_reg,
	    test_large_alloc_size);
}

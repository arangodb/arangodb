#ifndef JEMALLOC_INTERNAL_INLINES_C_H
#define JEMALLOC_INTERNAL_INLINES_C_H

#include "jemalloc/internal/hook.h"
#include "jemalloc/internal/jemalloc_internal_types.h"
#include "jemalloc/internal/sz.h"
#include "jemalloc/internal/witness.h"

/*
 * Translating the names of the 'i' functions:
 *   Abbreviations used in the first part of the function name (before
 *   alloc/dalloc) describe what that function accomplishes:
 *     a: arena (query)
 *     s: size (query, or sized deallocation)
 *     e: extent (query)
 *     p: aligned (allocates)
 *     vs: size (query, without knowing that the pointer is into the heap)
 *     r: rallocx implementation
 *     x: xallocx implementation
 *   Abbreviations used in the second part of the function name (after
 *   alloc/dalloc) describe the arguments it takes
 *     z: whether to return zeroed memory
 *     t: accepts a tcache_t * parameter
 *     m: accepts an arena_t * parameter
 */

JEMALLOC_ALWAYS_INLINE arena_t *
iaalloc(tsdn_t *tsdn, const void *ptr) {
	assert(ptr != NULL);

	return arena_aalloc(tsdn, ptr);
}

JEMALLOC_ALWAYS_INLINE size_t
isalloc(tsdn_t *tsdn, const void *ptr) {
	assert(ptr != NULL);

	return arena_salloc(tsdn, ptr);
}

JEMALLOC_ALWAYS_INLINE void *
iallocztm(tsdn_t *tsdn, size_t size, szind_t ind, bool zero, tcache_t *tcache,
    bool is_internal, arena_t *arena, bool slow_path) {
	void *ret;

	assert(!is_internal || tcache == NULL);
	assert(!is_internal || arena == NULL || arena_is_auto(arena));
	if (!tsdn_null(tsdn) && tsd_reentrancy_level_get(tsdn_tsd(tsdn)) == 0) {
		witness_assert_depth_to_rank(tsdn_witness_tsdp_get(tsdn),
		    WITNESS_RANK_CORE, 0);
	}

	ret = arena_malloc(tsdn, arena, size, ind, zero, tcache, slow_path);
	if (config_stats && is_internal && likely(ret != NULL)) {
		arena_internal_add(iaalloc(tsdn, ret), isalloc(tsdn, ret));
	}
	return ret;
}

JEMALLOC_ALWAYS_INLINE void *
ialloc(tsd_t *tsd, size_t size, szind_t ind, bool zero, bool slow_path) {
	return iallocztm(tsd_tsdn(tsd), size, ind, zero, tcache_get(tsd), false,
	    NULL, slow_path);
}

JEMALLOC_ALWAYS_INLINE void *
ipallocztm(tsdn_t *tsdn, size_t usize, size_t alignment, bool zero,
    tcache_t *tcache, bool is_internal, arena_t *arena) {
	void *ret;

	assert(usize != 0);
	assert(usize == sz_sa2u(usize, alignment));
	assert(!is_internal || tcache == NULL);
	assert(!is_internal || arena == NULL || arena_is_auto(arena));
	witness_assert_depth_to_rank(tsdn_witness_tsdp_get(tsdn),
	    WITNESS_RANK_CORE, 0);

	ret = arena_palloc(tsdn, arena, usize, alignment, zero, tcache);
	assert(ALIGNMENT_ADDR2BASE(ret, alignment) == ret);
	if (config_stats && is_internal && likely(ret != NULL)) {
		arena_internal_add(iaalloc(tsdn, ret), isalloc(tsdn, ret));
	}
	return ret;
}

JEMALLOC_ALWAYS_INLINE void *
ipalloct(tsdn_t *tsdn, size_t usize, size_t alignment, bool zero,
    tcache_t *tcache, arena_t *arena) {
	return ipallocztm(tsdn, usize, alignment, zero, tcache, false, arena);
}

JEMALLOC_ALWAYS_INLINE void *
ipalloc(tsd_t *tsd, size_t usize, size_t alignment, bool zero) {
	return ipallocztm(tsd_tsdn(tsd), usize, alignment, zero,
	    tcache_get(tsd), false, NULL);
}

JEMALLOC_ALWAYS_INLINE size_t
ivsalloc(tsdn_t *tsdn, const void *ptr) {
	return arena_vsalloc(tsdn, ptr);
}

JEMALLOC_ALWAYS_INLINE void
idalloctm(tsdn_t *tsdn, void *ptr, tcache_t *tcache, alloc_ctx_t *alloc_ctx,
    bool is_internal, bool slow_path) {
	assert(ptr != NULL);
	assert(!is_internal || tcache == NULL);
	assert(!is_internal || arena_is_auto(iaalloc(tsdn, ptr)));
	witness_assert_depth_to_rank(tsdn_witness_tsdp_get(tsdn),
	    WITNESS_RANK_CORE, 0);
	if (config_stats && is_internal) {
		arena_internal_sub(iaalloc(tsdn, ptr), isalloc(tsdn, ptr));
	}
	if (!is_internal && !tsdn_null(tsdn) &&
	    tsd_reentrancy_level_get(tsdn_tsd(tsdn)) != 0) {
		assert(tcache == NULL);
	}
	arena_dalloc(tsdn, ptr, tcache, alloc_ctx, slow_path);
}

JEMALLOC_ALWAYS_INLINE void
idalloc(tsd_t *tsd, void *ptr) {
	idalloctm(tsd_tsdn(tsd), ptr, tcache_get(tsd), NULL, false, true);
}

JEMALLOC_ALWAYS_INLINE void
isdalloct(tsdn_t *tsdn, void *ptr, size_t size, tcache_t *tcache,
    alloc_ctx_t *alloc_ctx, bool slow_path) {
	witness_assert_depth_to_rank(tsdn_witness_tsdp_get(tsdn),
	    WITNESS_RANK_CORE, 0);
	arena_sdalloc(tsdn, ptr, size, tcache, alloc_ctx, slow_path);
}

JEMALLOC_ALWAYS_INLINE void *
iralloct_realign(tsdn_t *tsdn, void *ptr, size_t oldsize, size_t size,
    size_t alignment, bool zero, tcache_t *tcache, arena_t *arena,
    hook_ralloc_args_t *hook_args) {
	witness_assert_depth_to_rank(tsdn_witness_tsdp_get(tsdn),
	    WITNESS_RANK_CORE, 0);
	void *p;
	size_t usize, copysize;

	usize = sz_sa2u(size, alignment);
	if (unlikely(usize == 0 || usize > SC_LARGE_MAXCLASS)) {
		return NULL;
	}
	p = ipalloct(tsdn, usize, alignment, zero, tcache, arena);
	if (p == NULL) {
		return NULL;
	}
	/*
	 * Copy at most size bytes (not size+extra), since the caller has no
	 * expectation that the extra bytes will be reliably preserved.
	 */
	copysize = (size < oldsize) ? size : oldsize;
	memcpy(p, ptr, copysize);
	hook_invoke_alloc(hook_args->is_realloc
	    ? hook_alloc_realloc : hook_alloc_rallocx, p, (uintptr_t)p,
	    hook_args->args);
	hook_invoke_dalloc(hook_args->is_realloc
	    ? hook_dalloc_realloc : hook_dalloc_rallocx, ptr, hook_args->args);
	isdalloct(tsdn, ptr, oldsize, tcache, NULL, true);
	return p;
}

/*
 * is_realloc threads through the knowledge of whether or not this call comes
 * from je_realloc (as opposed to je_rallocx); this ensures that we pass the
 * correct entry point into any hooks.
 * Note that these functions are all force-inlined, so no actual bool gets
 * passed-around anywhere.
 */
JEMALLOC_ALWAYS_INLINE void *
iralloct(tsdn_t *tsdn, void *ptr, size_t oldsize, size_t size, size_t alignment,
    bool zero, tcache_t *tcache, arena_t *arena, hook_ralloc_args_t *hook_args)
{
	assert(ptr != NULL);
	assert(size != 0);
	witness_assert_depth_to_rank(tsdn_witness_tsdp_get(tsdn),
	    WITNESS_RANK_CORE, 0);

	if (alignment != 0 && ((uintptr_t)ptr & ((uintptr_t)alignment-1))
	    != 0) {
		/*
		 * Existing object alignment is inadequate; allocate new space
		 * and copy.
		 */
		return iralloct_realign(tsdn, ptr, oldsize, size, alignment,
		    zero, tcache, arena, hook_args);
	}

	return arena_ralloc(tsdn, arena, ptr, oldsize, size, alignment, zero,
	    tcache, hook_args);
}

JEMALLOC_ALWAYS_INLINE void *
iralloc(tsd_t *tsd, void *ptr, size_t oldsize, size_t size, size_t alignment,
    bool zero, hook_ralloc_args_t *hook_args) {
	return iralloct(tsd_tsdn(tsd), ptr, oldsize, size, alignment, zero,
	    tcache_get(tsd), NULL, hook_args);
}

JEMALLOC_ALWAYS_INLINE bool
ixalloc(tsdn_t *tsdn, void *ptr, size_t oldsize, size_t size, size_t extra,
    size_t alignment, bool zero, size_t *newsize) {
	assert(ptr != NULL);
	assert(size != 0);
	witness_assert_depth_to_rank(tsdn_witness_tsdp_get(tsdn),
	    WITNESS_RANK_CORE, 0);

	if (alignment != 0 && ((uintptr_t)ptr & ((uintptr_t)alignment-1))
	    != 0) {
		/* Existing object alignment is inadequate. */
		*newsize = oldsize;
		return true;
	}

	return arena_ralloc_no_move(tsdn, ptr, oldsize, size, extra, zero,
	    newsize);
}

#endif /* JEMALLOC_INTERNAL_INLINES_C_H */

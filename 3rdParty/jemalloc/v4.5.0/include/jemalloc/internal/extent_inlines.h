#ifndef JEMALLOC_INTERNAL_EXTENT_INLINES_H
#define JEMALLOC_INTERNAL_EXTENT_INLINES_H

#ifndef JEMALLOC_ENABLE_INLINE
extent_t	*extent_lookup(tsdn_t *tsdn, const void *ptr, bool dependent);
arena_t	*extent_arena_get(const extent_t *extent);
void	*extent_base_get(const extent_t *extent);
void	*extent_addr_get(const extent_t *extent);
size_t	extent_size_get(const extent_t *extent);
size_t	extent_usize_get(const extent_t *extent);
void	*extent_before_get(const extent_t *extent);
void	*extent_last_get(const extent_t *extent);
void	*extent_past_get(const extent_t *extent);
size_t	extent_sn_get(const extent_t *extent);
extent_state_t	extent_state_get(const extent_t *extent);
bool	extent_zeroed_get(const extent_t *extent);
bool	extent_committed_get(const extent_t *extent);
bool	extent_slab_get(const extent_t *extent);
arena_slab_data_t	*extent_slab_data_get(extent_t *extent);
const arena_slab_data_t	*extent_slab_data_get_const(const extent_t *extent);
prof_tctx_t	*extent_prof_tctx_get(const extent_t *extent);
void	extent_arena_set(extent_t *extent, arena_t *arena);
void	extent_addr_set(extent_t *extent, void *addr);
void	extent_addr_randomize(tsdn_t *tsdn, extent_t *extent, size_t alignment);
void	extent_size_set(extent_t *extent, size_t size);
void	extent_usize_set(extent_t *extent, size_t usize);
void	extent_sn_set(extent_t *extent, size_t sn);
void	extent_state_set(extent_t *extent, extent_state_t state);
void	extent_zeroed_set(extent_t *extent, bool zeroed);
void	extent_committed_set(extent_t *extent, bool committed);
void	extent_slab_set(extent_t *extent, bool slab);
void	extent_prof_tctx_set(extent_t *extent, prof_tctx_t *tctx);
void	extent_init(extent_t *extent, arena_t *arena, void *addr,
    size_t size, size_t usize, size_t sn, extent_state_t state, bool zeroed,
    bool committed, bool slab);
void extent_list_init(extent_list_t *list);
extent_t *extent_list_first(const extent_list_t *list);
extent_t *extent_list_last(const extent_list_t *list);
void extent_list_append(extent_list_t *list, extent_t *extent);
void extent_list_remove(extent_list_t *list, extent_t *extent);
int	extent_sn_comp(const extent_t *a, const extent_t *b);
int	extent_ad_comp(const extent_t *a, const extent_t *b);
int	extent_snad_comp(const extent_t *a, const extent_t *b);
#endif

#if (defined(JEMALLOC_ENABLE_INLINE) || defined(JEMALLOC_EXTENT_C_))
JEMALLOC_INLINE extent_t *
extent_lookup(tsdn_t *tsdn, const void *ptr, bool dependent) {
	rtree_ctx_t rtree_ctx_fallback;
	rtree_ctx_t *rtree_ctx = tsdn_rtree_ctx(tsdn, &rtree_ctx_fallback);

	return rtree_read(tsdn, &extents_rtree, rtree_ctx, (uintptr_t)ptr,
	    dependent);
}

JEMALLOC_INLINE arena_t *
extent_arena_get(const extent_t *extent) {
	return extent->e_arena;
}

JEMALLOC_INLINE void *
extent_base_get(const extent_t *extent) {
	assert(extent->e_addr == PAGE_ADDR2BASE(extent->e_addr) ||
	    !extent->e_slab);
	return PAGE_ADDR2BASE(extent->e_addr);
}

JEMALLOC_INLINE void *
extent_addr_get(const extent_t *extent) {
	assert(extent->e_addr == PAGE_ADDR2BASE(extent->e_addr) ||
	    !extent->e_slab);
	return extent->e_addr;
}

JEMALLOC_INLINE size_t
extent_size_get(const extent_t *extent) {
	return extent->e_size;
}

JEMALLOC_INLINE size_t
extent_usize_get(const extent_t *extent) {
	assert(!extent->e_slab);
	return extent->e_usize;
}

JEMALLOC_INLINE void *
extent_before_get(const extent_t *extent) {
	return (void *)((uintptr_t)extent_base_get(extent) - PAGE);
}

JEMALLOC_INLINE void *
extent_last_get(const extent_t *extent) {
	return (void *)((uintptr_t)extent_base_get(extent) +
	    extent_size_get(extent) - PAGE);
}

JEMALLOC_INLINE void *
extent_past_get(const extent_t *extent) {
	return (void *)((uintptr_t)extent_base_get(extent) +
	    extent_size_get(extent));
}

JEMALLOC_INLINE size_t
extent_sn_get(const extent_t *extent) {
	return extent->e_sn;
}

JEMALLOC_INLINE extent_state_t
extent_state_get(const extent_t *extent) {
	return extent->e_state;
}

JEMALLOC_INLINE bool
extent_zeroed_get(const extent_t *extent) {
	return extent->e_zeroed;
}

JEMALLOC_INLINE bool
extent_committed_get(const extent_t *extent) {
	return extent->e_committed;
}

JEMALLOC_INLINE bool
extent_slab_get(const extent_t *extent) {
	return extent->e_slab;
}

JEMALLOC_INLINE arena_slab_data_t *
extent_slab_data_get(extent_t *extent) {
	assert(extent->e_slab);
	return &extent->e_slab_data;
}

JEMALLOC_INLINE const arena_slab_data_t *
extent_slab_data_get_const(const extent_t *extent) {
	assert(extent->e_slab);
	return &extent->e_slab_data;
}

JEMALLOC_INLINE prof_tctx_t *
extent_prof_tctx_get(const extent_t *extent) {
	return (prof_tctx_t *)atomic_read_p(
	    &((extent_t *)extent)->e_prof_tctx_pun);
}

JEMALLOC_INLINE void
extent_arena_set(extent_t *extent, arena_t *arena) {
	extent->e_arena = arena;
}

JEMALLOC_INLINE void
extent_addr_set(extent_t *extent, void *addr) {
	extent->e_addr = addr;
}

JEMALLOC_INLINE void
extent_addr_randomize(tsdn_t *tsdn, extent_t *extent, size_t alignment) {
	assert(extent_base_get(extent) == extent_addr_get(extent));

	if (alignment < PAGE) {
		unsigned lg_range = LG_PAGE -
		    lg_floor(CACHELINE_CEILING(alignment));
		size_t r =
		    prng_lg_range_zu(&extent_arena_get(extent)->offset_state,
		    lg_range, true);
		uintptr_t random_offset = ((uintptr_t)r) << (LG_PAGE -
		    lg_range);
		extent->e_addr = (void *)((uintptr_t)extent->e_addr +
		    random_offset);
		assert(ALIGNMENT_ADDR2BASE(extent->e_addr, alignment) ==
		    extent->e_addr);
	}
}

JEMALLOC_INLINE void
extent_size_set(extent_t *extent, size_t size) {
	extent->e_size = size;
}

JEMALLOC_INLINE void
extent_usize_set(extent_t *extent, size_t usize) {
	extent->e_usize = usize;
}

JEMALLOC_INLINE void
extent_sn_set(extent_t *extent, size_t sn) {
	extent->e_sn = sn;
}

JEMALLOC_INLINE void
extent_state_set(extent_t *extent, extent_state_t state) {
	extent->e_state = state;
}

JEMALLOC_INLINE void
extent_zeroed_set(extent_t *extent, bool zeroed) {
	extent->e_zeroed = zeroed;
}

JEMALLOC_INLINE void
extent_committed_set(extent_t *extent, bool committed) {
	extent->e_committed = committed;
}

JEMALLOC_INLINE void
extent_slab_set(extent_t *extent, bool slab) {
	extent->e_slab = slab;
}

JEMALLOC_INLINE void
extent_prof_tctx_set(extent_t *extent, prof_tctx_t *tctx) {
	atomic_write_p(&extent->e_prof_tctx_pun, tctx);
}

JEMALLOC_INLINE void
extent_init(extent_t *extent, arena_t *arena, void *addr, size_t size,
    size_t usize, size_t sn, extent_state_t state, bool zeroed, bool committed,
    bool slab) {
	assert(addr == PAGE_ADDR2BASE(addr) || !slab);

	extent_arena_set(extent, arena);
	extent_addr_set(extent, addr);
	extent_size_set(extent, size);
	extent_usize_set(extent, usize);
	extent_sn_set(extent, sn);
	extent_state_set(extent, state);
	extent_zeroed_set(extent, zeroed);
	extent_committed_set(extent, committed);
	extent_slab_set(extent, slab);
	if (config_prof) {
		extent_prof_tctx_set(extent, NULL);
	}
	ql_elm_new(extent, ql_link);
}

JEMALLOC_INLINE void
extent_list_init(extent_list_t *list) {
	ql_new(list);
}

JEMALLOC_INLINE extent_t *
extent_list_first(const extent_list_t *list) {
	return ql_first(list);
}

JEMALLOC_INLINE extent_t *
extent_list_last(const extent_list_t *list) {
	return ql_last(list, ql_link);
}

JEMALLOC_INLINE void
extent_list_append(extent_list_t *list, extent_t *extent) {
	ql_tail_insert(list, extent, ql_link);
}

JEMALLOC_INLINE void
extent_list_remove(extent_list_t *list, extent_t *extent) {
	ql_remove(list, extent, ql_link);
}

JEMALLOC_INLINE int
extent_sn_comp(const extent_t *a, const extent_t *b) {
	size_t a_sn = extent_sn_get(a);
	size_t b_sn = extent_sn_get(b);

	return (a_sn > b_sn) - (a_sn < b_sn);
}

JEMALLOC_INLINE int
extent_ad_comp(const extent_t *a, const extent_t *b) {
	uintptr_t a_addr = (uintptr_t)extent_addr_get(a);
	uintptr_t b_addr = (uintptr_t)extent_addr_get(b);

	return (a_addr > b_addr) - (a_addr < b_addr);
}

JEMALLOC_INLINE int
extent_snad_comp(const extent_t *a, const extent_t *b) {
	int ret;

	ret = extent_sn_comp(a, b);
	if (ret != 0) {
		return ret;
	}

	ret = extent_ad_comp(a, b);
	return ret;
}
#endif

#endif /* JEMALLOC_INTERNAL_EXTENT_INLINES_H */

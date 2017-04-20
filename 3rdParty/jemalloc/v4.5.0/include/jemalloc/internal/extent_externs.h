#ifndef JEMALLOC_INTERNAL_EXTENT_EXTERNS_H
#define JEMALLOC_INTERNAL_EXTENT_EXTERNS_H

extern rtree_t			extents_rtree;
extern const extent_hooks_t	extent_hooks_default;

extent_t	*extent_alloc(tsdn_t *tsdn, arena_t *arena);
void	extent_dalloc(tsdn_t *tsdn, arena_t *arena, extent_t *extent);

extent_hooks_t	*extent_hooks_get(arena_t *arena);
extent_hooks_t	*extent_hooks_set(arena_t *arena, extent_hooks_t *extent_hooks);

#ifdef JEMALLOC_JET
typedef size_t (extent_size_quantize_t)(size_t);
extern extent_size_quantize_t *extent_size_quantize_floor;
extern extent_size_quantize_t *extent_size_quantize_ceil;
#else
size_t	extent_size_quantize_floor(size_t size);
size_t	extent_size_quantize_ceil(size_t size);
#endif

ph_proto(, extent_heap_, extent_heap_t, extent_t)

bool extents_init(tsdn_t *tsdn, extents_t *extents, extent_state_t state,
    bool try_coalesce);
extent_state_t extents_state_get(const extents_t *extents);
size_t extents_npages_get(extents_t *extents);
extent_t *extents_evict(tsdn_t *tsdn, extents_t *extents, size_t npages_min);
void extents_prefork(tsdn_t *tsdn, extents_t *extents);
void extents_postfork_parent(tsdn_t *tsdn, extents_t *extents);
void extents_postfork_child(tsdn_t *tsdn, extents_t *extents);
extent_t	*extent_alloc_cache(tsdn_t *tsdn, arena_t *arena,
    extent_hooks_t **r_extent_hooks, void *new_addr, size_t usize, size_t pad,
    size_t alignment, bool *zero, bool *commit, bool slab);
extent_t	*extent_alloc_wrapper(tsdn_t *tsdn, arena_t *arena,
    extent_hooks_t **r_extent_hooks, void *new_addr, size_t usize, size_t pad,
    size_t alignment, bool *zero, bool *commit, bool slab);
void	extent_dalloc_gap(tsdn_t *tsdn, arena_t *arena, extent_t *extent);
void	extent_dalloc_cache(tsdn_t *tsdn, arena_t *arena,
    extent_hooks_t **r_extent_hooks, extent_t *extent);
bool	extent_dalloc_wrapper_try(tsdn_t *tsdn, arena_t *arena,
    extent_hooks_t **r_extent_hooks, extent_t *extent);
void	extent_dalloc_wrapper(tsdn_t *tsdn, arena_t *arena,
    extent_hooks_t **r_extent_hooks, extent_t *extent);
bool	extent_commit_wrapper(tsdn_t *tsdn, arena_t *arena,
    extent_hooks_t **r_extent_hooks, extent_t *extent, size_t offset,
    size_t length);
bool	extent_decommit_wrapper(tsdn_t *tsdn, arena_t *arena,
    extent_hooks_t **r_extent_hooks, extent_t *extent, size_t offset,
    size_t length);
bool	extent_purge_lazy_wrapper(tsdn_t *tsdn, arena_t *arena,
    extent_hooks_t **r_extent_hooks, extent_t *extent, size_t offset,
    size_t length);
bool	extent_purge_forced_wrapper(tsdn_t *tsdn, arena_t *arena,
    extent_hooks_t **r_extent_hooks, extent_t *extent, size_t offset,
    size_t length);
extent_t	*extent_split_wrapper(tsdn_t *tsdn, arena_t *arena,
    extent_hooks_t **r_extent_hooks, extent_t *extent, size_t size_a,
    size_t usize_a, size_t size_b, size_t usize_b);
bool	extent_merge_wrapper(tsdn_t *tsdn, arena_t *arena,
    extent_hooks_t **r_extent_hooks, extent_t *a, extent_t *b);

bool	extent_boot(void);

#endif /* JEMALLOC_INTERNAL_EXTENT_EXTERNS_H */

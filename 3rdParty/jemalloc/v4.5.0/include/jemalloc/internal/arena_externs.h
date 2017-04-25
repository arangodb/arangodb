#ifndef JEMALLOC_INTERNAL_ARENA_EXTERNS_H
#define JEMALLOC_INTERNAL_ARENA_EXTERNS_H

static const size_t	large_pad =
#ifdef JEMALLOC_CACHE_OBLIVIOUS
    PAGE
#else
    0
#endif
    ;

extern ssize_t		opt_decay_time;

extern const arena_bin_info_t	arena_bin_info[NBINS];

void arena_stats_large_nrequests_add(tsdn_t *tsdn, arena_stats_t *arena_stats,
    szind_t szind, uint64_t nrequests);
void arena_stats_mapped_add(tsdn_t *tsdn, arena_stats_t *arena_stats,
    size_t size);
void	arena_basic_stats_merge(tsdn_t *tsdn, arena_t *arena,
    unsigned *nthreads, const char **dss, ssize_t *decay_time, size_t *nactive,
    size_t *ndirty);
void arena_stats_merge(tsdn_t *tsdn, arena_t *arena, unsigned *nthreads,
    const char **dss, ssize_t *decay_time, size_t *nactive, size_t *ndirty,
    arena_stats_t *astats, malloc_bin_stats_t *bstats,
    malloc_large_stats_t *lstats);
void	arena_extent_cache_dalloc(tsdn_t *tsdn, arena_t *arena,
    extent_hooks_t **r_extent_hooks, extent_t *extent);
#ifdef JEMALLOC_JET
size_t	arena_slab_regind(extent_t *slab, szind_t binind, const void *ptr);
#endif
extent_t	*arena_extent_alloc_large(tsdn_t *tsdn, arena_t *arena,
    size_t usize, size_t alignment, bool *zero);
void	arena_extent_dalloc_large_prep(tsdn_t *tsdn, arena_t *arena,
    extent_t *extent);
void	arena_extent_dalloc_large_finish(tsdn_t *tsdn, arena_t *arena,
    extent_t *extent);
void	arena_extent_ralloc_large_shrink(tsdn_t *tsdn, arena_t *arena,
    extent_t *extent, size_t oldsize);
void	arena_extent_ralloc_large_expand(tsdn_t *tsdn, arena_t *arena,
    extent_t *extent, size_t oldsize);
ssize_t	arena_decay_time_get(tsdn_t *tsdn, arena_t *arena);
bool	arena_decay_time_set(tsdn_t *tsdn, arena_t *arena, ssize_t decay_time);
void	arena_purge(tsdn_t *tsdn, arena_t *arena, bool all);
void	arena_maybe_purge(tsdn_t *tsdn, arena_t *arena);
void	arena_reset(tsd_t *tsd, arena_t *arena);
void	arena_destroy(tsd_t *tsd, arena_t *arena);
void	arena_tcache_fill_small(tsdn_t *tsdn, arena_t *arena,
    tcache_bin_t *tbin, szind_t binind, uint64_t prof_accumbytes);
void	arena_alloc_junk_small(void *ptr, const arena_bin_info_t *bin_info,
    bool zero);
#ifdef JEMALLOC_JET
typedef void (arena_dalloc_junk_small_t)(void *, const arena_bin_info_t *);
extern arena_dalloc_junk_small_t *arena_dalloc_junk_small;
#else
void	arena_dalloc_junk_small(void *ptr, const arena_bin_info_t *bin_info);
#endif
void	*arena_malloc_hard(tsdn_t *tsdn, arena_t *arena, size_t size,
    szind_t ind, bool zero);
void	*arena_palloc(tsdn_t *tsdn, arena_t *arena, size_t usize,
    size_t alignment, bool zero, tcache_t *tcache);
void	arena_prof_promote(tsdn_t *tsdn, extent_t *extent, const void *ptr,
    size_t usize);
void	arena_dalloc_promoted(tsdn_t *tsdn, extent_t *extent, void *ptr,
    tcache_t *tcache, bool slow_path);
void	arena_dalloc_bin_junked_locked(tsdn_t *tsdn, arena_t *arena,
    extent_t *extent, void *ptr);
void	arena_dalloc_small(tsdn_t *tsdn, arena_t *arena, extent_t *extent,
    void *ptr);
bool	arena_ralloc_no_move(tsdn_t *tsdn, extent_t *extent, void *ptr,
    size_t oldsize, size_t size, size_t extra, bool zero);
void	*arena_ralloc(tsdn_t *tsdn, arena_t *arena, extent_t *extent, void *ptr,
    size_t oldsize, size_t size, size_t alignment, bool zero, tcache_t *tcache);
dss_prec_t	arena_dss_prec_get(arena_t *arena);
bool	arena_dss_prec_set(arena_t *arena, dss_prec_t dss_prec);
ssize_t	arena_decay_time_default_get(void);
bool	arena_decay_time_default_set(ssize_t decay_time);
unsigned	arena_nthreads_get(arena_t *arena, bool internal);
void	arena_nthreads_inc(arena_t *arena, bool internal);
void	arena_nthreads_dec(arena_t *arena, bool internal);
size_t	arena_extent_sn_next(arena_t *arena);
arena_t	*arena_new(tsdn_t *tsdn, unsigned ind, extent_hooks_t *extent_hooks);
void	arena_boot(void);
void	arena_prefork0(tsdn_t *tsdn, arena_t *arena);
void	arena_prefork1(tsdn_t *tsdn, arena_t *arena);
void	arena_prefork2(tsdn_t *tsdn, arena_t *arena);
void	arena_prefork3(tsdn_t *tsdn, arena_t *arena);
void	arena_postfork_parent(tsdn_t *tsdn, arena_t *arena);
void	arena_postfork_child(tsdn_t *tsdn, arena_t *arena);

#endif /* JEMALLOC_INTERNAL_ARENA_EXTERNS_H */

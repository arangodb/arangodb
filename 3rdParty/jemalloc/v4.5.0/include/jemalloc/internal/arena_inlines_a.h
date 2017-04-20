#ifndef JEMALLOC_INTERNAL_ARENA_INLINES_A_H
#define JEMALLOC_INTERNAL_ARENA_INLINES_A_H

#ifndef JEMALLOC_ENABLE_INLINE
unsigned	arena_ind_get(const arena_t *arena);
void	arena_internal_add(arena_t *arena, size_t size);
void	arena_internal_sub(arena_t *arena, size_t size);
size_t	arena_internal_get(arena_t *arena);
bool	arena_prof_accum(tsdn_t *tsdn, arena_t *arena, uint64_t accumbytes);
#endif /* JEMALLOC_ENABLE_INLINE */

#if (defined(JEMALLOC_ENABLE_INLINE) || defined(JEMALLOC_ARENA_C_))

JEMALLOC_INLINE unsigned
arena_ind_get(const arena_t *arena) {
	return base_ind_get(arena->base);
}

JEMALLOC_INLINE void
arena_internal_add(arena_t *arena, size_t size) {
	atomic_add_zu(&arena->stats.internal, size);
}

JEMALLOC_INLINE void
arena_internal_sub(arena_t *arena, size_t size) {
	atomic_sub_zu(&arena->stats.internal, size);
}

JEMALLOC_INLINE size_t
arena_internal_get(arena_t *arena) {
	return atomic_read_zu(&arena->stats.internal);
}

JEMALLOC_INLINE bool
arena_prof_accum(tsdn_t *tsdn, arena_t *arena, uint64_t accumbytes) {
	cassert(config_prof);

	if (likely(prof_interval == 0)) {
		return false;
	}

	return prof_accum_add(tsdn, &arena->prof_accum, accumbytes);
}

#endif /* (defined(JEMALLOC_ENABLE_INLINE) || defined(JEMALLOC_ARENA_C_)) */

#endif /* JEMALLOC_INTERNAL_ARENA_INLINES_A_H */

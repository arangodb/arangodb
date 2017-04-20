#ifndef JEMALLOC_INTERNAL_BASE_INLINES_H
#define JEMALLOC_INTERNAL_BASE_INLINES_H

#ifndef JEMALLOC_ENABLE_INLINE
unsigned	base_ind_get(const base_t *base);
#endif

#if (defined(JEMALLOC_ENABLE_INLINE) || defined(JEMALLOC_BASE_C_))
JEMALLOC_INLINE unsigned
base_ind_get(const base_t *base) {
	return base->ind;
}
#endif

#endif /* JEMALLOC_INTERNAL_BASE_INLINES_H */

#ifndef JEMALLOC_INTERNAL_EXTENT_DSS_EXTERNS_H
#define JEMALLOC_INTERNAL_EXTENT_DSS_EXTERNS_H

extern const char	*opt_dss;

dss_prec_t	extent_dss_prec_get(void);
bool	extent_dss_prec_set(dss_prec_t dss_prec);
void	*extent_alloc_dss(tsdn_t *tsdn, arena_t *arena, void *new_addr,
    size_t size, size_t alignment, bool *zero, bool *commit);
bool	extent_in_dss(void *addr);
bool	extent_dss_mergeable(void *addr_a, void *addr_b);
void	extent_dss_boot(void);

#endif /* JEMALLOC_INTERNAL_EXTENT_DSS_EXTERNS_H */

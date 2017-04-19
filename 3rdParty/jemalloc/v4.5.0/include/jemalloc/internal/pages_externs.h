#ifndef JEMALLOC_INTERNAL_PAGES_EXTERNS_H
#define JEMALLOC_INTERNAL_PAGES_EXTERNS_H

static const bool pages_can_purge_lazy =
#ifdef PAGES_CAN_PURGE_LAZY
    true
#else
    false
#endif
    ;
static const bool pages_can_purge_forced =
#ifdef PAGES_CAN_PURGE_FORCED
    true
#else
    false
#endif
    ;

void	*pages_map(void *addr, size_t size, bool *commit);
void	pages_unmap(void *addr, size_t size);
void	*pages_trim(void *addr, size_t alloc_size, size_t leadsize,
    size_t size, bool *commit);
bool	pages_commit(void *addr, size_t size);
bool	pages_decommit(void *addr, size_t size);
bool	pages_purge_lazy(void *addr, size_t size);
bool	pages_purge_forced(void *addr, size_t size);
bool	pages_huge(void *addr, size_t size);
bool	pages_nohuge(void *addr, size_t size);
void	pages_boot(void);

#endif /* JEMALLOC_INTERNAL_PAGES_EXTERNS_H */

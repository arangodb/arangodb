#ifndef JEMALLOC_INTERNAL_MUTEX_EXTERNS_H
#define JEMALLOC_INTERNAL_MUTEX_EXTERNS_H

#ifdef JEMALLOC_LAZY_LOCK
extern bool isthreaded;
#else
#  undef isthreaded /* Undo private_namespace.h definition. */
#  define isthreaded true
#endif

bool	malloc_mutex_init(malloc_mutex_t *mutex, const char *name,
    witness_rank_t rank);
void	malloc_mutex_prefork(tsdn_t *tsdn, malloc_mutex_t *mutex);
void	malloc_mutex_postfork_parent(tsdn_t *tsdn, malloc_mutex_t *mutex);
void	malloc_mutex_postfork_child(tsdn_t *tsdn, malloc_mutex_t *mutex);
bool	malloc_mutex_boot(void);

#endif /* JEMALLOC_INTERNAL_MUTEX_EXTERNS_H */

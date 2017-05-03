#ifndef JEMALLOC_INTERNAL_TSD_EXTERNS_H
#define JEMALLOC_INTERNAL_TSD_EXTERNS_H

void	*malloc_tsd_malloc(size_t size);
void	malloc_tsd_dalloc(void *wrapper);
void	malloc_tsd_no_cleanup(void *arg);
void	malloc_tsd_cleanup_register(bool (*f)(void));
tsd_t	*malloc_tsd_boot0(void);
void	malloc_tsd_boot1(void);
#if (!defined(JEMALLOC_MALLOC_THREAD_CLEANUP) && !defined(JEMALLOC_TLS) && \
    !defined(_WIN32))
void	*tsd_init_check_recursion(tsd_init_head_t *head,
    tsd_init_block_t *block);
void	tsd_init_finish(tsd_init_head_t *head, tsd_init_block_t *block);
#endif
void	tsd_cleanup(void *arg);

#endif /* JEMALLOC_INTERNAL_TSD_EXTERNS_H */

#ifndef JEMALLOC_INTERNAL_STATS_EXTERNS_H
#define JEMALLOC_INTERNAL_STATS_EXTERNS_H

extern bool	opt_stats_print;

void	stats_print(void (*write)(void *, const char *), void *cbopaque,
    const char *opts);

#endif /* JEMALLOC_INTERNAL_STATS_EXTERNS_H */

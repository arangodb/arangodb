#ifndef JEMALLOC_INTERNAL_UTIL_EXTERNS_H
#define JEMALLOC_INTERNAL_UTIL_EXTERNS_H

int	buferror(int err, char *buf, size_t buflen);
uintmax_t	malloc_strtoumax(const char *restrict nptr,
    char **restrict endptr, int base);
void	malloc_write(const char *s);

/*
 * malloc_vsnprintf() supports a subset of snprintf(3) that avoids floating
 * point math.
 */
size_t	malloc_vsnprintf(char *str, size_t size, const char *format,
    va_list ap);
size_t	malloc_snprintf(char *str, size_t size, const char *format, ...)
    JEMALLOC_FORMAT_PRINTF(3, 4);
void	malloc_vcprintf(void (*write_cb)(void *, const char *), void *cbopaque,
    const char *format, va_list ap);
void malloc_cprintf(void (*write)(void *, const char *), void *cbopaque,
    const char *format, ...) JEMALLOC_FORMAT_PRINTF(3, 4);
void	malloc_printf(const char *format, ...) JEMALLOC_FORMAT_PRINTF(1, 2);

#endif /* JEMALLOC_INTERNAL_UTIL_EXTERNS_H */

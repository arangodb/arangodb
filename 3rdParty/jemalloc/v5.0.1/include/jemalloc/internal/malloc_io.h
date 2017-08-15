#ifndef JEMALLOC_INTERNAL_MALLOC_IO_H
#define JEMALLOC_INTERNAL_MALLOC_IO_H

#ifdef _WIN32
#  ifdef _WIN64
#    define FMT64_PREFIX "ll"
#    define FMTPTR_PREFIX "ll"
#  else
#    define FMT64_PREFIX "ll"
#    define FMTPTR_PREFIX ""
#  endif
#  define FMTd32 "d"
#  define FMTu32 "u"
#  define FMTx32 "x"
#  define FMTd64 FMT64_PREFIX "d"
#  define FMTu64 FMT64_PREFIX "u"
#  define FMTx64 FMT64_PREFIX "x"
#  define FMTdPTR FMTPTR_PREFIX "d"
#  define FMTuPTR FMTPTR_PREFIX "u"
#  define FMTxPTR FMTPTR_PREFIX "x"
#else
#  include <inttypes.h>
#  define FMTd32 PRId32
#  define FMTu32 PRIu32
#  define FMTx32 PRIx32
#  define FMTd64 PRId64
#  define FMTu64 PRIu64
#  define FMTx64 PRIx64
#  define FMTdPTR PRIdPTR
#  define FMTuPTR PRIuPTR
#  define FMTxPTR PRIxPTR
#endif

/* Size of stack-allocated buffer passed to buferror(). */
#define BUFERROR_BUF		64

/*
 * Size of stack-allocated buffer used by malloc_{,v,vc}printf().  This must be
 * large enough for all possible uses within jemalloc.
 */
#define MALLOC_PRINTF_BUFSIZE	4096

int buferror(int err, char *buf, size_t buflen);
uintmax_t malloc_strtoumax(const char *restrict nptr, char **restrict endptr,
    int base);
void malloc_write(const char *s);

/*
 * malloc_vsnprintf() supports a subset of snprintf(3) that avoids floating
 * point math.
 */
size_t malloc_vsnprintf(char *str, size_t size, const char *format,
    va_list ap);
size_t malloc_snprintf(char *str, size_t size, const char *format, ...)
    JEMALLOC_FORMAT_PRINTF(3, 4);
void malloc_vcprintf(void (*write_cb)(void *, const char *), void *cbopaque,
    const char *format, va_list ap);
void malloc_cprintf(void (*write_cb)(void *, const char *), void *cbopaque,
    const char *format, ...) JEMALLOC_FORMAT_PRINTF(3, 4);
void malloc_printf(const char *format, ...) JEMALLOC_FORMAT_PRINTF(1, 2);

#endif /* JEMALLOC_INTERNAL_MALLOC_IO_H */

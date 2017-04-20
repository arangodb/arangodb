#ifndef JEMALLOC_INTERNAL_UTIL_TYPES_H
#define JEMALLOC_INTERNAL_UTIL_TYPES_H

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

/* Junk fill patterns. */
#ifndef JEMALLOC_ALLOC_JUNK
#  define JEMALLOC_ALLOC_JUNK	((uint8_t)0xa5)
#endif
#ifndef JEMALLOC_FREE_JUNK
#  define JEMALLOC_FREE_JUNK	((uint8_t)0x5a)
#endif

/*
 * Wrap a cpp argument that contains commas such that it isn't broken up into
 * multiple arguments.
 */
#define JEMALLOC_ARG_CONCAT(...) __VA_ARGS__

/* cpp macro definition stringification. */
#define STRINGIFY_HELPER(x) #x
#define STRINGIFY(x) STRINGIFY_HELPER(x)

/*
 * Silence compiler warnings due to uninitialized values.  This is used
 * wherever the compiler fails to recognize that the variable is never used
 * uninitialized.
 */
#ifdef JEMALLOC_CC_SILENCE
#  define JEMALLOC_CC_SILENCE_INIT(v) = v
#else
#  define JEMALLOC_CC_SILENCE_INIT(v)
#endif

#ifdef __GNUC__
#  define likely(x)   __builtin_expect(!!(x), 1)
#  define unlikely(x) __builtin_expect(!!(x), 0)
#else
#  define likely(x)   !!(x)
#  define unlikely(x) !!(x)
#endif

#if !defined(JEMALLOC_INTERNAL_UNREACHABLE)
#  error JEMALLOC_INTERNAL_UNREACHABLE should have been defined by configure
#endif

#define unreachable() JEMALLOC_INTERNAL_UNREACHABLE()

#include "jemalloc/internal/assert.h"

/* Use to assert a particular configuration, e.g., cassert(config_debug). */
#define cassert(c) do {							\
	if (unlikely(!(c))) {						\
		not_reached();						\
	}								\
} while (0)

#endif /* JEMALLOC_INTERNAL_UTIL_TYPES_H */

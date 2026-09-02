#ifndef JEMALLOC_INTERNAL_JEMALLOC_PROBE_H
#define JEMALLOC_INTERNAL_JEMALLOC_PROBE_H

#include <jemalloc/internal/jemalloc_preamble.h>

#ifdef JEMALLOC_EXPERIMENTAL_USDT_STAP
#include <jemalloc/internal/jemalloc_probe_stap.h>
#elif defined(JEMALLOC_EXPERIMENTAL_USDT_CUSTOM)
#include <jemalloc/internal/jemalloc_probe_custom.h>
#elif defined(_MSC_VER)
#define JE_USDT(name, N, ...) /* Nothing */
#else /*  no USDT, just check the args */

#define JE_USDT(name, N, ...) _JE_USDT_CHECK_ARG##N(__VA_ARGS__)

#define _JE_USDT_CHECK_ARG1(a)						\
	do {								\
		(void)(a);						\
	} while (0)
#define _JE_USDT_CHECK_ARG2(a, b)					\
	do {								\
		(void)(a);						\
		(void)(b);						\
	} while (0)
#define _JE_USDT_CHECK_ARG3(a, b, c)					\
	do {								\
		(void)(a);						\
		(void)(b);						\
		(void)(c);						\
	} while (0)
#define _JE_USDT_CHECK_ARG4(a, b, c, d)					\
	do {								\
		(void)(a);						\
		(void)(b);						\
		(void)(c);						\
		(void)(d);						\
	} while (0)
#define _JE_USDT_CHECK_ARG5(a, b, c, d, e)				\
	do {								\
		(void)(a);						\
		(void)(b);						\
		(void)(c);						\
		(void)(d);						\
		(void)(e);						\
	} while (0)

#endif /* JEMALLOC_EXPERIMENTAL_USDT_* */

#endif /* JEMALLOC_INTERNAL_JEMALLOC_PROBE_H */

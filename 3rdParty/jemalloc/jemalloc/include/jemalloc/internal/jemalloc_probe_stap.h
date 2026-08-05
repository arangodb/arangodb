#ifndef JEMALLOC_INTERNAL_JEMALLOC_PROBE_STAP_H
#define JEMALLOC_INTERNAL_JEMALLOC_PROBE_STAP_H

#include <sys/sdt.h>

#define JE_USDT(name, N, ...) JE_USDT_PROBE_N(name, N, ##__VA_ARGS__)

#define JE_USDT_PROBE_N(name, N, ...)                                          \
	STAP_PROBE##N(jemalloc, name, ##__VA_ARGS__)

#endif /* JEMALLOC_INTERNAL_JEMALLOC_PROBE_STAP_H */

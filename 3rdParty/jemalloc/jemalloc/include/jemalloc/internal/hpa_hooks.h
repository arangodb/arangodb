#ifndef JEMALLOC_INTERNAL_HPA_HOOKS_H
#define JEMALLOC_INTERNAL_HPA_HOOKS_H

#include "jemalloc/internal/jemalloc_preamble.h"
#include "jemalloc/internal/nstime.h"

typedef struct hpa_hooks_s hpa_hooks_t;
struct hpa_hooks_s {
	void *(*map)(size_t size);
	void (*unmap)(void *ptr, size_t size);
	void (*purge)(void *ptr, size_t size);
	bool (*hugify)(void *ptr, size_t size, bool sync);
	void (*dehugify)(void *ptr, size_t size);
	void (*curtime)(nstime_t *r_time, bool first_reading);
	uint64_t (*ms_since)(nstime_t *r_time);
	bool (*vectorized_purge)(void *vec, size_t vlen, size_t nbytes);
};

extern const hpa_hooks_t hpa_hooks_default;

#endif /* JEMALLOC_INTERNAL_HPA_HOOKS_H */

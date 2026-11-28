#ifdef JEMALLOC_INTERNAL_TSD_WIN_H
#	error This file should be included only once, by tsd.h.
#endif
#define JEMALLOC_INTERNAL_TSD_WIN_H

#include "jemalloc/internal/jemalloc_preamble.h"
#include "jemalloc/internal/tsd_internals.h"
#include "jemalloc/internal/tsd_types.h"

/* val should always be the first field of tsd_wrapper_t since accessing
   val is the common path and having val as the first field makes it possible
   that converting a pointer to tsd_wrapper_t to a pointer to val is no more
   than a type cast. */
typedef struct {
	tsd_t val;
	bool  initialized;
} tsd_wrapper_t;

#if defined(JEMALLOC_LEGACY_WINDOWS_SUPPORT) || !defined(_MSC_VER)

extern DWORD         tsd_tsd;
extern tsd_wrapper_t tsd_boot_wrapper;
extern bool          tsd_booted;
#        if defined(_M_ARM64EC)
#                define JEMALLOC_WIN32_TLSGETVALUE2 0
#        else
#                define JEMALLOC_WIN32_TLSGETVALUE2 1
#        endif
#        if JEMALLOC_WIN32_TLSGETVALUE2
typedef LPVOID(WINAPI *TGV2)(DWORD dwTlsIndex);
extern TGV2    tls_get_value2;
extern HMODULE tgv2_mod;
#	endif

/* Initialization/cleanup. */
JEMALLOC_ALWAYS_INLINE bool
tsd_cleanup_wrapper(void) {
	DWORD          error = GetLastError();
	tsd_wrapper_t *wrapper = (tsd_wrapper_t *)TlsGetValue(tsd_tsd);
	SetLastError(error);

	if (wrapper == NULL) {
		return false;
	}

	if (wrapper->initialized) {
		wrapper->initialized = false;
		tsd_cleanup(&wrapper->val);
		if (wrapper->initialized) {
			/* Trigger another cleanup round. */
			return true;
		}
	}
	malloc_tsd_dalloc(wrapper);
	return false;
}

JEMALLOC_ALWAYS_INLINE void
tsd_wrapper_set(tsd_wrapper_t *wrapper) {
	if (!TlsSetValue(tsd_tsd, (void *)wrapper)) {
		malloc_write("<jemalloc>: Error setting TSD\n");
		abort();
	}
}

JEMALLOC_ALWAYS_INLINE tsd_wrapper_t *
tsd_wrapper_get(bool init) {
	tsd_wrapper_t *wrapper;
#	if JEMALLOC_WIN32_TLSGETVALUE2
	if (tls_get_value2 != NULL) {
		wrapper = (tsd_wrapper_t *)tls_get_value2(tsd_tsd);
	} else
#	endif
	{
		DWORD error = GetLastError();
		wrapper = (tsd_wrapper_t *)TlsGetValue(tsd_tsd);
		SetLastError(error);
	}

	if (init && unlikely(wrapper == NULL)) {
		wrapper = (tsd_wrapper_t *)malloc_tsd_malloc(
		    sizeof(tsd_wrapper_t));
		if (wrapper == NULL) {
			malloc_write("<jemalloc>: Error allocating TSD\n");
			abort();
		} else {
			wrapper->initialized = false;
			/* MSVC is finicky about aggregate initialization. */
			tsd_t tsd_initializer = TSD_INITIALIZER;
			wrapper->val = tsd_initializer;
		}
		tsd_wrapper_set(wrapper);
	}
	return wrapper;
}

JEMALLOC_ALWAYS_INLINE bool
tsd_boot0(void) {
	tsd_tsd = TlsAlloc();
	if (tsd_tsd == TLS_OUT_OF_INDEXES) {
		return true;
	}
	_malloc_tsd_cleanup_register(&tsd_cleanup_wrapper);
	tsd_wrapper_set(&tsd_boot_wrapper);
#	if JEMALLOC_WIN32_TLSGETVALUE2
	tgv2_mod = LoadLibraryA("api-ms-win-core-processthreads-l1-1-8.dll");
	if (tgv2_mod != NULL) {
		tls_get_value2 = (TGV2)GetProcAddress(tgv2_mod, "TlsGetValue2");
	}
#	endif
	tsd_booted = true;
	return false;
}

JEMALLOC_ALWAYS_INLINE void
tsd_boot1(void) {
	tsd_wrapper_t *wrapper;
	wrapper = (tsd_wrapper_t *)malloc_tsd_malloc(sizeof(tsd_wrapper_t));
	if (wrapper == NULL) {
		malloc_write("<jemalloc>: Error allocating TSD\n");
		abort();
	}
	tsd_boot_wrapper.initialized = false;
	tsd_cleanup(&tsd_boot_wrapper.val);
	wrapper->initialized = false;
	tsd_t initializer = TSD_INITIALIZER;
	wrapper->val = initializer;
	tsd_wrapper_set(wrapper);
}
JEMALLOC_ALWAYS_INLINE bool
tsd_boot(void) {
	if (tsd_boot0()) {
		return true;
	}
	tsd_boot1();
	return false;
}

JEMALLOC_ALWAYS_INLINE bool
tsd_booted_get(void) {
	return tsd_booted;
}

JEMALLOC_ALWAYS_INLINE bool
tsd_get_allocates(void) {
	return true;
}

/* Get/set. */
JEMALLOC_ALWAYS_INLINE tsd_t *
tsd_get(bool init) {
	tsd_wrapper_t *wrapper;

	assert(tsd_booted);
	wrapper = tsd_wrapper_get(init);
	if (tsd_get_allocates() && !init && wrapper == NULL) {
		return NULL;
	}
	return &wrapper->val;
}

JEMALLOC_ALWAYS_INLINE void
tsd_set(tsd_t *val) {
	tsd_wrapper_t *wrapper;

	assert(tsd_booted);
	wrapper = tsd_wrapper_get(true);
	if (likely(&wrapper->val != val)) {
		wrapper->val = *(val);
	}
	wrapper->initialized = true;
}

#else // defined(JEMALLOC_LEGACY_WINDOWS_SUPPORT) || !defined(_MSC_VER)

#	define JEMALLOC_TSD_TYPE_ATTR(type) __declspec(thread) type

extern JEMALLOC_TSD_TYPE_ATTR(tsd_wrapper_t) tsd_wrapper_tls;
extern bool tsd_booted;

/* Initialization/cleanup. */
JEMALLOC_ALWAYS_INLINE bool
tsd_cleanup_wrapper(void) {
	if (tsd_wrapper_tls.initialized) {
		tsd_wrapper_tls.initialized = false;
		tsd_cleanup(&tsd_wrapper_tls.val);
		if (tsd_wrapper_tls.initialized) {
			/* Trigger another cleanup round. */
			return true;
		}
	}
	return false;
}

/* Initialization/cleanup. */
JEMALLOC_ALWAYS_INLINE bool
tsd_boot0(void) {
	_malloc_tsd_cleanup_register(tsd_cleanup_wrapper);
	tsd_booted = true;
	return false;
}

JEMALLOC_ALWAYS_INLINE void
tsd_boot1(void) {
	/* Do nothing. */
}

JEMALLOC_ALWAYS_INLINE bool
tsd_boot(void) {
	return tsd_boot0();
}

JEMALLOC_ALWAYS_INLINE bool
tsd_booted_get(void) {
	return tsd_booted;
}

JEMALLOC_ALWAYS_INLINE bool
tsd_get_allocates(void) {
	return false;
}

/* Get/set. */
JEMALLOC_ALWAYS_INLINE tsd_t *
tsd_get(bool init) {
	return &(tsd_wrapper_tls.val);
}

JEMALLOC_ALWAYS_INLINE void
tsd_set(tsd_t *val) {
	assert(tsd_booted);
	if (likely(&(tsd_wrapper_tls.val) != val)) {
		tsd_wrapper_tls.val = (*val);
	}
	tsd_wrapper_tls.initialized = true;
}

#endif // defined(JEMALLOC_LEGACY_WINDOWS_SUPPORT) || !defined(_MSC_VER)

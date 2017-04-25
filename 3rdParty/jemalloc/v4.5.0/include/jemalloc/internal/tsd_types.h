#ifndef JEMALLOC_INTERNAL_TSD_TYPES_H
#define JEMALLOC_INTERNAL_TSD_TYPES_H

/* Maximum number of malloc_tsd users with cleanup functions. */
#define MALLOC_TSD_CLEANUPS_MAX	2

typedef bool (*malloc_tsd_cleanup_t)(void);

#if (!defined(JEMALLOC_MALLOC_THREAD_CLEANUP) && !defined(JEMALLOC_TLS) && \
    !defined(_WIN32))
typedef struct tsd_init_block_s tsd_init_block_t;
typedef struct tsd_init_head_s tsd_init_head_t;
#endif

typedef struct tsd_s tsd_t;
typedef struct tsdn_s tsdn_t;

#define TSDN_NULL	((tsdn_t *)0)

typedef enum {
	tsd_state_uninitialized,
	tsd_state_nominal,
	tsd_state_purgatory,
	tsd_state_reincarnated
} tsd_state_t;

/*
 * TLS/TSD-agnostic macro-based implementation of thread-specific data.  There
 * are five macros that support (at least) three use cases: file-private,
 * library-private, and library-private inlined.  Following is an example
 * library-private tsd variable:
 *
 * In example.h:
 *   typedef struct {
 *           int x;
 *           int y;
 *   } example_t;
 *   #define EX_INITIALIZER JEMALLOC_CONCAT({0, 0})
 *   malloc_tsd_types(example_, example_t)
 *   malloc_tsd_protos(, example_, example_t)
 *   malloc_tsd_externs(example_, example_t)
 * In example.c:
 *   malloc_tsd_data(, example_, example_t, EX_INITIALIZER)
 *   malloc_tsd_funcs(, example_, example_t, EX_INITIALIZER,
 *       example_tsd_cleanup)
 *
 * The result is a set of generated functions, e.g.:
 *
 *   bool example_tsd_boot(void) {...}
 *   bool example_tsd_booted_get(void) {...}
 *   example_t *example_tsd_get(bool init) {...}
 *   void example_tsd_set(example_t *val) {...}
 *
 * Note that all of the functions deal in terms of (a_type *) rather than
 * (a_type) so that it is possible to support non-pointer types (unlike
 * pthreads TSD).  example_tsd_cleanup() is passed an (a_type *) pointer that is
 * cast to (void *).  This means that the cleanup function needs to cast the
 * function argument to (a_type *), then dereference the resulting pointer to
 * access fields, e.g.
 *
 *   void
 *   example_tsd_cleanup(void *arg)
 *   {
 *           example_t *example = (example_t *)arg;
 *
 *           example->x = 42;
 *           [...]
 *           if ([want the cleanup function to be called again])
 *                   example_tsd_set(example);
 *   }
 *
 * If example_tsd_set() is called within example_tsd_cleanup(), it will be
 * called again.  This is similar to how pthreads TSD destruction works, except
 * that pthreads only calls the cleanup function again if the value was set to
 * non-NULL.
 */

/* malloc_tsd_types(). */
#ifdef JEMALLOC_MALLOC_THREAD_CLEANUP
#define malloc_tsd_types(a_name, a_type)
#elif (defined(JEMALLOC_TLS))
#define malloc_tsd_types(a_name, a_type)
#elif (defined(_WIN32))
#define malloc_tsd_types(a_name, a_type)				\
typedef struct {							\
	bool	initialized;						\
	a_type	val;							\
} a_name##tsd_wrapper_t;
#else
#define malloc_tsd_types(a_name, a_type)				\
typedef struct {							\
	bool	initialized;						\
	a_type	val;							\
} a_name##tsd_wrapper_t;
#endif

/* malloc_tsd_protos(). */
#define malloc_tsd_protos(a_attr, a_name, a_type)			\
a_attr bool								\
a_name##tsd_boot0(void);						\
a_attr void								\
a_name##tsd_boot1(void);						\
a_attr bool								\
a_name##tsd_boot(void);							\
a_attr bool								\
a_name##tsd_booted_get(void);						\
a_attr a_type *								\
a_name##tsd_get(bool init);						\
a_attr void								\
a_name##tsd_set(a_type *val);

/* malloc_tsd_externs(). */
#ifdef JEMALLOC_MALLOC_THREAD_CLEANUP
#define malloc_tsd_externs(a_name, a_type)				\
extern __thread a_type	a_name##tsd_tls;				\
extern __thread bool	a_name##tsd_initialized;			\
extern bool		a_name##tsd_booted;
#elif (defined(JEMALLOC_TLS))
#define malloc_tsd_externs(a_name, a_type)				\
extern __thread a_type	a_name##tsd_tls;				\
extern pthread_key_t	a_name##tsd_tsd;				\
extern bool		a_name##tsd_booted;
#elif (defined(_WIN32))
#define malloc_tsd_externs(a_name, a_type)				\
extern DWORD		a_name##tsd_tsd;				\
extern a_name##tsd_wrapper_t	a_name##tsd_boot_wrapper;		\
extern bool		a_name##tsd_booted;
#else
#define malloc_tsd_externs(a_name, a_type)				\
extern pthread_key_t	a_name##tsd_tsd;				\
extern tsd_init_head_t	a_name##tsd_init_head;				\
extern a_name##tsd_wrapper_t	a_name##tsd_boot_wrapper;		\
extern bool		a_name##tsd_booted;
#endif

/* malloc_tsd_data(). */
#ifdef JEMALLOC_MALLOC_THREAD_CLEANUP
#define malloc_tsd_data(a_attr, a_name, a_type, a_initializer)		\
a_attr __thread a_type JEMALLOC_TLS_MODEL				\
    a_name##tsd_tls = a_initializer;					\
a_attr __thread bool JEMALLOC_TLS_MODEL					\
    a_name##tsd_initialized = false;					\
a_attr bool		a_name##tsd_booted = false;
#elif (defined(JEMALLOC_TLS))
#define malloc_tsd_data(a_attr, a_name, a_type, a_initializer)		\
a_attr __thread a_type JEMALLOC_TLS_MODEL				\
    a_name##tsd_tls = a_initializer;					\
a_attr pthread_key_t	a_name##tsd_tsd;				\
a_attr bool		a_name##tsd_booted = false;
#elif (defined(_WIN32))
#define malloc_tsd_data(a_attr, a_name, a_type, a_initializer)		\
a_attr DWORD		a_name##tsd_tsd;				\
a_attr a_name##tsd_wrapper_t a_name##tsd_boot_wrapper = {		\
	false,								\
	a_initializer							\
};									\
a_attr bool		a_name##tsd_booted = false;
#else
#define malloc_tsd_data(a_attr, a_name, a_type, a_initializer)		\
a_attr pthread_key_t	a_name##tsd_tsd;				\
a_attr tsd_init_head_t	a_name##tsd_init_head = {			\
	ql_head_initializer(blocks),					\
	MALLOC_MUTEX_INITIALIZER					\
};									\
a_attr a_name##tsd_wrapper_t a_name##tsd_boot_wrapper = {		\
	false,								\
	a_initializer							\
};									\
a_attr bool		a_name##tsd_booted = false;
#endif

/* malloc_tsd_funcs(). */
#ifdef JEMALLOC_MALLOC_THREAD_CLEANUP
#define malloc_tsd_funcs(a_attr, a_name, a_type, a_initializer,		\
    a_cleanup)								\
/* Initialization/cleanup. */						\
a_attr bool								\
a_name##tsd_cleanup_wrapper(void) {					\
	if (a_name##tsd_initialized) {					\
		a_name##tsd_initialized = false;			\
		a_cleanup(&a_name##tsd_tls);				\
	}								\
	return a_name##tsd_initialized;					\
}									\
a_attr bool								\
a_name##tsd_boot0(void) {						\
	if (a_cleanup != malloc_tsd_no_cleanup) {			\
		malloc_tsd_cleanup_register(				\
		    &a_name##tsd_cleanup_wrapper);			\
	}								\
	a_name##tsd_booted = true;					\
	return false;							\
}									\
a_attr void								\
a_name##tsd_boot1(void) {						\
	/* Do nothing. */						\
}									\
a_attr bool								\
a_name##tsd_boot(void) {						\
	return a_name##tsd_boot0();					\
}									\
a_attr bool								\
a_name##tsd_booted_get(void) {						\
	return a_name##tsd_booted;					\
}									\
a_attr bool								\
a_name##tsd_get_allocates(void) {					\
	return false;							\
}									\
/* Get/set. */								\
a_attr a_type *								\
a_name##tsd_get(bool init) {						\
	assert(a_name##tsd_booted);					\
	return &a_name##tsd_tls;					\
}									\
a_attr void								\
a_name##tsd_set(a_type *val) {						\
	assert(a_name##tsd_booted);					\
	if (likely(&a_name##tsd_tls != val)) {				\
		a_name##tsd_tls = (*val);				\
	}								\
	if (a_cleanup != malloc_tsd_no_cleanup) {			\
		a_name##tsd_initialized = true;				\
	}								\
}
#elif (defined(JEMALLOC_TLS))
#define malloc_tsd_funcs(a_attr, a_name, a_type, a_initializer,		\
    a_cleanup)								\
/* Initialization/cleanup. */						\
a_attr bool								\
a_name##tsd_boot0(void) {						\
	if (a_cleanup != malloc_tsd_no_cleanup) {			\
		if (pthread_key_create(&a_name##tsd_tsd, a_cleanup) !=	\
		    0) {						\
			return true;					\
		}							\
	}								\
	a_name##tsd_booted = true;					\
	return false;							\
}									\
a_attr void								\
a_name##tsd_boot1(void) {						\
	/* Do nothing. */						\
}									\
a_attr bool								\
a_name##tsd_boot(void) {						\
	return a_name##tsd_boot0();					\
}									\
a_attr bool								\
a_name##tsd_booted_get(void) {						\
	return a_name##tsd_booted;					\
}									\
a_attr bool								\
a_name##tsd_get_allocates(void) {					\
	return false;							\
}									\
/* Get/set. */								\
a_attr a_type *								\
a_name##tsd_get(bool init) {						\
	assert(a_name##tsd_booted);					\
	return &a_name##tsd_tls;					\
}									\
a_attr void								\
a_name##tsd_set(a_type *val) {						\
	assert(a_name##tsd_booted);					\
	if (likely(&a_name##tsd_tls != val)) {				\
		a_name##tsd_tls = (*val);				\
	}								\
	if (a_cleanup != malloc_tsd_no_cleanup) {			\
		if (pthread_setspecific(a_name##tsd_tsd,		\
		    (void *)(&a_name##tsd_tls))) {			\
			malloc_write("<jemalloc>: Error"		\
			    " setting TSD for "#a_name"\n");		\
			if (opt_abort) {				\
				abort();				\
			}						\
		}							\
	}								\
}
#elif (defined(_WIN32))
#define malloc_tsd_funcs(a_attr, a_name, a_type, a_initializer,		\
    a_cleanup)								\
/* Initialization/cleanup. */						\
a_attr bool								\
a_name##tsd_cleanup_wrapper(void) {					\
	DWORD error = GetLastError();					\
	a_name##tsd_wrapper_t *wrapper = (a_name##tsd_wrapper_t *)	\
	    TlsGetValue(a_name##tsd_tsd);				\
	SetLastError(error);						\
									\
	if (wrapper == NULL) {						\
		return false;						\
	}								\
	if (a_cleanup != malloc_tsd_no_cleanup &&			\
	    wrapper->initialized) {					\
		wrapper->initialized = false;				\
		a_cleanup(&wrapper->val);				\
		if (wrapper->initialized) {				\
			/* Trigger another cleanup round. */		\
			return true;					\
		}							\
	}								\
	malloc_tsd_dalloc(wrapper);					\
	return false;							\
}									\
a_attr void								\
a_name##tsd_wrapper_set(a_name##tsd_wrapper_t *wrapper) {		\
	if (!TlsSetValue(a_name##tsd_tsd, (void *)wrapper)) {		\
		malloc_write("<jemalloc>: Error setting"		\
		    " TSD for "#a_name"\n");				\
		abort();						\
	}								\
}									\
a_attr a_name##tsd_wrapper_t *						\
a_name##tsd_wrapper_get(bool init) {					\
	DWORD error = GetLastError();					\
	a_name##tsd_wrapper_t *wrapper = (a_name##tsd_wrapper_t *)	\
	    TlsGetValue(a_name##tsd_tsd);				\
	SetLastError(error);						\
									\
	if (init && unlikely(wrapper == NULL)) {			\
		wrapper = (a_name##tsd_wrapper_t *)			\
		    malloc_tsd_malloc(sizeof(a_name##tsd_wrapper_t));	\
		if (wrapper == NULL) {					\
			malloc_write("<jemalloc>: Error allocating"	\
			    " TSD for "#a_name"\n");			\
			abort();					\
		} else {						\
			wrapper->initialized = false;			\
			wrapper->val = a_initializer;			\
		}							\
		a_name##tsd_wrapper_set(wrapper);			\
	}								\
	return wrapper;							\
}									\
a_attr bool								\
a_name##tsd_boot0(void) {						\
	a_name##tsd_tsd = TlsAlloc();					\
	if (a_name##tsd_tsd == TLS_OUT_OF_INDEXES) {			\
		return true;						\
	}								\
	if (a_cleanup != malloc_tsd_no_cleanup) {			\
		malloc_tsd_cleanup_register(				\
		    &a_name##tsd_cleanup_wrapper);			\
	}								\
	a_name##tsd_wrapper_set(&a_name##tsd_boot_wrapper);		\
	a_name##tsd_booted = true;					\
	return false;							\
}									\
a_attr void								\
a_name##tsd_boot1(void) {						\
	a_name##tsd_wrapper_t *wrapper;					\
	wrapper = (a_name##tsd_wrapper_t *)				\
	    malloc_tsd_malloc(sizeof(a_name##tsd_wrapper_t));		\
	if (wrapper == NULL) {						\
		malloc_write("<jemalloc>: Error allocating"		\
		    " TSD for "#a_name"\n");				\
		abort();						\
	}								\
	memcpy(wrapper, &a_name##tsd_boot_wrapper,			\
	    sizeof(a_name##tsd_wrapper_t));				\
	a_name##tsd_wrapper_set(wrapper);				\
}									\
a_attr bool								\
a_name##tsd_boot(void) {						\
	if (a_name##tsd_boot0()) {					\
		return true;						\
	}								\
	a_name##tsd_boot1();						\
	return false;							\
}									\
a_attr bool								\
a_name##tsd_booted_get(void) {						\
	return a_name##tsd_booted;					\
}									\
a_attr bool								\
a_name##tsd_get_allocates(void) {					\
	return true;							\
}									\
/* Get/set. */								\
a_attr a_type *								\
a_name##tsd_get(bool init) {						\
	a_name##tsd_wrapper_t *wrapper;					\
									\
	assert(a_name##tsd_booted);					\
	wrapper = a_name##tsd_wrapper_get(init);			\
	if (a_name##tsd_get_allocates() && !init && wrapper == NULL) {	\
		return NULL;						\
	}								\
	return &wrapper->val;						\
}									\
a_attr void								\
a_name##tsd_set(a_type *val) {						\
	a_name##tsd_wrapper_t *wrapper;					\
									\
	assert(a_name##tsd_booted);					\
	wrapper = a_name##tsd_wrapper_get(true);			\
	if (likely(&wrapper->val != val)) {				\
		wrapper->val = *(val);					\
	}								\
	if (a_cleanup != malloc_tsd_no_cleanup) {			\
		wrapper->initialized = true;				\
	}								\
}
#else
#define malloc_tsd_funcs(a_attr, a_name, a_type, a_initializer,		\
    a_cleanup)								\
/* Initialization/cleanup. */						\
a_attr void								\
a_name##tsd_cleanup_wrapper(void *arg) {				\
	a_name##tsd_wrapper_t *wrapper = (a_name##tsd_wrapper_t *)arg;	\
									\
	if (a_cleanup != malloc_tsd_no_cleanup &&			\
	    wrapper->initialized) {					\
		wrapper->initialized = false;				\
		a_cleanup(&wrapper->val);				\
		if (wrapper->initialized) {				\
			/* Trigger another cleanup round. */		\
			if (pthread_setspecific(a_name##tsd_tsd,	\
			    (void *)wrapper)) {				\
				malloc_write("<jemalloc>: Error"	\
				    " setting TSD for "#a_name"\n");	\
				if (opt_abort) {			\
					abort();			\
				}					\
			}						\
			return;						\
		}							\
	}								\
	malloc_tsd_dalloc(wrapper);					\
}									\
a_attr void								\
a_name##tsd_wrapper_set(a_name##tsd_wrapper_t *wrapper) {		\
	if (pthread_setspecific(a_name##tsd_tsd,			\
	    (void *)wrapper)) {						\
		malloc_write("<jemalloc>: Error setting"		\
		    " TSD for "#a_name"\n");				\
		abort();						\
	}								\
}									\
a_attr a_name##tsd_wrapper_t *						\
a_name##tsd_wrapper_get(bool init) {					\
	a_name##tsd_wrapper_t *wrapper = (a_name##tsd_wrapper_t *)	\
	    pthread_getspecific(a_name##tsd_tsd);			\
									\
	if (init && unlikely(wrapper == NULL)) {			\
		tsd_init_block_t block;					\
		wrapper = (a_name##tsd_wrapper_t *)			\
		    tsd_init_check_recursion(&a_name##tsd_init_head,	\
		    &block);						\
		if (wrapper) {						\
			return wrapper;					\
		}							\
		wrapper = (a_name##tsd_wrapper_t *)			\
		    malloc_tsd_malloc(sizeof(a_name##tsd_wrapper_t));	\
		block.data = (void *)wrapper;				\
		if (wrapper == NULL) {					\
			malloc_write("<jemalloc>: Error allocating"	\
			    " TSD for "#a_name"\n");			\
			abort();					\
		} else {						\
			wrapper->initialized = false;			\
			wrapper->val = a_initializer;			\
		}							\
		a_name##tsd_wrapper_set(wrapper);			\
		tsd_init_finish(&a_name##tsd_init_head, &block);	\
	}								\
	return wrapper;							\
}									\
a_attr bool								\
a_name##tsd_boot0(void) {						\
	if (pthread_key_create(&a_name##tsd_tsd,			\
	    a_name##tsd_cleanup_wrapper) != 0) {			\
		return true;						\
	}								\
	a_name##tsd_wrapper_set(&a_name##tsd_boot_wrapper);		\
	a_name##tsd_booted = true;					\
	return false;							\
}									\
a_attr void								\
a_name##tsd_boot1(void) {						\
	a_name##tsd_wrapper_t *wrapper;					\
	wrapper = (a_name##tsd_wrapper_t *)				\
	    malloc_tsd_malloc(sizeof(a_name##tsd_wrapper_t));		\
	if (wrapper == NULL) {						\
		malloc_write("<jemalloc>: Error allocating"		\
		    " TSD for "#a_name"\n");				\
		abort();						\
	}								\
	memcpy(wrapper, &a_name##tsd_boot_wrapper,			\
	    sizeof(a_name##tsd_wrapper_t));				\
	a_name##tsd_wrapper_set(wrapper);				\
}									\
a_attr bool								\
a_name##tsd_boot(void) {						\
	if (a_name##tsd_boot0()) {					\
		return true;						\
	}								\
	a_name##tsd_boot1();						\
	return false;							\
}									\
a_attr bool								\
a_name##tsd_booted_get(void) {						\
	return a_name##tsd_booted;					\
}									\
a_attr bool								\
a_name##tsd_get_allocates(void) {					\
	return true;							\
}									\
/* Get/set. */								\
a_attr a_type *								\
a_name##tsd_get(bool init) {						\
	a_name##tsd_wrapper_t *wrapper;					\
									\
	assert(a_name##tsd_booted);					\
	wrapper = a_name##tsd_wrapper_get(init);			\
	if (a_name##tsd_get_allocates() && !init && wrapper == NULL) {	\
		return NULL;						\
	}								\
	return &wrapper->val;						\
}									\
a_attr void								\
a_name##tsd_set(a_type *val) {						\
	a_name##tsd_wrapper_t *wrapper;					\
									\
	assert(a_name##tsd_booted);					\
	wrapper = a_name##tsd_wrapper_get(true);			\
	if (likely(&wrapper->val != val)) {				\
		wrapper->val = *(val);					\
	}								\
	if (a_cleanup != malloc_tsd_no_cleanup) {			\
		wrapper->initialized = true;				\
	}								\
}
#endif

#endif /* JEMALLOC_INTERNAL_TSD_TYPES_H */

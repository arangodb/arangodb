#define JEMALLOC_C_
#include "jemalloc/internal/jemalloc_internal.h"

/******************************************************************************/
/* Data. */

/* Runtime configuration options. */
const char	*je_malloc_conf
#ifndef _WIN32
    JEMALLOC_ATTR(weak)
#endif
    ;
bool	opt_abort =
#ifdef JEMALLOC_DEBUG
    true
#else
    false
#endif
    ;
const char	*opt_junk =
#if (defined(JEMALLOC_DEBUG) && defined(JEMALLOC_FILL))
    "true"
#else
    "false"
#endif
    ;
bool	opt_junk_alloc =
#if (defined(JEMALLOC_DEBUG) && defined(JEMALLOC_FILL))
    true
#else
    false
#endif
    ;
bool	opt_junk_free =
#if (defined(JEMALLOC_DEBUG) && defined(JEMALLOC_FILL))
    true
#else
    false
#endif
    ;

bool	opt_utrace = false;
bool	opt_xmalloc = false;
bool	opt_zero = false;
unsigned	opt_narenas = 0;

unsigned	ncpus;

/* Protects arenas initialization. */
static malloc_mutex_t	arenas_lock;
/*
 * Arenas that are used to service external requests.  Not all elements of the
 * arenas array are necessarily used; arenas are created lazily as needed.
 *
 * arenas[0..narenas_auto) are used for automatic multiplexing of threads and
 * arenas.  arenas[narenas_auto..narenas_total) are only used if the application
 * takes some action to create them and allocate from them.
 */
arena_t			**arenas;
static unsigned		narenas_total; /* Use narenas_total_*(). */
static arena_t		*a0; /* arenas[0]; read-only after initialization. */
unsigned		narenas_auto; /* Read-only after initialization. */

typedef enum {
	malloc_init_uninitialized	= 3,
	malloc_init_a0_initialized	= 2,
	malloc_init_recursible		= 1,
	malloc_init_initialized		= 0 /* Common case --> jnz. */
} malloc_init_t;
static malloc_init_t	malloc_init_state = malloc_init_uninitialized;

/* False should be the common case.  Set to true to trigger initialization. */
static bool	malloc_slow = true;

/* When malloc_slow is true, set the corresponding bits for sanity check. */
enum {
	flag_opt_junk_alloc	= (1U),
	flag_opt_junk_free	= (1U << 1),
	flag_opt_zero		= (1U << 2),
	flag_opt_utrace		= (1U << 3),
	flag_opt_xmalloc	= (1U << 4)
};
static uint8_t	malloc_slow_flags;

JEMALLOC_ALIGNED(CACHELINE)
const size_t	pind2sz_tab[NPSIZES+1] = {
#define PSZ_yes(lg_grp, ndelta, lg_delta)				\
	(((ZU(1)<<lg_grp) + (ZU(ndelta)<<lg_delta))),
#define PSZ_no(lg_grp, ndelta, lg_delta)
#define SC(index, lg_grp, lg_delta, ndelta, psz, bin, pgs, lg_delta_lookup) \
	PSZ_##psz(lg_grp, ndelta, lg_delta)
	SIZE_CLASSES
#undef PSZ_yes
#undef PSZ_no
#undef SC
	(LARGE_MAXCLASS + PAGE)
};

JEMALLOC_ALIGNED(CACHELINE)
const size_t	index2size_tab[NSIZES] = {
#define SC(index, lg_grp, lg_delta, ndelta, psz, bin, pgs, lg_delta_lookup) \
	((ZU(1)<<lg_grp) + (ZU(ndelta)<<lg_delta)),
	SIZE_CLASSES
#undef SC
};

JEMALLOC_ALIGNED(CACHELINE)
const uint8_t	size2index_tab[] = {
#if LG_TINY_MIN == 0
#warning "Dangerous LG_TINY_MIN"
#define S2B_0(i)	i,
#elif LG_TINY_MIN == 1
#warning "Dangerous LG_TINY_MIN"
#define S2B_1(i)	i,
#elif LG_TINY_MIN == 2
#warning "Dangerous LG_TINY_MIN"
#define S2B_2(i)	i,
#elif LG_TINY_MIN == 3
#define S2B_3(i)	i,
#elif LG_TINY_MIN == 4
#define S2B_4(i)	i,
#elif LG_TINY_MIN == 5
#define S2B_5(i)	i,
#elif LG_TINY_MIN == 6
#define S2B_6(i)	i,
#elif LG_TINY_MIN == 7
#define S2B_7(i)	i,
#elif LG_TINY_MIN == 8
#define S2B_8(i)	i,
#elif LG_TINY_MIN == 9
#define S2B_9(i)	i,
#elif LG_TINY_MIN == 10
#define S2B_10(i)	i,
#elif LG_TINY_MIN == 11
#define S2B_11(i)	i,
#else
#error "Unsupported LG_TINY_MIN"
#endif
#if LG_TINY_MIN < 1
#define S2B_1(i)	S2B_0(i) S2B_0(i)
#endif
#if LG_TINY_MIN < 2
#define S2B_2(i)	S2B_1(i) S2B_1(i)
#endif
#if LG_TINY_MIN < 3
#define S2B_3(i)	S2B_2(i) S2B_2(i)
#endif
#if LG_TINY_MIN < 4
#define S2B_4(i)	S2B_3(i) S2B_3(i)
#endif
#if LG_TINY_MIN < 5
#define S2B_5(i)	S2B_4(i) S2B_4(i)
#endif
#if LG_TINY_MIN < 6
#define S2B_6(i)	S2B_5(i) S2B_5(i)
#endif
#if LG_TINY_MIN < 7
#define S2B_7(i)	S2B_6(i) S2B_6(i)
#endif
#if LG_TINY_MIN < 8
#define S2B_8(i)	S2B_7(i) S2B_7(i)
#endif
#if LG_TINY_MIN < 9
#define S2B_9(i)	S2B_8(i) S2B_8(i)
#endif
#if LG_TINY_MIN < 10
#define S2B_10(i)	S2B_9(i) S2B_9(i)
#endif
#if LG_TINY_MIN < 11
#define S2B_11(i)	S2B_10(i) S2B_10(i)
#endif
#define S2B_no(i)
#define SC(index, lg_grp, lg_delta, ndelta, psz, bin, pgs, lg_delta_lookup) \
	S2B_##lg_delta_lookup(index)
	SIZE_CLASSES
#undef S2B_3
#undef S2B_4
#undef S2B_5
#undef S2B_6
#undef S2B_7
#undef S2B_8
#undef S2B_9
#undef S2B_10
#undef S2B_11
#undef S2B_no
#undef SC
};

#ifdef JEMALLOC_THREADED_INIT
/* Used to let the initializing thread recursively allocate. */
#  define NO_INITIALIZER	((unsigned long)0)
#  define INITIALIZER		pthread_self()
#  define IS_INITIALIZER	(malloc_initializer == pthread_self())
static pthread_t		malloc_initializer = NO_INITIALIZER;
#else
#  define NO_INITIALIZER	false
#  define INITIALIZER		true
#  define IS_INITIALIZER	malloc_initializer
static bool			malloc_initializer = NO_INITIALIZER;
#endif

/* Used to avoid initialization races. */
#ifdef _WIN32
#if _WIN32_WINNT >= 0x0600
static malloc_mutex_t	init_lock = SRWLOCK_INIT;
#else
static malloc_mutex_t	init_lock;
static bool init_lock_initialized = false;

JEMALLOC_ATTR(constructor)
static void WINAPI
_init_init_lock(void) {
	/*
	 * If another constructor in the same binary is using mallctl to e.g.
	 * set up extent hooks, it may end up running before this one, and
	 * malloc_init_hard will crash trying to lock the uninitialized lock. So
	 * we force an initialization of the lock in malloc_init_hard as well.
	 * We don't try to care about atomicity of the accessed to the
	 * init_lock_initialized boolean, since it really only matters early in
	 * the process creation, before any separate thread normally starts
	 * doing anything.
	 */
	if (!init_lock_initialized) {
		malloc_mutex_init(&init_lock, "init", WITNESS_RANK_INIT);
	}
	init_lock_initialized = true;
}

#ifdef _MSC_VER
#  pragma section(".CRT$XCU", read)
JEMALLOC_SECTION(".CRT$XCU") JEMALLOC_ATTR(used)
static const void (WINAPI *init_init_lock)(void) = _init_init_lock;
#endif
#endif
#else
static malloc_mutex_t	init_lock = MALLOC_MUTEX_INITIALIZER;
#endif

typedef struct {
	void	*p;	/* Input pointer (as in realloc(p, s)). */
	size_t	s;	/* Request size. */
	void	*r;	/* Result pointer. */
} malloc_utrace_t;

#ifdef JEMALLOC_UTRACE
#  define UTRACE(a, b, c) do {						\
	if (unlikely(opt_utrace)) {					\
		int utrace_serrno = errno;				\
		malloc_utrace_t ut;					\
		ut.p = (a);						\
		ut.s = (b);						\
		ut.r = (c);						\
		utrace(&ut, sizeof(ut));				\
		errno = utrace_serrno;					\
	}								\
} while (0)
#else
#  define UTRACE(a, b, c)
#endif

/******************************************************************************/
/*
 * Function prototypes for static functions that are referenced prior to
 * definition.
 */

static bool	malloc_init_hard_a0(void);
static bool	malloc_init_hard(void);

/******************************************************************************/
/*
 * Begin miscellaneous support functions.
 */

JEMALLOC_ALWAYS_INLINE_C bool
malloc_initialized(void) {
	return (malloc_init_state == malloc_init_initialized);
}

JEMALLOC_ALWAYS_INLINE_C bool
malloc_init_a0(void) {
	if (unlikely(malloc_init_state == malloc_init_uninitialized)) {
		return malloc_init_hard_a0();
	}
	return false;
}

JEMALLOC_ALWAYS_INLINE_C bool
malloc_init(void) {
	if (unlikely(!malloc_initialized()) && malloc_init_hard()) {
		return true;
	}
	return false;
}

/*
 * The a0*() functions are used instead of i{d,}alloc() in situations that
 * cannot tolerate TLS variable access.
 */

static void *
a0ialloc(size_t size, bool zero, bool is_internal) {
	if (unlikely(malloc_init_a0())) {
		return NULL;
	}

	return iallocztm(TSDN_NULL, size, size2index(size), zero, NULL,
	    is_internal, arena_get(TSDN_NULL, 0, true), true);
}

static void
a0idalloc(extent_t *extent, void *ptr, bool is_internal) {
	idalloctm(TSDN_NULL, extent, ptr, false, is_internal, true);
}

void *
a0malloc(size_t size) {
	return a0ialloc(size, false, true);
}

void
a0dalloc(void *ptr) {
	a0idalloc(iealloc(NULL, ptr), ptr, true);
}

/*
 * FreeBSD's libc uses the bootstrap_*() functions in bootstrap-senstive
 * situations that cannot tolerate TLS variable access (TLS allocation and very
 * early internal data structure initialization).
 */

void *
bootstrap_malloc(size_t size) {
	if (unlikely(size == 0)) {
		size = 1;
	}

	return a0ialloc(size, false, false);
}

void *
bootstrap_calloc(size_t num, size_t size) {
	size_t num_size;

	num_size = num * size;
	if (unlikely(num_size == 0)) {
		assert(num == 0 || size == 0);
		num_size = 1;
	}

	return a0ialloc(num_size, true, false);
}

void
bootstrap_free(void *ptr) {
	if (unlikely(ptr == NULL)) {
		return;
	}

	a0idalloc(iealloc(NULL, ptr), ptr, false);
}

void
arena_set(unsigned ind, arena_t *arena) {
	atomic_write_p((void **)&arenas[ind], arena);
}

static void
narenas_total_set(unsigned narenas) {
	atomic_write_u(&narenas_total, narenas);
}

static void
narenas_total_inc(void) {
	atomic_add_u(&narenas_total, 1);
}

unsigned
narenas_total_get(void) {
	return atomic_read_u(&narenas_total);
}

/* Create a new arena and insert it into the arenas array at index ind. */
static arena_t *
arena_init_locked(tsdn_t *tsdn, unsigned ind, extent_hooks_t *extent_hooks) {
	arena_t *arena;

	assert(ind <= narenas_total_get());
	if (ind > MALLOCX_ARENA_MAX) {
		return NULL;
	}
	if (ind == narenas_total_get()) {
		narenas_total_inc();
	}

	/*
	 * Another thread may have already initialized arenas[ind] if it's an
	 * auto arena.
	 */
	arena = arena_get(tsdn, ind, false);
	if (arena != NULL) {
		assert(ind < narenas_auto);
		return arena;
	}

	/* Actually initialize the arena. */
	arena = arena_new(tsdn, ind, extent_hooks);
	arena_set(ind, arena);
	return arena;
}

arena_t *
arena_init(tsdn_t *tsdn, unsigned ind, extent_hooks_t *extent_hooks) {
	arena_t *arena;

	malloc_mutex_lock(tsdn, &arenas_lock);
	arena = arena_init_locked(tsdn, ind, extent_hooks);
	malloc_mutex_unlock(tsdn, &arenas_lock);
	return arena;
}

static void
arena_bind(tsd_t *tsd, unsigned ind, bool internal) {
	arena_t *arena;

	if (!tsd_nominal(tsd)) {
		return;
	}

	arena = arena_get(tsd_tsdn(tsd), ind, false);
	arena_nthreads_inc(arena, internal);

	if (internal) {
		tsd_iarena_set(tsd, arena);
	} else {
		tsd_arena_set(tsd, arena);
	}
}

void
arena_migrate(tsd_t *tsd, unsigned oldind, unsigned newind) {
	arena_t *oldarena, *newarena;

	oldarena = arena_get(tsd_tsdn(tsd), oldind, false);
	newarena = arena_get(tsd_tsdn(tsd), newind, false);
	arena_nthreads_dec(oldarena, false);
	arena_nthreads_inc(newarena, false);
	tsd_arena_set(tsd, newarena);
}

static void
arena_unbind(tsd_t *tsd, unsigned ind, bool internal) {
	arena_t *arena;

	arena = arena_get(tsd_tsdn(tsd), ind, false);
	arena_nthreads_dec(arena, internal);
	if (internal) {
		tsd_iarena_set(tsd, NULL);
	} else {
		tsd_arena_set(tsd, NULL);
	}
}

arena_tdata_t *
arena_tdata_get_hard(tsd_t *tsd, unsigned ind) {
	arena_tdata_t *tdata, *arenas_tdata_old;
	arena_tdata_t *arenas_tdata = tsd_arenas_tdata_get(tsd);
	unsigned narenas_tdata_old, i;
	unsigned narenas_tdata = tsd_narenas_tdata_get(tsd);
	unsigned narenas_actual = narenas_total_get();

	/*
	 * Dissociate old tdata array (and set up for deallocation upon return)
	 * if it's too small.
	 */
	if (arenas_tdata != NULL && narenas_tdata < narenas_actual) {
		arenas_tdata_old = arenas_tdata;
		narenas_tdata_old = narenas_tdata;
		arenas_tdata = NULL;
		narenas_tdata = 0;
		tsd_arenas_tdata_set(tsd, arenas_tdata);
		tsd_narenas_tdata_set(tsd, narenas_tdata);
	} else {
		arenas_tdata_old = NULL;
		narenas_tdata_old = 0;
	}

	/* Allocate tdata array if it's missing. */
	if (arenas_tdata == NULL) {
		bool *arenas_tdata_bypassp = tsd_arenas_tdata_bypassp_get(tsd);
		narenas_tdata = (ind < narenas_actual) ? narenas_actual : ind+1;

		if (tsd_nominal(tsd) && !*arenas_tdata_bypassp) {
			*arenas_tdata_bypassp = true;
			arenas_tdata = (arena_tdata_t *)a0malloc(
			    sizeof(arena_tdata_t) * narenas_tdata);
			*arenas_tdata_bypassp = false;
		}
		if (arenas_tdata == NULL) {
			tdata = NULL;
			goto label_return;
		}
		assert(tsd_nominal(tsd) && !*arenas_tdata_bypassp);
		tsd_arenas_tdata_set(tsd, arenas_tdata);
		tsd_narenas_tdata_set(tsd, narenas_tdata);
	}

	/*
	 * Copy to tdata array.  It's possible that the actual number of arenas
	 * has increased since narenas_total_get() was called above, but that
	 * causes no correctness issues unless two threads concurrently execute
	 * the arenas.create mallctl, which we trust mallctl synchronization to
	 * prevent.
	 */

	/* Copy/initialize tickers. */
	for (i = 0; i < narenas_actual; i++) {
		if (i < narenas_tdata_old) {
			ticker_copy(&arenas_tdata[i].decay_ticker,
			    &arenas_tdata_old[i].decay_ticker);
		} else {
			ticker_init(&arenas_tdata[i].decay_ticker,
			    DECAY_NTICKS_PER_UPDATE);
		}
	}
	if (narenas_tdata > narenas_actual) {
		memset(&arenas_tdata[narenas_actual], 0, sizeof(arena_tdata_t)
		    * (narenas_tdata - narenas_actual));
	}

	/* Read the refreshed tdata array. */
	tdata = &arenas_tdata[ind];
label_return:
	if (arenas_tdata_old != NULL) {
		a0dalloc(arenas_tdata_old);
	}
	return tdata;
}

/* Slow path, called only by arena_choose(). */
arena_t *
arena_choose_hard(tsd_t *tsd, bool internal) {
	arena_t *ret JEMALLOC_CC_SILENCE_INIT(NULL);

	if (narenas_auto > 1) {
		unsigned i, j, choose[2], first_null;

		/*
		 * Determine binding for both non-internal and internal
		 * allocation.
		 *
		 *   choose[0]: For application allocation.
		 *   choose[1]: For internal metadata allocation.
		 */

		for (j = 0; j < 2; j++) {
			choose[j] = 0;
		}

		first_null = narenas_auto;
		malloc_mutex_lock(tsd_tsdn(tsd), &arenas_lock);
		assert(arena_get(tsd_tsdn(tsd), 0, false) != NULL);
		for (i = 1; i < narenas_auto; i++) {
			if (arena_get(tsd_tsdn(tsd), i, false) != NULL) {
				/*
				 * Choose the first arena that has the lowest
				 * number of threads assigned to it.
				 */
				for (j = 0; j < 2; j++) {
					if (arena_nthreads_get(arena_get(
					    tsd_tsdn(tsd), i, false), !!j) <
					    arena_nthreads_get(arena_get(
					    tsd_tsdn(tsd), choose[j], false),
					    !!j)) {
						choose[j] = i;
					}
				}
			} else if (first_null == narenas_auto) {
				/*
				 * Record the index of the first uninitialized
				 * arena, in case all extant arenas are in use.
				 *
				 * NB: It is possible for there to be
				 * discontinuities in terms of initialized
				 * versus uninitialized arenas, due to the
				 * "thread.arena" mallctl.
				 */
				first_null = i;
			}
		}

		for (j = 0; j < 2; j++) {
			if (arena_nthreads_get(arena_get(tsd_tsdn(tsd),
			    choose[j], false), !!j) == 0 || first_null ==
			    narenas_auto) {
				/*
				 * Use an unloaded arena, or the least loaded
				 * arena if all arenas are already initialized.
				 */
				if (!!j == internal) {
					ret = arena_get(tsd_tsdn(tsd),
					    choose[j], false);
				}
			} else {
				arena_t *arena;

				/* Initialize a new arena. */
				choose[j] = first_null;
				arena = arena_init_locked(tsd_tsdn(tsd),
				    choose[j],
				    (extent_hooks_t *)&extent_hooks_default);
				if (arena == NULL) {
					malloc_mutex_unlock(tsd_tsdn(tsd),
					    &arenas_lock);
					return NULL;
				}
				if (!!j == internal) {
					ret = arena;
				}
			}
			arena_bind(tsd, choose[j], !!j);
		}
		malloc_mutex_unlock(tsd_tsdn(tsd), &arenas_lock);
	} else {
		ret = arena_get(tsd_tsdn(tsd), 0, false);
		arena_bind(tsd, 0, false);
		arena_bind(tsd, 0, true);
	}

	return ret;
}

void
iarena_cleanup(tsd_t *tsd) {
	arena_t *iarena;

	iarena = tsd_iarena_get(tsd);
	if (iarena != NULL) {
		arena_unbind(tsd, arena_ind_get(iarena), true);
	}
}

void
arena_cleanup(tsd_t *tsd) {
	arena_t *arena;

	arena = tsd_arena_get(tsd);
	if (arena != NULL) {
		arena_unbind(tsd, arena_ind_get(arena), false);
	}
}

void
arenas_tdata_cleanup(tsd_t *tsd) {
	arena_tdata_t *arenas_tdata;

	/* Prevent tsd->arenas_tdata from being (re)created. */
	*tsd_arenas_tdata_bypassp_get(tsd) = true;

	arenas_tdata = tsd_arenas_tdata_get(tsd);
	if (arenas_tdata != NULL) {
		tsd_arenas_tdata_set(tsd, NULL);
		a0dalloc(arenas_tdata);
	}
}

static void
stats_print_atexit(void) {
	if (config_tcache && config_stats) {
		tsdn_t *tsdn;
		unsigned narenas, i;

		tsdn = tsdn_fetch();

		/*
		 * Merge stats from extant threads.  This is racy, since
		 * individual threads do not lock when recording tcache stats
		 * events.  As a consequence, the final stats may be slightly
		 * out of date by the time they are reported, if other threads
		 * continue to allocate.
		 */
		for (i = 0, narenas = narenas_total_get(); i < narenas; i++) {
			arena_t *arena = arena_get(tsdn, i, false);
			if (arena != NULL) {
				tcache_t *tcache;

				malloc_mutex_lock(tsdn, &arena->tcache_ql_mtx);
				ql_foreach(tcache, &arena->tcache_ql, link) {
					tcache_stats_merge(tsdn, tcache, arena);
				}
				malloc_mutex_unlock(tsdn,
				    &arena->tcache_ql_mtx);
			}
		}
	}
	je_malloc_stats_print(NULL, NULL, NULL);
}

/*
 * End miscellaneous support functions.
 */
/******************************************************************************/
/*
 * Begin initialization functions.
 */

static char *
jemalloc_secure_getenv(const char *name) {
#ifdef JEMALLOC_HAVE_SECURE_GETENV
	return secure_getenv(name);
#else
#  ifdef JEMALLOC_HAVE_ISSETUGID
	if (issetugid() != 0) {
		return NULL;
	}
#  endif
	return getenv(name);
#endif
}

static unsigned
malloc_ncpus(void) {
	long result;

#ifdef _WIN32
	SYSTEM_INFO si;
	GetSystemInfo(&si);
	result = si.dwNumberOfProcessors;
#elif defined(JEMALLOC_GLIBC_MALLOC_HOOK) && defined(CPU_COUNT)
	/*
	 * glibc >= 2.6 has the CPU_COUNT macro.
	 *
	 * glibc's sysconf() uses isspace().  glibc allocates for the first time
	 * *before* setting up the isspace tables.  Therefore we need a
	 * different method to get the number of CPUs.
	 */
	{
		cpu_set_t set;

		pthread_getaffinity_np(pthread_self(), sizeof(set), &set);
		result = CPU_COUNT(&set);
	}
#else
	result = sysconf(_SC_NPROCESSORS_ONLN);
#endif
	return ((result == -1) ? 1 : (unsigned)result);
}

static bool
malloc_conf_next(char const **opts_p, char const **k_p, size_t *klen_p,
    char const **v_p, size_t *vlen_p) {
	bool accept;
	const char *opts = *opts_p;

	*k_p = opts;

	for (accept = false; !accept;) {
		switch (*opts) {
		case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
		case 'G': case 'H': case 'I': case 'J': case 'K': case 'L':
		case 'M': case 'N': case 'O': case 'P': case 'Q': case 'R':
		case 'S': case 'T': case 'U': case 'V': case 'W': case 'X':
		case 'Y': case 'Z':
		case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
		case 'g': case 'h': case 'i': case 'j': case 'k': case 'l':
		case 'm': case 'n': case 'o': case 'p': case 'q': case 'r':
		case 's': case 't': case 'u': case 'v': case 'w': case 'x':
		case 'y': case 'z':
		case '0': case '1': case '2': case '3': case '4': case '5':
		case '6': case '7': case '8': case '9':
		case '_':
			opts++;
			break;
		case ':':
			opts++;
			*klen_p = (uintptr_t)opts - 1 - (uintptr_t)*k_p;
			*v_p = opts;
			accept = true;
			break;
		case '\0':
			if (opts != *opts_p) {
				malloc_write("<jemalloc>: Conf string ends "
				    "with key\n");
			}
			return true;
		default:
			malloc_write("<jemalloc>: Malformed conf string\n");
			return true;
		}
	}

	for (accept = false; !accept;) {
		switch (*opts) {
		case ',':
			opts++;
			/*
			 * Look ahead one character here, because the next time
			 * this function is called, it will assume that end of
			 * input has been cleanly reached if no input remains,
			 * but we have optimistically already consumed the
			 * comma if one exists.
			 */
			if (*opts == '\0') {
				malloc_write("<jemalloc>: Conf string ends "
				    "with comma\n");
			}
			*vlen_p = (uintptr_t)opts - 1 - (uintptr_t)*v_p;
			accept = true;
			break;
		case '\0':
			*vlen_p = (uintptr_t)opts - (uintptr_t)*v_p;
			accept = true;
			break;
		default:
			opts++;
			break;
		}
	}

	*opts_p = opts;
	return false;
}

static void
malloc_conf_error(const char *msg, const char *k, size_t klen, const char *v,
    size_t vlen) {
	malloc_printf("<jemalloc>: %s: %.*s:%.*s\n", msg, (int)klen, k,
	    (int)vlen, v);
}

static void
malloc_slow_flag_init(void) {
	/*
	 * Combine the runtime options into malloc_slow for fast path.  Called
	 * after processing all the options.
	 */
	malloc_slow_flags |= (opt_junk_alloc ? flag_opt_junk_alloc : 0)
	    | (opt_junk_free ? flag_opt_junk_free : 0)
	    | (opt_zero ? flag_opt_zero : 0)
	    | (opt_utrace ? flag_opt_utrace : 0)
	    | (opt_xmalloc ? flag_opt_xmalloc : 0);

	malloc_slow = (malloc_slow_flags != 0);
}

static void
malloc_conf_init(void) {
	unsigned i;
	char buf[PATH_MAX + 1];
	const char *opts, *k, *v;
	size_t klen, vlen;

	for (i = 0; i < 4; i++) {
		/* Get runtime configuration. */
		switch (i) {
		case 0:
			opts = config_malloc_conf;
			break;
		case 1:
			if (je_malloc_conf != NULL) {
				/*
				 * Use options that were compiled into the
				 * program.
				 */
				opts = je_malloc_conf;
			} else {
				/* No configuration specified. */
				buf[0] = '\0';
				opts = buf;
			}
			break;
		case 2: {
			ssize_t linklen = 0;
#ifndef _WIN32
			int saved_errno = errno;
			const char *linkname =
#  ifdef JEMALLOC_PREFIX
			    "/etc/"JEMALLOC_PREFIX"malloc.conf"
#  else
			    "/etc/malloc.conf"
#  endif
			    ;

			/*
			 * Try to use the contents of the "/etc/malloc.conf"
			 * symbolic link's name.
			 */
			linklen = readlink(linkname, buf, sizeof(buf) - 1);
			if (linklen == -1) {
				/* No configuration specified. */
				linklen = 0;
				/* Restore errno. */
				set_errno(saved_errno);
			}
#endif
			buf[linklen] = '\0';
			opts = buf;
			break;
		} case 3: {
			const char *envname =
#ifdef JEMALLOC_PREFIX
			    JEMALLOC_CPREFIX"MALLOC_CONF"
#else
			    "MALLOC_CONF"
#endif
			    ;

			if ((opts = jemalloc_secure_getenv(envname)) != NULL) {
				/*
				 * Do nothing; opts is already initialized to
				 * the value of the MALLOC_CONF environment
				 * variable.
				 */
			} else {
				/* No configuration specified. */
				buf[0] = '\0';
				opts = buf;
			}
			break;
		} default:
			not_reached();
			buf[0] = '\0';
			opts = buf;
		}

		while (*opts != '\0' && !malloc_conf_next(&opts, &k, &klen, &v,
		    &vlen)) {
#define CONF_MATCH(n)							\
	(sizeof(n)-1 == klen && strncmp(n, k, klen) == 0)
#define CONF_MATCH_VALUE(n)						\
	(sizeof(n)-1 == vlen && strncmp(n, v, vlen) == 0)
#define CONF_HANDLE_BOOL(o, n, cont)					\
			if (CONF_MATCH(n)) {				\
				if (CONF_MATCH_VALUE("true")) {		\
					o = true;			\
				} else if (CONF_MATCH_VALUE("false")) {	\
					o = false;			\
				} else {				\
					malloc_conf_error(		\
					    "Invalid conf value",	\
					    k, klen, v, vlen);		\
				}					\
				if (cont) {				\
					continue;			\
				}					\
			}
#define CONF_MIN_no(um, min)	false
#define CONF_MIN_yes(um, min)	((um) < (min))
#define CONF_MAX_no(um, max)	false
#define CONF_MAX_yes(um, max)	((um) > (max))
#define CONF_HANDLE_T_U(t, o, n, min, max, check_min, check_max, clip)	\
			if (CONF_MATCH(n)) {				\
				uintmax_t um;				\
				char *end;				\
									\
				set_errno(0);				\
				um = malloc_strtoumax(v, &end, 0);	\
				if (get_errno() != 0 || (uintptr_t)end -\
				    (uintptr_t)v != vlen) {		\
					malloc_conf_error(		\
					    "Invalid conf value",	\
					    k, klen, v, vlen);		\
				} else if (clip) {			\
					if (CONF_MIN_##check_min(um,	\
					    (min))) {			\
						o = (t)(min);		\
					} else if (			\
					    CONF_MAX_##check_max(um,	\
					    (max))) {			\
						o = (t)(max);		\
					} else {			\
						o = (t)um;		\
					}				\
				} else {				\
					if (CONF_MIN_##check_min(um,	\
					    (min)) ||			\
					    CONF_MAX_##check_max(um,	\
					    (max))) {			\
						malloc_conf_error(	\
						    "Out-of-range "	\
						    "conf value",	\
						    k, klen, v, vlen);	\
					} else {			\
						o = (t)um;		\
					}				\
				}					\
				continue;				\
			}
#define CONF_HANDLE_UNSIGNED(o, n, min, max, check_min, check_max,	\
    clip)								\
			CONF_HANDLE_T_U(unsigned, o, n, min, max,	\
			    check_min, check_max, clip)
#define CONF_HANDLE_SIZE_T(o, n, min, max, check_min, check_max, clip)	\
			CONF_HANDLE_T_U(size_t, o, n, min, max,		\
			    check_min, check_max, clip)
#define CONF_HANDLE_SSIZE_T(o, n, min, max)				\
			if (CONF_MATCH(n)) {				\
				long l;					\
				char *end;				\
									\
				set_errno(0);				\
				l = strtol(v, &end, 0);			\
				if (get_errno() != 0 || (uintptr_t)end -\
				    (uintptr_t)v != vlen) {		\
					malloc_conf_error(		\
					    "Invalid conf value",	\
					    k, klen, v, vlen);		\
				} else if (l < (ssize_t)(min) || l >	\
				    (ssize_t)(max)) {			\
					malloc_conf_error(		\
					    "Out-of-range conf value",	\
					    k, klen, v, vlen);		\
				} else {				\
					o = l;				\
				}					\
				continue;				\
			}
#define CONF_HANDLE_CHAR_P(o, n, d)					\
			if (CONF_MATCH(n)) {				\
				size_t cpylen = (vlen <=		\
				    sizeof(o)-1) ? vlen :		\
				    sizeof(o)-1;			\
				strncpy(o, v, cpylen);			\
				o[cpylen] = '\0';			\
				continue;				\
			}

			CONF_HANDLE_BOOL(opt_abort, "abort", true)
			if (strncmp("dss", k, klen) == 0) {
				int i;
				bool match = false;
				for (i = 0; i < dss_prec_limit; i++) {
					if (strncmp(dss_prec_names[i], v, vlen)
					    == 0) {
						if (extent_dss_prec_set(i)) {
							malloc_conf_error(
							    "Error setting dss",
							    k, klen, v, vlen);
						} else {
							opt_dss =
							    dss_prec_names[i];
							match = true;
							break;
						}
					}
				}
				if (!match) {
					malloc_conf_error("Invalid conf value",
					    k, klen, v, vlen);
				}
				continue;
			}
			CONF_HANDLE_UNSIGNED(opt_narenas, "narenas", 1,
			    UINT_MAX, yes, no, false)
			CONF_HANDLE_SSIZE_T(opt_decay_time, "decay_time", -1,
			    NSTIME_SEC_MAX);
			CONF_HANDLE_BOOL(opt_stats_print, "stats_print", true)
			if (config_fill) {
				if (CONF_MATCH("junk")) {
					if (CONF_MATCH_VALUE("true")) {
						opt_junk = "true";
						opt_junk_alloc = opt_junk_free =
						    true;
					} else if (CONF_MATCH_VALUE("false")) {
						opt_junk = "false";
						opt_junk_alloc = opt_junk_free =
						    false;
					} else if (CONF_MATCH_VALUE("alloc")) {
						opt_junk = "alloc";
						opt_junk_alloc = true;
						opt_junk_free = false;
					} else if (CONF_MATCH_VALUE("free")) {
						opt_junk = "free";
						opt_junk_alloc = false;
						opt_junk_free = true;
					} else {
						malloc_conf_error(
						    "Invalid conf value", k,
						    klen, v, vlen);
					}
					continue;
				}
				CONF_HANDLE_BOOL(opt_zero, "zero", true)
			}
			if (config_utrace) {
				CONF_HANDLE_BOOL(opt_utrace, "utrace", true)
			}
			if (config_xmalloc) {
				CONF_HANDLE_BOOL(opt_xmalloc, "xmalloc", true)
			}
			if (config_tcache) {
				CONF_HANDLE_BOOL(opt_tcache, "tcache", true)
				CONF_HANDLE_SSIZE_T(opt_lg_tcache_max,
				    "lg_tcache_max", -1,
				    (sizeof(size_t) << 3) - 1)
			}
			if (config_prof) {
				CONF_HANDLE_BOOL(opt_prof, "prof", true)
				CONF_HANDLE_CHAR_P(opt_prof_prefix,
				    "prof_prefix", "jeprof")
				CONF_HANDLE_BOOL(opt_prof_active, "prof_active",
				    true)
				CONF_HANDLE_BOOL(opt_prof_thread_active_init,
				    "prof_thread_active_init", true)
				CONF_HANDLE_SIZE_T(opt_lg_prof_sample,
				    "lg_prof_sample", 0, (sizeof(uint64_t) << 3)
				    - 1, no, yes, true)
				CONF_HANDLE_BOOL(opt_prof_accum, "prof_accum",
				    true)
				CONF_HANDLE_SSIZE_T(opt_lg_prof_interval,
				    "lg_prof_interval", -1,
				    (sizeof(uint64_t) << 3) - 1)
				CONF_HANDLE_BOOL(opt_prof_gdump, "prof_gdump",
				    true)
				CONF_HANDLE_BOOL(opt_prof_final, "prof_final",
				    true)
				CONF_HANDLE_BOOL(opt_prof_leak, "prof_leak",
				    true)
			}
			malloc_conf_error("Invalid conf pair", k, klen, v,
			    vlen);
#undef CONF_MATCH
#undef CONF_MATCH_VALUE
#undef CONF_HANDLE_BOOL
#undef CONF_MIN_no
#undef CONF_MIN_yes
#undef CONF_MAX_no
#undef CONF_MAX_yes
#undef CONF_HANDLE_T_U
#undef CONF_HANDLE_UNSIGNED
#undef CONF_HANDLE_SIZE_T
#undef CONF_HANDLE_SSIZE_T
#undef CONF_HANDLE_CHAR_P
		}
	}
}

static bool
malloc_init_hard_needed(void) {
	if (malloc_initialized() || (IS_INITIALIZER && malloc_init_state ==
	    malloc_init_recursible)) {
		/*
		 * Another thread initialized the allocator before this one
		 * acquired init_lock, or this thread is the initializing
		 * thread, and it is recursively allocating.
		 */
		return false;
	}
#ifdef JEMALLOC_THREADED_INIT
	if (malloc_initializer != NO_INITIALIZER && !IS_INITIALIZER) {
		/* Busy-wait until the initializing thread completes. */
		spin_t spinner = SPIN_INITIALIZER;
		do {
			malloc_mutex_unlock(TSDN_NULL, &init_lock);
			spin_adaptive(&spinner);
			malloc_mutex_lock(TSDN_NULL, &init_lock);
		} while (!malloc_initialized());
		return false;
	}
#endif
	return true;
}

static bool
malloc_init_hard_a0_locked() {
	malloc_initializer = INITIALIZER;

	if (config_prof) {
		prof_boot0();
	}
	malloc_conf_init();
	if (opt_stats_print) {
		/* Print statistics at exit. */
		if (atexit(stats_print_atexit) != 0) {
			malloc_write("<jemalloc>: Error in atexit()\n");
			if (opt_abort) {
				abort();
			}
		}
	}
	pages_boot();
	if (base_boot(TSDN_NULL)) {
		return true;
	}
	if (extent_boot()) {
		return true;
	}
	if (ctl_boot()) {
		return true;
	}
	if (config_prof) {
		prof_boot1();
	}
	arena_boot();
	if (config_tcache && tcache_boot(TSDN_NULL)) {
		return true;
	}
	if (malloc_mutex_init(&arenas_lock, "arenas", WITNESS_RANK_ARENAS)) {
		return true;
	}
	/*
	 * Create enough scaffolding to allow recursive allocation in
	 * malloc_ncpus().
	 */
	narenas_auto = 1;
	narenas_total_set(narenas_auto);
	arenas = &a0;
	memset(arenas, 0, sizeof(arena_t *) * narenas_auto);
	/*
	 * Initialize one arena here.  The rest are lazily created in
	 * arena_choose_hard().
	 */
	if (arena_init(TSDN_NULL, 0, (extent_hooks_t *)&extent_hooks_default)
	    == NULL) {
		return true;
	}

	malloc_init_state = malloc_init_a0_initialized;

	return false;
}

static bool
malloc_init_hard_a0(void) {
	bool ret;

	malloc_mutex_lock(TSDN_NULL, &init_lock);
	ret = malloc_init_hard_a0_locked();
	malloc_mutex_unlock(TSDN_NULL, &init_lock);
	return ret;
}

/* Initialize data structures which may trigger recursive allocation. */
static bool
malloc_init_hard_recursible(void) {
	malloc_init_state = malloc_init_recursible;

	ncpus = malloc_ncpus();

#if (defined(JEMALLOC_HAVE_PTHREAD_ATFORK) && !defined(JEMALLOC_MUTEX_INIT_CB) \
    && !defined(JEMALLOC_ZONE) && !defined(_WIN32) && \
    !defined(__native_client__))
	/* LinuxThreads' pthread_atfork() allocates. */
	if (pthread_atfork(jemalloc_prefork, jemalloc_postfork_parent,
	    jemalloc_postfork_child) != 0) {
		malloc_write("<jemalloc>: Error in pthread_atfork()\n");
		if (opt_abort) {
			abort();
		}
		return true;
	}
#endif

	return false;
}

static bool
malloc_init_hard_finish(tsdn_t *tsdn) {
	if (malloc_mutex_boot()) {
		return true;
	}

	if (opt_narenas == 0) {
		/*
		 * For SMP systems, create more than one arena per CPU by
		 * default.
		 */
		if (ncpus > 1) {
			opt_narenas = ncpus << 2;
		} else {
			opt_narenas = 1;
		}
	}
	narenas_auto = opt_narenas;
	/*
	 * Limit the number of arenas to the indexing range of MALLOCX_ARENA().
	 */
	if (narenas_auto > MALLOCX_ARENA_MAX) {
		narenas_auto = MALLOCX_ARENA_MAX;
		malloc_printf("<jemalloc>: Reducing narenas to limit (%d)\n",
		    narenas_auto);
	}
	narenas_total_set(narenas_auto);

	/* Allocate and initialize arenas. */
	arenas = (arena_t **)base_alloc(tsdn, a0->base, sizeof(arena_t *) *
	    (MALLOCX_ARENA_MAX+1), CACHELINE);
	if (arenas == NULL) {
		return true;
	}
	/* Copy the pointer to the one arena that was already initialized. */
	arena_set(0, a0);

	malloc_init_state = malloc_init_initialized;
	malloc_slow_flag_init();

	return false;
}

static bool
malloc_init_hard(void) {
	tsd_t *tsd;

#if defined(_WIN32) && _WIN32_WINNT < 0x0600
	_init_init_lock();
#endif
	malloc_mutex_lock(TSDN_NULL, &init_lock);
	if (!malloc_init_hard_needed()) {
		malloc_mutex_unlock(TSDN_NULL, &init_lock);
		return false;
	}

	if (malloc_init_state != malloc_init_a0_initialized &&
	    malloc_init_hard_a0_locked()) {
		malloc_mutex_unlock(TSDN_NULL, &init_lock);
		return true;
	}

	malloc_mutex_unlock(TSDN_NULL, &init_lock);
	/* Recursive allocation relies on functional tsd. */
	tsd = malloc_tsd_boot0();
	if (tsd == NULL) {
		return true;
	}
	if (malloc_init_hard_recursible()) {
		return true;
	}
	malloc_mutex_lock(tsd_tsdn(tsd), &init_lock);

	if (config_prof && prof_boot2(tsd)) {
		malloc_mutex_unlock(tsd_tsdn(tsd), &init_lock);
		return true;
	}

	if (malloc_init_hard_finish(tsd_tsdn(tsd))) {
		malloc_mutex_unlock(tsd_tsdn(tsd), &init_lock);
		return true;
	}

	malloc_mutex_unlock(tsd_tsdn(tsd), &init_lock);
	malloc_tsd_boot1();
	return false;
}

/*
 * End initialization functions.
 */
/******************************************************************************/
/*
 * Begin allocation-path internal functions and data structures.
 */

/*
 * Settings determined by the documented behavior of the allocation functions.
 */
typedef struct static_opts_s static_opts_t;
struct static_opts_s {
	/* Whether or not allocation size may overflow. */
	bool may_overflow;
	/* Whether or not allocations of size 0 should be treated as size 1. */
	bool bump_empty_alloc;
	/*
	 * Whether to assert that allocations are not of size 0 (after any
	 * bumping).
	 */
	bool assert_nonempty_alloc;

	/*
	 * Whether or not to modify the 'result' argument to malloc in case of
	 * error.
	 */
	bool null_out_result_on_error;
	/* Whether to set errno when we encounter an error condition. */
	bool set_errno_on_error;

	/*
	 * The minimum valid alignment for functions requesting aligned storage.
	 */
	size_t min_alignment;

	/* The error string to use if we oom. */
	const char *oom_string;
	/* The error string to use if the passed-in alignment is invalid. */
	const char *invalid_alignment_string;

	/*
	 * False if we're configured to skip some time-consuming operations.
	 *
	 * This isn't really a malloc "behavior", but it acts as a useful
	 * summary of several other static (or at least, static after program
	 * initialization) options.
	 */
	bool slow;
};

JEMALLOC_ALWAYS_INLINE_C void
static_opts_init(static_opts_t *static_opts) {
	static_opts->may_overflow = false;
	static_opts->bump_empty_alloc = false;
	static_opts->assert_nonempty_alloc = false;
	static_opts->null_out_result_on_error = false;
	static_opts->set_errno_on_error = false;
	static_opts->min_alignment = 0;
	static_opts->oom_string = "";
	static_opts->invalid_alignment_string = "";
	static_opts->slow = false;
}

/*
 * These correspond to the macros in jemalloc/jemalloc_macros.h.  Broadly, we
 * should have one constant here per magic value there.  Note however that the
 * representations need not be related.
 */
#define TCACHE_IND_NONE ((unsigned)-1)
#define TCACHE_IND_AUTOMATIC ((unsigned)-2)
#define ARENA_IND_AUTOMATIC ((unsigned)-1)

typedef struct dynamic_opts_s dynamic_opts_t;
struct dynamic_opts_s {
	void **result;
	size_t num_items;
	size_t item_size;
	size_t alignment;
	bool zero;
	unsigned tcache_ind;
	unsigned arena_ind;
};

JEMALLOC_ALWAYS_INLINE_C void
dynamic_opts_init(dynamic_opts_t *dynamic_opts) {
	dynamic_opts->result = NULL;
	dynamic_opts->num_items = 0;
	dynamic_opts->item_size = 0;
	dynamic_opts->alignment = 0;
	dynamic_opts->zero = false;
	dynamic_opts->tcache_ind = TCACHE_IND_AUTOMATIC;
	dynamic_opts->arena_ind = ARENA_IND_AUTOMATIC;
}

/* ind is ignored if dopts->alignment > 0. */
JEMALLOC_ALWAYS_INLINE_C void *
imalloc_no_sample(static_opts_t *sopts, dynamic_opts_t *dopts, tsd_t *tsd,
    size_t size, size_t usize, szind_t ind) {
	tcache_t *tcache;
	arena_t *arena;

	/* Fill in the tcache. */
	if (dopts->tcache_ind == TCACHE_IND_AUTOMATIC) {
		tcache = tcache_get(tsd, true);
	} else if (dopts->tcache_ind == TCACHE_IND_NONE) {
		tcache = NULL;
	} else {
		tcache = tcaches_get(tsd, dopts->tcache_ind);
	}

	/* Fill in the arena. */
	if (dopts->arena_ind == ARENA_IND_AUTOMATIC) {
		/*
		 * In case of automatic arena management, we defer arena
		 * computation until as late as we can, hoping to fill the
		 * allocation out of the tcache.
		 */
		arena = NULL;
	} else {
		arena = arena_get(tsd_tsdn(tsd), dopts->arena_ind, true);
	}

	if (unlikely(dopts->alignment != 0)) {
		return ipalloct(tsd_tsdn(tsd), usize, dopts->alignment,
		    dopts->zero, tcache, arena);
	}

	return iallocztm(tsd_tsdn(tsd), size, ind, dopts->zero, tcache, false,
	    arena, sopts->slow);
}

JEMALLOC_ALWAYS_INLINE_C void *
imalloc_sample(static_opts_t *sopts, dynamic_opts_t *dopts, tsd_t *tsd,
    size_t usize, szind_t ind) {
	void *ret;

	/*
	 * For small allocations, sampling bumps the usize.  If so, we allocate
	 * from the ind_large bucket.
	 */
	szind_t ind_large;
	size_t bumped_usize = usize;

	if (usize <= SMALL_MAXCLASS) {
		assert(((dopts->alignment == 0) ? s2u(LARGE_MINCLASS) :
		    sa2u(LARGE_MINCLASS, dopts->alignment)) == LARGE_MINCLASS);
		ind_large = size2index(LARGE_MINCLASS);
		bumped_usize = s2u(LARGE_MINCLASS);
		ret = imalloc_no_sample(sopts, dopts, tsd, bumped_usize,
		    bumped_usize, ind_large);
		if (unlikely(ret == NULL)) {
			return NULL;
		}
		arena_prof_promote(tsd_tsdn(tsd), iealloc(tsd_tsdn(tsd), ret),
		    ret, usize);
	} else {
		ret = imalloc_no_sample(sopts, dopts, tsd, usize, usize, ind);
	}

	return ret;
}

/*
 * Returns true if the allocation will overflow, and false otherwise.  Sets
 * *size to the product either way.
 */
JEMALLOC_ALWAYS_INLINE_C bool
compute_size_with_overflow(bool may_overflow, dynamic_opts_t *dopts,
    size_t *size) {
	/*
	 * This function is just num_items * item_size, except that we may have
	 * to check for overflow.
	 */

	if (!may_overflow) {
		assert(dopts->num_items == 1);
		*size = dopts->item_size;
		return false;
	}

	/* A size_t with its high-half bits all set to 1. */
	const static size_t high_bits = SIZE_T_MAX << (sizeof(size_t) * 8 / 2);

	*size = dopts->item_size * dopts->num_items;

	if (unlikely(*size == 0)) {
		return (dopts->num_items != 0 && dopts->item_size != 0);
	}

	/*
	 * We got a non-zero size, but we don't know if we overflowed to get
	 * there.  To avoid having to do a divide, we'll be clever and note that
	 * if both A and B can be represented in N/2 bits, then their product
	 * can be represented in N bits (without the possibility of overflow).
	 */
	if (likely((high_bits & (dopts->num_items | dopts->item_size)) == 0)) {
		return false;
	}
	if (likely(*size / dopts->item_size == dopts->num_items)) {
		return false;
	}
	return true;
}

JEMALLOC_ALWAYS_INLINE_C int
imalloc_body(static_opts_t *sopts, dynamic_opts_t *dopts) {
	/* Where the actual allocated memory will live. */
	void *allocation = NULL;
	/* Filled in by compute_size_with_overflow below. */
	size_t size = 0;
	/* We compute a value for this right before allocating. */
	tsd_t *tsd = NULL;
	/*
	 * For unaligned allocations, we need only ind.  For aligned
	 * allocations, or in case of stats or profiling we need usize.
	 *
	 * These are actually dead stores, in that their values are reset before
	 * any branch on their value is taken.  Sometimes though, it's
	 * convenient to pass them as arguments before this point.  To avoid
	 * undefined behavior then, we initialize them with dummy stores.
	 */
	szind_t ind = 0;
	size_t usize = 0;

	/* Initialize (if we can't prove we don't have to). */
	if (sopts->slow) {
		if (unlikely(malloc_init())) {
			goto label_oom;
		}
	}

	/* Compute the amount of memory the user wants. */
	if (unlikely(compute_size_with_overflow(sopts->may_overflow, dopts,
	    &size))) {
		goto label_oom;
	}

	/* Validate the user input. */
	if (sopts->bump_empty_alloc) {
		if (unlikely(size == 0)) {
			size = 1;
		}
	}

	if (sopts->assert_nonempty_alloc) {
		assert (size != 0);
	}

	if (unlikely(dopts->alignment < sopts->min_alignment
	    || (dopts->alignment & (dopts->alignment - 1)) != 0)) {
		goto label_invalid_alignment;
	}

	/* This is the beginning of the "core" algorithm. */

	if (dopts->alignment == 0) {
		ind = size2index(size);
		if (unlikely(ind >= NSIZES)) {
			goto label_oom;
		}
		if (config_stats || (config_prof && opt_prof)) {
			usize = index2size(ind);
			assert(usize > 0 && usize <= LARGE_MAXCLASS);
		}
	} else {
		usize = sa2u(size, dopts->alignment);
		if (unlikely(usize == 0 || usize > LARGE_MAXCLASS)) {
			goto label_oom;
		}
	}

	/*
	 * We always need the tsd, even if we aren't going to use the tcache for
	 * some reason.  Let's grab it right away.
	 */
	tsd = tsd_fetch();
	witness_assert_lockless(tsd_tsdn(tsd));

	/* If profiling is on, get our profiling context. */
	if (config_prof && opt_prof) {
		/*
		 * Note that if we're going down this path, usize must have been
		 * initialized in the previous if statement.
		 */
		prof_tctx_t *tctx = prof_alloc_prep(
		    tsd, usize, prof_active_get_unlocked(), true);
		if (likely((uintptr_t)tctx == (uintptr_t)1U)) {
			allocation = imalloc_no_sample(
			    sopts, dopts, tsd, usize, usize, ind);
		} else if ((uintptr_t)tctx > (uintptr_t)1U) {
			/*
			 * Note that ind might still be 0 here.  This is fine;
			 * imalloc_sample ignores ind if dopts->alignment > 0.
			 */
			allocation = imalloc_sample(
			    sopts, dopts, tsd, usize, ind);
		} else {
			allocation = NULL;
		}

		if (unlikely(allocation == NULL)) {
			prof_alloc_rollback(tsd, tctx, true);
			goto label_oom;
		}

		prof_malloc(tsd_tsdn(tsd), iealloc(tsd_tsdn(tsd), allocation),
		    allocation, usize, tctx);

	} else {
		/*
		 * If dopts->alignment > 0, then ind is still 0, but usize was
		 * computed in the previous if statement.  Down the positive
		 * alignment path, imalloc_no_sample ignores ind and size
		 * (relying only on usize).
		 */
		allocation = imalloc_no_sample(sopts, dopts, tsd, size, usize,
		    ind);
		if (unlikely(allocation == NULL)) {
			goto label_oom;
		}
	}

	/*
	 * Allocation has been done at this point.  We still have some
	 * post-allocation work to do though.
	 */
	assert(dopts->alignment == 0
	    || ((uintptr_t)allocation & (dopts->alignment - 1)) == ZU(0));

	if (config_stats) {
		assert(usize == isalloc(tsd_tsdn(tsd), iealloc(tsd_tsdn(tsd),
		    allocation), allocation));
		*tsd_thread_allocatedp_get(tsd) += usize;
	}

	if (sopts->slow) {
		UTRACE(0, size, allocation);
	}

	witness_assert_lockless(tsd_tsdn(tsd));

	/* Success! */
	*dopts->result = allocation;
	return 0;

label_oom:
	if (unlikely(sopts->slow) && config_xmalloc && unlikely(opt_xmalloc)) {
		malloc_write(sopts->oom_string);
		abort();
	}

	if (sopts->slow) {
		UTRACE(NULL, size, NULL);
	}

	witness_assert_lockless(tsd_tsdn(tsd));

	if (sopts->set_errno_on_error) {
		set_errno(ENOMEM);
	}

	if (sopts->null_out_result_on_error) {
		*dopts->result = NULL;
	}

	return ENOMEM;

	/*
	 * This label is only jumped to by one goto; we move it out of line
	 * anyways to avoid obscuring the non-error paths, and for symmetry with
	 * the oom case.
	 */
label_invalid_alignment:
	if (config_xmalloc && unlikely(opt_xmalloc)) {
		malloc_write(sopts->invalid_alignment_string);
		abort();
	}

	if (sopts->set_errno_on_error) {
		set_errno(EINVAL);
	}

	if (sopts->slow) {
		UTRACE(NULL, size, NULL);
	}

	witness_assert_lockless(tsd_tsdn(tsd));

	if (sopts->null_out_result_on_error) {
		*dopts->result = NULL;
	}

	return EINVAL;
}

/* Returns the errno-style error code of the allocation. */
JEMALLOC_ALWAYS_INLINE_C int
imalloc(static_opts_t *sopts, dynamic_opts_t *dopts) {
	if (unlikely(malloc_slow)) {
		sopts->slow = true;
		return imalloc_body(sopts, dopts);
	} else {
		sopts->slow = false;
		return imalloc_body(sopts, dopts);
	}
}
/******************************************************************************/
/*
 * Begin malloc(3)-compatible functions.
 */

JEMALLOC_EXPORT JEMALLOC_ALLOCATOR JEMALLOC_RESTRICT_RETURN
void JEMALLOC_NOTHROW *
JEMALLOC_ATTR(malloc) JEMALLOC_ALLOC_SIZE(1)
je_malloc(size_t size) {
	void *ret;
	static_opts_t sopts;
	dynamic_opts_t dopts;

	static_opts_init(&sopts);
	dynamic_opts_init(&dopts);

	sopts.bump_empty_alloc = true;
	sopts.null_out_result_on_error = true;
	sopts.set_errno_on_error = true;
	sopts.oom_string = "<jemalloc>: Error in malloc(): out of memory\n";

	dopts.result = &ret;
	dopts.num_items = 1;
	dopts.item_size = size;

	imalloc(&sopts, &dopts);

	return ret;
}

JEMALLOC_EXPORT int JEMALLOC_NOTHROW
JEMALLOC_ATTR(nonnull(1))
je_posix_memalign(void **memptr, size_t alignment, size_t size) {
	int ret;
	static_opts_t sopts;
	dynamic_opts_t dopts;

	static_opts_init(&sopts);
	dynamic_opts_init(&dopts);

	sopts.bump_empty_alloc = true;
	sopts.min_alignment = sizeof(void *);
	sopts.oom_string =
	    "<jemalloc>: Error allocating aligned memory: out of memory\n";
	sopts.invalid_alignment_string =
	    "<jemalloc>: Error allocating aligned memory: invalid alignment\n";

	dopts.result = memptr;
	dopts.num_items = 1;
	dopts.item_size = size;
	dopts.alignment = alignment;

	ret = imalloc(&sopts, &dopts);
	return ret;
}

JEMALLOC_EXPORT JEMALLOC_ALLOCATOR JEMALLOC_RESTRICT_RETURN
void JEMALLOC_NOTHROW *
JEMALLOC_ATTR(malloc) JEMALLOC_ALLOC_SIZE(2)
je_aligned_alloc(size_t alignment, size_t size) {
	void *ret;

	static_opts_t sopts;
	dynamic_opts_t dopts;

	static_opts_init(&sopts);
	dynamic_opts_init(&dopts);

	sopts.bump_empty_alloc = true;
	sopts.null_out_result_on_error = true;
	sopts.set_errno_on_error = true;
	sopts.min_alignment = 1;
	sopts.oom_string =
	    "<jemalloc>: Error allocating aligned memory: out of memory\n";
	sopts.invalid_alignment_string =
	    "<jemalloc>: Error allocating aligned memory: invalid alignment\n";

	dopts.result = &ret;
	dopts.num_items = 1;
	dopts.item_size = size;
	dopts.alignment = alignment;

	imalloc(&sopts, &dopts);
	return ret;
}

JEMALLOC_EXPORT JEMALLOC_ALLOCATOR JEMALLOC_RESTRICT_RETURN
void JEMALLOC_NOTHROW *
JEMALLOC_ATTR(malloc) JEMALLOC_ALLOC_SIZE2(1, 2)
je_calloc(size_t num, size_t size) {
	void *ret;
	static_opts_t sopts;
	dynamic_opts_t dopts;

	static_opts_init(&sopts);
	dynamic_opts_init(&dopts);

	sopts.may_overflow = true;
	sopts.bump_empty_alloc = true;
	sopts.null_out_result_on_error = true;
	sopts.set_errno_on_error = true;
	sopts.oom_string = "<jemalloc>: Error in calloc(): out of memory\n";

	dopts.result = &ret;
	dopts.num_items = num;
	dopts.item_size = size;
	dopts.zero = true;

	imalloc(&sopts, &dopts);

	return ret;
}

static void *
irealloc_prof_sample(tsd_t *tsd, extent_t *extent, void *old_ptr,
    size_t old_usize, size_t usize, prof_tctx_t *tctx) {
	void *p;

	if (tctx == NULL) {
		return NULL;
	}
	if (usize <= SMALL_MAXCLASS) {
		p = iralloc(tsd, extent, old_ptr, old_usize, LARGE_MINCLASS, 0,
		    false);
		if (p == NULL) {
			return NULL;
		}
		arena_prof_promote(tsd_tsdn(tsd), iealloc(tsd_tsdn(tsd), p), p,
		    usize);
	} else {
		p = iralloc(tsd, extent, old_ptr, old_usize, usize, 0, false);
	}

	return p;
}

JEMALLOC_ALWAYS_INLINE_C void *
irealloc_prof(tsd_t *tsd, extent_t *old_extent, void *old_ptr, size_t old_usize,
    size_t usize) {
	void *p;
	extent_t *extent;
	bool prof_active;
	prof_tctx_t *old_tctx, *tctx;

	prof_active = prof_active_get_unlocked();
	old_tctx = prof_tctx_get(tsd_tsdn(tsd), old_extent, old_ptr);
	tctx = prof_alloc_prep(tsd, usize, prof_active, true);
	if (unlikely((uintptr_t)tctx != (uintptr_t)1U)) {
		p = irealloc_prof_sample(tsd, old_extent, old_ptr, old_usize,
		    usize, tctx);
	} else {
		p = iralloc(tsd, old_extent, old_ptr, old_usize, usize, 0,
		    false);
	}
	if (unlikely(p == NULL)) {
		prof_alloc_rollback(tsd, tctx, true);
		return NULL;
	}
	extent = (p == old_ptr) ? old_extent : iealloc(tsd_tsdn(tsd), p);
	prof_realloc(tsd, extent, p, usize, tctx, prof_active, true, old_extent,
	    old_ptr, old_usize, old_tctx);

	return p;
}

JEMALLOC_INLINE_C void
ifree(tsd_t *tsd, void *ptr, tcache_t *tcache, bool slow_path) {
	extent_t *extent;
	size_t usize;

	witness_assert_lockless(tsd_tsdn(tsd));

	assert(ptr != NULL);
	assert(malloc_initialized() || IS_INITIALIZER);

	extent = iealloc(tsd_tsdn(tsd), ptr);
	if (config_prof && opt_prof) {
		usize = isalloc(tsd_tsdn(tsd), extent, ptr);
		prof_free(tsd, extent, ptr, usize);
	} else if (config_stats) {
		usize = isalloc(tsd_tsdn(tsd), extent, ptr);
	}
	if (config_stats) {
		*tsd_thread_deallocatedp_get(tsd) += usize;
	}

	if (likely(!slow_path)) {
		idalloctm(tsd_tsdn(tsd), extent, ptr, tcache, false, false);
	} else {
		idalloctm(tsd_tsdn(tsd), extent, ptr, tcache, false, true);
	}
}

JEMALLOC_INLINE_C void
isfree(tsd_t *tsd, extent_t *extent, void *ptr, size_t usize, tcache_t *tcache,
    bool slow_path) {
	witness_assert_lockless(tsd_tsdn(tsd));

	assert(ptr != NULL);
	assert(malloc_initialized() || IS_INITIALIZER);

	if (config_prof && opt_prof) {
		prof_free(tsd, extent, ptr, usize);
	}
	if (config_stats) {
		*tsd_thread_deallocatedp_get(tsd) += usize;
	}

	if (likely(!slow_path)) {
		isdalloct(tsd_tsdn(tsd), extent, ptr, usize, tcache, false);
	} else {
		isdalloct(tsd_tsdn(tsd), extent, ptr, usize, tcache, true);
	}
}

JEMALLOC_EXPORT JEMALLOC_ALLOCATOR JEMALLOC_RESTRICT_RETURN
void JEMALLOC_NOTHROW *
JEMALLOC_ALLOC_SIZE(2)
je_realloc(void *ptr, size_t size) {
	void *ret;
	tsdn_t *tsdn JEMALLOC_CC_SILENCE_INIT(NULL);
	size_t usize JEMALLOC_CC_SILENCE_INIT(0);
	size_t old_usize = 0;

	if (unlikely(size == 0)) {
		if (ptr != NULL) {
			tsd_t *tsd;

			/* realloc(ptr, 0) is equivalent to free(ptr). */
			UTRACE(ptr, 0, 0);
			tsd = tsd_fetch();
			ifree(tsd, ptr, tcache_get(tsd, false), true);
			return NULL;
		}
		size = 1;
	}

	if (likely(ptr != NULL)) {
		tsd_t *tsd;
		extent_t *extent;

		assert(malloc_initialized() || IS_INITIALIZER);
		tsd = tsd_fetch();

		witness_assert_lockless(tsd_tsdn(tsd));

		extent = iealloc(tsd_tsdn(tsd), ptr);
		old_usize = isalloc(tsd_tsdn(tsd), extent, ptr);
		if (config_prof && opt_prof) {
			usize = s2u(size);
			ret = unlikely(usize == 0 || usize > LARGE_MAXCLASS) ?
			    NULL : irealloc_prof(tsd, extent, ptr, old_usize,
			    usize);
		} else {
			if (config_stats) {
				usize = s2u(size);
			}
			ret = iralloc(tsd, extent, ptr, old_usize, size, 0,
			    false);
		}
		tsdn = tsd_tsdn(tsd);
	} else {
		/* realloc(NULL, size) is equivalent to malloc(size). */
		return je_malloc(size);
	}

	if (unlikely(ret == NULL)) {
		if (config_xmalloc && unlikely(opt_xmalloc)) {
			malloc_write("<jemalloc>: Error in realloc(): "
			    "out of memory\n");
			abort();
		}
		set_errno(ENOMEM);
	}
	if (config_stats && likely(ret != NULL)) {
		tsd_t *tsd;

		assert(usize == isalloc(tsdn, iealloc(tsdn, ret), ret));
		tsd = tsdn_tsd(tsdn);
		*tsd_thread_allocatedp_get(tsd) += usize;
		*tsd_thread_deallocatedp_get(tsd) += old_usize;
	}
	UTRACE(ptr, size, ret);
	witness_assert_lockless(tsdn);
	return ret;
}

JEMALLOC_EXPORT void JEMALLOC_NOTHROW
je_free(void *ptr) {
	UTRACE(ptr, 0, 0);
	if (likely(ptr != NULL)) {
		tsd_t *tsd = tsd_fetch();
		witness_assert_lockless(tsd_tsdn(tsd));
		if (likely(!malloc_slow)) {
			ifree(tsd, ptr, tcache_get(tsd, false), false);
		} else {
			ifree(tsd, ptr, tcache_get(tsd, false), true);
		}
		witness_assert_lockless(tsd_tsdn(tsd));
	}
}

/*
 * End malloc(3)-compatible functions.
 */
/******************************************************************************/
/*
 * Begin non-standard override functions.
 */

#ifdef JEMALLOC_OVERRIDE_MEMALIGN
JEMALLOC_EXPORT JEMALLOC_ALLOCATOR JEMALLOC_RESTRICT_RETURN
void JEMALLOC_NOTHROW *
JEMALLOC_ATTR(malloc)
je_memalign(size_t alignment, size_t size) {
	void *ret;
	static_opts_t sopts;
	dynamic_opts_t dopts;

	static_opts_init(&sopts);
	dynamic_opts_init(&dopts);

	sopts.bump_empty_alloc = true;
	sopts.min_alignment = 1;
	sopts.oom_string =
	    "<jemalloc>: Error allocating aligned memory: out of memory\n";
	sopts.invalid_alignment_string =
	    "<jemalloc>: Error allocating aligned memory: invalid alignment\n";
	sopts.null_out_result_on_error = true;

	dopts.result = &ret;
	dopts.num_items = 1;
	dopts.item_size = size;
	dopts.alignment = alignment;

	imalloc(&sopts, &dopts);
	return ret;
}
#endif

#ifdef JEMALLOC_OVERRIDE_VALLOC
JEMALLOC_EXPORT JEMALLOC_ALLOCATOR JEMALLOC_RESTRICT_RETURN
void JEMALLOC_NOTHROW *
JEMALLOC_ATTR(malloc)
je_valloc(size_t size) {
	void *ret;

	static_opts_t sopts;
	dynamic_opts_t dopts;

	static_opts_init(&sopts);
	dynamic_opts_init(&dopts);

	sopts.bump_empty_alloc = true;
	sopts.null_out_result_on_error = true;
	sopts.min_alignment = PAGE;
	sopts.oom_string =
	    "<jemalloc>: Error allocating aligned memory: out of memory\n";
	sopts.invalid_alignment_string =
	    "<jemalloc>: Error allocating aligned memory: invalid alignment\n";

	dopts.result = &ret;
	dopts.num_items = 1;
	dopts.item_size = size;
	dopts.alignment = PAGE;

	imalloc(&sopts, &dopts);

	return ret;
}
#endif

/*
 * is_malloc(je_malloc) is some macro magic to detect if jemalloc_defs.h has
 * #define je_malloc malloc
 */
#define malloc_is_malloc 1
#define is_malloc_(a) malloc_is_ ## a
#define is_malloc(a) is_malloc_(a)

#if ((is_malloc(je_malloc) == 1) && defined(JEMALLOC_GLIBC_MALLOC_HOOK))
/*
 * glibc provides the RTLD_DEEPBIND flag for dlopen which can make it possible
 * to inconsistently reference libc's malloc(3)-compatible functions
 * (https://bugzilla.mozilla.org/show_bug.cgi?id=493541).
 *
 * These definitions interpose hooks in glibc.  The functions are actually
 * passed an extra argument for the caller return address, which will be
 * ignored.
 */
JEMALLOC_EXPORT void (*__free_hook)(void *ptr) = je_free;
JEMALLOC_EXPORT void *(*__malloc_hook)(size_t size) = je_malloc;
JEMALLOC_EXPORT void *(*__realloc_hook)(void *ptr, size_t size) = je_realloc;
# ifdef JEMALLOC_GLIBC_MEMALIGN_HOOK
JEMALLOC_EXPORT void *(*__memalign_hook)(size_t alignment, size_t size) =
    je_memalign;
# endif

#ifdef CPU_COUNT
/*
 * To enable static linking with glibc, the libc specific malloc interface must
 * be implemented also, so none of glibc's malloc.o functions are added to the
 * link.
 */
#define ALIAS(je_fn)	__attribute__((alias (#je_fn), used))
/* To force macro expansion of je_ prefix before stringification. */
#define PREALIAS(je_fn)  ALIAS(je_fn)
void	*__libc_malloc(size_t size) PREALIAS(je_malloc);
void	__libc_free(void* ptr) PREALIAS(je_free);
void	*__libc_realloc(void* ptr, size_t size) PREALIAS(je_realloc);
void	*__libc_calloc(size_t n, size_t size) PREALIAS(je_calloc);
void	*__libc_memalign(size_t align, size_t s) PREALIAS(je_memalign);
void	*__libc_valloc(size_t size) PREALIAS(je_valloc);
int	__posix_memalign(void** r, size_t a, size_t s)
    PREALIAS(je_posix_memalign);
#undef PREALIAS
#undef ALIAS

#endif

#endif

/*
 * End non-standard override functions.
 */
/******************************************************************************/
/*
 * Begin non-standard functions.
 */

JEMALLOC_EXPORT JEMALLOC_ALLOCATOR JEMALLOC_RESTRICT_RETURN
void JEMALLOC_NOTHROW *
JEMALLOC_ATTR(malloc) JEMALLOC_ALLOC_SIZE(1)
je_mallocx(size_t size, int flags) {
	void *ret;
	static_opts_t sopts;
	dynamic_opts_t dopts;

	static_opts_init(&sopts);
	dynamic_opts_init(&dopts);

	sopts.assert_nonempty_alloc = true;
	sopts.null_out_result_on_error = true;
	sopts.oom_string = "<jemalloc>: Error in mallocx(): out of memory\n";

	dopts.result = &ret;
	dopts.num_items = 1;
	dopts.item_size = size;
	if (unlikely(flags != 0)) {
		if ((flags & MALLOCX_LG_ALIGN_MASK) != 0) {
			dopts.alignment = MALLOCX_ALIGN_GET_SPECIFIED(flags);
		}

		dopts.zero = MALLOCX_ZERO_GET(flags);

		if ((flags & MALLOCX_TCACHE_MASK) != 0) {
			if ((flags & MALLOCX_TCACHE_MASK)
			    == MALLOCX_TCACHE_NONE) {
				dopts.tcache_ind = TCACHE_IND_NONE;
			} else {
				dopts.tcache_ind = MALLOCX_TCACHE_GET(flags);
			}
		} else {
			dopts.tcache_ind = TCACHE_IND_AUTOMATIC;
		}

		if ((flags & MALLOCX_ARENA_MASK) != 0)
			dopts.arena_ind = MALLOCX_ARENA_GET(flags);
	}

	imalloc(&sopts, &dopts);
	return ret;
}

static void *
irallocx_prof_sample(tsdn_t *tsdn, extent_t *extent, void *old_ptr,
    size_t old_usize, size_t usize, size_t alignment, bool zero,
    tcache_t *tcache, arena_t *arena, prof_tctx_t *tctx) {
	void *p;

	if (tctx == NULL) {
		return NULL;
	}
	if (usize <= SMALL_MAXCLASS) {
		p = iralloct(tsdn, extent, old_ptr, old_usize, LARGE_MINCLASS,
		    alignment, zero, tcache, arena);
		if (p == NULL) {
			return NULL;
		}
		arena_prof_promote(tsdn, iealloc(tsdn, p), p, usize);
	} else {
		p = iralloct(tsdn, extent, old_ptr, old_usize, usize, alignment,
		    zero, tcache, arena);
	}

	return p;
}

JEMALLOC_ALWAYS_INLINE_C void *
irallocx_prof(tsd_t *tsd, extent_t *old_extent, void *old_ptr, size_t old_usize,
    size_t size, size_t alignment, size_t *usize, bool zero, tcache_t *tcache,
    arena_t *arena) {
	void *p;
	extent_t *extent;
	bool prof_active;
	prof_tctx_t *old_tctx, *tctx;

	prof_active = prof_active_get_unlocked();
	old_tctx = prof_tctx_get(tsd_tsdn(tsd), old_extent, old_ptr);
	tctx = prof_alloc_prep(tsd, *usize, prof_active, false);
	if (unlikely((uintptr_t)tctx != (uintptr_t)1U)) {
		p = irallocx_prof_sample(tsd_tsdn(tsd), old_extent, old_ptr,
		    old_usize, *usize, alignment, zero, tcache, arena, tctx);
	} else {
		p = iralloct(tsd_tsdn(tsd), old_extent, old_ptr, old_usize,
		    size, alignment, zero, tcache, arena);
	}
	if (unlikely(p == NULL)) {
		prof_alloc_rollback(tsd, tctx, false);
		return NULL;
	}

	if (p == old_ptr && alignment != 0) {
		/*
		 * The allocation did not move, so it is possible that the size
		 * class is smaller than would guarantee the requested
		 * alignment, and that the alignment constraint was
		 * serendipitously satisfied.  Additionally, old_usize may not
		 * be the same as the current usize because of in-place large
		 * reallocation.  Therefore, query the actual value of usize.
		 */
		extent = old_extent;
		*usize = isalloc(tsd_tsdn(tsd), extent, p);
	} else {
		extent = iealloc(tsd_tsdn(tsd), p);
	}
	prof_realloc(tsd, extent, p, *usize, tctx, prof_active, false,
	    old_extent, old_ptr, old_usize, old_tctx);

	return p;
}

JEMALLOC_EXPORT JEMALLOC_ALLOCATOR JEMALLOC_RESTRICT_RETURN
void JEMALLOC_NOTHROW *
JEMALLOC_ALLOC_SIZE(2)
je_rallocx(void *ptr, size_t size, int flags) {
	void *p;
	tsd_t *tsd;
	extent_t *extent;
	size_t usize;
	size_t old_usize;
	size_t alignment = MALLOCX_ALIGN_GET(flags);
	bool zero = flags & MALLOCX_ZERO;
	arena_t *arena;
	tcache_t *tcache;

	assert(ptr != NULL);
	assert(size != 0);
	assert(malloc_initialized() || IS_INITIALIZER);
	tsd = tsd_fetch();
	witness_assert_lockless(tsd_tsdn(tsd));
	extent = iealloc(tsd_tsdn(tsd), ptr);

	if (unlikely((flags & MALLOCX_ARENA_MASK) != 0)) {
		unsigned arena_ind = MALLOCX_ARENA_GET(flags);
		arena = arena_get(tsd_tsdn(tsd), arena_ind, true);
		if (unlikely(arena == NULL)) {
			goto label_oom;
		}
	} else {
		arena = NULL;
	}

	if (unlikely((flags & MALLOCX_TCACHE_MASK) != 0)) {
		if ((flags & MALLOCX_TCACHE_MASK) == MALLOCX_TCACHE_NONE) {
			tcache = NULL;
		} else {
			tcache = tcaches_get(tsd, MALLOCX_TCACHE_GET(flags));
		}
	} else {
		tcache = tcache_get(tsd, true);
	}

	old_usize = isalloc(tsd_tsdn(tsd), extent, ptr);

	if (config_prof && opt_prof) {
		usize = (alignment == 0) ? s2u(size) : sa2u(size, alignment);
		if (unlikely(usize == 0 || usize > LARGE_MAXCLASS)) {
			goto label_oom;
		}
		p = irallocx_prof(tsd, extent, ptr, old_usize, size, alignment,
		    &usize, zero, tcache, arena);
		if (unlikely(p == NULL)) {
			goto label_oom;
		}
	} else {
		p = iralloct(tsd_tsdn(tsd), extent, ptr, old_usize, size,
		    alignment, zero, tcache, arena);
		if (unlikely(p == NULL)) {
			goto label_oom;
		}
		if (config_stats) {
			usize = isalloc(tsd_tsdn(tsd), iealloc(tsd_tsdn(tsd),
			    p), p);
		}
	}
	assert(alignment == 0 || ((uintptr_t)p & (alignment - 1)) == ZU(0));

	if (config_stats) {
		*tsd_thread_allocatedp_get(tsd) += usize;
		*tsd_thread_deallocatedp_get(tsd) += old_usize;
	}
	UTRACE(ptr, size, p);
	witness_assert_lockless(tsd_tsdn(tsd));
	return p;
label_oom:
	if (config_xmalloc && unlikely(opt_xmalloc)) {
		malloc_write("<jemalloc>: Error in rallocx(): out of memory\n");
		abort();
	}
	UTRACE(ptr, size, 0);
	witness_assert_lockless(tsd_tsdn(tsd));
	return NULL;
}

JEMALLOC_ALWAYS_INLINE_C size_t
ixallocx_helper(tsdn_t *tsdn, extent_t *extent, void *ptr, size_t old_usize,
    size_t size, size_t extra, size_t alignment, bool zero) {
	size_t usize;

	if (ixalloc(tsdn, extent, ptr, old_usize, size, extra, alignment,
	    zero)) {
		return old_usize;
	}
	usize = isalloc(tsdn, extent, ptr);

	return usize;
}

static size_t
ixallocx_prof_sample(tsdn_t *tsdn, extent_t *extent, void *ptr,
    size_t old_usize, size_t size, size_t extra, size_t alignment, bool zero,
    prof_tctx_t *tctx) {
	size_t usize;

	if (tctx == NULL) {
		return old_usize;
	}
	usize = ixallocx_helper(tsdn, extent, ptr, old_usize, size, extra,
	    alignment, zero);

	return usize;
}

JEMALLOC_ALWAYS_INLINE_C size_t
ixallocx_prof(tsd_t *tsd, extent_t *extent, void *ptr, size_t old_usize,
    size_t size, size_t extra, size_t alignment, bool zero) {
	size_t usize_max, usize;
	bool prof_active;
	prof_tctx_t *old_tctx, *tctx;

	prof_active = prof_active_get_unlocked();
	old_tctx = prof_tctx_get(tsd_tsdn(tsd), extent, ptr);
	/*
	 * usize isn't knowable before ixalloc() returns when extra is non-zero.
	 * Therefore, compute its maximum possible value and use that in
	 * prof_alloc_prep() to decide whether to capture a backtrace.
	 * prof_realloc() will use the actual usize to decide whether to sample.
	 */
	if (alignment == 0) {
		usize_max = s2u(size+extra);
		assert(usize_max > 0 && usize_max <= LARGE_MAXCLASS);
	} else {
		usize_max = sa2u(size+extra, alignment);
		if (unlikely(usize_max == 0 || usize_max > LARGE_MAXCLASS)) {
			/*
			 * usize_max is out of range, and chances are that
			 * allocation will fail, but use the maximum possible
			 * value and carry on with prof_alloc_prep(), just in
			 * case allocation succeeds.
			 */
			usize_max = LARGE_MAXCLASS;
		}
	}
	tctx = prof_alloc_prep(tsd, usize_max, prof_active, false);

	if (unlikely((uintptr_t)tctx != (uintptr_t)1U)) {
		usize = ixallocx_prof_sample(tsd_tsdn(tsd), extent, ptr,
		    old_usize, size, extra, alignment, zero, tctx);
	} else {
		usize = ixallocx_helper(tsd_tsdn(tsd), extent, ptr, old_usize,
		    size, extra, alignment, zero);
	}
	if (usize == old_usize) {
		prof_alloc_rollback(tsd, tctx, false);
		return usize;
	}
	prof_realloc(tsd, extent, ptr, usize, tctx, prof_active, false, extent,
	    ptr, old_usize, old_tctx);

	return usize;
}

JEMALLOC_EXPORT size_t JEMALLOC_NOTHROW
je_xallocx(void *ptr, size_t size, size_t extra, int flags) {
	tsd_t *tsd;
	extent_t *extent;
	size_t usize, old_usize;
	size_t alignment = MALLOCX_ALIGN_GET(flags);
	bool zero = flags & MALLOCX_ZERO;

	assert(ptr != NULL);
	assert(size != 0);
	assert(SIZE_T_MAX - size >= extra);
	assert(malloc_initialized() || IS_INITIALIZER);
	tsd = tsd_fetch();
	witness_assert_lockless(tsd_tsdn(tsd));
	extent = iealloc(tsd_tsdn(tsd), ptr);

	old_usize = isalloc(tsd_tsdn(tsd), extent, ptr);

	/*
	 * The API explicitly absolves itself of protecting against (size +
	 * extra) numerical overflow, but we may need to clamp extra to avoid
	 * exceeding LARGE_MAXCLASS.
	 *
	 * Ordinarily, size limit checking is handled deeper down, but here we
	 * have to check as part of (size + extra) clamping, since we need the
	 * clamped value in the above helper functions.
	 */
	if (unlikely(size > LARGE_MAXCLASS)) {
		usize = old_usize;
		goto label_not_resized;
	}
	if (unlikely(LARGE_MAXCLASS - size < extra)) {
		extra = LARGE_MAXCLASS - size;
	}

	if (config_prof && opt_prof) {
		usize = ixallocx_prof(tsd, extent, ptr, old_usize, size, extra,
		    alignment, zero);
	} else {
		usize = ixallocx_helper(tsd_tsdn(tsd), extent, ptr, old_usize,
		    size, extra, alignment, zero);
	}
	if (unlikely(usize == old_usize)) {
		goto label_not_resized;
	}

	if (config_stats) {
		*tsd_thread_allocatedp_get(tsd) += usize;
		*tsd_thread_deallocatedp_get(tsd) += old_usize;
	}
label_not_resized:
	UTRACE(ptr, size, ptr);
	witness_assert_lockless(tsd_tsdn(tsd));
	return usize;
}

JEMALLOC_EXPORT size_t JEMALLOC_NOTHROW
JEMALLOC_ATTR(pure)
je_sallocx(const void *ptr, int flags) {
	size_t usize;
	tsdn_t *tsdn;

	assert(malloc_initialized() || IS_INITIALIZER);

	tsdn = tsdn_fetch();
	witness_assert_lockless(tsdn);

	if (config_ivsalloc) {
		usize = ivsalloc(tsdn, ptr);
	} else {
		usize = isalloc(tsdn, iealloc(tsdn, ptr), ptr);
	}

	witness_assert_lockless(tsdn);
	return usize;
}

JEMALLOC_EXPORT void JEMALLOC_NOTHROW
je_dallocx(void *ptr, int flags) {
	tsd_t *tsd;
	tcache_t *tcache;

	assert(ptr != NULL);
	assert(malloc_initialized() || IS_INITIALIZER);

	tsd = tsd_fetch();
	witness_assert_lockless(tsd_tsdn(tsd));
	if (unlikely((flags & MALLOCX_TCACHE_MASK) != 0)) {
		if ((flags & MALLOCX_TCACHE_MASK) == MALLOCX_TCACHE_NONE) {
			tcache = NULL;
		} else {
			tcache = tcaches_get(tsd, MALLOCX_TCACHE_GET(flags));
		}
	} else {
		tcache = tcache_get(tsd, false);
	}

	UTRACE(ptr, 0, 0);
	if (likely(!malloc_slow)) {
		ifree(tsd, ptr, tcache, false);
	} else {
		ifree(tsd, ptr, tcache, true);
	}
	witness_assert_lockless(tsd_tsdn(tsd));
}

JEMALLOC_ALWAYS_INLINE_C size_t
inallocx(tsdn_t *tsdn, size_t size, int flags) {
	size_t usize;

	witness_assert_lockless(tsdn);

	if (likely((flags & MALLOCX_LG_ALIGN_MASK) == 0)) {
		usize = s2u(size);
	} else {
		usize = sa2u(size, MALLOCX_ALIGN_GET_SPECIFIED(flags));
	}
	witness_assert_lockless(tsdn);
	return usize;
}

JEMALLOC_EXPORT void JEMALLOC_NOTHROW
je_sdallocx(void *ptr, size_t size, int flags) {
	tsd_t *tsd;
	extent_t *extent;
	size_t usize;
	tcache_t *tcache;

	assert(ptr != NULL);
	assert(malloc_initialized() || IS_INITIALIZER);
	tsd = tsd_fetch();
	extent = iealloc(tsd_tsdn(tsd), ptr);
	usize = inallocx(tsd_tsdn(tsd), size, flags);
	assert(usize == isalloc(tsd_tsdn(tsd), extent, ptr));

	witness_assert_lockless(tsd_tsdn(tsd));
	if (unlikely((flags & MALLOCX_TCACHE_MASK) != 0)) {
		if ((flags & MALLOCX_TCACHE_MASK) == MALLOCX_TCACHE_NONE) {
			tcache = NULL;
		} else {
			tcache = tcaches_get(tsd, MALLOCX_TCACHE_GET(flags));
		}
	} else {
		tcache = tcache_get(tsd, false);
	}

	UTRACE(ptr, 0, 0);
	if (likely(!malloc_slow)) {
		isfree(tsd, extent, ptr, usize, tcache, false);
	} else {
		isfree(tsd, extent, ptr, usize, tcache, true);
	}
	witness_assert_lockless(tsd_tsdn(tsd));
}

JEMALLOC_EXPORT size_t JEMALLOC_NOTHROW
JEMALLOC_ATTR(pure)
je_nallocx(size_t size, int flags) {
	size_t usize;
	tsdn_t *tsdn;

	assert(size != 0);

	if (unlikely(malloc_init())) {
		return 0;
	}

	tsdn = tsdn_fetch();
	witness_assert_lockless(tsdn);

	usize = inallocx(tsdn, size, flags);
	if (unlikely(usize > LARGE_MAXCLASS)) {
		return 0;
	}

	witness_assert_lockless(tsdn);
	return usize;
}

JEMALLOC_EXPORT int JEMALLOC_NOTHROW
je_mallctl(const char *name, void *oldp, size_t *oldlenp, void *newp,
    size_t newlen) {
	int ret;
	tsd_t *tsd;

	if (unlikely(malloc_init())) {
		return EAGAIN;
	}

	tsd = tsd_fetch();
	witness_assert_lockless(tsd_tsdn(tsd));
	ret = ctl_byname(tsd, name, oldp, oldlenp, newp, newlen);
	witness_assert_lockless(tsd_tsdn(tsd));
	return ret;
}

JEMALLOC_EXPORT int JEMALLOC_NOTHROW
je_mallctlnametomib(const char *name, size_t *mibp, size_t *miblenp) {
	int ret;
	tsdn_t *tsdn;

	if (unlikely(malloc_init())) {
		return EAGAIN;
	}

	tsdn = tsdn_fetch();
	witness_assert_lockless(tsdn);
	ret = ctl_nametomib(tsdn, name, mibp, miblenp);
	witness_assert_lockless(tsdn);
	return ret;
}

JEMALLOC_EXPORT int JEMALLOC_NOTHROW
je_mallctlbymib(const size_t *mib, size_t miblen, void *oldp, size_t *oldlenp,
  void *newp, size_t newlen) {
	int ret;
	tsd_t *tsd;

	if (unlikely(malloc_init())) {
		return EAGAIN;
	}

	tsd = tsd_fetch();
	witness_assert_lockless(tsd_tsdn(tsd));
	ret = ctl_bymib(tsd, mib, miblen, oldp, oldlenp, newp, newlen);
	witness_assert_lockless(tsd_tsdn(tsd));
	return ret;
}

JEMALLOC_EXPORT void JEMALLOC_NOTHROW
je_malloc_stats_print(void (*write_cb)(void *, const char *), void *cbopaque,
    const char *opts) {
	tsdn_t *tsdn;

	tsdn = tsdn_fetch();
	witness_assert_lockless(tsdn);
	stats_print(write_cb, cbopaque, opts);
	witness_assert_lockless(tsdn);
}

JEMALLOC_EXPORT size_t JEMALLOC_NOTHROW
je_malloc_usable_size(JEMALLOC_USABLE_SIZE_CONST void *ptr) {
	size_t ret;
	tsdn_t *tsdn;

	assert(malloc_initialized() || IS_INITIALIZER);

	tsdn = tsdn_fetch();
	witness_assert_lockless(tsdn);

	if (config_ivsalloc) {
		ret = ivsalloc(tsdn, ptr);
	} else {
		ret = (ptr == NULL) ? 0 : isalloc(tsdn, iealloc(tsdn, ptr),
		    ptr);
	}

	witness_assert_lockless(tsdn);
	return ret;
}

/*
 * End non-standard functions.
 */
/******************************************************************************/
/*
 * The following functions are used by threading libraries for protection of
 * malloc during fork().
 */

/*
 * If an application creates a thread before doing any allocation in the main
 * thread, then calls fork(2) in the main thread followed by memory allocation
 * in the child process, a race can occur that results in deadlock within the
 * child: the main thread may have forked while the created thread had
 * partially initialized the allocator.  Ordinarily jemalloc prevents
 * fork/malloc races via the following functions it registers during
 * initialization using pthread_atfork(), but of course that does no good if
 * the allocator isn't fully initialized at fork time.  The following library
 * constructor is a partial solution to this problem.  It may still be possible
 * to trigger the deadlock described above, but doing so would involve forking
 * via a library constructor that runs before jemalloc's runs.
 */
#ifndef JEMALLOC_JET
JEMALLOC_ATTR(constructor)
static void
jemalloc_constructor(void) {
	malloc_init();
}
#endif

#ifndef JEMALLOC_MUTEX_INIT_CB
void
jemalloc_prefork(void)
#else
JEMALLOC_EXPORT void
_malloc_prefork(void)
#endif
{
	tsd_t *tsd;
	unsigned i, j, narenas;
	arena_t *arena;

#ifdef JEMALLOC_MUTEX_INIT_CB
	if (!malloc_initialized()) {
		return;
	}
#endif
	assert(malloc_initialized());

	tsd = tsd_fetch();

	narenas = narenas_total_get();

	witness_prefork(tsd);
	/* Acquire all mutexes in a safe order. */
	ctl_prefork(tsd_tsdn(tsd));
	tcache_prefork(tsd_tsdn(tsd));
	malloc_mutex_prefork(tsd_tsdn(tsd), &arenas_lock);
	prof_prefork0(tsd_tsdn(tsd));
	for (i = 0; i < 3; i++) {
		for (j = 0; j < narenas; j++) {
			if ((arena = arena_get(tsd_tsdn(tsd), j, false)) !=
			    NULL) {
				switch (i) {
				case 0:
					arena_prefork0(tsd_tsdn(tsd), arena);
					break;
				case 1:
					arena_prefork1(tsd_tsdn(tsd), arena);
					break;
				case 2:
					arena_prefork2(tsd_tsdn(tsd), arena);
					break;
				default: not_reached();
				}
			}
		}
	}
	for (i = 0; i < narenas; i++) {
		if ((arena = arena_get(tsd_tsdn(tsd), i, false)) != NULL) {
			arena_prefork3(tsd_tsdn(tsd), arena);
		}
	}
	prof_prefork1(tsd_tsdn(tsd));
}

#ifndef JEMALLOC_MUTEX_INIT_CB
void
jemalloc_postfork_parent(void)
#else
JEMALLOC_EXPORT void
_malloc_postfork(void)
#endif
{
	tsd_t *tsd;
	unsigned i, narenas;

#ifdef JEMALLOC_MUTEX_INIT_CB
	if (!malloc_initialized()) {
		return;
	}
#endif
	assert(malloc_initialized());

	tsd = tsd_fetch();

	witness_postfork_parent(tsd);
	/* Release all mutexes, now that fork() has completed. */
	for (i = 0, narenas = narenas_total_get(); i < narenas; i++) {
		arena_t *arena;

		if ((arena = arena_get(tsd_tsdn(tsd), i, false)) != NULL) {
			arena_postfork_parent(tsd_tsdn(tsd), arena);
		}
	}
	prof_postfork_parent(tsd_tsdn(tsd));
	malloc_mutex_postfork_parent(tsd_tsdn(tsd), &arenas_lock);
	tcache_postfork_parent(tsd_tsdn(tsd));
	ctl_postfork_parent(tsd_tsdn(tsd));
}

void
jemalloc_postfork_child(void) {
	tsd_t *tsd;
	unsigned i, narenas;

	assert(malloc_initialized());

	tsd = tsd_fetch();

	witness_postfork_child(tsd);
	/* Release all mutexes, now that fork() has completed. */
	for (i = 0, narenas = narenas_total_get(); i < narenas; i++) {
		arena_t *arena;

		if ((arena = arena_get(tsd_tsdn(tsd), i, false)) != NULL) {
			arena_postfork_child(tsd_tsdn(tsd), arena);
		}
	}
	prof_postfork_child(tsd_tsdn(tsd));
	malloc_mutex_postfork_child(tsd_tsdn(tsd), &arenas_lock);
	tcache_postfork_child(tsd_tsdn(tsd));
	ctl_postfork_child(tsd_tsdn(tsd));
}

/******************************************************************************/

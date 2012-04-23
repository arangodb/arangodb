/*
 * $Id: ptmalloc3.c,v 1.8 2006/03/31 15:57:28 wg Exp $
 * 

ptmalloc3 -- wrapper for Doug Lea's malloc-2.8.3 with concurrent
             allocations

Copyright (c) 2005, 2006 Wolfram Gloger  <ptmalloc@malloc.de>

Permission to use, copy, modify, distribute, and sell this software
and its documentation for any purpose is hereby granted without fee,
provided that (i) the above copyright notices and this permission
notice appear in all copies of the software and related documentation,
and (ii) the name of Wolfram Gloger may not be used in any advertising
or publicity relating to the software.

THE SOFTWARE IS PROVIDED "AS-IS" AND WITHOUT WARRANTY OF ANY KIND,
EXPRESS, IMPLIED OR OTHERWISE, INCLUDING WITHOUT LIMITATION, ANY
WARRANTY OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.

IN NO EVENT SHALL WOLFRAM GLOGER BE LIABLE FOR ANY SPECIAL,
INCIDENTAL, INDIRECT OR CONSEQUENTIAL DAMAGES OF ANY KIND, OR ANY
DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER OR NOT ADVISED OF THE POSSIBILITY OF DAMAGE, AND ON ANY THEORY
OF LIABILITY, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
PERFORMANCE OF THIS SOFTWARE.

 */

/*
 * TODO: optimization / better integration with malloc.c (partly done)
 *       malloc_{get,set}_state (probably hard to keep compatibility)
 *       debugging hooks
 *       better mstats
 */

#include <sys/types.h>   /* For size_t */
#include <sys/mman.h>    /* for mmap */
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>      /* for memset */

#include <malloc-machine.h>

#include "malloc-2.8.3.h"

/* ----------------------------------------------------------------------- */

/* The following section is replicated from malloc.c */

#include "malloc-private.h"

/* end of definitions replicated from malloc.c */

#define munmap_chunk(mst, p) do {                         \
  size_t prevsize = (p)->prev_foot & ~IS_MMAPPED_BIT;     \
  size_t psize = chunksize(p) + prevsize + MMAP_FOOT_PAD; \
  if (CALL_MUNMAP((char*)(p) - prevsize, psize) == 0)     \
    ((struct malloc_state*)(mst))->footprint -= psize;    \
} while (0)

/* ---------------------------------------------------------------------- */

/* Minimum size for a newly created arena.  */
#ifndef ARENA_SIZE_MIN
# define ARENA_SIZE_MIN	   (128*1024)
#endif
#define HAVE_MEMCPY        1

/* If THREAD_STATS is non-zero, some statistics on mutex locking are
   computed.  */
#ifndef THREAD_STATS
# define THREAD_STATS 0
#endif

#ifndef MALLOC_DEBUG
# define MALLOC_DEBUG 0
#endif

#define my_powerof2(x) ((((x)-1)&(x))==0)

/* Already initialized? */
int __malloc_initialized = -1;

#ifndef RETURN_ADDRESS
# define RETURN_ADDRESS(X_) (NULL)
#endif

#if THREAD_STATS
# define THREAD_STAT(x) x
#else
# define THREAD_STAT(x) do ; while(0)
#endif

#ifdef _LIBC

/* Special defines for the GNU C library.  */
#define public_cALLOc    __libc_calloc
#define public_fREe      __libc_free
#define public_cFREe     __libc_cfree
#define public_mALLOc    __libc_malloc
#define public_mEMALIGn  __libc_memalign
#define public_rEALLOc   __libc_realloc
#define public_vALLOc    __libc_valloc
#define public_pVALLOc   __libc_pvalloc
#define public_pMEMALIGn __posix_memalign
#define public_mALLINFo  __libc_mallinfo
#define public_mALLOPt   __libc_mallopt
#define public_mTRIm     __malloc_trim
#define public_mSTATs    __malloc_stats
#define public_mUSABLe   __malloc_usable_size
#define public_iCALLOc   __libc_independent_calloc
#define public_iCOMALLOc __libc_independent_comalloc
#define public_gET_STATe __malloc_get_state
#define public_sET_STATe __malloc_set_state
#define malloc_getpagesize __getpagesize()
#define open             __open
#define mmap             __mmap
#define munmap           __munmap
#define mremap           __mremap
#define mprotect         __mprotect
#define MORECORE         (*__morecore)
#define MORECORE_FAILURE 0

void * __default_morecore (ptrdiff_t);
void *(*__morecore)(ptrdiff_t) = __default_morecore;

#else /* !_LIBC */

#define public_cALLOc    calloc
#define public_fREe      free
#define public_cFREe     cfree
#define public_mALLOc    malloc
#define public_mEMALIGn  memalign
#define public_rEALLOc   realloc
#define public_vALLOc    valloc
#define public_pVALLOc   pvalloc
#define public_pMEMALIGn posix_memalign
#define public_mALLINFo  mallinfo
#define public_mALLOPt   mallopt
#define public_mTRIm     malloc_trim
#define public_mSTATs    malloc_stats
#define public_mUSABLe   malloc_usable_size
#define public_iCALLOc   independent_calloc
#define public_iCOMALLOc independent_comalloc
#define public_gET_STATe malloc_get_state
#define public_sET_STATe malloc_set_state

#endif /* _LIBC */

#if !defined _LIBC && (!defined __GNUC__ || __GNUC__<3)
#define __builtin_expect(expr, val) (expr)
#endif

#if MALLOC_DEBUG
#include <assert.h>
#else
#undef assert
#define assert(x) ((void)0)
#endif

/* USE_STARTER determines if and when the special "starter" hook
   functions are used: not at all (0), during ptmalloc_init (first bit
   set), or from the beginning until an explicit call to ptmalloc_init
   (second bit set).  This is necessary if thread-related
   initialization functions (e.g.  pthread_key_create) require
   malloc() calls (set USE_STARTER=1), or if those functions initially
   cannot be used at all (set USE_STARTER=2 and perform an explicit
   ptmalloc_init() when the thread library is ready, typically at the
   start of main()). */

#ifndef USE_STARTER
# ifndef _LIBC
#  define USE_STARTER 1
# else
#  if USE___THREAD || (defined USE_TLS && !defined SHARED)
    /* These routines are never needed in this configuration.  */
#   define USE_STARTER 0
#  else
#   define USE_STARTER (USE_TLS ? 4 : 1)
#  endif
# endif
#endif

/*----------------------------------------------------------------------*/

/* Arenas */
static tsd_key_t arena_key;
static mutex_t list_lock;

/* Arena structure */
struct malloc_arena {
  /* Serialize access.  */
  mutex_t mutex;

  /* Statistics for locking.  Only used if THREAD_STATS is defined.  */
  long stat_lock_direct, stat_lock_loop, stat_lock_wait;
  long stat_starter;

  /* Linked list */
  struct malloc_arena *next;

  /* Space for mstate.  The size is just the minimum such that
     create_mspace_with_base can be successfully called.  */
  char buf_[pad_request(sizeof(struct malloc_state)) + TOP_FOOT_SIZE +
	    CHUNK_ALIGN_MASK + 1];
};
#define MSPACE_OFFSET (((offsetof(struct malloc_arena, buf_) \
			 + CHUNK_ALIGN_MASK) & ~CHUNK_ALIGN_MASK))
#define arena_to_mspace(a) ((void *)chunk2mem((char*)(a) + MSPACE_OFFSET))

/* check for chunk from non-main arena */
#define chunk_non_main_arena(p) ((p)->head & NON_MAIN_ARENA)

static struct malloc_arena* _int_new_arena(size_t size);

/* Buffer for the main arena. */
static struct malloc_arena main_arena;

/* For now, store arena in footer.  This means typically 4bytes more
   overhead for each non-main-arena chunk, but is fast and easy to
   compute.  Note that the pointer stored in the extra footer must be
   properly aligned, though. */
#define FOOTER_OVERHEAD \
 (2*sizeof(struct malloc_arena*) - SIZE_T_SIZE)

#define arena_for_chunk(ptr) \
 (chunk_non_main_arena(ptr) ? *(struct malloc_arena**)              \
  ((char*)(ptr) + chunksize(ptr) - (FOOTER_OVERHEAD - SIZE_T_SIZE)) \
  : &main_arena)

/* special because of extra overhead */
#define arena_for_mmap_chunk(ptr) \
 (chunk_non_main_arena(ptr) ? *(struct malloc_arena**)             \
  ((char*)(ptr) + chunksize(ptr) - sizeof(struct malloc_arena*))   \
  : &main_arena)

#define set_non_main_arena(mem, ar_ptr) do {                   		      \
  mchunkptr P = mem2chunk(mem);                                               \
  size_t SZ = chunksize(P) - (is_mmapped(P) ? sizeof(struct malloc_arena*)    \
                              : (FOOTER_OVERHEAD - SIZE_T_SIZE));             \
  assert((unsigned long)((char*)(P) + SZ)%sizeof(struct malloc_arena*) == 0); \
  *(struct malloc_arena**)((char*)(P) + SZ) = (ar_ptr);                       \
  P->head |= NON_MAIN_ARENA;                                                  \
} while (0)

/* arena_get() acquires an arena and locks the corresponding mutex.
   First, try the one last locked successfully by this thread.  (This
   is the common case and handled with a macro for speed.)  Then, loop
   once over the circularly linked list of arenas.  If no arena is
   readily available, create a new one.  In this latter case, `size'
   is just a hint as to how much memory will be required immediately
   in the new arena. */

#define arena_get(ptr, size) do { \
  void *vptr = NULL; \
  ptr = (struct malloc_arena*)tsd_getspecific(arena_key, vptr); \
  if(ptr && !mutex_trylock(&ptr->mutex)) { \
    THREAD_STAT(++(ptr->stat_lock_direct)); \
  } else \
    ptr = arena_get2(ptr, (size)); \
} while(0)

static struct malloc_arena*
arena_get2(struct malloc_arena* a_tsd, size_t size)
{
  struct malloc_arena* a;
  int err;

  if(!a_tsd)
    a = a_tsd = &main_arena;
  else {
    a = a_tsd->next;
    if(!a) {
      /* This can only happen while initializing the new arena. */
      (void)mutex_lock(&main_arena.mutex);
      THREAD_STAT(++(main_arena.stat_lock_wait));
      return &main_arena;
    }
  }

  /* Check the global, circularly linked list for available arenas. */
 repeat:
  do {
    if(!mutex_trylock(&a->mutex)) {
      THREAD_STAT(++(a->stat_lock_loop));
      tsd_setspecific(arena_key, (void *)a);
      return a;
    }
    a = a->next;
  } while(a != a_tsd);

  /* If not even the list_lock can be obtained, try again.  This can
     happen during `atfork', or for example on systems where thread
     creation makes it temporarily impossible to obtain _any_
     locks. */
  if(mutex_trylock(&list_lock)) {
    a = a_tsd;
    goto repeat;
  }
  (void)mutex_unlock(&list_lock);

  /* Nothing immediately available, so generate a new arena.  */
  a = _int_new_arena(size);
  if(!a)
    return 0;

  tsd_setspecific(arena_key, (void *)a);
  mutex_init(&a->mutex);
  err = mutex_lock(&a->mutex); /* remember result */

  /* Add the new arena to the global list.  */
  (void)mutex_lock(&list_lock);
  a->next = main_arena.next;
  atomic_write_barrier ();
  main_arena.next = a;
  (void)mutex_unlock(&list_lock);

  if(err) /* locking failed; keep arena for further attempts later */
    return 0;

  THREAD_STAT(++(a->stat_lock_loop));
  return a;
}

/* Create a new arena with room for a chunk of size "size".  */

static struct malloc_arena*
_int_new_arena(size_t size)
{
  struct malloc_arena* a;
  size_t mmap_sz = sizeof(*a) + pad_request(size);
  void *m;

  if (mmap_sz < ARENA_SIZE_MIN)
    mmap_sz = ARENA_SIZE_MIN;
  /* conservative estimate for page size */
  mmap_sz = (mmap_sz + 8191) & ~(size_t)8191;
  a = CALL_MMAP(mmap_sz);
  if ((char*)a == (char*)-1)
    return 0;

  m = create_mspace_with_base((char*)a + MSPACE_OFFSET,
			      mmap_sz - MSPACE_OFFSET,
			      0);

  if (!m) { 
    CALL_MUNMAP(a, mmap_sz);
    a = 0;
  } else {
    /*a->next = NULL;*/
    /*a->system_mem = a->max_system_mem = h->size;*/
  }

  return a;
}

/*------------------------------------------------------------------------*/

/* Hook mechanism for proper initialization and atfork support. */

/* Define and initialize the hook variables.  These weak definitions must
   appear before any use of the variables in a function.  */
#ifndef weak_variable
#ifndef _LIBC
#define weak_variable /**/
#else
/* In GNU libc we want the hook variables to be weak definitions to
   avoid a problem with Emacs.  */
#define weak_variable weak_function
#endif
#endif

#if !(USE_STARTER & 2)
# define free_hook_ini     NULL
/* Forward declarations.  */
static void* malloc_hook_ini (size_t sz, const void *caller);
static void* realloc_hook_ini (void* ptr, size_t sz, const void* caller);
static void* memalign_hook_ini (size_t alignment, size_t sz,
				const void* caller);
#else
# define free_hook_ini     free_starter
# define malloc_hook_ini   malloc_starter
# define realloc_hook_ini  NULL
# define memalign_hook_ini memalign_starter
#endif

void weak_variable (*__malloc_initialize_hook) (void) = NULL;
void weak_variable (*__free_hook) (void * __ptr, const void *)
     = free_hook_ini;
void * weak_variable (*__malloc_hook) (size_t __size, const void *)
     = malloc_hook_ini;
void * weak_variable (*__realloc_hook)
     (void * __ptr, size_t __size, const void *) = realloc_hook_ini;
void * weak_variable (*__memalign_hook)
  (size_t __alignment, size_t __size, const void *) = memalign_hook_ini;
/*void weak_variable (*__after_morecore_hook) (void) = NULL;*/

/* The initial hooks just call the initialization routine, then do the
   normal work. */

#if !(USE_STARTER & 2)
static
#endif
void ptmalloc_init(void);

#if !(USE_STARTER & 2)

static void*
malloc_hook_ini(size_t sz, const void * caller)
{
  __malloc_hook = NULL;
  ptmalloc_init();
  return public_mALLOc(sz);
}

static void *
realloc_hook_ini(void *ptr, size_t sz, const void * caller)
{
  __malloc_hook = NULL;
  __realloc_hook = NULL;
  ptmalloc_init();
  return public_rEALLOc(ptr, sz);
}

static void*
memalign_hook_ini(size_t alignment, size_t sz, const void * caller)
{
  __memalign_hook = NULL;
  ptmalloc_init();
  return public_mEMALIGn(alignment, sz);
}

#endif /* !(USE_STARTER & 2) */

/*----------------------------------------------------------------------*/

#if !defined NO_THREADS && USE_STARTER

/* The following hooks are used when the global initialization in
   ptmalloc_init() hasn't completed yet. */

static void*
malloc_starter(size_t sz, const void *caller)
{
  void* victim;

  /*ptmalloc_init_minimal();*/
  victim = mspace_malloc(arena_to_mspace(&main_arena), sz);
  THREAD_STAT(++main_arena.stat_starter);

  return victim;
}

static void*
memalign_starter(size_t align, size_t sz, const void *caller)
{
  void* victim;

  /*ptmalloc_init_minimal();*/
  victim = mspace_memalign(arena_to_mspace(&main_arena), align, sz);
  THREAD_STAT(++main_arena.stat_starter);

  return victim;
}

static void
free_starter(void* mem, const void *caller)
{
  if (mem) {
    mchunkptr p = mem2chunk(mem);
    void *msp = arena_to_mspace(&main_arena);
    if (is_mmapped(p))
      munmap_chunk(msp, p);
    else
      mspace_free(msp, mem);
  }
  THREAD_STAT(++main_arena.stat_starter);
}

#endif /* !defined NO_THREADS && USE_STARTER */

/*----------------------------------------------------------------------*/

#ifndef NO_THREADS

/* atfork support.  */

static void * (*save_malloc_hook) (size_t __size, const void *);
# if !defined _LIBC || !defined USE_TLS || (defined SHARED && !USE___THREAD)
static void * (*save_memalign_hook) (size_t __align, size_t __size,
				     const void *);
# endif
static void   (*save_free_hook) (void * __ptr, const void *);
static void*  save_arena;

/* Magic value for the thread-specific arena pointer when
   malloc_atfork() is in use.  */

#define ATFORK_ARENA_PTR ((void*)-1)

/* The following hooks are used while the `atfork' handling mechanism
   is active. */

static void*
malloc_atfork(size_t sz, const void *caller)
{
  void *vptr = NULL;

  tsd_getspecific(arena_key, vptr);
  if(vptr == ATFORK_ARENA_PTR) {
    /* We are the only thread that may allocate at all.  */
    return mspace_malloc(arena_to_mspace(&main_arena), sz);
  } else {
    /* Suspend the thread until the `atfork' handlers have completed.
       By that time, the hooks will have been reset as well, so that
       mALLOc() can be used again. */
    (void)mutex_lock(&list_lock);
    (void)mutex_unlock(&list_lock);
    return public_mALLOc(sz);
  }
}

static void
free_atfork(void* mem, const void *caller)
{
  void *vptr = NULL;
  struct malloc_arena *ar_ptr;
  mchunkptr p;                          /* chunk corresponding to mem */

  if (mem == 0)                              /* free(0) has no effect */
    return;

  p = mem2chunk(mem);

  if (is_mmapped(p)) {                      /* release mmapped memory. */
    ar_ptr = arena_for_mmap_chunk(p);
    munmap_chunk(arena_to_mspace(ar_ptr), p);
    return;
  }

  ar_ptr = arena_for_chunk(p);
  tsd_getspecific(arena_key, vptr);
  if(vptr != ATFORK_ARENA_PTR)
    (void)mutex_lock(&ar_ptr->mutex);
  mspace_free(arena_to_mspace(ar_ptr), mem);
  if(vptr != ATFORK_ARENA_PTR)
    (void)mutex_unlock(&ar_ptr->mutex);
}

/* The following two functions are registered via thread_atfork() to
   make sure that the mutexes remain in a consistent state in the
   fork()ed version of a thread.  Also adapt the malloc and free hooks
   temporarily, because the `atfork' handler mechanism may use
   malloc/free internally (e.g. in LinuxThreads). */

static void
ptmalloc_lock_all (void)
{
  struct malloc_arena* ar_ptr;

  if(__malloc_initialized < 1)
    return;
  (void)mutex_lock(&list_lock);
  for(ar_ptr = &main_arena;;) {
    (void)mutex_lock(&ar_ptr->mutex);
    ar_ptr = ar_ptr->next;
    if(ar_ptr == &main_arena)
      break;
  }
  save_malloc_hook = __malloc_hook;
  save_free_hook = __free_hook;
  __malloc_hook = malloc_atfork;
  __free_hook = free_atfork;
  /* Only the current thread may perform malloc/free calls now. */
  tsd_getspecific(arena_key, save_arena);
  tsd_setspecific(arena_key, ATFORK_ARENA_PTR);
}

static void
ptmalloc_unlock_all (void)
{
  struct malloc_arena *ar_ptr;

  if(__malloc_initialized < 1)
    return;
  tsd_setspecific(arena_key, save_arena);
  __malloc_hook = save_malloc_hook;
  __free_hook = save_free_hook;
  for(ar_ptr = &main_arena;;) {
    (void)mutex_unlock(&ar_ptr->mutex);
    ar_ptr = ar_ptr->next;
    if(ar_ptr == &main_arena) break;
  }
  (void)mutex_unlock(&list_lock);
}

#ifdef __linux__

/* In LinuxThreads, unlocking a mutex in the child process after a
   fork() is currently unsafe, whereas re-initializing it is safe and
   does not leak resources.  Therefore, a special atfork handler is
   installed for the child. */

static void
ptmalloc_unlock_all2(void)
{
  struct malloc_arena *ar_ptr;

  if(__malloc_initialized < 1)
    return;
#if defined _LIBC || 1 /*defined MALLOC_HOOKS*/
  tsd_setspecific(arena_key, save_arena);
  __malloc_hook = save_malloc_hook;
  __free_hook = save_free_hook;
#endif
  for(ar_ptr = &main_arena;;) {
    (void)mutex_init(&ar_ptr->mutex);
    ar_ptr = ar_ptr->next;
    if(ar_ptr == &main_arena) break;
  }
  (void)mutex_init(&list_lock);
}

#else

#define ptmalloc_unlock_all2 ptmalloc_unlock_all

#endif

#endif /* !defined NO_THREADS */

/*---------------------------------------------------------------------*/

#if !(USE_STARTER & 2)
static
#endif
void
ptmalloc_init(void)
{
  const char* s;
  int secure = 0;
  void *mspace;

  if(__malloc_initialized >= 0) return;
  __malloc_initialized = 0;

  /*if (mp_.pagesize == 0)
    ptmalloc_init_minimal();*/

#ifndef NO_THREADS
# if USE_STARTER & 1
  /* With some threads implementations, creating thread-specific data
     or initializing a mutex may call malloc() itself.  Provide a
     simple starter version (realloc() won't work). */
  save_malloc_hook = __malloc_hook;
  save_memalign_hook = __memalign_hook;
  save_free_hook = __free_hook;
  __malloc_hook = malloc_starter;
  __memalign_hook = memalign_starter;
  __free_hook = free_starter;
#  ifdef _LIBC
  /* Initialize the pthreads interface. */
  if (__pthread_initialize != NULL)
    __pthread_initialize();
#  endif /* !defined _LIBC */
# endif /* USE_STARTER & 1 */
#endif /* !defined NO_THREADS */
  mutex_init(&main_arena.mutex);
  main_arena.next = &main_arena;
  mspace = create_mspace_with_base((char*)&main_arena + MSPACE_OFFSET,
				   sizeof(main_arena) - MSPACE_OFFSET,
				   0);
  assert(mspace == arena_to_mspace(&main_arena));

  mutex_init(&list_lock);
  tsd_key_create(&arena_key, NULL);
  tsd_setspecific(arena_key, (void *)&main_arena);
  thread_atfork(ptmalloc_lock_all, ptmalloc_unlock_all, ptmalloc_unlock_all2);
#ifndef NO_THREADS
# if USE_STARTER & 1
  __malloc_hook = save_malloc_hook;
  __memalign_hook = save_memalign_hook;
  __free_hook = save_free_hook;
# endif
# if USE_STARTER & 2
  __malloc_hook = 0;
  __memalign_hook = 0;
  __free_hook = 0;
# endif
#endif
#ifdef _LIBC
  secure = __libc_enable_secure;
#else
  if (! secure) {
    if ((s = getenv("MALLOC_TRIM_THRESHOLD_")))
      public_mALLOPt(M_TRIM_THRESHOLD, atoi(s));
    if ((s = getenv("MALLOC_TOP_PAD_")) ||
	(s = getenv("MALLOC_GRANULARITY_")))
      public_mALLOPt(M_GRANULARITY, atoi(s));
    if ((s = getenv("MALLOC_MMAP_THRESHOLD_")))
      public_mALLOPt(M_MMAP_THRESHOLD, atoi(s));
    /*if ((s = getenv("MALLOC_MMAP_MAX_"))) this is no longer available
      public_mALLOPt(M_MMAP_MAX, atoi(s));*/
  }
  s = getenv("MALLOC_CHECK_");
#endif
  if (s) {
    /*if(s[0]) mALLOPt(M_CHECK_ACTION, (int)(s[0] - '0'));
      __malloc_check_init();*/
  }
  if (__malloc_initialize_hook != NULL)
    (*__malloc_initialize_hook)();
  __malloc_initialized = 1;
}

/*------------------------ Public wrappers. --------------------------------*/

void*
public_mALLOc(size_t bytes)
{
  struct malloc_arena* ar_ptr;
  void *victim;

  void * (*hook) (size_t, const void *) = __malloc_hook;
  if (hook != NULL)
    return (*hook)(bytes, RETURN_ADDRESS (0));

  arena_get(ar_ptr, bytes + FOOTER_OVERHEAD);
  if (!ar_ptr)
    return 0;
  if (ar_ptr != &main_arena)
    bytes += FOOTER_OVERHEAD;
  victim = mspace_malloc(arena_to_mspace(ar_ptr), bytes);
  if (victim && ar_ptr != &main_arena)
    set_non_main_arena(victim, ar_ptr);
  (void)mutex_unlock(&ar_ptr->mutex);
  assert(!victim || is_mmapped(mem2chunk(victim)) ||
	 ar_ptr == arena_for_chunk(mem2chunk(victim)));
  return victim;
}
#ifdef libc_hidden_def
libc_hidden_def(public_mALLOc)
#endif

void
public_fREe(void* mem)
{
  struct malloc_arena* ar_ptr;
  mchunkptr p;                          /* chunk corresponding to mem */

  void (*hook) (void *, const void *) = __free_hook;
  if (hook != NULL) {
    (*hook)(mem, RETURN_ADDRESS (0));
    return;
  }

  if (mem == 0)                              /* free(0) has no effect */
    return;

  p = mem2chunk(mem);

  if (is_mmapped(p)) {                      /* release mmapped memory. */
    ar_ptr = arena_for_mmap_chunk(p);
    munmap_chunk(arena_to_mspace(ar_ptr), p);
    return;
  }

  ar_ptr = arena_for_chunk(p);
#if THREAD_STATS
  if(!mutex_trylock(&ar_ptr->mutex))
    ++(ar_ptr->stat_lock_direct);
  else {
    (void)mutex_lock(&ar_ptr->mutex);
    ++(ar_ptr->stat_lock_wait);
  }
#else
  (void)mutex_lock(&ar_ptr->mutex);
#endif
  mspace_free(arena_to_mspace(ar_ptr), mem);
  (void)mutex_unlock(&ar_ptr->mutex);
}
#ifdef libc_hidden_def
libc_hidden_def (public_fREe)
#endif

void*
public_rEALLOc(void* oldmem, size_t bytes)
{
  struct malloc_arena* ar_ptr;

  mchunkptr oldp;             /* chunk corresponding to oldmem */

  void* newp;             /* chunk to return */

  void * (*hook) (void *, size_t, const void *) = __realloc_hook;
  if (hook != NULL)
    return (*hook)(oldmem, bytes, RETURN_ADDRESS (0));

#if REALLOC_ZERO_BYTES_FREES
  if (bytes == 0 && oldmem != NULL) { public_fREe(oldmem); return 0; }
#endif

  /* realloc of null is supposed to be same as malloc */
  if (oldmem == 0)
    return public_mALLOc(bytes);

  oldp    = mem2chunk(oldmem);
  if (is_mmapped(oldp))
    ar_ptr = arena_for_mmap_chunk(oldp); /* FIXME: use mmap_resize */
  else
    ar_ptr = arena_for_chunk(oldp);
#if THREAD_STATS
  if(!mutex_trylock(&ar_ptr->mutex))
    ++(ar_ptr->stat_lock_direct);
  else {
    (void)mutex_lock(&ar_ptr->mutex);
    ++(ar_ptr->stat_lock_wait);
  }
#else
  (void)mutex_lock(&ar_ptr->mutex);
#endif

#ifndef NO_THREADS
  /* As in malloc(), remember this arena for the next allocation. */
  tsd_setspecific(arena_key, (void *)ar_ptr);
#endif

  if (ar_ptr != &main_arena)
    bytes += FOOTER_OVERHEAD;
  newp = mspace_realloc(arena_to_mspace(ar_ptr), oldmem, bytes);

  if (newp && ar_ptr != &main_arena)
    set_non_main_arena(newp, ar_ptr);
  (void)mutex_unlock(&ar_ptr->mutex);

  assert(!newp || is_mmapped(mem2chunk(newp)) ||
	 ar_ptr == arena_for_chunk(mem2chunk(newp)));
  return newp;
}
#ifdef libc_hidden_def
libc_hidden_def (public_rEALLOc)
#endif

void*
public_mEMALIGn(size_t alignment, size_t bytes)
{
  struct malloc_arena* ar_ptr;
  void *p;

  void * (*hook) (size_t, size_t, const void *) = __memalign_hook;
  if (hook != NULL)
    return (*hook)(alignment, bytes, RETURN_ADDRESS (0));

  /* If need less alignment than we give anyway, just relay to malloc */
  if (alignment <= MALLOC_ALIGNMENT) return public_mALLOc(bytes);

  /* Otherwise, ensure that it is at least a minimum chunk size */
  if (alignment <  MIN_CHUNK_SIZE)
    alignment = MIN_CHUNK_SIZE;

  arena_get(ar_ptr,
	    bytes + FOOTER_OVERHEAD + alignment + MIN_CHUNK_SIZE);
  if(!ar_ptr)
    return 0;

  if (ar_ptr != &main_arena)
    bytes += FOOTER_OVERHEAD;
  p = mspace_memalign(arena_to_mspace(ar_ptr), alignment, bytes);

  if (p && ar_ptr != &main_arena)
    set_non_main_arena(p, ar_ptr);
  (void)mutex_unlock(&ar_ptr->mutex);

  assert(!p || is_mmapped(mem2chunk(p)) ||
	 ar_ptr == arena_for_chunk(mem2chunk(p)));
  return p;
}
#ifdef libc_hidden_def
libc_hidden_def (public_mEMALIGn)
#endif

void*
public_vALLOc(size_t bytes)
{
  struct malloc_arena* ar_ptr;
  void *p;

  if(__malloc_initialized < 0)
    ptmalloc_init ();
  arena_get(ar_ptr, bytes + FOOTER_OVERHEAD + MIN_CHUNK_SIZE);
  if(!ar_ptr)
    return 0;
  if (ar_ptr != &main_arena)
    bytes += FOOTER_OVERHEAD;
  p = mspace_memalign(arena_to_mspace(ar_ptr), 4096, bytes);

  if (p && ar_ptr != &main_arena)
    set_non_main_arena(p, ar_ptr);
  (void)mutex_unlock(&ar_ptr->mutex);
  return p;
}

int
public_pMEMALIGn (void **memptr, size_t alignment, size_t size)
{
  void *mem;

  /* Test whether the SIZE argument is valid.  It must be a power of
     two multiple of sizeof (void *).  */
  if (alignment % sizeof (void *) != 0
      || !my_powerof2 (alignment / sizeof (void *)) != 0
      || alignment == 0)
    return EINVAL;

  mem = public_mEMALIGn (alignment, size);

  if (mem != NULL) {
    *memptr = mem;
    return 0;
  }

  return ENOMEM;
}

void*
public_cALLOc(size_t n_elements, size_t elem_size)
{
  struct malloc_arena* ar_ptr;
  size_t bytes, sz;
  void* mem;
  void * (*hook) (size_t, const void *) = __malloc_hook;

  /* size_t is unsigned so the behavior on overflow is defined.  */
  bytes = n_elements * elem_size;
#define HALF_INTERNAL_SIZE_T \
  (((size_t) 1) << (8 * sizeof (size_t) / 2))
  if (__builtin_expect ((n_elements | elem_size) >= HALF_INTERNAL_SIZE_T, 0)) {
    if (elem_size != 0 && bytes / elem_size != n_elements) {
      /*MALLOC_FAILURE_ACTION;*/
      return 0;
    }
  }

  if (hook != NULL) {
    sz = bytes;
    mem = (*hook)(sz, RETURN_ADDRESS (0));
    if(mem == 0)
      return 0;
#ifdef HAVE_MEMCPY
    return memset(mem, 0, sz);
#else
    while(sz > 0) ((char*)mem)[--sz] = 0; /* rather inefficient */
    return mem;
#endif
  }

  arena_get(ar_ptr, bytes + FOOTER_OVERHEAD);
  if(!ar_ptr)
    return 0;

  if (ar_ptr != &main_arena)
    bytes += FOOTER_OVERHEAD;
  mem = mspace_calloc(arena_to_mspace(ar_ptr), bytes, 1);

  if (mem && ar_ptr != &main_arena)
    set_non_main_arena(mem, ar_ptr);
  (void)mutex_unlock(&ar_ptr->mutex);
  
  assert(!mem || is_mmapped(mem2chunk(mem)) ||
	 ar_ptr == arena_for_chunk(mem2chunk(mem)));

  return mem;
}

void**
public_iCALLOc(size_t n, size_t elem_size, void* chunks[])
{
  struct malloc_arena* ar_ptr;
  void** m;

  arena_get(ar_ptr, n*(elem_size + FOOTER_OVERHEAD));
  if (!ar_ptr)
    return 0;

  if (ar_ptr != &main_arena)
    elem_size += FOOTER_OVERHEAD;
  m = mspace_independent_calloc(arena_to_mspace(ar_ptr), n, elem_size, chunks);

  if (m && ar_ptr != &main_arena) {
    while (n > 0)
      set_non_main_arena(m[--n], ar_ptr);
  }
  (void)mutex_unlock(&ar_ptr->mutex);
  return m;
}

void**
public_iCOMALLOc(size_t n, size_t sizes[], void* chunks[])
{
  struct malloc_arena* ar_ptr;
  size_t* m_sizes;
  size_t i;
  void** m;

  arena_get(ar_ptr, n*sizeof(size_t));
  if (!ar_ptr)
    return 0;

  if (ar_ptr != &main_arena) {
    /* Temporary m_sizes[] array is ugly but it would be surprising to
       change the original sizes[]... */
    m_sizes = mspace_malloc(arena_to_mspace(ar_ptr), n*sizeof(size_t));
    if (!m_sizes) {
      (void)mutex_unlock(&ar_ptr->mutex);
      return 0;
    }
    for (i=0; i<n; ++i)
      m_sizes[i] = sizes[i] + FOOTER_OVERHEAD;
    if (!chunks) {
      chunks = mspace_malloc(arena_to_mspace(ar_ptr),
			     n*sizeof(void*)+FOOTER_OVERHEAD);
      if (!chunks) {
	mspace_free(arena_to_mspace(ar_ptr), m_sizes);
	(void)mutex_unlock(&ar_ptr->mutex);
	return 0;
      }
      set_non_main_arena(chunks, ar_ptr);
    }
  } else
    m_sizes = sizes;

  m = mspace_independent_comalloc(arena_to_mspace(ar_ptr), n, m_sizes, chunks);

  if (ar_ptr != &main_arena) {
    mspace_free(arena_to_mspace(ar_ptr), m_sizes);
    if (m)
      for (i=0; i<n; ++i)
	set_non_main_arena(m[i], ar_ptr);
  }
  (void)mutex_unlock(&ar_ptr->mutex);
  return m;
}

#if 0 && !defined _LIBC

void
public_cFREe(void* m)
{
  public_fREe(m);
}

#endif /* _LIBC */

int
public_mTRIm(size_t s)
{
  int result;

  (void)mutex_lock(&main_arena.mutex);
  result = mspace_trim(arena_to_mspace(&main_arena), s);
  (void)mutex_unlock(&main_arena.mutex);
  return result;
}

size_t
public_mUSABLe(void* mem)
{
  if (mem != 0) {
    mchunkptr p = mem2chunk(mem);
    if (cinuse(p))
      return chunksize(p) - overhead_for(p);
  }
  return 0;
}

int
public_mALLOPt(int p, int v)
{
  int result;
  result = mspace_mallopt(p, v);
  return result;
}

void
public_mSTATs(void)
{
  int i;
  struct malloc_arena* ar_ptr;
  /*unsigned long in_use_b, system_b, avail_b;*/
#if THREAD_STATS
  long stat_lock_direct = 0, stat_lock_loop = 0, stat_lock_wait = 0;
#endif

  if(__malloc_initialized < 0)
    ptmalloc_init ();
  for (i=0, ar_ptr = &main_arena;; ++i) {
    struct malloc_state* msp = arena_to_mspace(ar_ptr);

    fprintf(stderr, "Arena %d:\n", i);
    mspace_malloc_stats(msp);
#if THREAD_STATS
    stat_lock_direct += ar_ptr->stat_lock_direct;
    stat_lock_loop += ar_ptr->stat_lock_loop;
    stat_lock_wait += ar_ptr->stat_lock_wait;
#endif
    if (MALLOC_DEBUG > 1) {
      struct malloc_segment* mseg = &msp->seg;
      while (mseg) {
	fprintf(stderr, " seg %08lx-%08lx\n", (unsigned long)mseg->base,
		(unsigned long)(mseg->base + mseg->size));
	mseg = mseg->next;
      }
    }
    ar_ptr = ar_ptr->next;
    if (ar_ptr == &main_arena)
      break;
  }
#if THREAD_STATS
  fprintf(stderr, "locked directly  = %10ld\n", stat_lock_direct);
  fprintf(stderr, "locked in loop   = %10ld\n", stat_lock_loop);
  fprintf(stderr, "locked waiting   = %10ld\n", stat_lock_wait);
  fprintf(stderr, "locked total     = %10ld\n",
          stat_lock_direct + stat_lock_loop + stat_lock_wait);
  if (main_arena.stat_starter > 0)
    fprintf(stderr, "starter hooks    = %10ld\n", main_arena.stat_starter);
#endif
}

/*
 * Local variables:
 * c-basic-offset: 2
 * End:
 */

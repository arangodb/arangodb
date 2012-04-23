/* Basic platform-independent macro definitions for mutexes,
   thread-specific data and parameters for malloc.
   Posix threads (pthreads) version.
   Copyright (C) 2004 Wolfram Gloger <wg@malloc.de>.

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

#ifndef _PTHREAD_MALLOC_MACHINE_H
#define _PTHREAD_MALLOC_MACHINE_H

#include <pthread.h>

#undef thread_atfork_static

/* Use fast inline spinlocks with gcc.  */
#if (defined __i386__ || defined __x86_64__) && defined __GNUC__ && \
    !defined USE_NO_SPINLOCKS

#include <time.h>
#include <sched.h>

typedef struct {
  volatile unsigned int lock;
  int pad0_;
} mutex_t;

#define MUTEX_INITIALIZER          { 0 }
#define mutex_init(m)              ((m)->lock = 0)
static inline int mutex_lock(mutex_t *m) {
  int cnt = 0, r;
  struct timespec tm;

  for(;;) {
    __asm__ __volatile__
      ("xchgl %0, %1"
       : "=r"(r), "=m"(m->lock)
       : "0"(1), "m"(m->lock)
       : "memory");
    if(!r)
      return 0;
    if(cnt < 50) {
      sched_yield();
      cnt++;
    } else {
      tm.tv_sec = 0;
      tm.tv_nsec = 2000001;
      nanosleep(&tm, NULL);
      cnt = 0;
    }
  }
}
static inline int mutex_trylock(mutex_t *m) {
  int r;

  __asm__ __volatile__
    ("xchgl %0, %1"
     : "=r"(r), "=m"(m->lock)
     : "0"(1), "m"(m->lock)
     : "memory");
  return r;
}
static inline int mutex_unlock(mutex_t *m) {
  __asm__ __volatile__ ("movl %1, %0" : "=m" (m->lock) : "g"(0) : "memory");
  return 0;
}

#else

/* Normal pthread mutex.  */
typedef pthread_mutex_t mutex_t;

#define MUTEX_INITIALIZER          PTHREAD_MUTEX_INITIALIZER
#define mutex_init(m)              pthread_mutex_init(m, NULL)
#define mutex_lock(m)              pthread_mutex_lock(m)
#define mutex_trylock(m)           pthread_mutex_trylock(m)
#define mutex_unlock(m)            pthread_mutex_unlock(m)

#endif /* (__i386__ || __x86_64__) && __GNUC__ && !USE_NO_SPINLOCKS */

/* thread specific data */
#if defined(__sgi) || defined(USE_TSD_DATA_HACK)

/* Hack for thread-specific data, e.g. on Irix 6.x.  We can't use
   pthread_setspecific because that function calls malloc() itself.
   The hack only works when pthread_t can be converted to an integral
   type. */

typedef void *tsd_key_t[256];
#define tsd_key_create(key, destr) do { \
  int i; \
  for(i=0; i<256; i++) (*key)[i] = 0; \
} while(0)
#define tsd_setspecific(key, data) \
 (key[(unsigned)pthread_self() % 256] = (data))
#define tsd_getspecific(key, vptr) \
 (vptr = key[(unsigned)pthread_self() % 256])

#else

typedef pthread_key_t tsd_key_t;

#define tsd_key_create(key, destr) pthread_key_create(key, destr)
#define tsd_setspecific(key, data) pthread_setspecific(key, data)
#define tsd_getspecific(key, vptr) (vptr = pthread_getspecific(key))

#endif

/* at fork */
#define thread_atfork(prepare, parent, child) \
                                   pthread_atfork(prepare, parent, child)

#include <sysdeps/generic/malloc-machine.h>

#endif /* !defined(_MALLOC_MACHINE_H) */

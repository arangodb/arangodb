/* Basic platform-independent macro definitions for mutexes,
   thread-specific data and parameters for malloc.
   Solaris threads version.
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

#ifndef _SOLARIS_MALLOC_MACHINE_H
#define _SOLARIS_MALLOC_MACHINE_H

#include <thread.h>

typedef thread_t thread_id;

#define MUTEX_INITIALIZER          { 0 }
#define mutex_init(m)              mutex_init(m, USYNC_THREAD, NULL)

/*
 * Hack for thread-specific data on Solaris.  We can't use thr_setspecific
 * because that function calls malloc() itself.
 */
typedef void *tsd_key_t[256];
#define tsd_key_create(key, destr) do { \
  int i; \
  for(i=0; i<256; i++) (*key)[i] = 0; \
} while(0)
#define tsd_setspecific(key, data) (key[(unsigned)thr_self() % 256] = (data))
#define tsd_getspecific(key, vptr) (vptr = key[(unsigned)thr_self() % 256])

#define thread_atfork(prepare, parent, child) do {} while(0)

#include <sysdeps/generic/malloc-machine.h>

#endif /* !defined(_SOLARIS_MALLOC_MACHINE_H) */

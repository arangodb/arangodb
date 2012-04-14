/* Basic platform-independent macro definitions for mutexes,
   thread-specific data and parameters for malloc.
   SGI threads (sprocs) version.
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

#ifndef _SPROC_MALLOC_MACHINE_H
#define _SPROC_MALLOC_MACHINE_H

#include <sys/wait.h>
#include <sys/types.h>
#include <sys/prctl.h>
#include <abi_mutex.h>

typedef abilock_t mutex_t;

#define MUTEX_INITIALIZER          { 0 }
#define mutex_init(m)              init_lock(m)
#define mutex_lock(m)              (spin_lock(m), 0)
#define mutex_trylock(m)           acquire_lock(m)
#define mutex_unlock(m)            release_lock(m)

typedef int tsd_key_t;
int tsd_key_next;
#define tsd_key_create(key, destr) ((*key) = tsd_key_next++)
#define tsd_setspecific(key, data) (((void **)(&PRDA->usr_prda))[key] = data)
#define tsd_getspecific(key, vptr) (vptr = ((void **)(&PRDA->usr_prda))[key])

#define thread_atfork(prepare, parent, child) do {} while(0)

#include <sysdeps/generic/malloc-machine.h>

#endif /* !defined(_SPROC_MALLOC_MACHINE_H) */

/***************************************************************************
 *                                  _   _ ____  _
 *  Project                     ___| | | |  _ \| |
 *                             / __| | | | |_) | |
 *                            | (__| |_| |  _ <| |___
 *                             \___|\___/|_| \_\_____|
 *
 * Copyright (C) 2013-2017, Daniel Stenberg, <daniel@haxx.se>, et al.
 * Copyright (C) 2010, 2011, Hoi-Ho Chan, <hoiho.chan@gmail.com>
 *
 * This software is licensed as described in the file COPYING, which
 * you should have received as part of this distribution. The terms
 * are also available at https://curl.haxx.se/docs/copyright.html.
 *
 * You may opt to use, copy, modify, merge, publish, distribute and/or sell
 * copies of the Software, and permit persons to whom the Software is
 * furnished to do so, under the terms of the COPYING file.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ***************************************************************************/
#include "curl_setup.h"

#if (defined(USE_POLARSSL) || defined(USE_MBEDTLS)) && \
    ((defined(USE_THREADS_POSIX) && defined(HAVE_PTHREAD_H)) || \
     (defined(USE_THREADS_WIN32) && defined(HAVE_PROCESS_H)))

#if defined(USE_THREADS_POSIX) && defined(HAVE_PTHREAD_H)
#  include <pthread.h>
#  define POLARSSL_MUTEX_T pthread_mutex_t
#elif defined(USE_THREADS_WIN32) && defined(HAVE_PROCESS_H)
#  include <process.h>
#  define POLARSSL_MUTEX_T HANDLE
#endif

#include "polarssl_threadlock.h"
#include "curl_printf.h"
#include "curl_memory.h"
/* The last #include file should be: */
#include "memdebug.h"

/* number of thread locks */
#define NUMT                    2

/* This array will store all of the mutexes available to PolarSSL. */
static POLARSSL_MUTEX_T *mutex_buf = NULL;

int Curl_polarsslthreadlock_thread_setup(void)
{
  int i;

  mutex_buf = calloc(NUMT * sizeof(POLARSSL_MUTEX_T), 1);
  if(!mutex_buf)
    return 0;     /* error, no number of threads defined */

  for(i = 0;  i < NUMT;  i++) {
    int ret;
#if defined(USE_THREADS_POSIX) && defined(HAVE_PTHREAD_H)
    ret = pthread_mutex_init(&mutex_buf[i], NULL);
    if(ret)
      return 0; /* pthread_mutex_init failed */
#elif defined(USE_THREADS_WIN32) && defined(HAVE_PROCESS_H)
    mutex_buf[i] = CreateMutex(0, FALSE, 0);
    if(mutex_buf[i] == 0)
      return 0;  /* CreateMutex failed */
#endif /* USE_THREADS_POSIX && HAVE_PTHREAD_H */
  }

  return 1; /* OK */
}

int Curl_polarsslthreadlock_thread_cleanup(void)
{
  int i;

  if(!mutex_buf)
    return 0; /* error, no threads locks defined */

  for(i = 0; i < NUMT; i++) {
    int ret;
#if defined(USE_THREADS_POSIX) && defined(HAVE_PTHREAD_H)
    ret = pthread_mutex_destroy(&mutex_buf[i]);
    if(ret)
      return 0; /* pthread_mutex_destroy failed */
#elif defined(USE_THREADS_WIN32) && defined(HAVE_PROCESS_H)
    ret = CloseHandle(mutex_buf[i]);
    if(!ret)
      return 0; /* CloseHandle failed */
#endif /* USE_THREADS_POSIX && HAVE_PTHREAD_H */
  }
  free(mutex_buf);
  mutex_buf = NULL;

  return 1; /* OK */
}

int Curl_polarsslthreadlock_lock_function(int n)
{
  if(n < NUMT) {
    int ret;
#if defined(USE_THREADS_POSIX) && defined(HAVE_PTHREAD_H)
    ret = pthread_mutex_lock(&mutex_buf[n]);
    if(ret) {
      DEBUGF(fprintf(stderr,
                     "Error: polarsslthreadlock_lock_function failed\n"));
      return 0; /* pthread_mutex_lock failed */
    }
#elif defined(USE_THREADS_WIN32) && defined(HAVE_PROCESS_H)
    ret = (WaitForSingleObject(mutex_buf[n], INFINITE) == WAIT_FAILED?1:0);
    if(ret) {
      DEBUGF(fprintf(stderr,
                     "Error: polarsslthreadlock_lock_function failed\n"));
      return 0; /* pthread_mutex_lock failed */
    }
#endif /* USE_THREADS_POSIX && HAVE_PTHREAD_H */
  }
  return 1; /* OK */
}

int Curl_polarsslthreadlock_unlock_function(int n)
{
  if(n < NUMT) {
    int ret;
#if defined(USE_THREADS_POSIX) && defined(HAVE_PTHREAD_H)
    ret = pthread_mutex_unlock(&mutex_buf[n]);
    if(ret) {
      DEBUGF(fprintf(stderr,
                     "Error: polarsslthreadlock_unlock_function failed\n"));
      return 0; /* pthread_mutex_unlock failed */
    }
#elif defined(USE_THREADS_WIN32) && defined(HAVE_PROCESS_H)
    ret = ReleaseMutex(mutex_buf[n]);
    if(!ret) {
      DEBUGF(fprintf(stderr,
                     "Error: polarsslthreadlock_unlock_function failed\n"));
      return 0; /* pthread_mutex_lock failed */
    }
#endif /* USE_THREADS_POSIX && HAVE_PTHREAD_H */
  }
  return 1; /* OK */
}

#endif /* USE_POLARSSL || USE_MBEDTLS */

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include "threads.h"

#ifdef TRI_HAVE_POSIX_THREADS

#ifdef TRI_HAVE_SYS_PRCTL_H
#include <sys/prctl.h>
#endif

#include "Basics/Logger.h"
#include "Basics/tri-strings.h"

////////////////////////////////////////////////////////////////////////////////
/// @brief data block for thread starter
////////////////////////////////////////////////////////////////////////////////

struct thread_data_t {
  void (*starter)(void*);
  void* _data;
  char* _name;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief starter function for thread
////////////////////////////////////////////////////////////////////////////////

static void* ThreadStarter(void* data) {
  sigset_t all;
  sigfillset(&all);
  pthread_sigmask(SIG_SETMASK, &all, 0);

  thread_data_t* d = static_cast<thread_data_t*>(data);

  TRI_ASSERT(d != nullptr);

#ifdef TRI_HAVE_SYS_PRCTL_H
  prctl(PR_SET_NAME, d->_name, 0, 0, 0);
#endif

  try {
    d->starter(d->_data);
  } catch (...) {
    TRI_FreeString(TRI_CORE_MEM_ZONE, d->_name);
    TRI_Free(TRI_CORE_MEM_ZONE, d);
    throw;
  }

  TRI_FreeString(TRI_CORE_MEM_ZONE, d->_name);
  TRI_Free(TRI_CORE_MEM_ZONE, d);

  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initializes a thread
////////////////////////////////////////////////////////////////////////////////

void TRI_InitThread(TRI_thread_t* thread) {
  memset(thread, 0, sizeof(TRI_thread_t));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the current process identifier
////////////////////////////////////////////////////////////////////////////////

TRI_pid_t TRI_CurrentProcessId() { return getpid(); }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the current thread identifier
////////////////////////////////////////////////////////////////////////////////

TRI_tid_t TRI_CurrentThreadId() { return pthread_self(); }

////////////////////////////////////////////////////////////////////////////////
/// @brief starts a thread
////////////////////////////////////////////////////////////////////////////////

bool TRI_StartThread(TRI_thread_t* thread, TRI_tid_t* threadId,
                     char const* name, void (*starter)(void*), void* data) {
  thread_data_t* d = static_cast<thread_data_t*>(
      TRI_Allocate(TRI_CORE_MEM_ZONE, sizeof(thread_data_t), false));

  d->starter = starter;
  d->_data = data;
  d->_name = TRI_DuplicateString(name);

  int rc = pthread_create(thread, 0, &ThreadStarter, d);

  if (rc != 0) {
    TRI_set_errno(TRI_ERROR_SYS_ERROR);
    LOG(ERR) << "could not start thread: " << strerror(errno);

    TRI_Free(TRI_CORE_MEM_ZONE, d);
    return false;
  }

  if (threadId != nullptr) {
    *threadId = (TRI_tid_t)*thread;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief trys to stops a thread
////////////////////////////////////////////////////////////////////////////////

int TRI_StopThread(TRI_thread_t* thread) { return pthread_cancel(*thread); }

////////////////////////////////////////////////////////////////////////////////
/// @brief detaches a thread
////////////////////////////////////////////////////////////////////////////////

int TRI_DetachThread(TRI_thread_t* thread) { return pthread_detach(*thread); }

////////////////////////////////////////////////////////////////////////////////
/// @brief waits for a thread to finish
////////////////////////////////////////////////////////////////////////////////

int TRI_JoinThread(TRI_thread_t* thread) { return pthread_join(*thread, 0); }

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if we are the thread
////////////////////////////////////////////////////////////////////////////////

bool TRI_IsSelfThread(TRI_thread_t* thread) {
  return pthread_self() == *thread;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief allow cancelation
////////////////////////////////////////////////////////////////////////////////

void TRI_AllowCancelation() {
  pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, 0);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the process affinity
////////////////////////////////////////////////////////////////////////////////

void TRI_SetProcessorAffinity(TRI_thread_t* thread, size_t core) {
#ifdef TRI_HAVE_THREAD_AFFINITY

  cpu_set_t cpuset;

  CPU_ZERO(&cpuset);
  CPU_SET(core, &cpuset);

  int s = pthread_setaffinity_np(*thread, sizeof(cpu_set_t), &cpuset);

  if (s != 0) {
    LOG(ERR) << "cannot set affinity to core " << core << ": " << strerror(errno);
  }

#endif
}

#endif

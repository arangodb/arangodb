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

#ifdef ARANGODB_HAVE_THREAD_POLICY
#include <mach/mach.h>
#endif

#include "Basics/tri-strings.h"
#include "Logger/Logger.h"

////////////////////////////////////////////////////////////////////////////////
/// @brief data block for thread starter
////////////////////////////////////////////////////////////////////////////////

struct thread_data_t {
  void (*_starter)(void*);
  void* _data;
  std::string _name;

  thread_data_t(void (*starter)(void*), void* data, char const* name) 
      : _starter(starter),
        _data(data),
        _name(name) {}
};

////////////////////////////////////////////////////////////////////////////////
/// @brief starter function for thread
////////////////////////////////////////////////////////////////////////////////

static void* ThreadStarter(void* data) {
  sigset_t all;
  sigfillset(&all);
  pthread_sigmask(SIG_SETMASK, &all, 0);

  // this will automatically free the thread struct when leaving this function
  std::unique_ptr<thread_data_t> d(static_cast<thread_data_t*>(data));

  TRI_ASSERT(d != nullptr);

#ifdef TRI_HAVE_SYS_PRCTL_H
  prctl(PR_SET_NAME, d->_name.c_str(), 0, 0, 0);
#endif

  try {
    d->_starter(d->_data);
  } catch (...) {
    // we must not throw from here
  }

  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initializes a thread
////////////////////////////////////////////////////////////////////////////////

void TRI_InitThread(TRI_thread_t* thread) {
  memset(thread, 0, sizeof(TRI_thread_t));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief starts a thread
////////////////////////////////////////////////////////////////////////////////

bool TRI_StartThread(TRI_thread_t* thread, TRI_tid_t* threadId,
                     char const* name, void (*starter)(void*), void* data) {
  std::unique_ptr<thread_data_t> d;

  try {
    d.reset(new thread_data_t(starter, data, name));
  } catch (...) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "could not start thread: out of memory";
    return false;
  }

  TRI_ASSERT(d != nullptr);

  int rc = pthread_create(thread, nullptr, &ThreadStarter, d.get());

  if (rc != 0) {
    errno = rc;
    TRI_set_errno(TRI_ERROR_SYS_ERROR);
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "could not start thread: " << strerror(errno);

    return false;
  }

  if (threadId != nullptr) {
    *threadId = (TRI_tid_t)*thread;
  }

  // object must linger around until later
  d.release();

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief waits for a thread to finish
////////////////////////////////////////////////////////////////////////////////

int TRI_JoinThread(TRI_thread_t* thread) {
  TRI_ASSERT(!TRI_IsSelfThread(thread));
  int res = pthread_join(*thread, nullptr);

  if (res != TRI_ERROR_NO_ERROR) {
    LOG_TOPIC(WARN, arangodb::Logger::THREADS) << "cannot join thread: " << strerror(res);
  }
  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief detaches a thread
////////////////////////////////////////////////////////////////////////////////

int TRI_DetachThread(TRI_thread_t* thread) {
  int res = pthread_detach(*thread);

  if (res != TRI_ERROR_NO_ERROR) {
    LOG_TOPIC(WARN, arangodb::Logger::THREADS) << "cannot detach thread: " << strerror(res);
  }
  return res;
}

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
#ifdef ARANGODB_HAVE_THREAD_AFFINITY

  cpu_set_t cpuset;

  CPU_ZERO(&cpuset);
  CPU_SET(core, &cpuset);

  int s = pthread_setaffinity_np(*thread, sizeof(cpu_set_t), &cpuset);

  if (s != 0) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "cannot set affinity to core " << core << ": "
             << strerror(errno);
  }

#endif

#ifdef ARANGODB_HAVE_THREAD_POLICY

  thread_affinity_policy_data_t policy = {(int)core};
  auto mach_thread = pthread_mach_thread_np(*thread);
  auto res = thread_policy_set(mach_thread, THREAD_AFFINITY_POLICY,
                               (thread_policy_t)&policy, 1);

  if (res != KERN_SUCCESS) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "cannot set affinity to core " << core << ": "
             << strerror(errno);
  }

#endif
}

#endif

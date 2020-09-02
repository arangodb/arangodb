////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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

#include <string.h>

#include "threads.h"

#include "Basics/application-exit.h"
#include "Basics/debugging.h"
#include "Basics/error.h"
#include "Basics/voc-errors.h"

#ifdef TRI_HAVE_POSIX_THREADS
#include <time.h>

#ifdef TRI_HAVE_SYS_PRCTL_H
#include <sys/prctl.h>
#endif

#ifdef ARANGODB_HAVE_THREAD_POLICY
#include <mach/mach.h>
#endif

#include "Basics/signals.h"
#include "Basics/tri-strings.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"

////////////////////////////////////////////////////////////////////////////////
/// @brief data block for thread starter
////////////////////////////////////////////////////////////////////////////////

struct thread_data_t {
  void (*_starter)(void*);
  void* _data;
  std::string _name;

  thread_data_t(void (*starter)(void*), void* data, char const* name)
      : _starter(starter), _data(data), _name(name) {}
};

////////////////////////////////////////////////////////////////////////////////
/// @brief starter function for thread
////////////////////////////////////////////////////////////////////////////////

static void* ThreadStarter(void* data) {
  arangodb::signals::maskAllSignals();

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

bool TRI_StartThread(TRI_thread_t* thread, char const* name,
                     void (*starter)(void*), void* data) {
  std::unique_ptr<thread_data_t> d;

  try {
    d.reset(new thread_data_t(starter, data, name));
  } catch (...) {
    LOG_TOPIC("a89bc", ERR, arangodb::Logger::FIXME)
        << "could not start thread: out of memory";
    return false;
  }

  TRI_ASSERT(d != nullptr);

  pthread_attr_t stackSizeAttribute;
  size_t stackSize = 0;

  auto err = pthread_attr_init(&stackSizeAttribute);
  if (err) {
    LOG_TOPIC("a51ed", ERR, arangodb::Logger::FIXME)
        << "could not initialize stack size attribute.";
    return false;
  }
  err = pthread_attr_getstacksize(&stackSizeAttribute, &stackSize);
  if (err) {
    LOG_TOPIC("0b962", ERR, arangodb::Logger::FIXME)
        << "could not acquire stack size from pthread.";
    return false;
  }

  if (stackSize < 8388608) {  // 8MB
    err = pthread_attr_setstacksize(&stackSizeAttribute, 8388608);
    if (err) {
      LOG_TOPIC("94bca", ERR, arangodb::Logger::FIXME)
          << "could not assign new stack size in pthread.";
      return false;
    }
  }

  int rc = pthread_create(thread, &stackSizeAttribute, &ThreadStarter, d.get());

  if (rc != 0) {
    errno = rc;
    TRI_set_errno(TRI_ERROR_SYS_ERROR);
    LOG_TOPIC("f91c0", ERR, arangodb::Logger::FIXME)
        << "could not start thread: " << strerror(errno);

    return false;
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
    LOG_TOPIC("d5426", WARN, arangodb::Logger::THREADS) << "cannot join thread: " << strerror(res);
  }
  return res;
}

#ifndef __APPLE__

////////////////////////////////////////////////////////////////////////////////
/// @brief waits for a thread to finish within the specified timeout (in ms).
////////////////////////////////////////////////////////////////////////////////

int TRI_JoinThreadWithTimeout(TRI_thread_t* thread, std::uint32_t timeout) {
  if (timeout == INFINITE) {
    return TRI_JoinThread(thread);
  }
  
  TRI_ASSERT(!TRI_IsSelfThread(thread));
  
  timespec ts;
  if (!timespec_get(&ts, TIME_UTC)) {
    LOG_TOPIC("80661", FATAL, arangodb::Logger::FIXME) << "could not initialize timespec with current time";
    FATAL_ERROR_ABORT();
  }
  ts.tv_sec += timeout / 1000;
  ts.tv_nsec = (timeout % 1000) * 1'000'000;

  int res = pthread_timedjoin_np(*thread, nullptr, &ts);
  if (res != TRI_ERROR_NO_ERROR) {
    LOG_TOPIC("1f02d", WARN, arangodb::Logger::THREADS) << "cannot join thread: " << strerror(res);
    return TRI_ERROR_FAILED;
  }
  return TRI_ERROR_NO_ERROR;
}

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief detaches a thread
////////////////////////////////////////////////////////////////////////////////

int TRI_DetachThread(TRI_thread_t* thread) {
  int res = pthread_detach(*thread);

  if (res != TRI_ERROR_NO_ERROR) {
    LOG_TOPIC("e880a", WARN, arangodb::Logger::THREADS)
        << "cannot detach thread: " << strerror(res);
  }
  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if we are the thread
////////////////////////////////////////////////////////////////////////////////

bool TRI_IsSelfThread(TRI_thread_t* thread) {
  return pthread_self() == *thread;
}

#endif

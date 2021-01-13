////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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

#include <errno.h>
#include <string.h>
#include <chrono>
#include <thread>

#include "locks.h"

#ifdef TRI_HAVE_POSIX_THREADS

#ifdef TRI_HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#include "Basics/application-exit.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"

/// @brief initializes a new condition variable
void TRI_InitCondition(TRI_condition_t* cond) {
  pthread_cond_init(&cond->_cond, nullptr);
  pthread_mutex_init(&cond->_mutex, nullptr);
}

/// @brief destroys a condition variable
void TRI_DestroyCondition(TRI_condition_t* cond) {
  pthread_cond_destroy(&cond->_cond);
  pthread_mutex_destroy(&cond->_mutex);
}

/// @brief signals a condition variable
///
/// Note that you must hold the lock.
void TRI_SignalCondition(TRI_condition_t* cond) {
  int rc = pthread_cond_signal(&cond->_cond);

  if (rc != 0) {
    LOG_TOPIC("59b64", FATAL, arangodb::Logger::FIXME)
        << "could not signal the condition: " << strerror(rc);
    FATAL_ERROR_ABORT();
  }
}

/// @brief broad casts a condition variable
///
/// Note that you must hold the lock.
void TRI_BroadcastCondition(TRI_condition_t* cond) {
  int rc = pthread_cond_broadcast(&cond->_cond);

  if (rc != 0) {
    LOG_TOPIC("263d7", FATAL, arangodb::Logger::FIXME)
        << "could not broadcast the condition: " << strerror(rc);
    FATAL_ERROR_ABORT();
  }
}

/// @brief waits for a signal on a condition variable
///
/// Note that you must hold the lock.
void TRI_WaitCondition(TRI_condition_t* cond) {
  int rc = pthread_cond_wait(&cond->_cond, &cond->_mutex);

  if (rc != 0) {
    LOG_TOPIC("674d8", FATAL, arangodb::Logger::FIXME)
        << "could not wait for the condition: " << strerror(rc);
    FATAL_ERROR_ABORT();
  }
}

/// @brief waits for a signal with a timeout in micro-seconds
/// returns true when the condition was signaled, false on timeout
/// Note that you must hold the lock.
bool TRI_TimedWaitCondition(TRI_condition_t* cond, uint64_t delay) {
  struct timespec ts;
  struct timeval tp;
  uint64_t x, y;

  if (gettimeofday(&tp, nullptr) != 0) {
    LOG_TOPIC("3515f", FATAL, arangodb::Logger::FIXME) << "could not get time of day";
    FATAL_ERROR_ABORT();
  }

  // Convert from timeval to timespec
  ts.tv_sec = tp.tv_sec;
  x = (tp.tv_usec * 1000) + (delay * 1000);
  y = (x % 1000000000);
  ts.tv_nsec = y;
  ts.tv_sec = ts.tv_sec + ((x - y) / 1000000000);

  // and wait
  int rc = pthread_cond_timedwait(&cond->_cond, &cond->_mutex, &ts);

  if (rc != 0) {
    if (rc == ETIMEDOUT) {
      return false;
    }

    LOG_TOPIC("e3e39", FATAL, arangodb::Logger::FIXME)
        << "could not wait for the condition: " << strerror(rc);
    FATAL_ERROR_ABORT();
  }

  return true;
}

/// @brief locks the mutex of a condition variable
void TRI_LockCondition(TRI_condition_t* cond) {
  int rc = pthread_mutex_lock(&cond->_mutex);

  if (rc != 0) {
    LOG_TOPIC("30e7f", FATAL, arangodb::Logger::FIXME)
        << "could not lock the condition: " << strerror(rc);
    FATAL_ERROR_ABORT();
  }
}

/// @brief unlocks the mutex of a condition variable
void TRI_UnlockCondition(TRI_condition_t* cond) {
  int rc = pthread_mutex_unlock(&cond->_mutex);

  if (rc != 0) {
    LOG_TOPIC("08fbe", FATAL, arangodb::Logger::FIXME)
        << "could not unlock the condition: " << strerror(rc);
    FATAL_ERROR_ABORT();
  }
}

#endif

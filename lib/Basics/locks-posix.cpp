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

#include "locks.h"

#ifdef TRI_HAVE_POSIX_THREADS

#include "Logger/Logger.h"

#define BUSY_LOCK_DELAY (10 * 1000)

#ifdef TRI_TRACE_LOCKS

#include <iostream>

static thread_local std::unordered_map<TRI_read_write_lock_t*, int>
    _threadLocks;

////////////////////////////////////////////////////////////////////////////////
/// @brief busy wait delay (in microseconds)
///
/// busy waiting is used if we cannot acquire a read-lock on a shared read/write
/// in case of too many concurrent lock requests. we'll wait in a busy
/// loop until we can acquire the lock
////////////////////////////////////////////////////////////////////////////////

static void LockError(TRI_read_write_lock_t* lock, int mode) {
  auto it = _threadLocks.find(lock);
  auto m = (*it).second;
  std::cerr << "ERROR. TRYING TO ACQUIRE " << (mode == 1 ? "READ" : "WRITE")
            << " LOCK WHILE ALREADY HOLDING IT IN "
            << (m == 1 ? "READ" : "WRITE") << " MODE" << std::endl;
  TRI_ASSERT(false);
}

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief initializes a new mutex
////////////////////////////////////////////////////////////////////////////////

int TRI_InitMutex(TRI_mutex_t* mutex) {
  return pthread_mutex_init(mutex, nullptr);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a mutex
////////////////////////////////////////////////////////////////////////////////

int TRI_DestroyMutex(TRI_mutex_t* mutex) {
  return pthread_mutex_destroy(mutex);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief locks mutex
////////////////////////////////////////////////////////////////////////////////

void TRI_LockMutex(TRI_mutex_t* mutex) {
  int rc = pthread_mutex_lock(mutex);

  if (rc != 0) {
    if (rc == EDEADLK) {
      LOG(ERR) << "mutex deadlock detected";
    }
    LOG(FATAL) << "could not lock the mutex: " << strerror(rc); 
    FATAL_ERROR_EXIT();
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief unlocks mutex
////////////////////////////////////////////////////////////////////////////////

void TRI_UnlockMutex(TRI_mutex_t* mutex) {
  int rc = pthread_mutex_unlock(mutex);

  if (rc != 0) {
    LOG(FATAL) << "could not release the mutex: " << strerror(rc); 
    FATAL_ERROR_EXIT();
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initializes a new read-write lock
////////////////////////////////////////////////////////////////////////////////

void TRI_InitReadWriteLock(TRI_read_write_lock_t* lock) {
  pthread_rwlock_init(lock, nullptr);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a read-write lock
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyReadWriteLock(TRI_read_write_lock_t* lock) {
  pthread_rwlock_destroy(lock);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief tries to read lock read-write lock
////////////////////////////////////////////////////////////////////////////////

bool TRI_TryReadLockReadWriteLock(TRI_read_write_lock_t* lock) {
#ifdef TRI_TRACE_LOCKS
  if (_threadLocks.find(lock) != _threadLocks.end()) {
    LockError(lock, 1);
  }
#endif
  int rc = pthread_rwlock_tryrdlock(lock);

#ifdef TRI_TRACE_LOCKS
  if (rc == 0) {
    _threadLocks.emplace(lock, 1);
  }
#endif

  return (rc == 0);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief read locks read-write lock
////////////////////////////////////////////////////////////////////////////////

void TRI_ReadLockReadWriteLock(TRI_read_write_lock_t* lock) {
  bool complained = false;

#ifdef TRI_TRACE_LOCKS
  if (_threadLocks.find(lock) != _threadLocks.end()) {
    LockError(lock, 1);
  }
#endif

again:
  int rc = pthread_rwlock_rdlock(lock);

  if (rc != 0) {
    if (rc == EAGAIN) {
      // use busy waiting if we cannot acquire the read-lock in case of too many
      // concurrent read locks ("resource temporarily unavailable").
      // in this case we'll wait in a busy loop until we can acquire the lock
      if (!complained) {
        LOG(WARN) << "too many read-locks on read-write lock";
        complained = true;
      }
      usleep(BUSY_LOCK_DELAY);
#ifdef TRI_HAVE_SCHED_H
      // let other threads do things
      sched_yield();
#endif

      // ideal use case for goto :-)
      goto again;
    }

    if (rc == EDEADLK) {
      LOG(ERR) << "rw-lock deadlock detected";
    }

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    LOG(ERR) << "could not read-lock the read-write lock: " << strerror(rc);
    TRI_ASSERT(false);
#endif
    LOG(FATAL) << "could not read-lock the read-write lock: " << strerror(rc); 
    FATAL_ERROR_EXIT();
  }

#ifdef TRI_TRACE_LOCKS
  _threadLocks.emplace(lock, 1);
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief read unlocks read-write lock
////////////////////////////////////////////////////////////////////////////////

void TRI_ReadUnlockReadWriteLock(TRI_read_write_lock_t* lock) {
  int rc = pthread_rwlock_unlock(lock);

  if (rc != 0) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    LOG(ERR) << "could not read-unlock the read-write lock: " << strerror(rc);
    TRI_ASSERT(false);
#endif
    LOG(FATAL) << "could not read-unlock the read-write lock: " << strerror(rc); 
    FATAL_ERROR_EXIT();
  }

#ifdef TRI_TRACE_LOCKS
  _threadLocks.erase(lock);
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief tries to write lock read-write lock
////////////////////////////////////////////////////////////////////////////////

bool TRI_TryWriteLockReadWriteLock(TRI_read_write_lock_t* lock) {
#ifdef TRI_TRACE_LOCKS
  if (_threadLocks.find(lock) != _threadLocks.end()) {
    LockError(lock, 2);
  }
#endif

  int rc = pthread_rwlock_trywrlock(lock);

#ifdef TRI_TRACE_LOCKS
  if (rc == 0) {
    _threadLocks.emplace(lock, 2);
  }
#endif

  return (rc == 0);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief write locks read-write lock
////////////////////////////////////////////////////////////////////////////////

void TRI_WriteLockReadWriteLock(TRI_read_write_lock_t* lock) {
#ifdef TRI_TRACE_LOCKS
  if (_threadLocks.find(lock) != _threadLocks.end()) {
    LockError(lock, 2);
  }
#endif

  int rc = pthread_rwlock_wrlock(lock);

  if (rc != 0) {
    if (rc == EDEADLK) {
      LOG(ERR) << "rw-lock deadlock detected";
    }
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    LOG(ERR) << "could not write-lock the read-write lock: " << strerror(rc);
    TRI_ASSERT(false);
#endif
    LOG(FATAL) << "could not write-lock the read-write lock: " << strerror(rc); FATAL_ERROR_EXIT();
  }

#ifdef TRI_TRACE_LOCKS
  _threadLocks.emplace(lock, 2);
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief write unlocks read-write lock
////////////////////////////////////////////////////////////////////////////////

void TRI_WriteUnlockReadWriteLock(TRI_read_write_lock_t* lock) {
  int rc = pthread_rwlock_unlock(lock);

  if (rc != 0) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    LOG(ERR) << "could not write-unlock the read-write lock: " << strerror(rc);
    TRI_ASSERT(false);
#endif
    LOG(FATAL) << "could not write-unlock the read-write lock: " << strerror(rc); FATAL_ERROR_EXIT();
  }

#ifdef TRI_TRACE_LOCKS
  _threadLocks.erase(lock);
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initializes a new condition variable
////////////////////////////////////////////////////////////////////////////////

void TRI_InitCondition(TRI_condition_t* cond) {
  pthread_cond_init(&cond->_cond, nullptr);
  pthread_mutex_init(&cond->_mutex, nullptr);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a condition variable
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyCondition(TRI_condition_t* cond) {
  pthread_cond_destroy(&cond->_cond);
  pthread_mutex_destroy(&cond->_mutex);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief signals a condition variable
///
/// Note that you must hold the lock.
////////////////////////////////////////////////////////////////////////////////

void TRI_SignalCondition(TRI_condition_t* cond) {
  int rc = pthread_cond_signal(&cond->_cond);

  if (rc != 0) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    LOG(ERR) << "could not signal the condition: " << strerror(rc);
    TRI_ASSERT(false);
#endif
    LOG(FATAL) << "could not signal the condition: " << strerror(rc); FATAL_ERROR_EXIT();
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief broad casts a condition variable
///
/// Note that you must hold the lock.
////////////////////////////////////////////////////////////////////////////////

void TRI_BroadcastCondition(TRI_condition_t* cond) {
  int rc = pthread_cond_broadcast(&cond->_cond);

  if (rc != 0) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    LOG(ERR) << "could not broadcast the condition: " << strerror(rc);
    TRI_ASSERT(false);
#endif
    LOG(FATAL) << "could not broadcast the condition: " << strerror(rc); FATAL_ERROR_EXIT();
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief waits for a signal on a condition variable
///
/// Note that you must hold the lock.
////////////////////////////////////////////////////////////////////////////////

void TRI_WaitCondition(TRI_condition_t* cond) {
  int rc = pthread_cond_wait(&cond->_cond, &cond->_mutex);

  if (rc != 0) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    LOG(ERR) << "could not wait for the condition: " << strerror(rc);
    TRI_ASSERT(false);
#endif
    LOG(FATAL) << "could not wait for the condition: " << strerror(rc); FATAL_ERROR_EXIT();
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief waits for a signal with a timeout in micro-seconds
/// returns true when the condition was signaled, false on timeout 
/// Note that you must hold the lock.
////////////////////////////////////////////////////////////////////////////////

bool TRI_TimedWaitCondition(TRI_condition_t* cond, uint64_t delay) {
  struct timespec ts;
  struct timeval tp;
  uint64_t x, y;

  if (gettimeofday(&tp, nullptr) != 0) {
    LOG(FATAL) << "could not get time of day"; FATAL_ERROR_EXIT();
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

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    LOG(ERR) << "could not wait for the condition: " << strerror(rc);
    TRI_ASSERT(false);
#endif
    LOG(FATAL) << "could not wait for the condition: " << strerror(rc); FATAL_ERROR_EXIT();
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief locks the mutex of a condition variable
////////////////////////////////////////////////////////////////////////////////

void TRI_LockCondition(TRI_condition_t* cond) {
  int rc = pthread_mutex_lock(&cond->_mutex);

  if (rc != 0) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    LOG(ERR) << "could not lock the condition: " << strerror(rc);
    TRI_ASSERT(false);
#endif
    LOG(FATAL) << "could not lock the condition: " << strerror(rc); FATAL_ERROR_EXIT();
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief unlocks the mutex of a condition variable
////////////////////////////////////////////////////////////////////////////////

void TRI_UnlockCondition(TRI_condition_t* cond) {
  int rc = pthread_mutex_unlock(&cond->_mutex);

  if (rc != 0) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    LOG(ERR) << "could not unlock the condition: " << strerror(rc);
    TRI_ASSERT(false);
#endif
    LOG(FATAL) << "could not unlock the condition: " << strerror(rc); FATAL_ERROR_EXIT();
  }
}

#endif

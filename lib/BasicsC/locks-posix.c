////////////////////////////////////////////////////////////////////////////////
/// @brief mutexes, locks and condition variables in posix
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2013 triAGENS GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "locks.h"

#ifdef TRI_HAVE_POSIX_THREADS

#include "BasicsC/logging.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                           DEFINES
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                    private macros
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Threading
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief busy wait delay (in microseconds)
///
/// busy waiting is used if we cannot acquire a read-lock on a shared read/write
/// in case of too many concurrent lock requests. we'll wait in a busy
/// loop until we can acquire the lock
////////////////////////////////////////////////////////////////////////////////

#define BUSY_LOCK_DELAY        (10 * 1000)

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                             MUTEX
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Threading
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises a new mutex
////////////////////////////////////////////////////////////////////////////////

void TRI_InitMutex (TRI_mutex_t* mutex) {
  pthread_mutex_init(mutex, 0);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a mutex
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyMutex (TRI_mutex_t* mutex) {
  pthread_mutex_destroy(mutex);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Threading
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief locks mutex
////////////////////////////////////////////////////////////////////////////////

void TRI_LockMutex (TRI_mutex_t* mutex) {
  int rc;

  rc = pthread_mutex_lock(mutex);

  if (rc != 0) {
    if (rc == EDEADLK) {
      LOG_ERROR("mutex deadlock detected");
    }

    LOG_FATAL_AND_EXIT("could not lock the mutex: %s", strerror(rc));
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief unlocks mutex
////////////////////////////////////////////////////////////////////////////////

void TRI_UnlockMutex (TRI_mutex_t* mutex) {
  int rc;

  rc = pthread_mutex_unlock(mutex);

  if (rc != 0) {
    LOG_FATAL_AND_EXIT("could not release the mutex: %s", strerror(rc));
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                              SPIN
// -----------------------------------------------------------------------------

#ifdef TRI_HAVE_POSIX_SPIN

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Threading
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises a new spin-lock
////////////////////////////////////////////////////////////////////////////////

void TRI_InitSpin (TRI_spin_t* spinLock) {
  pthread_spin_init(spinLock, 0);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a spin-lock
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroySpin (TRI_spin_t* spinLock) {
  pthread_spin_destroy(spinLock);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Threading
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief locks spin-lock
////////////////////////////////////////////////////////////////////////////////

void TRI_LockSpin (TRI_spin_t* spinLock) {
  int rc;

  rc = pthread_spin_lock(spinLock);

 if (rc != 0) {
    if (rc == EDEADLK) {
      LOG_ERROR("spinlock deadlock detected");
    }

    LOG_FATAL_AND_EXIT("could not lock the spin-lock: %s", strerror(rc));
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief unlocks spin-lock
////////////////////////////////////////////////////////////////////////////////

void TRI_UnlockSpin (TRI_spin_t* spinLock) {
  int rc;

  rc = pthread_spin_unlock(spinLock);

  if (rc != 0) {
    LOG_FATAL_AND_EXIT("could not release the spin-lock: %s", strerror(rc));
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                   READ-WRITE LOCK
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Threading
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises a new read-write lock
////////////////////////////////////////////////////////////////////////////////

void TRI_InitReadWriteLock (TRI_read_write_lock_t* lock) {
  pthread_rwlock_init(lock, 0);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a read-write lock
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyReadWriteLock (TRI_read_write_lock_t* lock) {
  pthread_rwlock_destroy(lock);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Threading
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief tries to read lock read-write lock
////////////////////////////////////////////////////////////////////////////////

bool TRI_TryReadLockReadWriteLock (TRI_read_write_lock_t* lock) {
  int rc;

  rc = pthread_rwlock_tryrdlock(lock);

  return (rc == 0);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief read locks read-write lock
////////////////////////////////////////////////////////////////////////////////

void TRI_ReadLockReadWriteLock (TRI_read_write_lock_t* lock) {
  int rc;
  bool complained = false;

again:
  rc = pthread_rwlock_rdlock(lock);

  if (rc != 0) {
    if (rc == EAGAIN) {
      // use busy waiting if we cannot acquire the read-lock in case of too many
      // concurrent read locks ("resource temporarily unavailable").
      // in this case we'll wait in a busy loop until we can acquire the lock
      if (! complained) {
        LOG_WARNING("too many read-locks on read-write lock");
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
      LOG_ERROR("rw-lock deadlock detected");
    }

    LOG_FATAL_AND_EXIT("could not read-lock the read-write lock: %s", strerror(rc));
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief read unlocks read-write lock
////////////////////////////////////////////////////////////////////////////////

void TRI_ReadUnlockReadWriteLock (TRI_read_write_lock_t* lock) {
  int rc;

  rc = pthread_rwlock_unlock(lock);

  if (rc != 0) {
    LOG_FATAL_AND_EXIT("could not read-unlock the read-write lock: %s", strerror(rc));
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief tries to write lock read-write lock
////////////////////////////////////////////////////////////////////////////////

bool TRI_TryWriteLockReadWriteLock (TRI_read_write_lock_t* lock) {
  int rc;

  rc = pthread_rwlock_trywrlock(lock);

  return (rc == 0);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief write locks read-write lock
////////////////////////////////////////////////////////////////////////////////

void TRI_WriteLockReadWriteLock (TRI_read_write_lock_t* lock) {
  int rc;

  rc = pthread_rwlock_wrlock(lock);

  if (rc != 0) {
    if (rc == EDEADLK) {
      LOG_ERROR("rw-lock deadlock detected");
    }

    LOG_FATAL_AND_EXIT("could not write-lock the read-write lock: %s", strerror(rc));
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief write unlocks read-write lock
////////////////////////////////////////////////////////////////////////////////

void TRI_WriteUnlockReadWriteLock (TRI_read_write_lock_t* lock) {
  int rc;

  rc  = pthread_rwlock_unlock(lock);

  if (rc != 0) {
    LOG_FATAL_AND_EXIT("could not read-unlock the read-write lock: %s", strerror(rc));
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                CONDITION VARIABLE
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Threading
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises a new condition variable
////////////////////////////////////////////////////////////////////////////////

void TRI_InitCondition (TRI_condition_t* cond) {
  pthread_cond_init(&cond->_cond, 0);

  cond->_ownMutex = true;
  cond->_mutex = TRI_Allocate(TRI_CORE_MEM_ZONE, sizeof(pthread_mutex_t), false);

  if (cond->_mutex == NULL) {
    LOG_FATAL_AND_EXIT("could not allocate memory for condition variable mutex");
  }

  pthread_mutex_init(cond->_mutex, 0);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises a new condition variable with existing mutex
////////////////////////////////////////////////////////////////////////////////

void TRI_Init2Condition (TRI_condition_t* cond, TRI_mutex_t* mutex) {
  pthread_cond_init(&cond->_cond, 0);

  cond->_ownMutex = false;
  cond->_mutex = mutex;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a condition variable
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyCondition (TRI_condition_t* cond) {
  pthread_cond_destroy(&cond->_cond);

  if (cond->_ownMutex) {
    pthread_mutex_destroy(cond->_mutex);
    TRI_Free(TRI_CORE_MEM_ZONE, cond->_mutex);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Threading
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief signals a condition variable
///
/// Note that you must hold the lock.
////////////////////////////////////////////////////////////////////////////////

void TRI_SignalCondition (TRI_condition_t* cond) {
  int rc;

  rc = pthread_cond_signal(&cond->_cond);

  if (rc != 0) {
    LOG_FATAL_AND_EXIT("could not signal the condition: %s", strerror(rc));
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief broad casts a condition variable
///
/// Note that you must hold the lock.
////////////////////////////////////////////////////////////////////////////////

void TRI_BroadcastCondition (TRI_condition_t* cond) {
  int rc;

  rc = pthread_cond_broadcast(&cond->_cond);

  if (rc != 0) {
    LOG_FATAL_AND_EXIT("could not croadcast the condition: %s", strerror(rc));
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief waits for a signal on a condition variable
///
/// Note that you must hold the lock.
////////////////////////////////////////////////////////////////////////////////

void TRI_WaitCondition (TRI_condition_t* cond) {
  int rc;

  rc = pthread_cond_wait(&cond->_cond, cond->_mutex);

  if (rc != 0) {
    LOG_FATAL_AND_EXIT("could not wait for the condition: %s", strerror(rc));
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief waits for a signal with a timeout in micro-seconds
///
/// Note that you must hold the lock.
////////////////////////////////////////////////////////////////////////////////

bool TRI_TimedWaitCondition (TRI_condition_t* cond, uint64_t delay) {
  int rc;
  struct timespec ts;
  struct timeval tp;
  uint64_t x, y;

  rc = gettimeofday(&tp, NULL);

  if (rc != 0) {
    LOG_FATAL_AND_EXIT("could not get time of day for the condition: %s", strerror(rc));
  }

  // Convert from timeval to timespec
  ts.tv_sec = tp.tv_sec;
  x = (tp.tv_usec * 1000) + (delay * 1000);
  y = (x % 1000000000);
  ts.tv_nsec = y;
  ts.tv_sec  = ts.tv_sec + ((x - y) / 1000000000);

  // and wait
  rc = pthread_cond_timedwait(&cond->_cond, cond->_mutex, &ts);

  if (rc != 0) {
    if (rc == ETIMEDOUT) {
      return false;
    }

    LOG_FATAL_AND_EXIT("could not wait for the condition: %s", strerror(rc));
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief locks the mutex of a condition variable
////////////////////////////////////////////////////////////////////////////////

void TRI_LockCondition (TRI_condition_t* cond) {
  int rc;

  rc = pthread_mutex_lock(cond->_mutex);

  if (rc != 0) {
    LOG_FATAL_AND_EXIT("could not lock the condition: %s", strerror(rc));
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief unlocks the mutex of a condition variable
////////////////////////////////////////////////////////////////////////////////

void TRI_UnlockCondition (TRI_condition_t* cond) {
  int rc;

  rc = pthread_mutex_unlock(cond->_mutex);

  if (rc != 0) {
    LOG_FATAL_AND_EXIT("could not unlock the condition: %s", strerror(rc));
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:

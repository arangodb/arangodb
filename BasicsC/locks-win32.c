////////////////////////////////////////////////////////////////////////////////
/// @brief mutexes, locks and condition variables in win32
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2011 triagens GmbH, Cologne, Germany
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
/// @author Copyright 2011, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "locks.h"

#include <BasicsC/logging.h>

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
  mutex->_mutex = CreateMutex(NULL, FALSE, NULL);

  if (mutex->_mutex == NULL) {
    LOG_FATAL("cannot create the mutex");
    exit(EXIT_FAILURE);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroyes a new mutex
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyMutex (TRI_mutex_t* mutex) {
  CloseHandle(mutex->_mutex);
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
  DWORD result = WaitForSingleObject(mutex->_mutex, INFINITE);

  if (result != WAIT_OBJECT_0) {
    LOG_FATAL("could not lock the mutex");
    exit(EXIT_FAILURE);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief unlocks mutex
////////////////////////////////////////////////////////////////////////////////

void TRI_UnlockMutex (TRI_mutex_t* mutex) {
  BOOL ok = ReleaseMutex(mutex->_mutex);

  if (! ok) {
    LOG_FATAL("could not unlock the mutex");
    exit(EXIT_FAILURE);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                              SPIN
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Threading
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises a new spin
////////////////////////////////////////////////////////////////////////////////

void TRI_InitSpin (TRI_spin_t* spin) {
  InitializeCriticalSection(spin);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroyes a new spin
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroySpin (TRI_spin_t* spin) {
  DeleteCriticalSection(spin);
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
/// @brief locks spin
////////////////////////////////////////////////////////////////////////////////

void TRI_LockSpin (TRI_spin_t* spin) {
  EnterCriticalSection(spin);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief unlocks spin
////////////////////////////////////////////////////////////////////////////////

void TRI_UnlockSpin (TRI_spin_t* spin) {
  LeaveCriticalSection(spin);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

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
  lock->_readers = 0;

  // Signaled:     writer has no access
  // Non-Signaled: writer has access, block readers

  lock->_writerEvent = CreateEvent(0, TRUE, TRUE, 0);

  // Signaled:     no readers
  // Non-Signaled: some readers have access, block writer

  lock->_readersEvent = CreateEvent(0, TRUE, TRUE, 0);

  InitializeCriticalSection(&lock->_lockWriter);
  InitializeCriticalSection(&lock->_lockReaders);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroyes a read-write lock
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyReadWriteLock (TRI_read_write_lock_t* lock) {
  DeleteCriticalSection(&lock->_lockWriter);
  DeleteCriticalSection(&lock->_lockReaders);

  CloseHandle(lock->_writerEvent);
  CloseHandle(lock->_readersEvent);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Threading
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief increments readers
////////////////////////////////////////////////////////////////////////////////

static void IncrementReaders (TRI_read_write_lock_t* lock) {
  lock->_readers++;

  ResetEvent(lock->_readersEvent);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief decrements readers
////////////////////////////////////////////////////////////////////////////////

static void DecrementReaders (TRI_read_write_lock_t* lock) {
  lock->_readers--;

  if (lock->_readers == 0) {
    SetEvent(lock->_readersEvent);
  }
  else if (lock->_readers < 0) {
    LOG_FATAL("reader count is negative");
    exit(EXIT_FAILURE);
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
/// @brief read locks read-write lock
////////////////////////////////////////////////////////////////////////////////

void TRI_ReadLockReadWriteLock (TRI_read_write_lock_t* lock) {
  while (true) {
    WaitForSingleObject(lock->_writerEvent, INFINITE);

    EnterCriticalSection(&lock->_lockReaders);
    IncrementReaders(lock);
    LeaveCriticalSection(&lock->_lockReaders);

    if (WaitForSingleObject(lock->_writerEvent, 0) != WAIT_OBJECT_0) {
      EnterCriticalSection(&lock->_lockReaders);
      DecrementReaders(lock);
      LeaveCriticalSection(&lock->_lockReaders);
    }
    else {
      break;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief read unlocks read-write lock
////////////////////////////////////////////////////////////////////////////////

void TRI_ReadUnlockReadWriteLock (TRI_read_write_lock_t* lock) {
  EnterCriticalSection(&lock->_lockReaders);

  // a write lock eists
  if (WaitForSingleObject(lock->_writerEvent, 0) != WAIT_OBJECT_0) {
    LOG_FATAL("write lock, but trying to unlock read");
    exit(EXIT_FAILURE);
  }

  // at least one reader exists
  else if (0 < lock->_readers) {
    DecrementReaders(lock);
  }

  // ups, no writer and no reader
  else {
    LeaveCriticalSection(&lock->_lockWriter);
    LOG_FATAL("no reader and no writer, but trying to unlock");
    exit(EXIT_FAILURE);
  }

  LeaveCriticalSection(&lock->_lockWriter);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief write locks read-write lock
////////////////////////////////////////////////////////////////////////////////

void TRI_WriteLockReadWriteLock (TRI_read_write_lock_t* lock) {
  EnterCriticalSection(&lock->_lockWriter);

  WaitForSingleObject(lock->_writerEvent, INFINITE);

  ResetEvent(lock->_writerEvent);

  WaitForSingleObject(lock->_readersEvent, INFINITE);

  LeaveCriticalSection(&lock->_lockWriter);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief write unlocks read-write lock
////////////////////////////////////////////////////////////////////////////////

void TRI_WriteUnlockReadWriteLock (TRI_read_write_lock_t* lock) {
  EnterCriticalSection(&lock->_lockReaders);

  // a write lock eists
  if (WaitForSingleObject(lock->_writerEvent, 0) != WAIT_OBJECT_0) {
    SetEvent(lock->_writerEvent);
  }

  // at least one reader exists
  else if (0 < lock->_readers) {
    LOG_FATAL("read lock, but trying to unlock write");
    exit(EXIT_FAILURE);
  }

  // ups, no writer and no reader
  else {
    LeaveCriticalSection(&lock->_lockWriter);
    LOG_FATAL("no reader and no writer, but trying to unlock");
    exit(EXIT_FAILURE);
  }

  LeaveCriticalSection(&lock->_lockWriter);
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
  cond->_waiters = 0;
  cond->_broadcast = false;

  cond->_sema = CreateSemaphore(NULL,       // no security
                                0,          // initially 0
                                0x7fffffff, // max count
                                NULL);      // unnamed

  InitializeCriticalSection(&cond->_lockWaiters);

  cond->_waitersDone = CreateEvent(NULL,  // no security
                                   FALSE, // auto-reset
                                   FALSE, // non-signaled initially
                                   NULL); // unnamed

  cond->_ownMutex = true;
  cond->_mutex = CreateMutex(NULL,  // default security attributes
                             FALSE, // initially not owned
                             NULL);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises a new condition variable with existing mutex
////////////////////////////////////////////////////////////////////////////////

void TRI_Init2Condition (TRI_condition_t* cond, TRI_mutex_t* mutex) {
  cond->_waiters = 0;
  cond->_broadcast = false;

  cond->_sema = CreateSemaphore(NULL,       // no security
                                0,          // initially 0
                                0x7fffffff, // max count
                                NULL);      // unnamed

  InitializeCriticalSection(&cond->_lockWaiters);

  cond->_waitersDone = CreateEvent(NULL,  // no security
                                   FALSE, // auto-reset
                                   FALSE, // non-signaled initially
                                   NULL); // unnamed

  cond->_ownMutex = false;
  cond->_mutex = mutex->_mutex;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a condition variable
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyCondition (TRI_condition_t* cond) {
  CloseHandle(cond->_waitersDone);
  DeleteCriticalSection(&cond->_lockWaiters);
  CloseHandle(cond->_sema);

  if (cond->_ownMutex) {
    CloseHandle(cond->_mutex);
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
  bool haveWaiters;

  EnterCriticalSection(&cond->_lockWaiters);
  haveWaiters = cond->_waiters > 0;
  LeaveCriticalSection(&cond->_lockWaiters);

  // if there aren't any waiters, then this is a no-op.
  if (haveWaiters) {
    ReleaseSemaphore(cond->_sema, 1, 0);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief broad casts a condition variable
///
/// Note that you must hold the lock.
////////////////////////////////////////////////////////////////////////////////

void TRI_BroadcastCondition (TRI_condition_t* cond) {
  bool haveWaiters;

  // This is needed to ensure that _waiters and _broadcast are
  // consistent relative to each other.
  EnterCriticalSection(&cond->_lockWaiters);
  haveWaiters = false;

  if (cond->_waiters > 0) {

    // We are broadcasting, even if there is just one waiter...
    // Record that we are broadcasting, which helps optimize
    // wait for the non-broadcast case.
    cond->_broadcast = true;
    haveWaiters = true;
  }

  if (haveWaiters) {

    // Wake up all the waiters atomically.
    ReleaseSemaphore(cond->_sema, cond->_waiters, 0);

    LeaveCriticalSection(&cond->_lockWaiters);

    // Wait for all the awakened threads to acquire the counting
    // semaphore.
    WaitForSingleObject(cond->_waitersDone, INFINITE);

    // This assignment is okay, even without the _lockWaiters held
    // because no other waiter threads can wake up to access it.
    cond->_broadcast = false;
  }
  else {
    LeaveCriticalSection (&cond->_lockWaiters);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief waits for a signal on a condition variable
///
/// Note that you must hold the lock.
////////////////////////////////////////////////////////////////////////////////

void TRI_WaitCondition (TRI_condition_t* cond) {
  bool lastWaiter;

  // avoid race conditions
  EnterCriticalSection(&cond->_lockWaiters);
  cond->_waiters++;
  LeaveCriticalSection(&cond->_lockWaiters);

  // This call atomically releases the mutex and waits on the
  // semaphore until pthread_cond_signal or pthread_cond_broadcast
  // are called by another thread.
  SignalObjectAndWait(cond->_mutex, cond->_sema, INFINITE, FALSE);

  // reacquire lock to avoid race conditions.
  EnterCriticalSection(&cond->_lockWaiters);

  // we're no longer waiting...
  cond->_waiters--;

  // check to see if we're the last waiter after pthread_cond_broadcast
  lastWaiter = cond->_broadcast && (cond->_waiters == 0);

  LeaveCriticalSection(&cond->_lockWaiters);

  // If we're the last waiter thread during this particular broadcast
  // then let all the other threads proceed.
  if (lastWaiter) {

    // This call atomically signals the waitersDone event and waits until
    // it can acquire the mutex.  This is required to ensure fairness.
    SignalObjectAndWait(cond->_waitersDone, cond->_mutex, INFINITE, FALSE);
  }
  else {

    // Always regain the external mutex since that's the guarantee we
    // give to our callers.
    WaitForSingleObject(cond->_mutex, INFINITE);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief waits for a signal with a timeout in milli-seconds
///
/// Note that you must hold the lock.
////////////////////////////////////////////////////////////////////////////////

bool TRI_TimedWaitCondition (TRI_condition_t* cond, uint64_t delay) {
  bool lastWaiter;
  DWORD res;

  // avoid race conditions
  EnterCriticalSection(&cond->_lockWaiters);
  cond->_waiters++;
  LeaveCriticalSection(&cond->_lockWaiters);

  // This call atomically releases the mutex and waits on the
  // semaphore until pthread_cond_signal or pthread_cond_broadcast
  // are called by another thread.
  res = SignalObjectAndWait(cond->_mutex, cond->_sema, (DWORD) delay, FALSE);

  if (res == WAIT_TIMEOUT) {
    EnterCriticalSection(&cond->_lockWaiters);
    cond->_waiters--;
    LeaveCriticalSection(&cond->_lockWaiters);

    WaitForSingleObject(cond->_mutex, INFINITE);

    return false;
  }

  // reacquire lock to avoid race conditions.
  EnterCriticalSection(&cond->_lockWaiters);

  // we're no longer waiting...
  cond->_waiters--;

  // check to see if we're the last waiter after pthread_cond_broadcast
  lastWaiter = cond->_broadcast && (cond->_waiters == 0);

  LeaveCriticalSection(&cond->_lockWaiters);

  // If we're the last waiter thread during this particular broadcast
  // then let all the other threads proceed.
  if (lastWaiter) {

    // This call atomically signals the waitersDone event and waits until
    // it can acquire the mutex.  This is required to ensure fairness.
    SignalObjectAndWait(cond->_waitersDone, cond->_mutex, INFINITE, FALSE);
  }
  else {

    // Always regain the external mutex since that's the guarantee we
    // give to our callers.
    WaitForSingleObject(cond->_mutex, INFINITE);
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief locks the mutex of a condition variable
////////////////////////////////////////////////////////////////////////////////

void TRI_LockCondition (TRI_condition_t* cond) {
  DWORD result = WaitForSingleObject(cond->_mutex, INFINITE);

  if (result != WAIT_OBJECT_0) {
    LOG_FATAL("could not lock the mutex");
    exit(EXIT_FAILURE);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief unlocks the mutex of a condition variable
////////////////////////////////////////////////////////////////////////////////

void TRI_UnlockCondition (TRI_condition_t* cond) {
  BOOL ok = ReleaseMutex(cond->_mutex);

  if (! ok) {
    LOG_FATAL("could not unlock the mutex");
    exit(EXIT_FAILURE);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:

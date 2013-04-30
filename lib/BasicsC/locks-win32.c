////////////////////////////////////////////////////////////////////////////////
/// @brief mutexes, locks and condition variables in win32
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

#include "BasicsC/logging.h"

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
    LOG_FATAL_AND_EXIT("cannot create the mutex");
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a mutex
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

  switch (result) {

    case WAIT_ABANDONED: {
      LOG_FATAL_AND_EXIT("locks-win32.c:TRI_LockMutex:could not lock the condition --> WAIT_ABANDONED");
    }

    case WAIT_OBJECT_0: {
      // everything ok
      break;
    }

    case WAIT_TIMEOUT: {
      LOG_FATAL_AND_EXIT("locks-win32.c:TRI_LockMutex:could not lock the condition --> WAIT_TIMEOUT");
    }

    case WAIT_FAILED: {
      result = GetLastError();
      LOG_FATAL_AND_EXIT("locks-win32.c:TRI_LockMutex:could not lock the condition --> WAIT_FAILED - reason -->%d",result);
    }

  }

}

////////////////////////////////////////////////////////////////////////////////
/// @brief unlocks mutex
////////////////////////////////////////////////////////////////////////////////

void TRI_UnlockMutex (TRI_mutex_t* mutex) {
  BOOL ok = ReleaseMutex(mutex->_mutex);

  if (! ok) {
    LOG_FATAL_AND_EXIT("could not unlock the mutex");
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
/// @brief destroys a spin
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

  // ...........................................................................
  // set the number of readers reading on the read_write lock to 0
  // ...........................................................................

  lock->_readers = 0;


  // ...........................................................................
  // Signaled:     writer has no access
  // Non-Signaled: writer has access, block readers
  // Creates an event which allows a thread to wait on it (perhaps should use
  // a mutux rather than an event here). The writer event is set to signalled
  // when CreateEvent is called with these parameters.
  // ...........................................................................

  lock->_writerEvent = CreateEvent(0, TRUE, TRUE, 0);



  // ...........................................................................
  // Signaled:     no readers
  // Non-Signaled: some readers have access, block writer
  // Same as the writer event above except this is the reader event
  // ...........................................................................

  lock->_readersEvent = CreateEvent(0, TRUE, TRUE, 0);


  // ...........................................................................
  // Creates critical sections for writer and readers.
  // Waits for ownership of the specified critical section object.
  // The function returns when the calling thread is granted ownership.
  // ...........................................................................

  InitializeCriticalSection(&lock->_lockWriter);
  InitializeCriticalSection(&lock->_lockReaders);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a read-write lock
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

  // ...........................................................................
  // increment the number of readers we have on the read_write lock
  // ...........................................................................

  lock->_readers++;


  // ...........................................................................
  // Since the number of readers must be positive, set the readers event to
  // non-signalled so that any write event will have to wait.
  // ...........................................................................
  ResetEvent(lock->_readersEvent);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief decrements readers
////////////////////////////////////////////////////////////////////////////////

static void DecrementReaders (TRI_read_write_lock_t* lock) {

  // ...........................................................................
  // reduce the number of readers using the read_write lock by 1
  // ...........................................................................

  lock->_readers--;


  // ...........................................................................
  // When the number of readers is 0, set the event to signalled which allows
  // a writer to use the read_write lock.
  // ...........................................................................

  if (lock->_readers == 0) {
    SetEvent(lock->_readersEvent);
  }
  else if (lock->_readers < 0) {
    LOG_FATAL_AND_EXIT("reader count is negative");
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
/// @brief tries to read lock a read-write lock
////////////////////////////////////////////////////////////////////////////////

bool TRI_TryReadLockReadWriteLock (TRI_read_write_lock_t* lock) {
  WaitForSingleObject(lock->_writerEvent, 10); // 10 millis timeout

  EnterCriticalSection(&lock->_lockReaders);
  IncrementReaders(lock);
  LeaveCriticalSection(&lock->_lockReaders);

  if (WaitForSingleObject(lock->_writerEvent, 0) != WAIT_OBJECT_0) {
    EnterCriticalSection(&lock->_lockReaders);
    DecrementReaders(lock);
    LeaveCriticalSection(&lock->_lockReaders);
    return false;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief read locks read-write lock
////////////////////////////////////////////////////////////////////////////////

void TRI_ReadLockReadWriteLock (TRI_read_write_lock_t* lock) {

  while (true) {

    // ........................................................................
    // Waits for a writer to finish if there is one. This function only
    // returns when the writer event is in a signalled state
    // ........................................................................

    WaitForSingleObject(lock->_writerEvent, INFINITE);


    // .........................................................................
    // This thread will wait here until this resource becomes excusively available
    // .........................................................................

    EnterCriticalSection(&lock->_lockReaders);
    IncrementReaders(lock);

    // .........................................................................
    // allows some other thread to use this resource
    // .........................................................................

    LeaveCriticalSection(&lock->_lockReaders);


    // it could have happened that the writer event is no longer in a signalled
    // state. Between leaving the crtical section and here a writer sneaked in.
    //
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

  /* this is wrong since it is possible for the write locker to block this event
  // a write lock eists
  if (WaitForSingleObject(lock->_writerEvent, 0) != WAIT_OBJECT_0) {
    LOG_FATAL_AND_EXIT("write lock, but trying to unlock read");
  }

  // at least one reader exists
  else if (0 < lock->_readers) {
    DecrementReaders(lock);
  }

  // ups, no writer and no reader
  else {
    LeaveCriticalSection(&lock->_lockReaders);
    LOG_FATAL_AND_EXIT("no reader and no writer, but trying to unlock");
  }
*/

 if (0 < lock->_readers) {
    DecrementReaders(lock);
  }

  // oops no reader
  else {
    LeaveCriticalSection(&lock->_lockReaders);
    LOG_FATAL_AND_EXIT("no reader, but trying to unlock read lock");
  }

  LeaveCriticalSection(&lock->_lockReaders);
}



////////////////////////////////////////////////////////////////////////////////
/// @brief tries to write lock a read-write lock
////////////////////////////////////////////////////////////////////////////////

bool TRI_TryWriteLockReadWriteLock (TRI_read_write_lock_t* lock) {

  BOOL result;
  // ...........................................................................
  // Here we use TryEnterCriticalSection instead of EnterCriticalSection
  // There could already be a write lock - which will actuall block from this
  // point on.
  // ...........................................................................

  result = TryEnterCriticalSection(&lock->_lockWriter);

  if (result == 0) {
    // appears some other writer is writing
    return false;
  }


  // ...........................................................................
  // Wait until the lock->_writerEvent is in a 'signalled' state
  // This might fail because a reader is just about to read
  // ...........................................................................

  if (WaitForSingleObject(lock->_writerEvent, 0) != WAIT_OBJECT_0) {
    LeaveCriticalSection(&lock->_lockWriter);
    return false;
  }

  // ...........................................................................
  // Set _writeEvent as nonsignalled -- this will block other read/write
  // lockers
  // ...........................................................................

  ResetEvent(lock->_writerEvent);


  // ...........................................................................
  // If there are ANY read locks outstanding, leave
  // ...........................................................................

  if (WaitForSingleObject(lock->_readersEvent, 0) != WAIT_OBJECT_0) {
    LeaveCriticalSection(&lock->_lockWriter);
    SetEvent(lock->_writerEvent);
    return false;
  }


  // ...........................................................................
  // Allow other threads to access this function
  // ...........................................................................

  LeaveCriticalSection(&lock->_lockWriter);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief write locks read-write lock
////////////////////////////////////////////////////////////////////////////////

void TRI_WriteLockReadWriteLock (TRI_read_write_lock_t* lock) {

  // ...........................................................................
  // Lock so no other thread can access this
  // EnterCriticalSection(&lock->_lockWriter) will block this thread until
  // it has been released by the other thread.
  // ...........................................................................

  EnterCriticalSection(&lock->_lockWriter);


  // ...........................................................................
  // Wait until the lock->_writerEvent is in a 'signalled' state
  // ...........................................................................

  WaitForSingleObject(lock->_writerEvent, INFINITE);


  // ...........................................................................
  // Set _writeEvent as nonsignalled -- this will block other read/write
  // lockers
  // ...........................................................................

  ResetEvent(lock->_writerEvent);


  // ...........................................................................
  // If there are ANY read locks outstanding, then  wait until these are cleared
  // ...........................................................................

  WaitForSingleObject(lock->_readersEvent, INFINITE);


  // ...........................................................................
  // Allow other threads to access this function
  // ...........................................................................

  LeaveCriticalSection(&lock->_lockWriter);
}



////////////////////////////////////////////////////////////////////////////////
/// @brief write unlocks read-write lock
////////////////////////////////////////////////////////////////////////////////

void TRI_WriteUnlockReadWriteLock (TRI_read_write_lock_t* lock) {

  // ...........................................................................
  // Write lock this _lockReader so no other threads can access this
  // This will block this thread until it is released by the other thread
  // We do not need to lock the _lockWriter SINCE the TRI_WriteLockReadWriteLock
  // function above will lock (due to the ResetEvent(lock->_writerEvent); )
  // ...........................................................................

  EnterCriticalSection(&lock->_lockReaders);


  // ...........................................................................
  // In the function TRI_WriteLockReadWriteLock we set the _writerEvent to
  // 'nonsignalled'. So if a write lock  exists clear it by setting it to
  // 'signalled'
  // ...........................................................................

  if (WaitForSingleObject(lock->_writerEvent, 0) != WAIT_OBJECT_0) {
    SetEvent(lock->_writerEvent);
  }

  // ...........................................................................
  // Oops at least one reader exists - something terrible happened.
  // ...........................................................................

  else if (0 < lock->_readers) {
    LeaveCriticalSection(&lock->_lockReaders);
    LOG_FATAL_AND_EXIT("read lock, but trying to unlock write");
  }


  // ...........................................................................
  // Oops we are trying to unlock a write lock, but there isn't one! Something
  // terrible happend.
  // ...........................................................................

  else {
    LeaveCriticalSection(&lock->_lockReaders);
    LOG_FATAL_AND_EXIT("no reader and no writer, but trying to unlock");
  }


  // ...........................................................................
  // Allow read locks to be applied now.
  // ...........................................................................

  LeaveCriticalSection(&lock->_lockReaders);
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
/// @brief waits for a signal with a timeout in micro-seconds
///
/// Note that you must hold the lock.
////////////////////////////////////////////////////////////////////////////////

bool TRI_TimedWaitCondition (TRI_condition_t* cond, uint64_t delay) {
  bool lastWaiter;
  DWORD res;

  // ...........................................................................
  // The POSIX threads function pthread_cond_timedwait accepts microseconds
  // while the the function SignalObjectAndWait  accepts milliseconds
  // ...........................................................................

  delay = delay / 1000;

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

  switch (result) {

    case WAIT_ABANDONED: {
      LOG_FATAL_AND_EXIT("locks-win32.c:TRI_LockCondition:could not lock the condition --> WAIT_ABANDONED");
    }

    case WAIT_OBJECT_0: {
      // everything ok
      break;
    }

    case WAIT_TIMEOUT: {
      LOG_FATAL_AND_EXIT("locks-win32.c:TRI_LockCondition:could not lock the condition --> WAIT_TIMEOUT");
    }

    case WAIT_FAILED: {
      result = GetLastError();
      LOG_FATAL_AND_EXIT("locks-win32.c:TRI_LockCondition:could not lock the condition --> WAIT_FAILED - reason -->%d",result);
    }

  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief unlocks the mutex of a condition variable
////////////////////////////////////////////////////////////////////////////////

void TRI_UnlockCondition (TRI_condition_t* cond) {
  BOOL ok = ReleaseMutex(cond->_mutex);

  if (! ok) {
    LOG_FATAL_AND_EXIT("could not unlock the mutex");
  }
}



// -----------------------------------------------------------------------------
// COMPARE & SWAP operations below for windows
// Note that for the MAC OS we use the 'barrier' functions which ensure that
// read/write operations on the scalars are executed in order. According to the
// available documentation, the GCC variants of these COMPARE & SWAP operations
// are implemented with a memory barrier. The MS Windows variants of these 
// operations (according to the documentation on MS site) also provide a full
// memory barrier.
// -----------------------------------------------------------------------------


////////////////////////////////////////////////////////////////////////////////
/// @brief atomically compares and swaps 32bit integers with full memory barrier
////////////////////////////////////////////////////////////////////////////////

bool TRI_CompareAndSwapIntegerInt32 (volatile int32_t* theValue, int32_t oldValue, int32_t newValue) {
  return ( (int32_t)( InterlockedCompareExchange((volatile LONG*)(theValue), (LONG)(newValue), (LONG)(oldValue) ) ) == oldValue );
}

bool TRI_CompareIntegerInt32 (volatile int32_t* theValue, int32_t oldValue) {
  return ( (int32_t)( InterlockedCompareExchange((volatile LONG*)(theValue), (LONG)(oldValue), (LONG)(oldValue) ) ) == oldValue );
}

bool TRI_CompareAndSwapIntegerUInt32 (volatile uint32_t* theValue, uint32_t oldValue, uint32_t newValue) {
  return ( (uint32_t)(InterlockedCompareExchange((volatile LONG*)(theValue), (LONG)(newValue), (LONG)(oldValue) ) ) == oldValue );
}

bool TRI_CompareIntegerUInt32 (volatile uint32_t* theValue, uint32_t oldValue) {
  return ( (uint32_t)(InterlockedCompareExchange((volatile LONG*)(theValue), (LONG)(oldValue), (LONG)(oldValue) ) ) == oldValue );
}


////////////////////////////////////////////////////////////////////////////////
/// @brief atomically compares and swaps 64bit integers with full memory barrier
////////////////////////////////////////////////////////////////////////////////

bool TRI_CompareAndSwapIntegerInt64 (volatile int64_t* theValue, int64_t oldValue, int64_t newValue) {
  return ( (int64_t)(InterlockedCompareExchange64((volatile LONGLONG*)(theValue), (LONGLONG)(newValue), (LONGLONG)(oldValue) ) ) == oldValue );
}

bool TRI_CompareIntegerInt64 (volatile int64_t* theValue, int64_t oldValue) {
  return ( (int64_t)(InterlockedCompareExchange64((volatile LONGLONG*)(theValue), (LONGLONG)(oldValue), (LONGLONG)(oldValue) ) ) == oldValue );
}

bool TRI_CompareAndSwapIntegerUInt64 (volatile uint64_t* theValue, uint64_t oldValue, uint64_t newValue) {
  return ( (uint64_t)(InterlockedCompareExchange64((volatile LONGLONG*)(theValue), (LONGLONG)(newValue), (LONGLONG)(oldValue) ) ) == oldValue );
}

bool TRI_CompareIntegerUInt64 (volatile uint64_t* theValue, uint64_t oldValue) {
  return ( (uint64_t)(InterlockedCompareExchange64((volatile LONGLONG*)(theValue), (LONGLONG)(oldValue), (LONGLONG)(oldValue) ) ) == oldValue );
}


////////////////////////////////////////////////////////////////////////////////
/// @brief atomically compares and swaps pointers with full memory barrier
////////////////////////////////////////////////////////////////////////////////

bool TRI_CompareAndSwapPointer(void* volatile* theValue, void* oldValue, void* newValue) {
  return ( InterlockedCompareExchangePointer(theValue, newValue, oldValue) == oldValue );
}

bool TRI_ComparePointer(void* volatile* theValue, void* oldValue) {
  return ( InterlockedCompareExchangePointer(theValue, oldValue, oldValue) == oldValue );
}


////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:

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

int TRI_InitMutex (TRI_mutex_t* mutex) {
  mutex->_mutex = CreateMutex(NULL, FALSE, NULL);

  if (mutex->_mutex == NULL) {
    LOG_FATAL_AND_EXIT("cannot create the mutex");
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a mutex
////////////////////////////////////////////////////////////////////////////////

int TRI_DestroyMutex (TRI_mutex_t* mutex) {
  if (CloseHandle(mutex->_mutex) == 0) {
    DWORD result = GetLastError();
      
    LOG_FATAL_AND_EXIT("locks-win32.c:TRI_DestroyMutex:could not destroy the mutex -->%d",result);
  }
  
  return TRI_ERROR_NO_ERROR;
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
  InitializeSRWLock(&lock->_lock);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a read-write lock
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyReadWriteLock (TRI_read_write_lock_t* lock) {
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
  return (TryAcquireSRWLockShared(&lock->_lock) != 0);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief read locks read-write lock
////////////////////////////////////////////////////////////////////////////////

void TRI_ReadLockReadWriteLock (TRI_read_write_lock_t* lock) {
  AcquireSRWLockShared(&lock->_lock);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief read unlocks read-write lock
////////////////////////////////////////////////////////////////////////////////

void TRI_ReadUnlockReadWriteLock (TRI_read_write_lock_t* lock) {
  ReleaseSRWLockShared(&lock->_lock);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief tries to write lock a read-write lock
////////////////////////////////////////////////////////////////////////////////

bool TRI_TryWriteLockReadWriteLock (TRI_read_write_lock_t* lock) {
  return (TryAcquireSRWLockExclusive(&lock->_lock) != 0);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief write locks read-write lock
////////////////////////////////////////////////////////////////////////////////

void TRI_WriteLockReadWriteLock (TRI_read_write_lock_t* lock) {
  AcquireSRWLockExclusive(&lock->_lock);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief write unlocks read-write lock
////////////////////////////////////////////////////////////////////////////////

void TRI_WriteUnlockReadWriteLock (TRI_read_write_lock_t* lock) {
  ReleaseSRWLockExclusive(&lock->_lock);
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
  InitializeCriticalSection(&cond->_lockWaiters);
  InitializeConditionVariable(&cond->_conditionVariable);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a condition variable
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyCondition (TRI_condition_t* cond) {
  DeleteCriticalSection(&cond->_lockWaiters);
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
  WakeConditionVariable(&cond->_conditionVariable);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief broad casts a condition variable
///
/// Note that you must hold the lock.
////////////////////////////////////////////////////////////////////////////////

void TRI_BroadcastCondition (TRI_condition_t* cond) {
  WakeAllConditionVariable(&cond->_conditionVariable);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief waits for a signal on a condition variable
///
/// Note that you must hold the lock.
////////////////////////////////////////////////////////////////////////////////

void TRI_WaitCondition (TRI_condition_t* cond) {
  SleepConditionVariableCS(&cond->_conditionVariable, &cond->_lockWaiters, INFINITE);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief waits for a signal with a timeout in micro-seconds
///
/// Note that you must hold the lock.
////////////////////////////////////////////////////////////////////////////////

bool TRI_TimedWaitCondition (TRI_condition_t* cond, uint64_t delay) {
  // ...........................................................................
  // The POSIX threads function pthread_cond_timedwait accepts microseconds
  // while the Windows function accepts milliseconds
  // ...........................................................................
  DWORD res;

  delay = delay / 1000;

  if (SleepConditionVariableCS(&cond->_conditionVariable, &cond->_lockWaiters, (DWORD) delay) != 0) {
	return true;
  }

  res = GetLastError();
  if (res == ERROR_TIMEOUT) {
	return false;
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief locks the mutex of a condition variable
////////////////////////////////////////////////////////////////////////////////

void TRI_LockCondition (TRI_condition_t* cond) {
  EnterCriticalSection(&cond->_lockWaiters);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief unlocks the mutex of a condition variable
////////////////////////////////////////////////////////////////////////////////

void TRI_UnlockCondition (TRI_condition_t* cond) {
  LeaveCriticalSection(&cond->_lockWaiters);
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

#ifdef TRI_SKIPLIST_EX

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

#endif

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:

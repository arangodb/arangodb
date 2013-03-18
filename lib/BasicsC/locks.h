////////////////////////////////////////////////////////////////////////////////
/// @brief mutexes, locks and condition variables
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

#ifndef TRIAGENS_BASICS_C_LOCKS_H
#define TRIAGENS_BASICS_C_LOCKS_H 1

#include "BasicsC/common.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                     PUBLIC MACROS
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Threading
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                     POSIX THREADS
// -----------------------------------------------------------------------------

#ifdef TRI_HAVE_POSIX_THREADS
#include "BasicsC/locks-posix.h"
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                   WINDOWS THREADS
// -----------------------------------------------------------------------------

#ifdef TRI_HAVE_WIN32_THREADS
#include "BasicsC/locks-win32.h"
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                     MAC OS X SPIN
// -----------------------------------------------------------------------------

#ifdef TRI_HAVE_MACOS_SPIN
#include "BasicsC/locks-macos.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

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
///
/// Mutual exclusion (often abbreviated to mutex) algorithms are used in
/// concurrent programming to avoid the simultaneous use of a common resource,
/// such as a global variable, by pieces of computer code called critical
/// sections. A critical section is a piece of code in which a process or thread
/// accesses a common resource. The critical section by itself is not a
/// mechanism or algorithm for mutual exclusion. A program, process, or thread
/// can have the critical section in it without any mechanism or algorithm which
/// implements mutual exclusion. For details see www.wikipedia.org.
////////////////////////////////////////////////////////////////////////////////

void TRI_InitMutex (TRI_mutex_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief destroyes a mutex
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyMutex (TRI_mutex_t*);

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

void TRI_LockMutex (TRI_mutex_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief unlocks mutex
////////////////////////////////////////////////////////////////////////////////

void TRI_UnlockMutex (TRI_mutex_t*);

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
/// @brief initialises a new spin-lock
////////////////////////////////////////////////////////////////////////////////

void TRI_InitSpin (TRI_spin_t* spin);

////////////////////////////////////////////////////////////////////////////////
/// @brief destroyes a spin-lock
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroySpin (TRI_spin_t* spin);

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

void TRI_LockSpin (TRI_spin_t* spin);

////////////////////////////////////////////////////////////////////////////////
/// @brief unlocks spin-lock
////////////////////////////////////////////////////////////////////////////////

void TRI_UnlockSpin (TRI_spin_t* spin);

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
///
/// A ReadWriteLock maintains a pair of associated locks, one for read-only
/// operations and one for writing. The read lock may be held simultaneously by
/// multiple reader threads, so long as there are no writers. The write lock is
/// exclusive.
///
/// A read-write lock allows for a greater level of concurrency in accessing
/// shared data than that permitted by a mutual exclusion lock. It exploits the
/// fact that while only a single thread at a time (a writer thread) can modify
/// the shared data, in many cases any number of threads can concurrently read
/// the data (hence reader threads). In theory, the increase in concurrency
/// permitted by the use of a read-write lock will lead to performance
/// improvements over the use of a mutual exclusion lock. In practice this
/// increase in concurrency will only be fully realized on a multi-processor,
/// and then only if the access patterns for the shared data are suitable.
////////////////////////////////////////////////////////////////////////////////

void TRI_InitReadWriteLock (TRI_read_write_lock_t* lock);

////////////////////////////////////////////////////////////////////////////////
/// @brief destroyes a read-write lock
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyReadWriteLock (TRI_read_write_lock_t* lock);

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

bool TRI_TryReadLockReadWriteLock (TRI_read_write_lock_t* lock);

////////////////////////////////////////////////////////////////////////////////
/// @brief read locks read-write lock
////////////////////////////////////////////////////////////////////////////////

void TRI_ReadLockReadWriteLock (TRI_read_write_lock_t* lock);

////////////////////////////////////////////////////////////////////////////////
/// @brief read unlocks read-write lock
////////////////////////////////////////////////////////////////////////////////

void TRI_ReadUnlockReadWriteLock (TRI_read_write_lock_t* lock);

////////////////////////////////////////////////////////////////////////////////
/// @brief tries to write lock read-write lock
////////////////////////////////////////////////////////////////////////////////

bool TRI_TryWriteLockReadWriteLock (TRI_read_write_lock_t* lock);

////////////////////////////////////////////////////////////////////////////////
/// @brief write locks read-write lock
////////////////////////////////////////////////////////////////////////////////

void TRI_WriteLockReadWriteLock (TRI_read_write_lock_t* lock);

////////////////////////////////////////////////////////////////////////////////
/// @brief write unlocks read-write lock
////////////////////////////////////////////////////////////////////////////////

void TRI_WriteUnlockReadWriteLock (TRI_read_write_lock_t* lock);

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

void TRI_InitCondition (TRI_condition_t* cond);

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises a new condition variable with existing mutex
////////////////////////////////////////////////////////////////////////////////

void TRI_Init2Condition (TRI_condition_t* cond, TRI_mutex_t* mutex);

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a condition variable
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyCondition (TRI_condition_t* cond);

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

void TRI_SignalCondition (TRI_condition_t* cond);

////////////////////////////////////////////////////////////////////////////////
/// @brief broad casts a condition variable
///
/// Note that you must hold the lock.
////////////////////////////////////////////////////////////////////////////////

void TRI_BroadcastCondition (TRI_condition_t* cond);

////////////////////////////////////////////////////////////////////////////////
/// @brief waits for a signal on a condition variable
///
/// Note that you must hold the lock.
////////////////////////////////////////////////////////////////////////////////

void TRI_WaitCondition (TRI_condition_t* cond);

////////////////////////////////////////////////////////////////////////////////
/// @brief waits for a signal with a timeout in micro-seconds
///
/// Note that you must hold the lock.
////////////////////////////////////////////////////////////////////////////////

bool TRI_TimedWaitCondition (TRI_condition_t* cond, uint64_t delay);

////////////////////////////////////////////////////////////////////////////////
/// @brief locks the mutex of a condition variable
////////////////////////////////////////////////////////////////////////////////

void TRI_LockCondition (TRI_condition_t* cond);

////////////////////////////////////////////////////////////////////////////////
/// @brief unlocks the mutex of a condition variable
////////////////////////////////////////////////////////////////////////////////

void TRI_UnlockCondition (TRI_condition_t* cond);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////





// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup CAS operations
/// @{
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
/// @brief performs an atomic compare and swap operation on a 32bit integer.
///
/// The position of 'theValue' must be aligned on a 32 bit boundary. The function
/// performs the following atomically: compares the value stored in the position 
/// pointed to by <theValue> with the value of <oldValue>. if the value stored
/// in position <theValue> is EQUAL to the value of <oldValue>, then the
/// <newValue> is stored in the position pointed to by <theValue> (true is 
/// returned), otherwise no operation is performed (false is returned).
////////////////////////////////////////////////////////////////////////////////

// .............................................................................
// The position of 'theValue' must be aligned on a 32 bit boundary. The function
// performs the following atomically: compares the value stored in the position 
// pointed to by <theValue> with the value of <oldValue>. if the value stored
// in position <theValue> is EQUAL to the value of <oldValue>, then the
// <newValue> is stored in the position pointed to by <theValue> (true is 
// returned), otherwise no operation is performed (false is returned).
// .............................................................................

bool TRI_CompareAndSwapIntegerInt32  (volatile int32_t* theValue, int32_t oldValue, int32_t newValue);

bool TRI_CompareAndSwapIntegerUInt32 (volatile uint32_t* theValue, uint32_t oldValue, uint32_t newValue);


////////////////////////////////////////////////////////////////////////////////
/// @brief performs an atomic compare and swap operation on a 64bit integer.
///
/// The position of 'theValue' must be aligned on a 64 bit boundary. This function is
/// simply the 64bit equivalent of the function above.
////////////////////////////////////////////////////////////////////////////////

// .............................................................................
// The position of 'theValue' must be aligned on a 64 bit boundary. This function is
// simply the 64bit equivalent of the function above.
// .............................................................................

bool TRI_CompareAndSwapIntegerInt64  (volatile int64_t* theValue, int64_t oldValue, int64_t newValue);

bool TRI_CompareAndSwapIntegerUInt64 (volatile uint64_t* theValue, uint64_t oldValue, uint64_t newValue);


////////////////////////////////////////////////////////////////////////////////
/// @brief performs an atomic compare and swap operation on a pointer.
///
/// On a 32bit machine, the position of 'theValue' must be aligned on a 32 bit 
/// boundary. On a 64bit machine the alignment must be on a 64bit boundary.
/// The function performs the following atomically: compares the value stored in 
/// the position pointed to by <theValue> with the value of <oldValue>. if the 
/// value stored in position <theValue> is EQUAL to the value of <oldValue>, 
/// then the <newValue> is stored in the position pointed to by <theValue> 
/// (true is returned), otherwise no operation is performed (false is returned).
////////////////////////////////////////////////////////////////////////////////

bool TRI_CompareAndSwapPointer(void* volatile* theValue, void* oldValue, void* newValue);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:

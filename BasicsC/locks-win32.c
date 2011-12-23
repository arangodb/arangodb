////////////////////////////////////////////////////////////////////////////////
/// @brief mutexes, locks and condition variables in win32
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2011 triagens GmbH, Cologne, Germany
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
  InitializeCriticalSection(mutex);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroyes a new mutex
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyMutex (TRI_mutex_t* mutex) {
  DeleteCriticalSection(mutex);
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

bool TRI_LockMutex (TRI_mutex_t* mutex) {
  EnterCriticalSection(mutex);
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief unlocks mutex
////////////////////////////////////////////////////////////////////////////////

bool TRI_UnlockMutex (TRI_mutex_t* mutex) {
  LeaveCriticalSection(mutex);
  return true;
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

namespace triagens {
  namespace basics {

    // -----------------------------------------------------------------------------
    // constructors and destructora
    // -----------------------------------------------------------------------------

    ReadWriteLock::ReadWriteLock ()
      : _readers(0),
        _writerEvent(0),
        _readersEvent(0) {

      // Signaled:     writer has no access
      // Non-Signaled: writer has access, block readers

      _writerEvent = CreateEvent(0, TRUE, TRUE, 0);

      // Signaled:     no readers
      // Non-Signaled: some readers have access, block writer

      _readersEvent = CreateEvent(0, TRUE, TRUE, 0);

      InitializeCriticalSection(&_lockWriter);
      InitializeCriticalSection(&_lockReaders);
    }



    ReadWriteLock::~ReadWriteLock () {
      DeleteCriticalSection(&_lockWriter);
      DeleteCriticalSection(&_lockReaders);

      CloseHandle(_writerEvent);
      CloseHandle(_readersEvent);
    }

    // -----------------------------------------------------------------------------
    // public methods
    // -----------------------------------------------------------------------------

    bool ReadWriteLock::readLock () {
      while (true) {
        WaitForSingleObject(_writerEvent, INFINITE);

        EnterCriticalSection(&_lockReaders);
        incrementReaders();
        LeaveCriticalSection(&_lockReaders);

        if (WaitForSingleObject(_writerEvent, 0) != WAIT_OBJECT_0) {
          EnterCriticalSection(&_lockReaders);
          decrementReaders();
          LeaveCriticalSection(&_lockReaders);
        }
        else {
          break;
        }
      }

      return true;
    }



    bool ReadWriteLock::writeLock () {
      EnterCriticalSection(&_lockWriter);

      WaitForSingleObject(_writerEvent, INFINITE);

      ResetEvent(_writerEvent);

      WaitForSingleObject(_readersEvent, INFINITE);

      LeaveCriticalSection(&_lockWriter);

      return true;
    }



    bool ReadWriteLock::unlock () {
      EnterCriticalSection(&_lockReaders);

      // a write lock eists
      if (WaitForSingleObject(_writerEvent, 0) != WAIT_OBJECT_0) {
        SetEvent(_writerEvent);
      }

      // at least one reader exists
      else if (0 < _readers) {
        decrementReaders();
      }

      // ups, no writer and no reader
      else {
        LeaveCriticalSection(&_lockWriter);
        THROW_INTERNAL_ERROR("no reader and no writer, but trying to unlock");
      }

      LeaveCriticalSection(&_lockWriter);

      return true;
    }

    // -----------------------------------------------------------------------------
    // private methods
    // -----------------------------------------------------------------------------

    void ReadWriteLock::incrementReaders () {
      _readers++;

      ResetEvent(_readersEvent);
    }



    void ReadWriteLock::decrementReaders () {
      _readers--;

      if (_readers == 0) {
        SetEvent(_readersEvent);
      }
      else if (_readers < 0) {
        THROW_INTERNAL_ERROR("reader count is negative");
      }
    }
  }
}

      private:
        CRITICAL_SECTION _lockWaiters;

        HANDLE _waitersDone;
        HANDLE _mutex;
        HANDLE _sema;

        int _waiters;
        bool _broadcast;
    };

namespace triagens {
  namespace basics {

    // -----------------------------------------------------------------------------
    // constructors and destructors
    // -----------------------------------------------------------------------------

    ConditionVariable::ConditionVariable ()
      : _waiters(0), _broadcast(false) {

      _sema = CreateSemaphore(NULL,       // no security
                              0,          // initially 0
                              0x7fffffff, // max count
                              NULL);      // unnamed

      InitializeCriticalSection (&_lockWaiters);

      _waitersDone = CreateEvent(NULL,  // no security
                                 FALSE, // auto-reset
                                 FALSE, // non-signaled initially
                                 NULL); // unnamed

      _mutex = CreateMutex(NULL,              // default security attributes
                           FALSE,             // initially not owned
                           NULL);
    }



    ConditionVariable::~ConditionVariable () {
      CloseHandle(_waitersDone);
      DeleteCriticalSection(&_lockWaiters);
      CloseHandle(_sema);
      CloseHandle(_mutex);
    }

    // -----------------------------------------------------------------------------
    // public methods
    // -----------------------------------------------------------------------------

    bool ConditionVariable::lock () {
      DWORD result = WaitForSingleObject(_mutex, INFINITE);

      if (result != WAIT_OBJECT_0) {
        LOGGER_ERROR << "could not lock the mutex";
        return false;
      }

      return true;
    }



    bool ConditionVariable::unlock () {
      BOOL ok = ReleaseMutex(_mutex);

      if (! ok) {
        LOGGER_ERROR << "could not unlock the mutex";
        return false;
      }

      return true;
    }



    bool ConditionVariable::wait () {

      // avoid race conditions
      EnterCriticalSection(&_lockWaiters);
      _waiters++;
      LeaveCriticalSection(&_lockWaiters);

      // This call atomically releases the mutex and waits on the
      // semaphore until pthread_cond_signal or pthread_cond_broadcast
      // are called by another thread.
      SignalObjectAndWait(_mutex, _sema, INFINITE, FALSE);

      // reacquire lock to avoid race conditions.
      EnterCriticalSection(&_lockWaiters);

      // we're no longer waiting...
      _waiters--;

      // check to see if we're the last waiter after pthread_cond_broadcast
      bool lastWaiter = _broadcast && (_waiters == 0);

      LeaveCriticalSection(&_lockWaiters);

      // If we're the last waiter thread during this particular broadcast
      // then let all the other threads proceed.
      if (lastWaiter) {

        // This call atomically signals the waitersDone event and waits until
        // it can acquire the mutex.  This is required to ensure fairness.
        SignalObjectAndWait(_waitersDone, _mutex, INFINITE, FALSE);
      }
      else {

        // Always regain the external mutex since that's the guarantee we
        // give to our callers.
        WaitForSingleObject(_mutex, INFINITE);
      }

      return true;
    }



    bool ConditionVariable::broadcast () {

      // This is needed to ensure that _waiters and _broadcast are
      // consistent relative to each other.
      EnterCriticalSection (&_lockWaiters);
      bool haveWaiters = false;

      if (_waiters > 0) {

        // We are broadcasting, even if there is just one waiter...
        // Record that we are broadcasting, which helps optimize
        // wait for the non-broadcast case.
        _broadcast = true;
        haveWaiters = true;
      }

      if (haveWaiters) {

        // Wake up all the waiters atomically.
        ReleaseSemaphore(_sema, _waiters, 0);

        LeaveCriticalSection(&_lockWaiters);

        // Wait for all the awakened threads to acquire the counting
        // semaphore.
        WaitForSingleObject(_waitersDone, INFINITE);

        // This assignment is okay, even without the _lockWaiters held
        // because no other waiter threads can wake up to access it.
        _broadcast = false;
      }
      else {
        LeaveCriticalSection (&_lockWaiters);
      }

      return true;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:

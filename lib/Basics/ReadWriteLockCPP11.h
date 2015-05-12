////////////////////////////////////////////////////////////////////////////////
/// @brief Read-Write Lock implemented with C++11 standard semantics
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
/// @author Max Neunhoeffer
/// @author Copyright 2015, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_BASICS_READ_WRITE_LOCK_CPP11_H
#define ARANGODB_BASICS_READ_WRITE_LOCK_CPP11_H 1

#include "Basics/Common.h"

#include <mutex>
#include <condition_variable>
#include <thread>

namespace triagens {
  namespace basics {

// -----------------------------------------------------------------------------
// --SECTION--                                          class ReadWriteLockCPP11
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief read-write lock, slow but just using CPP11
/// This class has two other advantages:
///  (1) it is possible that a thread tries to acquire a lock even if it
///      has it already. This is important when we are running a thread
///      pool that works on task groups and a task group needs to acquire
///      a lock across multiple (non-concurrent) tasks. This must work,
///      even if tasks from different groups that fight for a lock are
///      actually executed by the same thread! POSIX RW-locks do not have
///      this property.
///  (2) write locks have a preference over read locks: as long as a task 
///      wants to get a write lock, no other task can get a (new) read lock.
///      This is necessary to avoid starvation of writers by many readers.
///      The current implementation can starve readers, though.
////////////////////////////////////////////////////////////////////////////////

    class ReadWriteLockCPP11 {

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

      public:

        ReadWriteLockCPP11 () : _state(0), _wantWrite(false) {
        }

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief locks for writing
////////////////////////////////////////////////////////////////////////////////

        void writeLock () {
          std::unique_lock<std::mutex> guard(_mut);
          if (_state == 0) {
            _state = -1;
            return;
          }
          do {
            _wantWrite = true;
            _bell.wait(guard);
          }
          while (_state != 0);
          _state = -1;
          _wantWrite = false;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief locks for writing, but only tries
////////////////////////////////////////////////////////////////////////////////

        bool tryWriteLock () {
          std::unique_lock<std::mutex> guard(_mut);
          if (_state == 0) {
            _state = -1;
            return true;
          }
          return false;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief locks for reading
////////////////////////////////////////////////////////////////////////////////

        void readLock () {
          std::unique_lock<std::mutex> guard(_mut);
          if (! _wantWrite && _state >= 0) {
            _state += 1;
            return;
          }
          while (true) {
            while (_wantWrite || _state < 0) {
              _bell.wait(guard);
            }
            if (! _wantWrite) {
              break;
            }
          }
          _state += 1;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief locks for reading, tries only
////////////////////////////////////////////////////////////////////////////////

        bool tryReadLock () {
          std::unique_lock<std::mutex> guard(_mut);
          if (! _wantWrite && _state >= 0) {
            _state += 1;
            return true;
          }
          return false;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief releases the read-lock or write-lock
////////////////////////////////////////////////////////////////////////////////

        void unlock () {
          std::unique_lock<std::mutex> guard(_mut);
          if (_state == -1) {
            _state = 0;
            _bell.notify_all();
          }
          else {
            _state -= 1;
            if (_state == 0) {
              _bell.notify_all();
            }
          }
        }

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief a mutex
////////////////////////////////////////////////////////////////////////////////

        std::mutex _mut;

////////////////////////////////////////////////////////////////////////////////
/// @brief a condition variable to wake up threads
////////////////////////////////////////////////////////////////////////////////

        std::condition_variable _bell;

////////////////////////////////////////////////////////////////////////////////
/// @brief _state, 0 means unlocked, -1 means write locked, positive means
/// a number of read locks
////////////////////////////////////////////////////////////////////////////////

        int _state;

////////////////////////////////////////////////////////////////////////////////
/// @brief _wantWrite, is set if somebody is waiting for the write lock
////////////////////////////////////////////////////////////////////////////////

        bool _wantWrite;

    };
  }
}

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:

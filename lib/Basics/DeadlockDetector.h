////////////////////////////////////////////////////////////////////////////////
/// @brief deadlock detector
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
/// @author Jan Steemann
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2013-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_BASICS_DEADLOCK_DETECTOR_H
#define ARANGODB_BASICS_DEADLOCK_DETECTOR_H 1

#include "Basics/Common.h"
#include "Basics/Mutex.h"
#include "Basics/MutexLocker.h"
#include "Basics/threads.h"

namespace triagens {
  namespace basics {

// -----------------------------------------------------------------------------
// --SECTION--                                                  DeadlockDetector
// -----------------------------------------------------------------------------

    template<typename T>
    class DeadlockDetector {

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

      public:

        DeadlockDetector () = default;
        ~DeadlockDetector () = default;

        DeadlockDetector (DeadlockDetector const&) = delete;
        DeadlockDetector& operator= (DeadlockDetector const&) = delete;

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

      public:

        bool isDeadlocked (T const* value) {
          auto tid = TRI_CurrentThreadId();
          std::unordered_set<TRI_tid_t> watchFor({ tid });

          std::vector<TRI_tid_t> stack;

          TRI_tid_t writerTid;

          {
            MUTEX_LOCKER(_writersLock);
            // find responsible writer
            auto it = _writers.find(value); 

            if (it == _writers.end()) {
              return false;
            }

            writerTid = (*it).second;
          }
            
          stack.push_back(writerTid);
          
          MUTEX_LOCKER(_readersLock);

          while (! stack.empty()) {
            TRI_tid_t current = stack.back();
            stack.pop_back();

            watchFor.emplace(current);
            auto it2 = _readersBlocked.find(current);

            if (it2 == _readersBlocked.end()) {
              return false;
            }

            if (watchFor.find((*it2).second) != watchFor.end()) {
              // deadlock!
              return true; 
            }

            stack.push_back((*it2).second);
          }

          // no deadlock found
          return false;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief insert a reader into the list of blocked readers
/// returns true if a deadlock was detected and false otherwise
////////////////////////////////////////////////////////////////////////////////

        bool setReaderBlocked (T const* value) {
          auto tid = TRI_CurrentThreadId();
          std::unordered_set<TRI_tid_t> watchFor({ tid });

          std::vector<TRI_tid_t> stack;

          TRI_tid_t writerTid;

          {
            MUTEX_LOCKER(_writersLock);
            // find responsible writer
            auto it = _writers.find(value); 

            if (it == _writers.end()) {
              return false;
            }

            writerTid = (*it).second;
          }
            
          stack.push_back(writerTid);
          
          MUTEX_LOCKER(_readersLock);
          _readersBlocked.emplace(tid, writerTid);

          try {
            while (! stack.empty()) {
              TRI_tid_t current = stack.back();
              stack.pop_back();

              watchFor.emplace(current);
              auto it2 = _readersBlocked.find(current);

              if (it2 == _readersBlocked.end()) {
                return false;
              }

              if (watchFor.find((*it2).second) != watchFor.end()) {
                // deadlock!
                _readersBlocked.erase(tid); 
                return true; 
              }

              stack.push_back((*it2).second);
            }

            // no deadlock found
            return false;
          }
          catch (...) {
            // clean up and re-throw
            _readersBlocked.erase(tid); 
            throw;
          }
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief remove a reader from the list of blocked readers
////////////////////////////////////////////////////////////////////////////////

        void setReaderUnblocked (T const* value) noexcept {
          auto tid = TRI_CurrentThreadId();

          try {
            MUTEX_LOCKER(_readersLock);
            _readersBlocked.erase(tid); 
          }
          catch (...) {
          }
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief inserts a writer into the list of writers
////////////////////////////////////////////////////////////////////////////////

        void setWriterStarted (T const* value) {
          auto tid = TRI_CurrentThreadId();

          MUTEX_LOCKER(_writersLock);
          _writers.emplace(value, tid);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief removes a writers from the list of writers
////////////////////////////////////////////////////////////////////////////////

        void setWriterFinished (T const* value) noexcept {
          try {
            MUTEX_LOCKER(_writersLock);
            _writers.erase(value);
          }
          catch (...) {
          }
        }

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief lock for managing the writers
////////////////////////////////////////////////////////////////////////////////

        triagens::basics::Mutex _writersLock;

////////////////////////////////////////////////////////////////////////////////
/// @brief all operating writers
////////////////////////////////////////////////////////////////////////////////

        std::unordered_map<T const*, TRI_tid_t> _writers;

////////////////////////////////////////////////////////////////////////////////
/// @brief lock for managing the readers
////////////////////////////////////////////////////////////////////////////////

        triagens::basics::Mutex _readersLock;

////////////////////////////////////////////////////////////////////////////////
/// @brief readers that are blocked on writers
////////////////////////////////////////////////////////////////////////////////

        std::unordered_map<TRI_tid_t, TRI_tid_t> _readersBlocked;

    };

  }   // namespace triagens::basics
}   // namespace triagens

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:

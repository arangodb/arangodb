////////////////////////////////////////////////////////////////////////////////
/// @brief read-write lock
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
/// @author Frank Celler
/// @author Achim Brandt
/// @author Copyright 2010-2011, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_BASICS_READ_WRITE_LOCK_POSIX_H
#define TRIAGENS_BASICS_READ_WRITE_LOCK_POSIX_H 1

#include <Basics/Common.h>

#define READ_WRITE_LOCK_COUNTER

namespace triagens {
  namespace basics {

    ////////////////////////////////////////////////////////////////////////////////
    /// @ingroup Threads
    /// @brief read-write lock
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

    class ReadWriteLock {
      private:
        ReadWriteLock (ReadWriteLock const&);
        ReadWriteLock& operator= (ReadWriteLock const&);

      public:

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief constructs a read-write lock
        ////////////////////////////////////////////////////////////////////////////////

        ReadWriteLock ();

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief deletes read-write lock
        ////////////////////////////////////////////////////////////////////////////////

        ~ReadWriteLock ();

      public:

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief check for read locked
        ////////////////////////////////////////////////////////////////////////////////

#ifdef READ_WRITE_LOCK_COUNTER

        bool isReadLocked () const {
          return _readLocked > 0;
        }

#endif

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief locks for reading
        ////////////////////////////////////////////////////////////////////////////////

        bool readLock () WARN_UNUSED;

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief check for write locked
        ////////////////////////////////////////////////////////////////////////////////

#ifdef READ_WRITE_LOCK_COUNTER

        bool isWriteLocked () const {
          return _writeLocked > 0;
        }

#endif

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief locks for writing
        ////////////////////////////////////////////////////////////////////////////////

        bool writeLock () WARN_UNUSED;

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief releases the read-lock or write-lock
        ////////////////////////////////////////////////////////////////////////////////

        bool unlock () WARN_UNUSED;

      private:
        pthread_rwlock_t _rwlock;

#ifdef READ_WRITE_LOCK_COUNTER
        pthread_mutex_t _mutex;
        uint32_t _readLocked;
        uint32_t _writeLocked;
#endif
    };
  }
}

#endif

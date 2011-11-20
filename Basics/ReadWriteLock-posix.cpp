////////////////////////////////////////////////////////////////////////////////
/// @brief Read-Write Lock
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

#include "ReadWriteLock-posix.h"

#include <Basics/Logger.h>

namespace triagens {
  namespace basics {

    // -----------------------------------------------------------------------------
    // constructors and destructora
    // -----------------------------------------------------------------------------

    ReadWriteLock::ReadWriteLock () {
      int rc = pthread_rwlock_init(&_rwlock, 0);

      if (rc != 0) {
        LOGGER_ERROR << "pthread_rwlock_init() != 0: " << strerror(rc);
      }

#ifdef READ_WRITE_LOCK_COUNTER
      pthread_mutex_init(&_mutex, 0);

      _readLocked = 0;
      _writeLocked = 0;
#endif
    }



    ReadWriteLock::~ReadWriteLock () {
      int rc = pthread_rwlock_destroy(&_rwlock);

      if (rc != 0) {
        LOGGER_ERROR << "pthread_rwlock_destroy() != 0: " << strerror(rc);
      }

#ifdef READ_WRITE_LOCK_COUNTER
      pthread_mutex_destroy(&_mutex);
#endif
    }

    // -----------------------------------------------------------------------------
    // public methods
    // -----------------------------------------------------------------------------

    bool ReadWriteLock::readLock () {
      int rc = pthread_rwlock_rdlock(&_rwlock);

      if (rc != 0) {
        LOGGER_ERROR << "could not lock for reading: " << strerror(rc);
        return false;
      }

#ifdef READ_WRITE_LOCK_COUNTER

      {
        pthread_mutex_lock(&_mutex);
        _readLocked++;
        pthread_mutex_unlock(&_mutex);
      }

#endif

      return true;
    }



    bool ReadWriteLock::writeLock () {
      int rc = pthread_rwlock_wrlock(&_rwlock);

      if (rc != 0) {
        LOGGER_ERROR << "could not lock for writing: " << strerror(rc);
        return false;
      }

#ifdef READ_WRITE_LOCK_COUNTER

      {
        pthread_mutex_lock(&_mutex);
        _writeLocked++;
        pthread_mutex_unlock(&_mutex);
      }

#endif

      return true;
    }



    bool ReadWriteLock::unlock () {
#ifdef READ_WRITE_LOCK_COUNTER

      {
        pthread_mutex_lock(&_mutex);

        if (_writeLocked > 0) {
          _writeLocked--;
        }
        else {
          _readLocked--;
        }

        pthread_mutex_unlock(&_mutex);
      }

#endif

      int rc = pthread_rwlock_unlock(&_rwlock);

      if (rc != 0) {
        LOGGER_ERROR << "could not release lock: " << strerror(rc);
        return false;
      }

      return true;
    }
  }
}

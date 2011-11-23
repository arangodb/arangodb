////////////////////////////////////////////////////////////////////////////////
/// @brief Condition Variable
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
/// @author Achim Brandt
/// @author Copyright 2008-2011, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "ConditionVariable.h"

#include <errno.h>

#include <Basics/Logger.h>

namespace triagens {
  namespace basics {

    // -----------------------------------------------------------------------------
    // constructors and destructors
    // -----------------------------------------------------------------------------

    ConditionVariable::ConditionVariable ()
      : _mutex(),
        _condition() {
      pthread_mutex_init(&_mutex, 0);
      pthread_cond_init(&_condition, 0);
    }



    ConditionVariable::~ConditionVariable () {
      pthread_cond_destroy(&_condition);
      pthread_mutex_destroy(&_mutex);
    }

    // -----------------------------------------------------------------------------
    // public methods
    // -----------------------------------------------------------------------------

    bool ConditionVariable::lock () {
      int rc = pthread_mutex_lock(&_mutex);

      if (rc != 0) {
        LOGGER_ERROR << "could not lock the mutex: " << strerror(errno);
        return false;
      }

      return true;
    }



    bool ConditionVariable::unlock () {
      int rc = pthread_mutex_unlock(&_mutex);

      if (rc != 0) {
        LOGGER_ERROR << "could not release the mutex: " << strerror(errno);
        return false;
      }

      return true;
    }



    bool ConditionVariable::wait () {
      int rc = pthread_cond_wait(&_condition, &_mutex);

      if (rc != 0) {
        LOGGER_ERROR << "could not wait for condition: " << strerror(errno);
        return false;
      }

      return true;
    }


    bool ConditionVariable::timedwait (uint64_t delay) {

      int               rc;
      struct timespec   ts;
      struct timeval    tp;

      rc =  gettimeofday(&tp, NULL);
      if (rc != 0) {
        LOGGER_ERROR << "could not get time of day for condition: " << strerror(errno);
        return false;
      }

      /* Convert from timeval to timespec */
      ts.tv_sec  = tp.tv_sec;
      uint64_t x = (tp.tv_usec * 1000) + (delay * 1000);
      uint64_t y = (x % 1000000000);
      ts.tv_nsec = y;
      ts.tv_sec  = ts.tv_sec + ((x - y) / 1000000000);

      rc = pthread_cond_timedwait(&_condition, &_mutex, &ts);

      if (rc != 0) {
        if (rc != ETIMEDOUT) {
          LOGGER_ERROR << "could not timedwait for condition: " << strerror(errno);
          return false;
        }
        return false;
      }

      return true;
    }



    bool ConditionVariable::broadcast () {
      int rc = pthread_cond_broadcast(&_condition);

      if (rc != 0) {
        LOGGER_ERROR << "could not send condition broadcast: " << strerror(errno);
        return false;
      }

      return true;
    }
  }
}

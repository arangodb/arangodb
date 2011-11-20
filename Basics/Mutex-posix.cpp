////////////////////////////////////////////////////////////////////////////////
/// @brief Mutex
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

#include "Mutex-posix.h"

#include <Basics/Logger.h>

namespace triagens {
  namespace basics {

    // -----------------------------------------------------------------------------
    // constructors and destructors
    // -----------------------------------------------------------------------------

    Mutex::Mutex () {
      pthread_mutex_init(&_mutex, 0);
    }



    Mutex::~Mutex () {
      pthread_mutex_destroy(&_mutex);
    }

    // -----------------------------------------------------------------------------
    // public methods
    // -----------------------------------------------------------------------------

    bool Mutex::lock () {
      int rc = pthread_mutex_lock(&_mutex);

      if (rc != 0) {
        LOGGER_ERROR << "could not lock the mutex: " << strerror(rc);
        return false;
      }

      return true;
    }



    bool Mutex::unlock () {
      int rc = pthread_mutex_unlock(&_mutex);

      if (rc != 0) {
        LOGGER_ERROR << "could not release the mutex: " << strerror(rc);
        return false;
      }

      return true;
    }
  }
}

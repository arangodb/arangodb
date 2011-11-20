////////////////////////////////////////////////////////////////////////////////
/// @brief Condition Locker
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

#include "ConditionLocker.h"

#include <Basics/Exceptions.h>
#include <Basics/StringUtils.h>

namespace triagens {
  namespace basics {

    // -----------------------------------------------------------------------------
    // constructors and destructors
    // -----------------------------------------------------------------------------

    ConditionLocker::ConditionLocker (ConditionVariable* conditionVariable)
      : _conditionVariable(conditionVariable), _file(0), _line(0) {
      bool ok = _conditionVariable->lock();

      if (! ok) {
        THROW_INTERNAL_ERROR("condition lock failed");
      }
    }



    ConditionLocker::ConditionLocker (ConditionVariable* conditionVariable, char const* file, int line)
      : _conditionVariable(conditionVariable), _file(file), _line(line) {
      bool ok = _conditionVariable->lock();

      if (! ok) {
        if (_file != 0) {
          THROW_INTERNAL_ERROR_L("condition lock failed", _file, _line);
        }
        else {
          THROW_INTERNAL_ERROR("condition lock failed");
        }
      }
    }



    ConditionLocker::~ConditionLocker () {
      bool ok = _conditionVariable->unlock();

      if (! ok) {
        if (_file != 0) {
          THROW_INTERNAL_ERROR_L("condition unlock failed", _file, _line);
        }
        else {
          THROW_INTERNAL_ERROR("condition unlock failed");
        }
      }
    }

    // -----------------------------------------------------------------------------
    // public methods
    // -----------------------------------------------------------------------------

    void ConditionLocker::wait () {
      bool ok = _conditionVariable->wait();

      if (! ok) {
        if (_file != 0) {
          THROW_INTERNAL_ERROR_L("condition wait failed", _file, _line);
        }
        else {
          THROW_INTERNAL_ERROR("condition wait failed");
        }
      }
    }



    void ConditionLocker::broadcast () {
      bool ok = _conditionVariable->broadcast();

      if (! ok) {
        if (_file != 0) {
          THROW_INTERNAL_ERROR_L("condition wait failed", _file, _line);
        }
        else {
          THROW_INTERNAL_ERROR("condition wait failed");
        }
      }
    }
  }
}



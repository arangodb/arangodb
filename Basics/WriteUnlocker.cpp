////////////////////////////////////////////////////////////////////////////////
/// @brief Write Unlocker
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

#include "WriteUnlocker.h"

#include <Basics/Exceptions.h>
#include <Basics/StringUtils.h>

namespace triagens {
  namespace basics {

    // -----------------------------------------------------------------------------
    // constructors and destructors
    // -----------------------------------------------------------------------------

    WriteUnlocker::WriteUnlocker (ReadWriteLock* readWriteLock)
      : _readWriteLock(readWriteLock), _file(0), _line(0) {
      bool ok = _readWriteLock->unlock();

      if (! ok) {
        THROW_INTERNAL_ERROR("write unlock failed");
      }
    }



    WriteUnlocker::WriteUnlocker (ReadWriteLock* readWriteLock, char const* file, int line)
      : _readWriteLock(readWriteLock), _file(file), _line(line) {
      bool ok = _readWriteLock->unlock();

      if (! ok) {
        if (_file != 0) {
          THROW_INTERNAL_ERROR_L("write unlock failed", _file, _line);
        }
        else {
          THROW_INTERNAL_ERROR("write unlock failed");
        }
      }
    }




    WriteUnlocker::~WriteUnlocker () {
      bool ok = _readWriteLock->writeLock();

      if (! ok) {
        if (_file != 0) {
          THROW_INTERNAL_ERROR_L("write lock failed", _file, _line);
        }
        else {
          THROW_INTERNAL_ERROR("write lock failed");
        }
      }
    }
  }
}


////////////////////////////////////////////////////////////////////////////////
/// @brief Write-ahead logfile remover thread
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
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "RemoverThread.h"
#include "Basics/logging.h"
#include "Basics/ConditionLocker.h"
#include "Utils/Exception.h"
#include "Wal/LogfileManager.h"

using namespace triagens::wal;

// -----------------------------------------------------------------------------
// --SECTION--                                               class RemoverThread
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief wait interval for the remover thread when idle
////////////////////////////////////////////////////////////////////////////////

uint64_t const RemoverThread::Interval = 500000;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create the remover thread
////////////////////////////////////////////////////////////////////////////////

RemoverThread::RemoverThread (LogfileManager* logfileManager)
  : Thread("WalRemover"),
    _logfileManager(logfileManager),
    _condition(),
    _stop(0) {

  allowAsynchronousCancelation();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the remover thread
////////////////////////////////////////////////////////////////////////////////

RemoverThread::~RemoverThread () {
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief stops the remover thread
////////////////////////////////////////////////////////////////////////////////

void RemoverThread::stop () {
  if (_stop > 0) {
    return;
  }

  _stop = 1;
  _condition.signal();

  while (_stop != 2) {
    usleep(10000);
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    Thread methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief main loop
////////////////////////////////////////////////////////////////////////////////

void RemoverThread::run () {
  while (true) {
    int stop = (int) _stop;
    bool worked = false;

    try {
      if (stop == 0) {
        worked = _logfileManager->removeLogfiles();
      }
    }
    catch (triagens::arango::Exception const& ex) {
      int res = ex.code();
      LOG_ERROR("got unexpected error in removerThread::run: %s", TRI_errno_string(res));
    }
    catch (...) {
      LOG_ERROR("got unspecific error in removerThread::run");
    }

    if (stop == 0 && ! worked) {
      // sleep only if there was nothing to do
      CONDITION_LOCKER(guard, _condition);

      guard.wait(Interval);
    }
    else if (stop == 1) {
      break;
    }

    // next iteration
  }

  _stop = 2;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:

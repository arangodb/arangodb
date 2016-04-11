////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#include "RemoverThread.h"

#include "Logger/Logger.h"
#include "Basics/ConditionLocker.h"
#include "Basics/Exceptions.h"
#include "Wal/LogfileManager.h"

using namespace arangodb::wal;

/// @brief wait interval for the remover thread when idle
uint64_t const RemoverThread::Interval = 2000000;

RemoverThread::RemoverThread(LogfileManager* logfileManager)
    : Thread("WalRemover"), _logfileManager(logfileManager), _condition() {}

/// @brief begin shutdown sequence
void RemoverThread::beginShutdown() {
  Thread::beginShutdown();

  CONDITION_LOCKER(guard, _condition);
  guard.signal();
}

/// @brief main loop
void RemoverThread::run() {
  int64_t iterations = 0;

  while (!isStopping()) {
    bool worked = false;

    try {
      worked = _logfileManager->removeLogfiles();

      if (++iterations == 5) {
        iterations = 0;
        _logfileManager->collectLogfileBarriers();
      }
    } catch (arangodb::basics::Exception const& ex) {
      int res = ex.code();
      LOG(ERR) << "got unexpected error in removerThread::run: "
               << TRI_errno_string(res);
    } catch (...) {
      LOG(ERR) << "got unspecific error in removerThread::run";
    }

    // sleep only if there was nothing to do
    if (!worked) {
      CONDITION_LOCKER(guard, _condition);

      if (!isStopping()) {
        guard.wait(Interval);
      }
    }
  }
}

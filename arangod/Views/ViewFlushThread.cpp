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

#include "ViewFlushThread.h"

#include "Basics/ConditionLocker.h"
#include "Basics/Exceptions.h"
#include "Logger/Logger.h"

using namespace arangodb;
  
ViewFlushThread::ViewFlushThread(uint64_t syncInterval)
    : Thread("ViewFlusher"),
      _condition(),
      _syncInterval(syncInterval) {}

/// @brief begin shutdown sequence
void ViewFlushThread::beginShutdown() {
  Thread::beginShutdown();

  // wake up ourselves
  wakeup();
}

/// @brief wake up the flush thread
void ViewFlushThread::wakeup() {
  CONDITION_LOCKER(guard, _condition);
  guard.signal();
}

/// @brief main loop
void ViewFlushThread::run() {
  while (!isStopping()) {
    try {
      // TODO: implement flush logic here
      // LOG_TOPIC(ERR, Logger::FIXME) << "view flush ping";

      // sleep if nothing to do
      CONDITION_LOCKER(guard, _condition);
      guard.wait(_syncInterval);

    } catch (std::exception const& ex) {
      LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "caught exception in ViewFlushThread: " << ex.what();
    } catch (...) {
      LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "caught unknown exception in ViewFlushThread";
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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

#include "FlushThread.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/ConditionLocker.h"
#include "Basics/Exceptions.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "RestServer/FlushFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"

using namespace arangodb;

FlushThread::FlushThread(FlushFeature& feature, uint64_t flushInterval)
    : Thread(feature.server(), "FlushThread"),
      _condition(),
      _feature(feature),
      _flushInterval(flushInterval) {}

/// @brief begin shutdown sequence
void FlushThread::beginShutdown() {
  Thread::beginShutdown();

  // wake up ourselves
  wakeup();
}

/// @brief wake up the flush thread
void FlushThread::wakeup() {
  CONDITION_LOCKER(guard, _condition);
  guard.signal();
}

/// @brief main loop
void FlushThread::run() {
  size_t count = 0;
  TRI_voc_tick_t tick = 0;

  while (!isStopping()) {
    try {
      TRI_IF_FAILURE("FlushThreadDisableAll") {
        CONDITION_LOCKER(guard, _condition);
        guard.wait(_flushInterval);

        continue;
      }

      _feature.releaseUnusedTicks(count, tick);

      LOG_TOPIC_IF("2b2e1", DEBUG, arangodb::Logger::FLUSH, count)
          << "Flush subscription(s) released: '" << count;

      LOG_TOPIC("2b2e2", DEBUG, arangodb::Logger::FLUSH)
          << "Tick released: '" << tick << "'";

      // sleep if nothing to do
      CONDITION_LOCKER(guard, _condition);
      guard.wait(_flushInterval);
    } catch (basics::Exception const& ex) {
      if (ex.code() == TRI_ERROR_SHUTTING_DOWN) {
        break;
      }
      LOG_TOPIC("2b211", ERR, arangodb::Logger::FLUSH)
          << "caught exception in FlushThread: " << ex.what();
    } catch (std::exception const& ex) {
      LOG_TOPIC("a3cfc", ERR, arangodb::Logger::FLUSH)
          << "caught exception in FlushThread: " << ex.what();
    } catch (...) {
      LOG_TOPIC("40b52", ERR, arangodb::Logger::FLUSH)
          << "caught unknown exception in FlushThread";
    }
  }
}

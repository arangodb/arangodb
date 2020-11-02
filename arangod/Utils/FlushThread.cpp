////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
    : TaskThread(feature.server(), "FlushThread"),
      _condition(),
      _feature(feature),
      _flushInterval(flushInterval),
      _count(0),
      _tick(0) {}

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
bool FlushThread::runTask() {
  TRI_IF_FAILURE("FlushThreadDisableAll") {
    CONDITION_LOCKER(guard, _condition);
    guard.wait(_flushInterval);

    return true;
  }

  _feature.releaseUnusedTicks(_count, _tick);

  LOG_TOPIC_IF("2b2e1", DEBUG, arangodb::Logger::FLUSH, _count)
      << "Flush subscription(s) released: '" << _count;

  LOG_TOPIC("2b2e2", DEBUG, arangodb::Logger::FLUSH)
      << "Tick released: '" << _tick << "'";

  // sleep if nothing to do
  CONDITION_LOCKER(guard, _condition);
  guard.wait(_flushInterval);

  return true;
}

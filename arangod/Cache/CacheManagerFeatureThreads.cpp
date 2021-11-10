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
/// @author Dan Larkin-York
////////////////////////////////////////////////////////////////////////////////

#include <cstdint>

#include "Basics/ConditionLocker.h"
#include "Basics/ConditionVariable.h"
#include "Basics/Thread.h"
#include "Basics/voc-errors.h"
#include "Cache/CacheManagerFeatureThreads.h"
#include "Cache/Manager.h"
#include "Cache/Rebalancer.h"
#include "Logger/LogMacros.h"

using namespace arangodb;

CacheRebalancerThread::CacheRebalancerThread(application_features::ApplicationServer& server,
                                             cache::Manager* manager, std::uint64_t interval)
    : Thread(server, "CacheRebalancerThread"),
      _manager(manager),
      _rebalancer(_manager),
      _fullInterval(interval),
      _shortInterval(10000) {}

CacheRebalancerThread::~CacheRebalancerThread() { shutdown(); }

void CacheRebalancerThread::beginShutdown() {
  Thread::beginShutdown();

  CONDITION_LOCKER(guard, _condition);
  guard.signal();
}

void CacheRebalancerThread::run() {
  while (!isStopping()) {
    try {
      auto result = _rebalancer.rebalance();
      std::uint64_t interval = (result != TRI_ERROR_ARANGO_BUSY) ? _fullInterval : _shortInterval;

      CONDITION_LOCKER(guard, _condition);
      guard.wait(interval);
    } catch (std::exception const& ex) {
      LOG_TOPIC("e78b8", ERR, Logger::CACHE) << "cache rebalancer thread caught exception: " << ex.what();
    } catch (...) {
      LOG_TOPIC("7269a", ERR, Logger::CACHE) << "cache rebalancer thread caught unknown exception";
    }
  }
}

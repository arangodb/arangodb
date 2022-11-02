////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrey Abramov
////////////////////////////////////////////////////////////////////////////////

#include "FlushFeature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/encoding.h"
#include "Cluster/ServerState.h"
#include "Logger/Logger.h"
#include "Logger/LogMacros.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"

using namespace arangodb::application_features;
using namespace arangodb::basics;
using namespace arangodb::options;

namespace arangodb {

FlushFeature::FlushFeature(Server& server)
    : ArangodFeature{server, *this}, _stopped(false) {
  setOptional(true);
  startsAfter<BasicFeaturePhaseServer>();
  startsAfter<StorageEngineFeature>();
}

FlushFeature::~FlushFeature() = default;

void FlushFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addObsoleteOption("--server.flush-interval",
                             "interval (in microseconds) for flushing data",
                             true);
}

void FlushFeature::registerFlushSubscription(
    std::shared_ptr<FlushSubscription> const& subscription) {
  if (!subscription) {
    return;
  }

  std::lock_guard lock{_flushSubscriptionsMutex};

  if (_stopped) {
    LOG_TOPIC("798c4", ERR, Logger::FLUSH) << "FlushFeature not running";
    return;
  }

  _flushSubscriptions.emplace_back(subscription);
}

std::tuple<size_t, size_t, TRI_voc_tick_t> FlushFeature::releaseUnusedTicks() {
  auto& engine = server().getFeature<EngineSelectorFeature>().engine();
  auto const initialTick = engine.currentTick();

  size_t stale = 0;
  size_t active = 0;
  auto minTick = initialTick;

  {
    std::lock_guard lock{_flushSubscriptionsMutex};

    // find min tick and remove stale subscriptions
    for (auto itr = _flushSubscriptions.begin();
         itr != _flushSubscriptions.end();) {
      auto entry = itr->lock();

      if (!entry) {
        // remove stale
        itr = _flushSubscriptions.erase(itr);
        ++stale;
      } else {
        minTick = std::min(minTick, entry->tick());
        ++active;
        ++itr;
      }
    }
  }

  TRI_ASSERT(minTick <= engine.currentTick());

  TRI_IF_FAILURE("FlushCrashBeforeSyncingMinTick") {
    if (ServerState::instance()->isDBServer() ||
        ServerState::instance()->isSingleServer()) {
      TRI_TerminateDebugging("crashing before syncing min tick");
    }
  }

  engine.releaseTick(minTick);

  TRI_IF_FAILURE("FlushCrashAfterReleasingMinTick") {
    if (ServerState::instance()->isDBServer() ||
        ServerState::instance()->isSingleServer()) {
      TRI_TerminateDebugging("crashing after releasing min tick");
    }
  }

  LOG_TOPIC("2b2e2", DEBUG, arangodb::Logger::FLUSH)
      << "Flush tick released: '" << minTick << "'"
      << ", stale flush subscription(s) released: " << stale
      << ", active flush subscription(s) released: " << active
      << ", initial engine tick: " << initialTick;

  return std::tuple{active, stale, minTick};
}

void FlushFeature::stop() {
  std::lock_guard lock{_flushSubscriptionsMutex};

  // release any remaining flush subscriptions so that they may get
  // deallocated ASAP subscriptions could survive after
  // FlushFeature::stop(), e.g. DatabaseFeature::unprepare()
  _flushSubscriptions.clear();
  _stopped = true;
}

}  // namespace arangodb

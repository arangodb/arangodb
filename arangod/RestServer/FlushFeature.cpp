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
/// @author Andrey Abramov
////////////////////////////////////////////////////////////////////////////////

#include "FlushFeature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Aql/QueryCache.h"
#include "Basics/ReadLocker.h"
#include "Basics/WriteLocker.h"
#include "Basics/application-exit.h"
#include "Basics/encoding.h"
#include "Cluster/ServerState.h"
#include "FeaturePhases/BasicFeaturePhaseServer.h"
#include "Logger/Logger.h"
#include "Logger/LogMacros.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "RestServer/DatabaseFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngineFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "Utils/FlushThread.h"

using namespace arangodb::application_features;
using namespace arangodb::basics;
using namespace arangodb::options;

namespace arangodb {

std::atomic<bool> FlushFeature::_isRunning(false);

FlushFeature::FlushFeature(application_features::ApplicationServer& server)
    : ApplicationFeature(server, "Flush"),
      _flushInterval(1000000),
      _stopped(false) {
  setOptional(true);
  startsAfter<BasicFeaturePhaseServer>();

  startsAfter<StorageEngineFeature>();
}

void FlushFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addSection("server", "Server features");
  options->addOption("--server.flush-interval",
                     "interval (in microseconds) for flushing data",
                     new UInt64Parameter(&_flushInterval),
                     arangodb::options::makeDefaultFlags(arangodb::options::Flags::Hidden));
}

void FlushFeature::registerFlushSubscription(const std::shared_ptr<FlushSubscription>& subscription) {
  if (!subscription) {
    return;
  }

  std::lock_guard<std::mutex> lock(_flushSubscriptionsMutex);

  if (_stopped) {
    LOG_TOPIC("798c4", ERR, Logger::FLUSH) << "FlushFeature not running";

    return;
  }

  _flushSubscriptions.emplace_back(subscription);
}

arangodb::Result FlushFeature::releaseUnusedTicks(size_t& count, TRI_voc_tick_t& minTick) {
  count = 0;
  auto& engine = server().getFeature<EngineSelectorFeature>().engine();

  minTick = engine.currentTick();

  {
    std::lock_guard<std::mutex> lock(_flushSubscriptionsMutex);

    // find min tick and remove stale subscriptions
    for (auto itr = _flushSubscriptions.begin(); itr != _flushSubscriptions.end();) {
      auto entry = itr->lock();

      if (!entry) {
        // remove stale
        itr = _flushSubscriptions.erase(itr);
        ++count;
      } else {
        minTick = std::min(minTick, entry->tick());
        ++itr;
      }
    }
  }

  TRI_ASSERT(minTick <= engine.currentTick());

  TRI_IF_FAILURE("FlushCrashBeforeSyncingMinTick") {
    TRI_TerminateDebugging("crashing before syncing min tick");
  }

  // WAL tick has to be synced prior to releasing it, if the storage
  // engine supports it
  //   engine->waitForSyncTick(minTick);
  
  TRI_IF_FAILURE("FlushCrashAfterSyncingMinTick") {
    TRI_TerminateDebugging("crashing after syncing min tick");
  }

  engine.releaseTick(minTick);

  TRI_IF_FAILURE("FlushCrashAfterReleasingMinTick") {
    TRI_TerminateDebugging("crashing after releasing min tick");
  }

  return {};
}

void FlushFeature::validateOptions(std::shared_ptr<options::ProgramOptions> options) {
  if (_flushInterval < 1000) {
    // do not go below 1000 microseconds
    _flushInterval = 1000;
  }
}

void FlushFeature::prepare() {
  // At least for now we need FlushThread for ArangoSearch views
  // on a DB/Single server only, so we avoid starting FlushThread on
  // a coordinator and on agency nodes.
  setEnabled(!arangodb::ServerState::instance()->isCoordinator() &&
             !arangodb::ServerState::instance()->isAgent());

  if (!isEnabled()) {
    return;
  }
}

void FlushFeature::start() {
  {
    WRITE_LOCKER(lock, _threadLock);
    _flushThread.reset(new FlushThread(*this, _flushInterval));
  }
  DatabaseFeature& dbFeature = server().getFeature<DatabaseFeature>();
  dbFeature.registerPostRecoveryCallback([this]() -> Result {
    READ_LOCKER(lock, _threadLock);
    if (!this->_flushThread->start()) {
      LOG_TOPIC("bdc3c", FATAL, Logger::FLUSH) << "unable to start FlushThread";
      FATAL_ERROR_ABORT();
    } else {
      LOG_TOPIC("ed9cd", DEBUG, Logger::FLUSH) << "started FlushThread";
    }

    this->_isRunning.store(true);

    return {TRI_ERROR_NO_ERROR};
  });
}

void FlushFeature::beginShutdown() {
  // pass on the shutdown signal
  READ_LOCKER(lock, _threadLock);
  if (_flushThread != nullptr) {
    _flushThread->beginShutdown();
  }
}

void FlushFeature::stop() {
  LOG_TOPIC("2b0a6", TRACE, arangodb::Logger::FLUSH) << "stopping FlushThread";
  // wait until thread is fully finished

  FlushThread* thread = nullptr;
  {
    READ_LOCKER(lock, _threadLock);
    thread = _flushThread.get();
  }
  if (thread != nullptr) {
    while (thread->isRunning()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    {
      WRITE_LOCKER(wlock, _threadLock);
      _isRunning.store(false);
      _flushThread.reset();
    }

    {
      std::lock_guard<std::mutex> lock(_flushSubscriptionsMutex);

      // release any remaining flush subscriptions so that they may get deallocated ASAP
      // subscriptions could survive after FlushFeature::stop(), e.g. DatabaseFeature::unprepare()
      _flushSubscriptions.clear();
      _stopped = true;
    }
  }
}

}  // namespace arangodb

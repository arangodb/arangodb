////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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

#include "RocksDBDumpManager.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/Exceptions.h"
#include "Basics/system-functions.h"
#include "Cluster/ServerState.h"
#include "RestServer/DatabaseFeature.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "RocksDBEngine/RocksDBDumpContext.h"
#include "VocBase/ticks.h"

#include <absl/strings/str_cat.h>

using namespace arangodb;

RocksDBDumpManager::RocksDBDumpManager(RocksDBEngine& engine)
    : _engine(engine) {}

RocksDBDumpManager::~RocksDBDumpManager() {
  std::lock_guard mutexLocker{_lock};
  _contexts.clear();
}

RocksDBDumpContextGuard RocksDBDumpManager::createContext(
    uint64_t batchSize, uint64_t prefetchCount, uint64_t parallelism,
    std::vector<std::string> shards, double ttl, std::string const& user,
    std::string const& database) {
  TRI_ASSERT(ServerState::instance()->isSingleServer() ||
             ServerState::instance()->isDBServer());

  // generating the dump context can throw exceptions. if it does, then
  // no harm is done, and no resources will be leaked.
  auto context = std::make_shared<RocksDBDumpContext>(
      _engine, _engine.server().getFeature<DatabaseFeature>(), generateId(),
      batchSize, prefetchCount, parallelism, std::move(shards), ttl, user,
      database);

  std::lock_guard mutexLocker{_lock};

  if (_engine.server().isStopping()) {
    // do not accept any further contexts when we are already shutting down
    THROW_ARANGO_EXCEPTION(TRI_ERROR_SHUTTING_DOWN);
  }

  bool inserted = _contexts.try_emplace(context->id(), context).second;
  if (!inserted) {
    // cannot insert into map. should never happen
    TRI_ASSERT(false);
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "unable to insert dump context");
  }

  return RocksDBDumpContextGuard(*this, std::move(context));
}

RocksDBDumpContextGuard RocksDBDumpManager::find(std::string const& id,
                                                 std::string const& database,
                                                 std::string const& user) {
  std::lock_guard mutexLocker{_lock};

  auto it = _contexts.find(id);
  if (it == _contexts.end()) {
    // "cursor not found" is not a great return code, but it is much more
    // specific than a generic error. we can also think of a dump context
    // as a collection of cursors for shard dumping.
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_CURSOR_NOT_FOUND,
                                   "requested dump context not found");
  }

  auto& context = (*it).second;
  if (!context->canAccess(database, user)) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_FORBIDDEN,
                                   "insufficient permissions");
  }

  // whenever we access a context, we proactively extend its lifetime,
  // so it does not expire quickly after
  context->extendLifetime();

  return RocksDBDumpContextGuard(*this, std::move(context));
}

void RocksDBDumpManager::remove(std::string const& id,
                                std::string const& database,
                                std::string const& user) {
  std::lock_guard mutexLocker{_lock};

  auto it = _contexts.find(id);
  if (it == _contexts.end()) {
    // "cursor not found" is not a great return code, but it is much more
    // specific than a generic error. we can also think of a dump context
    // as a collection of cursors for shard dumping.
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_CURSOR_NOT_FOUND,
                                   "requested dump context not found");
  }

  auto& context = (*it).second;
  if (!context->canAccess(database, user)) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_FORBIDDEN,
                                   "insufficient permissions");
  }

  // if we remove the context from the map, then the context will be
  // destroyed if it is not in use by any other thread. if it is in
  // use by another thread, the thread will have a shared_ptr of the
  // context, and the context will be destroyed once the shared_ptr
  // goes out of scope in the other thread
  _contexts.erase(it);
}

void RocksDBDumpManager::dropDatabase(TRI_vocbase_t& vocbase) {
  std::lock_guard mutexLocker{_lock};

  for (auto it = _contexts.begin(); it != _contexts.end();
       /* no hoisting */) {
    auto& context = it->second;
    if (context->database() == vocbase.name()) {
      it = _contexts.erase(it);
    } else {
      ++it;
    }
  }
}

void RocksDBDumpManager::garbageCollect(bool force) {
  auto const now = TRI_microtime();

  std::lock_guard mutexLocker{_lock};

  for (auto it = _contexts.begin(); it != _contexts.end();
       /* no hoisting */) {
    auto& context = it->second;

    if (force || context->expires() < now) {
      it = _contexts.erase(it);
    } else {
      ++it;
    }
  }
}

void RocksDBDumpManager::beginShutdown() { garbageCollect(false); }

std::string RocksDBDumpManager::generateId() {
  // rationale: we use a HLC value here, because it is guaranteed to
  // move forward, even across restarts. the last HLC value is persisted
  // on server shutdown, so we avoid handing out an HLC value, shutting
  // down the server, and handing out the same HLC value for a different
  // dump after the restart.
  return absl::StrCat("dump-", TRI_HybridLogicalClock());
}

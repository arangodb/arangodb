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
#include "RocksDBEngine/RocksDBFormat.h"
#include "RocksDBEngine/RocksDBDumpContext.h"
#include "VocBase/ticks.h"

#include <absl/strings/str_cat.h>

using namespace arangodb;

RocksDBDumpManager::RocksDBDumpManager(RocksDBEngine& engine)
    : _engine(engine) {}

RocksDBDumpManager::~RocksDBDumpManager() = default;

std::shared_ptr<RocksDBDumpContext> RocksDBDumpManager::createContext(
    RocksDBDumpContextOptions opts, std::string const& user,
    std::string const& database, bool useVPack) {
  TRI_ASSERT(ServerState::instance()->isSingleServer() ||
             ServerState::instance()->isDBServer());

  // If the local RocksDB database still uses little endian key encoding,
  // then the whole new dump method does not work, since ranges in _revs
  // do not correspond to ranges in RocksDB keys in the documents column
  // family. Therefore, we block the creation of a dump context right away.
  if (rocksutils::rocksDBEndianness == RocksDBEndianness::Little) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_OLD_ROCKSDB_FORMAT);
  }

  // generating the dump context can throw exceptions. if it does, then
  // no harm is done, and no resources will be leaked.
  auto context = std::make_shared<RocksDBDumpContext>(
      _engine, _engine.server().getFeature<DatabaseFeature>(), generateId(),
      std::move(opts), user, database, useVPack);

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

  TRI_ASSERT(_contexts.at(context->id()) != nullptr);

  return context;
}

std::shared_ptr<RocksDBDumpContext> RocksDBDumpManager::find(
    std::string const& id, std::string const& database,
    std::string const& user) {
  std::lock_guard mutexLocker{_lock};

  // this will throw in case the context cannot be found or belongs to a
  // different user
  auto it = lookupContext(id, database, user);
  TRI_ASSERT(it != _contexts.end());

  return (*it).second;
}

void RocksDBDumpManager::remove(std::string const& id,
                                std::string const& database,
                                std::string const& user) {
  std::lock_guard mutexLocker{_lock};

  // this will throw in case the context cannot be found or belongs to a
  // different user
  auto it = lookupContext(id, database, user);
  TRI_ASSERT(it != _contexts.end());

  // if we remove the context from the map, then the context will be
  // destroyed if it is not in use by any other thread. if it is in
  // use by another thread, the thread will have a shared_ptr of the
  // context, and the context will be destroyed once the shared_ptr
  // goes out of scope in the other thread
  _contexts.erase(it);
}

void RocksDBDumpManager::dropDatabase(TRI_vocbase_t& vocbase) {
  std::lock_guard mutexLocker{_lock};

  std::erase_if(_contexts, [&](auto const& x) {
    return x.second->database() == vocbase.name();
  });
}

void RocksDBDumpManager::garbageCollect(bool force) {
  std::lock_guard mutexLocker{_lock};

  if (force) {
    _contexts.clear();
  } else {
    auto const now = TRI_microtime();
    std::erase_if(_contexts,
                  [&](auto const& x) { return x.second->expires() < now; });
  }
}

void RocksDBDumpManager::beginShutdown() { garbageCollect(true); }

std::string RocksDBDumpManager::generateId() {
  // rationale: we use a HLC value here, because it is guaranteed to
  // move forward, even across restarts. the last HLC value is persisted
  // on server shutdown, so we avoid handing out an HLC value, shutting
  // down the server, and handing out the same HLC value for a different
  // dump after the restart.
  return absl::StrCat("dump-", TRI_HybridLogicalClock());
}

RocksDBDumpManager::MapType::iterator RocksDBDumpManager::lookupContext(
    std::string const& id, std::string const& database,
    std::string const& user) {
  auto it = _contexts.find(id);
  if (it == _contexts.end()) {
    // "cursor not found" is not a great return code, but it is much more
    // specific than a generic error. we can also think of a dump context
    // as a collection of cursors for shard dumping.
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_CURSOR_NOT_FOUND,
                                   "requested dump context not found");
  }

  auto& context = (*it).second;
  TRI_ASSERT(context != nullptr);
  if (!context->canAccess(database, user)) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_FORBIDDEN,
                                   "insufficient permissions");
  }
  return it;
}

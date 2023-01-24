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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "RocksDBReplicationManager.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/Exceptions.h"
#include "Basics/MutexLocker.h"
#include "Basics/system-functions.h"
#include "Basics/ResultT.h"
#include "Logger/LogMacros.h"
#include "RocksDBEngine/RocksDBCommon.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "RocksDBEngine/RocksDBReplicationContext.h"
#include "RocksDBEngine/RocksDBReplicationContextGuard.h"
#include "VocBase/LogicalCollection.h"

#include <velocypack/Builder.h>

using namespace arangodb;
using namespace arangodb::rocksutils;

/// @brief create a context repository
RocksDBReplicationManager::RocksDBReplicationManager(RocksDBEngine& engine)
    : _lock() {
  _contexts.reserve(64);
}

/// @brief destroy a context repository
RocksDBReplicationManager::~RocksDBReplicationManager() {
  MUTEX_LOCKER(mutexLocker, _lock);
  _contexts.clear();
}

/// @brief creates a new context which must be later returned using release()
/// or destroy(); guarantees that RocksDB file deletion is disabled while
/// there are active contexts
RocksDBReplicationContextGuard RocksDBReplicationManager::createContext(
    RocksDBEngine& engine, double ttl, SyncerId const syncerId,
    ServerId const clientId, std::string const& patchCount) {
  // patchCount should only be set on single servers or DB servers
  TRI_ASSERT(patchCount.empty() || (ServerState::instance()->isSingleServer() ||
                                    ServerState::instance()->isDBServer()));

  auto context = std::make_shared<RocksDBReplicationContext>(
      engine, ttl, syncerId, clientId);

  RocksDBReplicationId const id = context->id();

  MUTEX_LOCKER(mutexLocker, _lock);

  if (engine.server().isStopping()) {
    // do not accept any further contexts when we are already shutting down
    THROW_ARANGO_EXCEPTION(TRI_ERROR_SHUTTING_DOWN);
  }

  if (!patchCount.empty()) {
    // patchCount was set. this is happening only during the getting-in-sync
    // protocol. now check if any other context has the same patchCount
    // value set. in this case, the other context is responsible for applying
    // count patches, and we have to drop ours

    // note: it is safe here to access the patchCount() method of any context,
    // as the only place that modifies a context's _patchCount instance
    // variable, is the call to setPatchcount() a few lines below. there is no
    // concurrency here, as this method here is executed under a mutex. in
    // addition, _contexts is only modified under this same mutex,
    bool foundOther =
        _contexts.end() !=
        std::find_if(
            _contexts.begin(), _contexts.end(),
            [&patchCount](decltype(_contexts)::value_type const& entry) {
              return entry.second->patchCount() == patchCount;
            });
    if (!foundOther) {
      // no other context exists that has "leadership" for patching counts to
      // the same collection/shard
      context->setPatchCount(patchCount);
    }
    // if we found a different context here, then the other context is
    // responsible for applying count patches.
  }

  bool inserted = _contexts.try_emplace(id, context).second;
  if (!inserted) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "unable to insert replication context");
  }

  LOG_TOPIC("27c43", TRACE, Logger::REPLICATION)
      << "created replication context " << id << ", ttl: " << ttl;

  return RocksDBReplicationContextGuard(*this, std::move(context));
}

/// @brief remove a context by id
ResultT<std::tuple<SyncerId, ServerId, std::string>>
RocksDBReplicationManager::remove(RocksDBReplicationId id) {
  MUTEX_LOCKER(mutexLocker, _lock);

  auto it = _contexts.find(id);

  if (it == _contexts.end()) {
    // not found
    return {TRI_ERROR_CURSOR_NOT_FOUND};
  }

  LOG_TOPIC("71233", TRACE, Logger::REPLICATION)
      << "removing replication context " << id;

  auto& context = it->second;
  TRI_ASSERT(context != nullptr);

  SyncerId syncerId = context->syncerId();
  ServerId clientId = context->replicationClientServerId();
  std::string clientInfo = context->clientInfo();

  _contexts.erase(it);

  return {std::make_tuple(syncerId, clientId, std::move(clientInfo))};
}

/// @brief find an existing context by id
/// if found, the context will be returned with the usage flag set to true.
/// it must be returned later using release()
RocksDBReplicationContextGuard RocksDBReplicationManager::find(
    RocksDBReplicationId id, double ttl) {
  MUTEX_LOCKER(mutexLocker, _lock);

  auto it = _contexts.find(id);

  if (it == _contexts.end()) {
    // not found
    LOG_TOPIC("629ab", TRACE, Logger::REPLICATION)
        << "trying to find non-existing context " << id;
    return RocksDBReplicationContextGuard(*this);
  }

  auto& context = it->second;
  TRI_ASSERT(context != nullptr);

  context->extendLifetime(ttl);

  return RocksDBReplicationContextGuard(*this, context);
}

/// @brief find an existing context by id and extend lifetime
/// may be used concurrently on used contexts
/// populates clientId
ResultT<std::tuple<SyncerId, ServerId, std::string>>
RocksDBReplicationManager::extendLifetime(RocksDBReplicationId id, double ttl) {
  MUTEX_LOCKER(mutexLocker, _lock);

  auto it = _contexts.find(id);

  if (it == _contexts.end()) {
    // not found
    return {TRI_ERROR_CURSOR_NOT_FOUND};
  }

  LOG_TOPIC("71234", TRACE, Logger::REPLICATION)
      << "extending lifetime of replication context " << id;

  auto& context = it->second;
  TRI_ASSERT(context != nullptr);

  // populate clientId
  SyncerId syncerId = context->syncerId();
  ServerId clientId = context->replicationClientServerId();
  std::string clientInfo = context->clientInfo();

  context->extendLifetime(ttl);

  return {std::make_tuple(syncerId, clientId, std::move(clientInfo))};
}

/// @brief return a context for later use
void RocksDBReplicationManager::release(
    std::shared_ptr<RocksDBReplicationContext>&& context, bool deleted) {
  TRI_ASSERT(context != nullptr);

  MUTEX_LOCKER(mutexLocker, _lock);

  if (deleted) {
    // remove from map
    _contexts.erase(context->id());
  } else if (_contexts.contains(context->id())) {
    // context is still present (not deleted by gc - increase its lifetime)
    context->extendLifetime();
  }
}

/// @brief return a context for garbage collection
void RocksDBReplicationManager::destroy(RocksDBReplicationContext* context) {
  if (context != nullptr) {
    remove(context->id());
  }
}

/// @brief drop contexts by database
void RocksDBReplicationManager::drop(TRI_vocbase_t& vocbase) {
  LOG_TOPIC("ce3b0", TRACE, Logger::REPLICATION)
      << "dropping all replication contexts for database " << vocbase.name();

  MUTEX_LOCKER(mutexLocker, _lock);

  for (auto it = _contexts.begin(); it != _contexts.end();
       /* no hoisting */) {
    auto& context = it->second;
    if (context->containsVocbase(vocbase)) {
      it = _contexts.erase(it);
    } else {
      ++it;
    }
  }
}

/// @brief drop contexts by collection
void RocksDBReplicationManager::drop(LogicalCollection& collection) {
  LOG_TOPIC("fe4bb", TRACE, Logger::REPLICATION)
      << "dropping all replication contexts for collection "
      << collection.name();

  MUTEX_LOCKER(mutexLocker, _lock);

  for (auto it = _contexts.begin(); it != _contexts.end();
       /* no hoisting */) {
    auto& context = it->second;
    if (context->containsCollection(collection)) {
      it = _contexts.erase(it);
    } else {
      ++it;
    }
  }
}

/// @brief drop all contexts
void RocksDBReplicationManager::dropAll() {
  LOG_TOPIC("bc8a8", TRACE, Logger::REPLICATION)
      << "deleting all replication contexts";

  MUTEX_LOCKER(mutexLocker, _lock);
  _contexts.clear();
}

/// @brief run a garbage collection on the contexts
bool RocksDBReplicationManager::garbageCollect(bool force) {
  LOG_TOPIC("79b22", TRACE, Logger::REPLICATION)
      << "garbage-collecting replication contexts";

  auto const now = TRI_microtime();
  size_t deleted = 0;

  MUTEX_LOCKER(mutexLocker, _lock);

  for (auto it = _contexts.begin(); it != _contexts.end();
       /* no hoisting */) {
    auto& context = it->second;

    if (force || context->expires() < now) {
      if (force) {
        LOG_TOPIC("26ab2", TRACE, Logger::REPLICATION)
            << "force-deleting context " << context->id();
      } else {
        LOG_TOPIC("be214", TRACE, Logger::REPLICATION)
            << "context " << context->id() << " is expired";
      }

      LOG_TOPIC("44874", TRACE, Logger::REPLICATION)
          << "garbage collecting replication context " << context->id();
      ++deleted;
      it = _contexts.erase(it);
    } else {
      ++it;
    }
  }

  if (deleted > 0) {
    LOG_TOPIC("7b2b0", TRACE, Logger::REPLICATION)
        << "garbage-collection deleted contexts: " << deleted;
  }

  return deleted > 0;
}

/// @brief tell the replication manager that a shutdown is in progress
/// effectively this will block the creation of new contexts
void RocksDBReplicationManager::beginShutdown() { garbageCollect(false); }

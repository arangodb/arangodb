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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "RocksDBReplicationManager.h"

#include "Basics/Exceptions.h"
#include "Basics/MutexLocker.h"
#include "Basics/system-functions.h"
#include "Basics/ResultT.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "RocksDBEngine/RocksDBCommon.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "RocksDBEngine/RocksDBReplicationContext.h"
#include "VocBase/LogicalCollection.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::rocksutils;

/// @brief maximum number of contexts to garbage-collect in one go
static constexpr size_t maxCollectCount = 32;

////////////////////////////////////////////////////////////////////////////////
/// @brief create a context repository
////////////////////////////////////////////////////////////////////////////////

RocksDBReplicationManager::RocksDBReplicationManager(RocksDBEngine& engine)
    : _lock(), _contexts(), _isShuttingDown(false) {
  _contexts.reserve(64);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy a context repository
////////////////////////////////////////////////////////////////////////////////

RocksDBReplicationManager::~RocksDBReplicationManager() {
  try {
    garbageCollect(true);
  } catch (...) {
  }

  // wait until all used contexts have vanished
  int tries = 0;

  while (true) {
    if (!containsUsedContext()) {
      break;
    }

    if (tries == 0) {
      LOG_TOPIC("77d11", INFO, arangodb::Logger::ENGINES)
          << "waiting for used contexts to become unused";
    } else if (tries == 120) {
      LOG_TOPIC("930b3", WARN, arangodb::Logger::ENGINES)
          << "giving up waiting for unused contexts";
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    ++tries;
  }

  {
    MUTEX_LOCKER(mutexLocker, _lock);

    for (auto it : _contexts) {
      delete it.second;
    }

    _contexts.clear();
  }
}

//////////////////////////////////////////////////////////////////////////////
/// @brief creates a new context which must be later returned using release()
/// or destroy(); guarantees that RocksDB file deletion is disabled while
/// there are active contexts
//////////////////////////////////////////////////////////////////////////////

RocksDBReplicationContext* RocksDBReplicationManager::createContext(
    RocksDBEngine& engine, double ttl, SyncerId const syncerId, ServerId const clientId,
    std::string const& patchCount) {
  // patchCount should only be set on single servers or DB servers
  TRI_ASSERT(patchCount.empty() ||
             (ServerState::instance()->isSingleServer() || ServerState::instance()->isDBServer())); 
 
  auto context =
      std::make_unique<RocksDBReplicationContext>(engine, ttl, syncerId, clientId);

  TRI_ASSERT(context->isUsed());

  RocksDBReplicationId const id = context->id();

  {
    MUTEX_LOCKER(mutexLocker, _lock);

    if (_isShuttingDown) {
      // do not accept any further contexts when we are already shutting down
      THROW_ARANGO_EXCEPTION(TRI_ERROR_SHUTTING_DOWN);
    }

    if (!patchCount.empty()) {
      // patchCount was set. this is happening only during the getting-in-sync
      // protocol. now check if any other context has the same patchCount
      // value set. in this case, the other context is responsible for applying
      // count patches, and we have to drop ours
      
      // note: it is safe here to access the patchCount() method of any context,
      // as the only place that modifies a context's _patchCount instance variable,
      // is the call to setPatchcount() a few lines below. there is no concurrency
      // here, as this method here is executed under a mutex. in addition, _contexts
      // is only modified under this same mutex, 
      bool foundOther = 
        _contexts.end() != std::find_if(_contexts.begin(), _contexts.end(), [&patchCount](decltype(_contexts)::value_type const& entry) {
          return entry.second->patchCount() == patchCount;
        });
      if (!foundOther) {
        // no other context exists that has "leadership" for patching counts to the
        // same collection/shard
        context->setPatchCount(patchCount);
      }
      // if we found a different context here, then the other context is responsible
      // for applying count patches.
    }

    _contexts.try_emplace(id, context.get());
  }

  LOG_TOPIC("27c43", TRACE, Logger::REPLICATION)
      << "created replication context " << id << ", ttl: " << ttl;

  return context.release();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief remove a context by id
////////////////////////////////////////////////////////////////////////////////

bool RocksDBReplicationManager::remove(RocksDBReplicationId id) {
  RocksDBReplicationContext* context = nullptr;

  {
    MUTEX_LOCKER(mutexLocker, _lock);

    auto it = _contexts.find(id);

    if (it == _contexts.end()) {
      // not found
      return false;
    }

    LOG_TOPIC("71233", TRACE, Logger::REPLICATION) << "removing replication context " << id;

    context = it->second;
    TRI_ASSERT(context != nullptr);

    if (context->isDeleted()) {
      // already deleted
      return false;
    }

    if (context->isUsed()) {
      // context is in use by someone else. now mark as deleted
      context->setDeleted();
      return true;
    }

    // context not in use by someone else
    _contexts.erase(it);
  }

  TRI_ASSERT(context != nullptr);

  delete context;
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief find an existing context by id
/// if found, the context will be returned with the usage flag set to true.
/// it must be returned later using release()
////////////////////////////////////////////////////////////////////////////////

RocksDBReplicationContext* RocksDBReplicationManager::find(RocksDBReplicationId id, double ttl) {
  RocksDBReplicationContext* context = nullptr;

  {
    MUTEX_LOCKER(mutexLocker, _lock);

    auto it = _contexts.find(id);

    if (it == _contexts.end()) {
      // not found
      LOG_TOPIC("629ab", TRACE, Logger::REPLICATION)
          << "trying to find non-existing context " << id;
      return nullptr;
    }

    context = it->second;
    TRI_ASSERT(context != nullptr);

    if (context->isDeleted()) {
      LOG_TOPIC("86214", WARN, Logger::REPLICATION) << "Trying to use deleted "
                                           << "replication context with id " << id;
      // already deleted
      return nullptr;
    }
    context->use(ttl);
  }

  return context;
}

//////////////////////////////////////////////////////////////////////////////
/// @brief find an existing context by id and extend lifetime
/// may be used concurrently on used contexts
/// populates clientId
//////////////////////////////////////////////////////////////////////////////

ResultT<std::tuple<SyncerId, ServerId, std::string>> RocksDBReplicationManager::extendLifetime(
    RocksDBReplicationId id, double ttl) {
  MUTEX_LOCKER(mutexLocker, _lock);

  auto it = _contexts.find(id);

  if (it == _contexts.end()) {
    // not found
    return {TRI_ERROR_CURSOR_NOT_FOUND};
  }

  LOG_TOPIC("71234", TRACE, Logger::REPLICATION)
      << "extending lifetime of replication context " << id;

  RocksDBReplicationContext* context = it->second;
  TRI_ASSERT(context != nullptr);

  if (context->isDeleted()) {
    // already deleted
    return {TRI_ERROR_CURSOR_NOT_FOUND};
  }

  // populate clientId
  SyncerId const syncerId = context->syncerId();
  ServerId const clientId = context->replicationClientServerId();
  std::string const& clientInfo = context->clientInfo();

  context->extendLifetime(ttl);

  return {std::make_tuple(syncerId, clientId, clientInfo)};
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return a context for later use
////////////////////////////////////////////////////////////////////////////////

void RocksDBReplicationManager::release(RocksDBReplicationContext* context) {
  {
    MUTEX_LOCKER(mutexLocker, _lock);

    TRI_ASSERT(context->isUsed());
    context->release();

    if (!context->isDeleted() || context->isUsed()) {
      return;
    }

    // remove from the list
    LOG_TOPIC("eb2f6", TRACE, Logger::REPLICATION)
        << "removing deleted replication context " << context->id();
    _contexts.erase(context->id());
  }

  // and free the context
  delete context;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return a context for garbage collection
////////////////////////////////////////////////////////////////////////////////

void RocksDBReplicationManager::destroy(RocksDBReplicationContext* context) {
  if (context != nullptr) {
    remove(context->id());
  }
}
////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the repository contains a used context
////////////////////////////////////////////////////////////////////////////////

bool RocksDBReplicationManager::containsUsedContext() {
  MUTEX_LOCKER(mutexLocker, _lock);

  for (auto it : _contexts) {
    if (it.second->isUsed()) {
      return true;
    }
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief drop contexts by database (at least mark them as deleted)
////////////////////////////////////////////////////////////////////////////////

void RocksDBReplicationManager::drop(TRI_vocbase_t* vocbase) {
  TRI_ASSERT(vocbase != nullptr);

  LOG_TOPIC("ce3b0", TRACE, Logger::REPLICATION)
      << "dropping all replication contexts for database " << vocbase->name();

  {
    MUTEX_LOCKER(mutexLocker, _lock);

    for (auto& context : _contexts) {
      context.second->removeVocbase(*vocbase);  // will set deleted flag
    }
  }

  garbageCollect(false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief drop contexts by collection (at least mark them as deleted)
////////////////////////////////////////////////////////////////////////////////

void RocksDBReplicationManager::drop(LogicalCollection* collection) {
  TRI_ASSERT(collection != nullptr);

  LOG_TOPIC("fe4bb", TRACE, Logger::REPLICATION)
      << "dropping all replication contexts for collection " << collection->name();

  bool found = false;
  {
    MUTEX_LOCKER(mutexLocker, _lock);

    for (auto& context : _contexts) {
      found |= context.second->removeCollection(*collection);
    }
  }

  if (found) {
    garbageCollect(false);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief drop all contexts (at least mark them as deleted)
////////////////////////////////////////////////////////////////////////////////

void RocksDBReplicationManager::dropAll() {
  LOG_TOPIC("bc8a8", TRACE, Logger::REPLICATION) << "deleting all replication contexts";

  {
    MUTEX_LOCKER(mutexLocker, _lock);

    for (auto& context : _contexts) {
      context.second->setDeleted();
    }
  }

  garbageCollect(true);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief run a garbage collection on the contexts
////////////////////////////////////////////////////////////////////////////////

bool RocksDBReplicationManager::garbageCollect(bool force) {
  LOG_TOPIC("79b22", TRACE, Logger::REPLICATION)
      << "garbage-collecting replication contexts";

  auto const now = TRI_microtime();
  std::vector<RocksDBReplicationContext*> found;

  try {
    found.reserve(maxCollectCount);

    MUTEX_LOCKER(mutexLocker, _lock);

    for (auto it = _contexts.begin(); it != _contexts.end();
         /* no hoisting */) {
      auto context = it->second;

      if (!force && context->isUsed()) {
        // must not physically destroy contexts that are currently used
        ++it;
        continue;
      }

      if (force || context->expires() < now) {
        if (force) {
          LOG_TOPIC("26ab2", TRACE, Logger::REPLICATION)
              << "force-deleting context " << context->id();
        } else {
          LOG_TOPIC("be214", TRACE, Logger::REPLICATION) << "context " << context->id() << " is expired";
        }
        context->setDeleted();
      }

      if (context->isDeleted()) {
        try {
          found.emplace_back(context);
          it = _contexts.erase(it);
        } catch (...) {
          // stop iteration
          break;
        }

        if (!force && found.size() >= maxCollectCount) {
          break;
        }
      } else {
        ++it;
      }
    }
  } catch (...) {
    // go on and remove whatever we found so far
  }

  // remove contexts outside the lock
  for (auto it : found) {
    LOG_TOPIC("44874", TRACE, Logger::REPLICATION)
        << "garbage collecting replication context " << it->id();
    delete it;
  }

  return (!found.empty());
}

//////////////////////////////////////////////////////////////////////////////
/// @brief tell the replication manager that a shutdown is in progress
/// effectively this will block the creation of new contexts
//////////////////////////////////////////////////////////////////////////////

void RocksDBReplicationManager::beginShutdown() {
  {
    MUTEX_LOCKER(mutexLocker, _lock);
    _isShuttingDown = true;
  }
  garbageCollect(false);
}

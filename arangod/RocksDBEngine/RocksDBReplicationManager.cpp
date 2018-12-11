////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
#include "Logger/Logger.h"
#include "RocksDBEngine/RocksDBCommon.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "RocksDBEngine/RocksDBReplicationContext.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::rocksutils;

/// @brief maximum number of contexts to garbage-collect in one go
static constexpr size_t maxCollectCount = 32;

////////////////////////////////////////////////////////////////////////////////
/// @brief create a context repository
////////////////////////////////////////////////////////////////////////////////

RocksDBReplicationManager::RocksDBReplicationManager()
    : _lock(),
      _contexts(),
      _isShuttingDown(false) {
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
      LOG_TOPIC(INFO, arangodb::Logger::ENGINES)
          << "waiting for used contexts to become unused";
    } else if (tries == 120) {
      LOG_TOPIC(WARN, arangodb::Logger::ENGINES)
          << "giving up waiting for unused contexts";
    }

    std::this_thread::sleep_for(std::chrono::microseconds(500000));
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

RocksDBReplicationContext* RocksDBReplicationManager::createContext(double ttl, TRI_server_id_t serverId) {
  auto context = std::make_unique<RocksDBReplicationContext>(ttl, serverId);
  TRI_ASSERT(context.get() != nullptr);
  TRI_ASSERT(context->isUsed());

  RocksDBReplicationId const id = context->id();

  {
    MUTEX_LOCKER(mutexLocker, _lock);

    if (_isShuttingDown) {
      // do not accept any further contexts when we are already shutting down
      THROW_ARANGO_EXCEPTION(TRI_ERROR_SHUTTING_DOWN);
    }

    _contexts.emplace(id, context.get());
  }

  LOG_TOPIC(TRACE, Logger::REPLICATION) << "created replication context " << id << ", ttl: " << ttl;

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

    LOG_TOPIC(TRACE, Logger::REPLICATION) << "removing replication context " << id;

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

RocksDBReplicationContext* RocksDBReplicationManager::find(
    RocksDBReplicationId id, double ttl) {
  RocksDBReplicationContext* context = nullptr;

  {
    MUTEX_LOCKER(mutexLocker, _lock);

    auto it = _contexts.find(id);

    if (it == _contexts.end()) {
      // not found
      LOG_TOPIC(TRACE, Logger::REPLICATION) << "trying to find non-existing context " << id;
      return nullptr;
    }

    context = it->second;
    TRI_ASSERT(context != nullptr);

    if (context->isDeleted()) {
      LOG_TOPIC(WARN, Logger::REPLICATION) << "Trying to use deleted "
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
/// may be used concurrently on used contextes
//////////////////////////////////////////////////////////////////////////////

int RocksDBReplicationManager::extendLifetime(RocksDBReplicationId id,
                                              double ttl) {
  MUTEX_LOCKER(mutexLocker, _lock);
  
  auto it = _contexts.find(id);
  
  if (it == _contexts.end()) {
    // not found
    return TRI_ERROR_CURSOR_NOT_FOUND;
  }
  
  RocksDBReplicationContext* context = it->second;
  TRI_ASSERT(context != nullptr);
  
  if (context->isDeleted()) {
    // already deleted
    return TRI_ERROR_CURSOR_NOT_FOUND;
  }
  
  context->extendLifetime(ttl);
  
  return TRI_ERROR_NO_ERROR;
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
    LOG_TOPIC(TRACE, Logger::REPLICATION) << "removing deleted replication context " << context->id();
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
  LOG_TOPIC(TRACE, Logger::REPLICATION) << "dropping all replication contexts for database " << vocbase->name();

  {
    MUTEX_LOCKER(mutexLocker, _lock);

    for (auto& context : _contexts) {
      context.second->removeVocbase(*vocbase); // will set deleted flag
    }
  }

  garbageCollect(false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief drop all contexts (at least mark them as deleted)
////////////////////////////////////////////////////////////////////////////////

void RocksDBReplicationManager::dropAll() {
  LOG_TOPIC(TRACE, Logger::REPLICATION) << "deleting all replication contexts";

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
  LOG_TOPIC(TRACE, Logger::REPLICATION) << "garbage-collecting replication contexts";

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
          LOG_TOPIC(TRACE, Logger::REPLICATION) << "force-deleting context " << context->id();
        } else {
          LOG_TOPIC(TRACE, Logger::REPLICATION) << "context " << context->id() << " is expired";
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
    LOG_TOPIC(TRACE, Logger::REPLICATION) << "garbage collecting replication context " << it->id();
    delete it;
  }

  return (!found.empty());
}

//////////////////////////////////////////////////////////////////////////////
/// @brief tell the replication manager that a shutdown is in progress
/// effectively this will block the creation of new contexts
//////////////////////////////////////////////////////////////////////////////

void RocksDBReplicationManager::beginShutdown() {
  MUTEX_LOCKER(mutexLocker, _lock);
  _isShuttingDown = true;
}

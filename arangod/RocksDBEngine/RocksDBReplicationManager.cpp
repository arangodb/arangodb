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
#include "Basics/MutexLocker.h"
#include "Logger/Logger.h"
#include "RocksDBEngine/RocksDBCommon.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "RocksDBEngine/RocksDBReplicationContext.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::rocksutils;

size_t const RocksDBReplicationManager::MaxCollectCount = 32;

////////////////////////////////////////////////////////////////////////////////
/// @brief create a context repository
////////////////////////////////////////////////////////////////////////////////

RocksDBReplicationManager::RocksDBReplicationManager() : _lock(), _contexts() {
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
      LOG_TOPIC(INFO, arangodb::Logger::FIXME)
          << "waiting for used contexts to become unused";
    } else if (tries == 120) {
      LOG_TOPIC(WARN, arangodb::Logger::FIXME)
          << "giving up waiting for unused contexts";
    }

    usleep(500000);
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

RocksDBReplicationContext* RocksDBReplicationManager::createContext() {
  auto context = std::make_unique<RocksDBReplicationContext>();
  TRI_ASSERT(context.get() != nullptr);
  TRI_ASSERT(context->isUsed());

  RocksDBReplicationId const id = context->id();

  {
    MUTEX_LOCKER(mutexLocker, _lock);
    _contexts.emplace(id, context.get());
  }
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

    context = it->second;
    TRI_ASSERT(context != nullptr);

    if (context->isDeleted()) {
      // already deleted
      return false;
    }

    if (context->isUsed()) {
      // context is in use by someone else. now mark as deleted
      context->deleted();
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
    RocksDBReplicationId id, bool& busy, double ttl) {
  RocksDBReplicationContext* context = nullptr;
  busy = false;

  {
    MUTEX_LOCKER(mutexLocker, _lock);

    auto it = _contexts.find(id);
    if (it == _contexts.end()) {
      // not found
      return nullptr;
    }

    context = it->second;
    TRI_ASSERT(context != nullptr);

    if (context->isDeleted()) {
      // already deleted
      return nullptr;
    }

    if (context->isUsed()) {
      busy = true;
      return nullptr;
    }

    context->use(ttl);
  }

  return context;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return a context for later use
////////////////////////////////////////////////////////////////////////////////

void RocksDBReplicationManager::release(RocksDBReplicationContext* context) {
  {
    MUTEX_LOCKER(mutexLocker, _lock);

    TRI_ASSERT(context->isUsed());
    context->release();

    if (!context->isDeleted()) {
      return;
    }

    // remove from the list
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
  {
    MUTEX_LOCKER(mutexLocker, _lock);

    for (auto& context : _contexts) {
      if (context.second->vocbase() == vocbase) {
        context.second->deleted();
      }
    }
  }

  garbageCollect(true);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief drop all contexts (at least mark them as deleted)
////////////////////////////////////////////////////////////////////////////////

void RocksDBReplicationManager::dropAll() {
  {
    MUTEX_LOCKER(mutexLocker, _lock);

    for (auto& context : _contexts) {
      context.second->deleted();
    }
  }

  garbageCollect(true);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief run a garbage collection on the contexts
////////////////////////////////////////////////////////////////////////////////

bool RocksDBReplicationManager::garbageCollect(bool force) {
  auto const now = TRI_microtime();
  std::vector<RocksDBReplicationContext*> found;

  try {
    found.reserve(MaxCollectCount);

    MUTEX_LOCKER(mutexLocker, _lock);

    for (auto it = _contexts.begin(); it != _contexts.end();
         /* no hoisting */) {
      auto context = it->second;

      if (context->isUsed()) {
        // must not destroy used contexts
        ++it;
        continue;
      }

      if (force || context->expires() < now) {
        context->deleted();
      }

      if (context->isDeleted()) {
        try {
          found.emplace_back(context);
          it = _contexts.erase(it);
        } catch (...) {
          // stop iteration
          break;
        }

        if (!force && found.size() >= MaxCollectCount) {
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
    delete it;
  }

  return (!found.empty());
}

RocksDBReplicationContextGuard::RocksDBReplicationContextGuard(
    RocksDBReplicationManager* manager, RocksDBReplicationContext* ctx)
    : _manager(manager), _ctx(ctx) {}

RocksDBReplicationContextGuard::~RocksDBReplicationContextGuard() {
  if (_ctx != nullptr) {
    _manager->release(_ctx);
  }
}

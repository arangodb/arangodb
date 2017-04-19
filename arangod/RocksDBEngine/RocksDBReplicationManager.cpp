////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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

#include "RocksDBReplicationManager.h"
#include "RocksDBEngine/RocksDBReplicationContext.h"
#include "Basics/MutexLocker.h"
#include "Logger/Logger.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;

size_t const RocksDBReplicationManager::MaxCollectCount = 32;

////////////////////////////////////////////////////////////////////////////////
/// @brief create a cursor repository
////////////////////////////////////////////////////////////////////////////////

RocksDBReplicationManager::RocksDBReplicationManager()
    : _lock(), _contexts() {
  _contexts.reserve(64);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy a cursor repository
////////////////////////////////////////////////////////////////////////////////

RocksDBReplicationManager::~RocksDBReplicationManager() {
  try {
    garbageCollect(true);
  } catch (...) {
  }

  // wait until all used cursors have vanished
  int tries = 0;

  while (true) {
    if (!containsUsedCursor()) {
      break;
    }

    if (tries == 0) {
      LOG_TOPIC(INFO, arangodb::Logger::FIXME) << "waiting for used cursors to become unused";
    } else if (tries == 120) {
      LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "giving up waiting for unused cursors";
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

////////////////////////////////////////////////////////////////////////////////
/// @brief stores a cursor in the registry
/// the repository will take ownership of the cursor
////////////////////////////////////////////////////////////////////////////////

RocksDBReplicationContext* RocksDBReplicationManager::addCursor(std::unique_ptr<RocksDBReplicationContext> cursor) {
  TRI_ASSERT(cursor != nullptr);
  TRI_ASSERT(cursor->isUsed());

  RocksDBReplicationId const id = cursor->id();

  MUTEX_LOCKER(mutexLocker, _lock);
  _contexts.emplace(id, cursor.get());

  return cursor.release();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief remove a cursor by id
////////////////////////////////////////////////////////////////////////////////

bool RocksDBReplicationManager::remove(RocksDBReplicationId id) {
  RocksDBReplicationContext* cursor = nullptr;

  {
    MUTEX_LOCKER(mutexLocker, _lock);

    auto it = _contexts.find(id);
    if (it == _contexts.end()) {
      // not found
      return false;
    }

    cursor = (*it).second;

    if (cursor->isDeleted()) {
      // already deleted
      return false;
    }

    if (cursor->isUsed()) {
      // cursor is in use by someone else. now mark as deleted
      //cursor->deleted();
      return true;
    }

    // cursor not in use by someone else
    _contexts.erase(it);
  }

  TRI_ASSERT(cursor != nullptr);

  delete cursor;
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief find an existing cursor by id
/// if found, the cursor will be returned with the usage flag set to true.
/// it must be returned later using release()
////////////////////////////////////////////////////////////////////////////////

RocksDBReplicationContext* RocksDBReplicationManager::find(RocksDBReplicationId id, bool& busy) {
  RocksDBReplicationContext* cursor = nullptr;
  busy = false;

  {
    MUTEX_LOCKER(mutexLocker, _lock);

    auto it = _contexts.find(id);
    if (it == _contexts.end()) {
      // not found
      return nullptr;
    }

    cursor = (*it).second;

    if (cursor->isDeleted()) {
      // already deleted
      return nullptr;
    }
    
    if (cursor->isUsed()) {
      busy = true;
      return nullptr;
    }

    cursor->use();
  }

  return cursor;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return a cursor
////////////////////////////////////////////////////////////////////////////////

void RocksDBReplicationManager::release(RocksDBReplicationContext* cursor) {
  {
    MUTEX_LOCKER(mutexLocker, _lock);

    TRI_ASSERT(cursor->isUsed());
    cursor->release();

    if (!cursor->isDeleted()) {
      return;
    }

    // remove from the list
    _contexts.erase(cursor->id());
  }

  // and free the cursor
  delete cursor;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the repository contains a used cursor
////////////////////////////////////////////////////////////////////////////////

bool RocksDBReplicationManager::containsUsedCursor() {
  MUTEX_LOCKER(mutexLocker, _lock);

  for (auto it : _contexts) {
    if (it.second->isUsed()) {
      return true;
    }
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief run a garbage collection on the cursors
////////////////////////////////////////////////////////////////////////////////

bool RocksDBReplicationManager::garbageCollect(bool force) {
  auto const now = TRI_microtime();
  std::vector<RocksDBReplicationContext*> found;

  try {
    found.reserve(MaxCollectCount);

    MUTEX_LOCKER(mutexLocker, _lock);

    for (auto it = _contexts.begin(); it != _contexts.end(); /* no hoisting */) {
      auto cursor = (*it).second;

      if (cursor->isUsed()) {
        // must not destroy used cursors
        ++it;
        continue;
      }

      if (force || cursor->expires() < now) {
        cursor->deleted();
      }

      if (cursor->isDeleted()) {
        try {
          found.emplace_back(cursor);
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

  // remove cursors outside the lock
  for (auto it : found) {
    delete it;
  }

  return (!found.empty());
}

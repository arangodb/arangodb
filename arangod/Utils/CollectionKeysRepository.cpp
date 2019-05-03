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

#include "CollectionKeysRepository.h"
#include "Basics/MutexLocker.h"
#include "Logger/Logger.h"
#include "VocBase/vocbase.h"

using namespace arangodb;

size_t const CollectionKeysRepository::MaxCollectCount = 32;

////////////////////////////////////////////////////////////////////////////////
/// @brief create a collection keys repository
////////////////////////////////////////////////////////////////////////////////

CollectionKeysRepository::CollectionKeysRepository() : _lock(), _keys() {
  _keys.reserve(64);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy a collection keys repository
////////////////////////////////////////////////////////////////////////////////

CollectionKeysRepository::~CollectionKeysRepository() {
  try {
    garbageCollect(true);
  } catch (...) {
  }

  // wait until all used entries have vanished
  int tries = 0;

  while (true) {
    if (!containsUsed()) {
      break;
    }

    if (tries == 0) {
      LOG_TOPIC("88129", INFO, arangodb::Logger::FIXME)
          << "waiting for used keys to become unused";
    } else if (tries == 120) {
      LOG_TOPIC("be20d", WARN, arangodb::Logger::FIXME)
          << "giving up waiting for unused keys";
    }

    std::this_thread::sleep_for(std::chrono::microseconds(500000));
    ++tries;
  }

  {
    MUTEX_LOCKER(mutexLocker, _lock);
    _keys.clear();
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stores collection keys in the repository
////////////////////////////////////////////////////////////////////////////////

void CollectionKeysRepository::store(std::unique_ptr<arangodb::CollectionKeys> keys) {
  MUTEX_LOCKER(mutexLocker, _lock);
  _keys.emplace(keys->id(), std::move(keys));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief remove collection keys by id
////////////////////////////////////////////////////////////////////////////////

bool CollectionKeysRepository::remove(CollectionKeysId id) {
  std::unique_ptr<arangodb::CollectionKeys> owner;

  {
    MUTEX_LOCKER(mutexLocker, _lock);

    auto it = _keys.find(id);

    if (it == _keys.end()) {
      // not found
      return false;
    }

    if ((*it).second->isDeleted()) {
      // already deleted
      return false;
    }

    if ((*it).second->isUsed()) {
      // keys are in use by someone else. now mark as deleted
      (*it).second->deleted();
      return true;
    }

    // keys are not in use by someone else
    // move ownership for item to our local variable
    owner = std::move((*it).second);
    // clear entry from map
    _keys.erase(it);
  }

  // when we leave this scope, this will fire the dtor of the
  // entry, outside the lock
  TRI_ASSERT(owner != nullptr);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief find an existing keys entry by id
/// if found, the keys will be returned with the usage flag set to true.
/// they must be returned later using release()
////////////////////////////////////////////////////////////////////////////////

CollectionKeys* CollectionKeysRepository::find(CollectionKeysId id) {
  arangodb::CollectionKeys* collectionKeys = nullptr;

  {
    MUTEX_LOCKER(mutexLocker, _lock);

    auto it = _keys.find(id);

    if (it == _keys.end()) {
      // not found
      return nullptr;
    }

    if ((*it).second->isDeleted()) {
      // already deleted
      return nullptr;
    }

    (*it).second->use();
    collectionKeys = (*it).second.get();
  }

  return collectionKeys;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return collection keys
////////////////////////////////////////////////////////////////////////////////

void CollectionKeysRepository::release(CollectionKeys* collectionKeys) {
  std::unique_ptr<CollectionKeys> owner;

  {
    MUTEX_LOCKER(mutexLocker, _lock);

    TRI_ASSERT(collectionKeys->isUsed());
    collectionKeys->release();

    if (!collectionKeys->isDeleted()) {
      return;
    }

    // remove from the list and take ownership
    auto it = _keys.find(collectionKeys->id());

    if (it != _keys.end()) {
      owner = std::move((*it).second);
      _keys.erase(it);
    }
  }

  // when we get here, the dtor of the entry will be called,
  // outside the lock
}

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the repository contains a used keys entry
////////////////////////////////////////////////////////////////////////////////

bool CollectionKeysRepository::containsUsed() {
  MUTEX_LOCKER(mutexLocker, _lock);

  for (auto const& it : _keys) {
    if (it.second->isUsed()) {
      return true;
    }
  }

  return false;
}

size_t CollectionKeysRepository::count() {
  MUTEX_LOCKER(mutexLocker, _lock);
  return _keys.size();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief run a garbage collection on the data
////////////////////////////////////////////////////////////////////////////////

bool CollectionKeysRepository::garbageCollect(bool force) {
  std::vector<std::unique_ptr<arangodb::CollectionKeys>> found;
  found.reserve(MaxCollectCount);

  auto const now = TRI_microtime();

  {
    MUTEX_LOCKER(mutexLocker, _lock);

    for (auto it = _keys.begin(); it != _keys.end(); /* no hoisting */) {
      auto& collectionKeys = (*it).second;

      if (collectionKeys->isUsed()) {
        // must not destroy anything currently in use
        ++it;
        continue;
      }

      if (force || collectionKeys->expires() < now) {
        collectionKeys->deleted();
      }

      if (collectionKeys->isDeleted()) {
        try {
          found.emplace_back(std::move(collectionKeys));
        } catch (...) {
          // stop iteration
          break;
        }
          
        it = _keys.erase(it);

        if (!force && found.size() >= MaxCollectCount) {
          break;
        }
      } else {
        ++it;
      }
    }
  }

  // when we get here, all instances in found will be destroyed
  // outside the lock
  return (!found.empty());
}

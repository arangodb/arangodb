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
#include "Logger/Logger.h"
#include "Basics/MutexLocker.h"
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
      LOG(INFO) << "waiting for used keys to become unused";
    } else if (tries == 120) {
      LOG(WARN) << "giving up waiting for unused keys";
    }

    usleep(500000);
    ++tries;
  }

  {
    MUTEX_LOCKER(mutexLocker, _lock);

    for (auto it : _keys) {
      delete it.second;
    }

    _keys.clear();
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stores collection keys in the repository
////////////////////////////////////////////////////////////////////////////////

void CollectionKeysRepository::store(arangodb::CollectionKeys* keys) {
  MUTEX_LOCKER(mutexLocker, _lock);
  _keys.emplace(keys->id(), keys);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief remove collection keys by id
////////////////////////////////////////////////////////////////////////////////

bool CollectionKeysRepository::remove(CollectionKeysId id) {
  arangodb::CollectionKeys* collectionKeys = nullptr;

  {
    MUTEX_LOCKER(mutexLocker, _lock);

    auto it = _keys.find(id);

    if (it == _keys.end()) {
      // not found
      return false;
    }

    collectionKeys = (*it).second;

    if (collectionKeys->isDeleted()) {
      // already deleted
      return false;
    }

    if (collectionKeys->isUsed()) {
      // keys are in use by someone else. now mark as deleted
      collectionKeys->deleted();
      return true;
    }

    // keys are not in use by someone else
    _keys.erase(it);
  }

  TRI_ASSERT(collectionKeys != nullptr);

  delete collectionKeys;
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

    collectionKeys = (*it).second;

    if (collectionKeys->isDeleted()) {
      // already deleted
      return nullptr;
    }

    collectionKeys->use();
  }

  return collectionKeys;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return collection keys
////////////////////////////////////////////////////////////////////////////////

void CollectionKeysRepository::release(CollectionKeys* collectionKeys) {
  {
    MUTEX_LOCKER(mutexLocker, _lock);

    TRI_ASSERT(collectionKeys->isUsed());
    collectionKeys->release();

    if (!collectionKeys->isDeleted()) {
      return;
    }

    // remove from the list
    _keys.erase(collectionKeys->id());
  }

  // and free the keys
  delete collectionKeys;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the repository contains a used keys entry
////////////////////////////////////////////////////////////////////////////////

bool CollectionKeysRepository::containsUsed() {
  MUTEX_LOCKER(mutexLocker, _lock);

  for (auto it : _keys) {
    if (it.second->isUsed()) {
      return true;
    }
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief run a garbage collection on the data
////////////////////////////////////////////////////////////////////////////////

bool CollectionKeysRepository::garbageCollect(bool force) {
  std::vector<arangodb::CollectionKeys*> found;
  found.reserve(MaxCollectCount);

  auto const now = TRI_microtime();

  {
    MUTEX_LOCKER(mutexLocker, _lock);

    for (auto it = _keys.begin(); it != _keys.end(); /* no hoisting */) {
      auto collectionKeys = (*it).second;

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
          found.emplace_back(collectionKeys);
          it = _keys.erase(it);
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
  }

  // remove keys outside the lock
  for (auto it : found) {
    delete it;
  }

  return (!found.empty());
}

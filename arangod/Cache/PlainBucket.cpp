////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Dan Larkin-York
////////////////////////////////////////////////////////////////////////////////

#include <atomic>
#include <cstdint>

#include "Cache/PlainBucket.h"

#include "Basics/Common.h"
#include "Basics/debugging.h"
#include "Cache/CachedValue.h"

namespace arangodb::cache {

PlainBucket::PlainBucket() {
  _state.lock();
  clear();
}

bool PlainBucket::lock(std::uint64_t maxTries) { return _state.lock(maxTries); }

void PlainBucket::unlock() {
  TRI_ASSERT(_state.isLocked());
  _state.unlock();
}

bool PlainBucket::isLocked() const { return _state.isLocked(); }

bool PlainBucket::isMigrated() const {
  TRI_ASSERT(isLocked());
  return _state.isSet(BucketState::Flag::migrated);
}

bool PlainBucket::isFull() const {
  TRI_ASSERT(isLocked());
  bool hasEmptySlot = false;
  for (size_t i = 0; i < slotsData; i++) {
    size_t slot = slotsData - (i + 1);
    if (_cachedHashes[slot] == 0) {
      hasEmptySlot = true;
      break;
    }
  }

  return !hasEmptySlot;
}

CachedValue* PlainBucket::find(std::uint32_t hash, void const* key,
                               std::size_t keySize, bool moveToFront) {
  TRI_ASSERT(isLocked());
  CachedValue* result = nullptr;

  for (std::size_t i = 0; i < slotsData; i++) {
    if (_cachedHashes[i] == 0) {
      break;
    }
    if (_cachedHashes[i] == hash && _cachedData[i]->sameKey(key, keySize)) {
      result = _cachedData[i];
      if (moveToFront) {
        moveSlot(i, true);
      }
      break;
    }
  }

  return result;
}

// requires there to be an open slot, otherwise will not be inserted
void PlainBucket::insert(std::uint32_t hash, CachedValue* value) {
  TRI_ASSERT(isLocked());
  for (std::size_t i = 0; i < slotsData; i++) {
    if (_cachedHashes[i] == 0) {
      // found an empty slot
      _cachedHashes[i] = hash;
      _cachedData[i] = value;
      if (i != 0) {
        moveSlot(i, true);
      }
      return;
    }
  }
}

CachedValue* PlainBucket::remove(std::uint32_t hash, void const* key, std::size_t keySize) {
  TRI_ASSERT(isLocked());
  CachedValue* value = find(hash, key, keySize, false);
  if (value != nullptr) {
    evict(value, false);
  }

  return value;
}

CachedValue* PlainBucket::evictionCandidate(bool ignoreRefCount) const {
  TRI_ASSERT(isLocked());
  for (std::size_t i = 0; i < slotsData; i++) {
    std::size_t slot = slotsData - (i + 1);
    if (_cachedData[slot] == nullptr) {
      continue;
    }
    if (ignoreRefCount || _cachedData[slot]->isFreeable()) {
      return _cachedData[slot];
    }
  }

  return nullptr;
}

void PlainBucket::evict(CachedValue* value, bool optimizeForInsertion) {
  TRI_ASSERT(isLocked());
  for (std::size_t i = 0; i < slotsData; i++) {
    std::size_t slot = slotsData - (i + 1);
    if (_cachedData[slot] == value) {
      // found a match
      _cachedHashes[slot] = 0;
      _cachedData[slot] = nullptr;
      moveSlot(slot, optimizeForInsertion);
      return;
    }
  }
}

void PlainBucket::clear() {
  TRI_ASSERT(isLocked());
  _state.clear();  // "clear" will keep the lock!

  for (std::size_t i = 0; i < slotsData; ++i) {
    _cachedHashes[i] = 0;
    _cachedData[i] = nullptr;
  }

  _state.unlock();
}

void PlainBucket::moveSlot(std::size_t slot, bool moveToFront) {
  TRI_ASSERT(isLocked());
  std::uint32_t hash = _cachedHashes[slot];
  CachedValue* value = _cachedData[slot];
  std::size_t i = slot;
  if (moveToFront) {
    // move slot to front
    for (; i >= 1; i--) {
      _cachedHashes[i] = _cachedHashes[i - 1];
      _cachedData[i] = _cachedData[i - 1];
    }
  } else {
    // move slot to back
    for (; (i < slotsData - 1) && (_cachedHashes[i + 1] != 0); i++) {
      _cachedHashes[i] = _cachedHashes[i + 1];
      _cachedData[i] = _cachedData[i + 1];
    }
  }
  _cachedHashes[i] = hash;
  _cachedData[i] = value;
}

}  // namespace arangodb::cache
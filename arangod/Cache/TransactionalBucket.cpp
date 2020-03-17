////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2017 ArangoDB GmbH, Cologne, Germany
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

#include "Cache/TransactionalBucket.h"

#include "Basics/Common.h"
#include "Basics/debugging.h"
#include "Cache/CachedValue.h"

namespace arangodb::cache {

TransactionalBucket::TransactionalBucket() {
  _state.lock();
  clear();
}

bool TransactionalBucket::lock(std::uint64_t maxTries) {
  return _state.lock(maxTries);
}

void TransactionalBucket::unlock() {
  TRI_ASSERT(isLocked());
  _state.unlock();
}

bool TransactionalBucket::isLocked() const { return _state.isLocked(); }

bool TransactionalBucket::isMigrated() const {
  TRI_ASSERT(isLocked());
  return _state.isSet(BucketState::Flag::migrated);
}

bool TransactionalBucket::isFullyBlacklisted() const {
  TRI_ASSERT(isLocked());
  return (haveOpenTransaction() && _state.isSet(BucketState::Flag::blacklisted));
}

bool TransactionalBucket::isFull() const {
  TRI_ASSERT(isLocked());
  bool hasEmptySlot = false;
  for (std::size_t i = 0; i < slotsData; i++) {
    std::size_t slot = slotsData - (i + 1);
    if (_cachedData[slot] == nullptr) {
      hasEmptySlot = true;
      break;
    }
  }

  return !hasEmptySlot;
}

CachedValue* TransactionalBucket::find(std::uint32_t hash, void const* key,
                                       std::size_t keySize, bool moveToFront) {
  TRI_ASSERT(isLocked());
  CachedValue* result = nullptr;

  for (std::size_t i = 0; i < slotsData; i++) {
    if (_cachedData[i] == nullptr) {
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

void TransactionalBucket::insert(std::uint32_t hash, CachedValue* value) {
  TRI_ASSERT(isLocked());
  TRI_ASSERT(!isBlacklisted(hash));  // checks needs to be done outside

  for (std::size_t i = 0; i < slotsData; i++) {
    if (_cachedData[i] == nullptr) {
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

CachedValue* TransactionalBucket::remove(std::uint32_t hash, void const* key,
                                         std::size_t keySize) {
  TRI_ASSERT(isLocked());
  CachedValue* value = find(hash, key, keySize, false);
  if (value != nullptr) {
    evict(value, false);
  }

  return value;
}

CachedValue* TransactionalBucket::blacklist(std::uint32_t hash, void const* key,
                                            std::size_t keySize) {
  TRI_ASSERT(isLocked());
  if (!haveOpenTransaction()) {
    return nullptr;
  }

  // remove key if it's here
  CachedValue* value = (keySize == 0) ? nullptr : remove(hash, key, keySize);

  if (isBlacklisted(hash)) {
    return value;
  }

  for (std::size_t i = 0; i < slotsBlacklist; i++) {
    if (_blacklistHashes[i] == 0) {
      // found an empty slot
      _blacklistHashes[i] = hash;
      return value;
    }
  }

  // no empty slot found, fully blacklist
  _state.toggleFlag(BucketState::Flag::blacklisted);
  return value;
}

bool TransactionalBucket::isBlacklisted(std::uint32_t hash) const {
  TRI_ASSERT(isLocked());
  if (!haveOpenTransaction()) {
    return false;
  }

  if (isFullyBlacklisted()) {
    return true;
  }

  bool blacklisted = false;
  for (std::size_t i = 0; i < slotsBlacklist; i++) {
    if (_blacklistHashes[i] == hash) {
      blacklisted = true;
      break;
    }
  }

  return blacklisted;
}

CachedValue* TransactionalBucket::evictionCandidate(bool ignoreRefCount) const {
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

void TransactionalBucket::evict(CachedValue* value, bool optimizeForInsertion) {
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

void TransactionalBucket::clear() {
  TRI_ASSERT(isLocked());
  _state.clear();  // "clear" will keep the lock!
  for (std::size_t i = 0; i < slotsBlacklist; ++i) {
    _blacklistHashes[i] = 0;
  }
  _blacklistTerm = 0;
  for (std::size_t i = 0; i < slotsData; ++i) {
    _cachedHashes[i] = 0;
    _cachedData[i] = nullptr;
  }
  _state.unlock();
}

void TransactionalBucket::updateBlacklistTerm(std::uint64_t term) {
  if (term > _blacklistTerm) {
    _blacklistTerm = term;

    if (isFullyBlacklisted()) {
      _state.toggleFlag(BucketState::Flag::blacklisted);
    }

    memset(_blacklistHashes, 0, (slotsBlacklist * sizeof(std::uint32_t)));
  }
}

void TransactionalBucket::moveSlot(std::size_t slot, bool moveToFront) {
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

bool TransactionalBucket::haveOpenTransaction() const {
  TRI_ASSERT(isLocked());
  // only have open transactions if term is odd
  return ((_blacklistTerm & static_cast<uint64_t>(1)) > 0);
}
}

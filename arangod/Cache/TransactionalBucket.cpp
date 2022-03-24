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
/// @author Dan Larkin-York
////////////////////////////////////////////////////////////////////////////////

#include <atomic>
#include <cstdint>

#include "Cache/TransactionalBucket.h"

#include "Basics/Common.h"
#include "Basics/debugging.h"
#include "Cache/BinaryKeyHasher.h"
#include "Cache/CachedValue.h"
#include "Cache/VPackKeyHasher.h"

namespace arangodb::cache {

TransactionalBucket::TransactionalBucket() noexcept {
  _state.lock();
  clear();
}

bool TransactionalBucket::lock(std::uint64_t maxTries) noexcept {
  return _state.lock(maxTries);
}

void TransactionalBucket::unlock() noexcept {
  TRI_ASSERT(isLocked());
  _state.unlock();
}

bool TransactionalBucket::isLocked() const noexcept {
  return _state.isLocked();
}

bool TransactionalBucket::isMigrated() const noexcept {
  TRI_ASSERT(isLocked());
  return _state.isSet(BucketState::Flag::migrated);
}

bool TransactionalBucket::isFullyBanished() const noexcept {
  TRI_ASSERT(isLocked());
  return (haveOpenTransaction() && _state.isSet(BucketState::Flag::banished));
}

bool TransactionalBucket::isFull() const noexcept {
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

template<typename Hasher>
CachedValue* TransactionalBucket::find(std::uint32_t hash, void const* key,
                                       std::size_t keySize,
                                       bool moveToFront) noexcept {
  TRI_ASSERT(isLocked());
  CachedValue* result = nullptr;

  for (std::size_t i = 0; i < slotsData; i++) {
    if (_cachedData[i] == nullptr) {
      break;
    }
    if (_cachedHashes[i] == hash &&
        Hasher::sameKey(_cachedData[i]->key(), _cachedData[i]->keySize(), key,
                        keySize)) {
      result = _cachedData[i];
      if (moveToFront && i != 0) {
        moveSlot(i, true);
      }
      break;
    }
  }

  return result;
}

void TransactionalBucket::insert(std::uint32_t hash,
                                 CachedValue* value) noexcept {
  TRI_ASSERT(isLocked());
  TRI_ASSERT(!isBanished(hash));  // checks needs to be done outside

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

template<typename Hasher>
CachedValue* TransactionalBucket::remove(std::uint32_t hash, void const* key,
                                         std::size_t keySize) noexcept {
  TRI_ASSERT(isLocked());
  CachedValue* result = nullptr;

  for (std::size_t i = 0; i < slotsData; i++) {
    if (_cachedData[i] == nullptr) {
      break;
    }
    if (_cachedHashes[i] == hash &&
        Hasher::sameKey(_cachedData[i]->key(), _cachedData[i]->keySize(), key,
                        keySize)) {
      result = _cachedData[i];
      _cachedHashes[i] = 0;
      _cachedData[i] = nullptr;
      moveSlot(i, false);
      break;
    }
  }

  return result;
}

template<typename Hasher>
CachedValue* TransactionalBucket::banish(std::uint32_t hash, void const* key,
                                         std::size_t keySize) noexcept {
  TRI_ASSERT(isLocked());
  if (!haveOpenTransaction()) {
    return nullptr;
  }

  // remove key if it's here
  CachedValue* value =
      (keySize == 0) ? nullptr : remove<Hasher>(hash, key, keySize);

  if (isBanished(hash)) {
    return value;
  }

  for (std::size_t i = 0; i < slotsBanish; i++) {
    if (_banishHashes[i] == 0) {
      // found an empty slot
      _banishHashes[i] = hash;
      return value;
    }
  }

  // no empty slot found, fully banish
  _state.toggleFlag(BucketState::Flag::banished);
  return value;
}

bool TransactionalBucket::isBanished(std::uint32_t hash) const noexcept {
  TRI_ASSERT(isLocked());
  if (!haveOpenTransaction()) {
    return false;
  }

  if (isFullyBanished()) {
    return true;
  }

  bool banished = false;
  for (std::size_t i = 0; i < slotsBanish; i++) {
    if (_banishHashes[i] == hash) {
      banished = true;
      break;
    }
  }

  return banished;
}

std::uint64_t TransactionalBucket::evictCandidate() noexcept {
  TRI_ASSERT(isLocked());
  for (std::size_t i = 0; i < slotsData; i++) {
    std::size_t slot = slotsData - (i + 1);
    if (_cachedData[slot] == nullptr) {
      continue;
    }
    if (_cachedData[slot]->isFreeable()) {
      std::uint64_t size = _cachedData[slot]->size();
      // evict value. we checked that it is freeable
      delete _cachedData[slot];
      _cachedHashes[slot] = 0;
      _cachedData[slot] = nullptr;
      moveSlot(slot, true);
      return size;
    }
  }

  // nothing evicted
  return 0;
}

CachedValue* TransactionalBucket::evictionCandidate(
    bool ignoreRefCount) const noexcept {
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

void TransactionalBucket::evict(CachedValue* value,
                                bool optimizeForInsertion) noexcept {
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

void TransactionalBucket::clear() noexcept {
  TRI_ASSERT(isLocked());
  _state.clear();  // "clear" will keep the lock!
  for (std::size_t i = 0; i < slotsBanish; ++i) {
    _banishHashes[i] = 0;
  }
  _banishTerm = 0;
  for (std::size_t i = 0; i < slotsData; ++i) {
    _cachedHashes[i] = 0;
    _cachedData[i] = nullptr;
  }
  _state.unlock();
}

void TransactionalBucket::updateBanishTerm(std::uint64_t term) noexcept {
  if (term > _banishTerm) {
    _banishTerm = term;

    if (isFullyBanished()) {
      _state.toggleFlag(BucketState::Flag::banished);
    }

    memset(_banishHashes, 0, (slotsBanish * sizeof(std::uint32_t)));
  }
}

void TransactionalBucket::moveSlot(std::size_t slot,
                                   bool moveToFront) noexcept {
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

bool TransactionalBucket::haveOpenTransaction() const noexcept {
  TRI_ASSERT(isLocked());
  // only have open transactions if term is odd
  return ((_banishTerm & static_cast<uint64_t>(1)) > 0);
}

template CachedValue* TransactionalBucket::find<BinaryKeyHasher>(
    std::uint32_t hash, void const* key, std::size_t keySize,
    bool moveToFront) noexcept;

template CachedValue* TransactionalBucket::remove<BinaryKeyHasher>(
    std::uint32_t hash, void const* key, std::size_t keySize) noexcept;

template CachedValue* TransactionalBucket::banish<BinaryKeyHasher>(
    std::uint32_t hash, void const* key, std::size_t keySize) noexcept;

template CachedValue* TransactionalBucket::find<VPackKeyHasher>(
    std::uint32_t hash, void const* key, std::size_t keySize,
    bool moveToFront) noexcept;

template CachedValue* TransactionalBucket::remove<VPackKeyHasher>(
    std::uint32_t hash, void const* key, std::size_t keySize) noexcept;

template CachedValue* TransactionalBucket::banish<VPackKeyHasher>(
    std::uint32_t hash, void const* key, std::size_t keySize) noexcept;

}  // namespace arangodb::cache

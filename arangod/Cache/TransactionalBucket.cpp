////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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

TransactionalBucket::TransactionalBucket() noexcept : _slotsUsed(0) {
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
  return _slotsUsed == kSlotsData;
}

template<typename Hasher>
CachedValue* TransactionalBucket::find(std::uint32_t hash, void const* key,
                                       std::size_t keySize,
                                       bool moveToFront) noexcept {
  TRI_ASSERT(isLocked());
  CachedValue* result = nullptr;

  // check from the front, so more frequently accessed items are found quicker
  for (std::size_t slot = 0; slot < _slotsUsed; ++slot) {
    if (_cachedHashes[slot] == hash &&
        Hasher::sameKey(_cachedData[slot]->key(), _cachedData[slot]->keySize(),
                        key, keySize)) {
      result = _cachedData[slot];
      if (moveToFront) {
        moveSlotToFront(slot);
        checkInvariants();
      }
      break;
    }
  }

  return result;
}

void TransactionalBucket::insert(std::uint32_t hash,
                                 CachedValue* value) noexcept {
  TRI_ASSERT(isLocked());
  TRI_ASSERT(!isBanished(hash));  // check needs to be done outside

  if (_slotsUsed < kSlotsData) {
    // found an empty slot.
    // insert at the end
    TRI_ASSERT(_cachedData[_slotsUsed] == nullptr);
    _cachedHashes[_slotsUsed] = hash;
    _cachedData[_slotsUsed] = value;
    if (_slotsUsed != 0) {
      moveSlotToFront(_slotsUsed);
    }
    ++_slotsUsed;
    TRI_ASSERT(_slotsUsed <= kSlotsData);
    checkInvariants();
  }
}

template<typename Hasher>
CachedValue* TransactionalBucket::remove(std::uint32_t hash, void const* key,
                                         std::size_t keySize) noexcept {
  TRI_ASSERT(isLocked());
  CachedValue* result = nullptr;

  // check from the front to the back. the order does not really
  // matter, as we have no idea where the to-be-removed item is.
  for (std::size_t slot = 0; slot < _slotsUsed; ++slot) {
    if (_cachedHashes[slot] == hash &&
        Hasher::sameKey(_cachedData[slot]->key(), _cachedData[slot]->keySize(),
                        key, keySize)) {
      result = _cachedData[slot];
      closeGap(slot);
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

  for (std::size_t slot = 0; slot < kSlotsBanish; ++slot) {
    if (_banishHashes[slot] == 0) {
      // found an empty slot
      _banishHashes[slot] = hash;
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
  for (std::size_t slot = 0; slot < kSlotsBanish; ++slot) {
    if (_banishHashes[slot] == hash) {
      banished = true;
      break;
    }
  }

  return banished;
}

std::uint64_t TransactionalBucket::evictCandidate() noexcept {
  TRI_ASSERT(isLocked());
  // try to find a freeable slot from the back.
  std::size_t slot = _slotsUsed;
  while (slot-- > 0) {
    TRI_ASSERT(_cachedData[slot] != nullptr);
    if (!_cachedData[slot]->isFreeable()) {
      continue;
    }

    std::uint64_t size = _cachedData[slot]->size();
    // evict value. we checked that it is freeable
    delete _cachedData[slot];
    closeGap(slot);
    return size;
  }

  // nothing evicted
  return 0;
}

CachedValue* TransactionalBucket::evictionCandidate() const noexcept {
  TRI_ASSERT(isLocked());
  // try to find a freeable slot from the back.
  std::size_t slot = _slotsUsed;
  while (slot-- > 0) {
    TRI_ASSERT(_cachedData[slot] != nullptr);
    if (_cachedData[slot]->isFreeable()) {
      return _cachedData[slot];
    }
  }

  return nullptr;
}

void TransactionalBucket::evict(CachedValue* value) noexcept {
  TRI_ASSERT(isLocked());
  for (std::size_t slot = 0; slot < _slotsUsed; ++slot) {
    if (_cachedData[slot] == value) {
      // found a match
      closeGap(slot);
      return;
    }
  }
}

void TransactionalBucket::clear() noexcept {
  TRI_ASSERT(isLocked());
  _state.clear();  // "clear" will keep the lock!
                   //
  for (std::size_t slot = 0; slot < kSlotsBanish; ++slot) {
    _banishHashes[slot] = 0;
  }
  _banishTerm = 0;
  for (std::size_t slot = 0; slot < kSlotsData; ++slot) {
    _cachedHashes[slot] = 0;
    _cachedData[slot] = nullptr;
  }
  _slotsUsed = 0;
  checkInvariants();

  _state.unlock();
}

void TransactionalBucket::updateBanishTerm(std::uint64_t term) noexcept {
  if (term > _banishTerm) {
    _banishTerm = term;

    if (isFullyBanished()) {
      _state.toggleFlag(BucketState::Flag::banished);
    }

    for (std::size_t slot = 0; slot < kSlotsBanish; ++slot) {
      _banishHashes[slot] = 0;
    }
  }
}

void TransactionalBucket::closeGap(std::size_t slot) noexcept {
  _cachedHashes[slot] = _cachedHashes[_slotsUsed - 1];
  _cachedData[slot] = _cachedData[_slotsUsed - 1];
  _cachedHashes[_slotsUsed - 1] = 0;
  _cachedData[_slotsUsed - 1] = nullptr;
  TRI_ASSERT(_slotsUsed > 0);
  --_slotsUsed;
  checkInvariants();
}

void TransactionalBucket::moveSlotToFront(std::size_t slot) noexcept {
  TRI_ASSERT(isLocked());
  std::uint32_t hash = _cachedHashes[slot];
  CachedValue* value = _cachedData[slot];
  // move slot to front
  for (; slot >= 1; slot--) {
    _cachedHashes[slot] = _cachedHashes[slot - 1];
    _cachedData[slot] = _cachedData[slot - 1];
  }
  _cachedHashes[slot] = hash;
  _cachedData[slot] = value;
}

bool TransactionalBucket::haveOpenTransaction() const noexcept {
  TRI_ASSERT(isLocked());
  // only have open transactions if term is odd
  return ((_banishTerm & static_cast<uint64_t>(1)) > 0);
}

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
void TransactionalBucket::checkInvariants() const noexcept {
#if 1
  // check code can be disabled when it takes too long during testing.
  TRI_ASSERT(_slotsUsed <= kSlotsData);
  for (std::size_t slot = 0; slot < kSlotsData; ++slot) {
    if (slot < _slotsUsed) {
      TRI_ASSERT(_cachedHashes[slot] != 0);
      TRI_ASSERT(_cachedData[slot] != nullptr);
    } else {
      TRI_ASSERT(_cachedHashes[slot] == 0);
      TRI_ASSERT(_cachedData[slot] == nullptr);
    }
  }
#endif
}
#endif

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

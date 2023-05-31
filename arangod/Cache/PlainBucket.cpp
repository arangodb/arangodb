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

#include <cstdint>

#include "Cache/PlainBucket.h"

#include "Basics/Common.h"
#include "Basics/debugging.h"
#include "Cache/BinaryKeyHasher.h"
#include "Cache/CachedValue.h"
#include "Cache/VPackKeyHasher.h"

namespace arangodb::cache {

PlainBucket::PlainBucket() noexcept : _slotsUsed(0) {
  _state.lock();
  clear();
}

bool PlainBucket::lock(std::uint64_t maxTries) noexcept {
  return _state.lock(maxTries);
}

void PlainBucket::unlock() noexcept {
  TRI_ASSERT(_state.isLocked());
  _state.unlock();
}

bool PlainBucket::isLocked() const noexcept { return _state.isLocked(); }

bool PlainBucket::isMigrated() const noexcept {
  TRI_ASSERT(isLocked());
  return _state.isSet(BucketState::Flag::migrated);
}

bool PlainBucket::isFull() const noexcept {
  TRI_ASSERT(isLocked());
  return _slotsUsed == kSlotsData;
}

template<typename Hasher>
CachedValue* PlainBucket::find(std::uint32_t hash, void const* key,
                               std::size_t keySize, bool moveToFront) noexcept {
  TRI_ASSERT(isLocked());
  CachedValue* result = nullptr;

  for (std::size_t i = 0; i < _slotsUsed; i++) {
    TRI_ASSERT(_cachedData[i] != nullptr);

    if (_cachedHashes[i] == hash &&
        Hasher::sameKey(_cachedData[i]->key(), _cachedData[i]->keySize(), key,
                        keySize)) {
      result = _cachedData[i];
      if (moveToFront) {
        moveSlot(i);
        checkInvariants();
      }
      break;
    }
  }

  return result;
}

// requires there to be an open slot, otherwise will not be inserted
void PlainBucket::insert(std::uint32_t hash, CachedValue* value) noexcept {
  TRI_ASSERT(isLocked());
  if (_slotsUsed < kSlotsData) {
    // found an empty slot
    TRI_ASSERT(_cachedData[_slotsUsed] == nullptr);
    _cachedHashes[_slotsUsed] = hash;
    _cachedData[_slotsUsed] = value;
    if (_slotsUsed != 0) {
      moveSlot(_slotsUsed);
    }
    ++_slotsUsed;
    TRI_ASSERT(_slotsUsed <= kSlotsData);
    checkInvariants();
  }
}

template<typename Hasher>
CachedValue* PlainBucket::remove(std::uint32_t hash, void const* key,
                                 std::size_t keySize) noexcept {
  TRI_ASSERT(isLocked());
  CachedValue* result = nullptr;

  for (std::size_t i = 0; i < _slotsUsed; i++) {
    if (_cachedHashes[i] == hash &&
        Hasher::sameKey(_cachedData[i]->key(), _cachedData[i]->keySize(), key,
                        keySize)) {
      result = _cachedData[i];
      _cachedHashes[i] = _cachedHashes[_slotsUsed - 1];
      _cachedData[i] = _cachedData[_slotsUsed - 1];
      _cachedHashes[_slotsUsed - 1] = 0;
      _cachedData[_slotsUsed - 1] = nullptr;
      TRI_ASSERT(_slotsUsed > 0);
      --_slotsUsed;
      checkInvariants();
      break;
    }
  }

  return result;
}

std::uint64_t PlainBucket::evictCandidate() noexcept {
  TRI_ASSERT(isLocked());
  std::size_t slot = _slotsUsed;
  while (slot-- > 0) {
    TRI_ASSERT(_cachedData[slot] != nullptr);
    if (!_cachedData[slot]->isFreeable()) {
      continue;
    }

    std::uint64_t size = _cachedData[slot]->size();
    // evict value. we checked that it is freeable
    delete _cachedData[slot];
    _cachedHashes[slot] = _cachedHashes[_slotsUsed - 1];
    _cachedData[slot] = _cachedData[_slotsUsed - 1];
    _cachedHashes[_slotsUsed - 1] = 0;
    _cachedData[_slotsUsed - 1] = nullptr;
    TRI_ASSERT(_slotsUsed > 0);
    --_slotsUsed;
    checkInvariants();
    return size;
  }

  // nothing evicted
  return 0;
}

CachedValue* PlainBucket::evictionCandidate() const noexcept {
  TRI_ASSERT(isLocked());
  std::size_t slot = _slotsUsed;
  while (slot-- > 0) {
    TRI_ASSERT(_cachedData[slot] != nullptr);
    if (_cachedData[slot]->isFreeable()) {
      return _cachedData[slot];
    }
  }

  return nullptr;
}

void PlainBucket::evict(CachedValue* value) noexcept {
  TRI_ASSERT(isLocked());
  for (std::size_t i = 0; i < _slotsUsed; i++) {
    if (_cachedData[i] == value) {
      // found a match
      _cachedHashes[i] = _cachedHashes[_slotsUsed - 1];
      _cachedData[i] = _cachedData[_slotsUsed - 1];
      _cachedHashes[_slotsUsed - 1] = 0;
      _cachedData[_slotsUsed - 1] = nullptr;
      TRI_ASSERT(_slotsUsed > 0);
      --_slotsUsed;
      checkInvariants();
      return;
    }
  }
}

void PlainBucket::clear() noexcept {
  TRI_ASSERT(isLocked());
  _state.clear();  // "clear" will keep the lock!

  for (std::size_t i = 0; i < kSlotsData; ++i) {
    _cachedHashes[i] = 0;
    _cachedData[i] = nullptr;
  }
  _slotsUsed = 0;
  checkInvariants();

  _state.unlock();
}

void PlainBucket::moveSlot(std::size_t slot) noexcept {
  TRI_ASSERT(isLocked());
  std::uint32_t hash = _cachedHashes[slot];
  CachedValue* value = _cachedData[slot];
  std::size_t i = slot;
  // move slot to front
  for (; i >= 1; i--) {
    TRI_ASSERT(_cachedData[i - 1] != nullptr);
    _cachedHashes[i] = _cachedHashes[i - 1];
    _cachedData[i] = _cachedData[i - 1];
  }
  _cachedHashes[i] = hash;
  _cachedData[i] = value;
}

void PlainBucket::checkInvariants() const noexcept {
#if 0
  // intentionally disabled. can be reenabled when there
  // is need for debugging.
  TRI_ASSERT(_slotsUsed <= kSlotsData);
  for (std::size_t i = 0; i < kSlotsData; ++i) {
    if (i < _slotsUsed) {
      TRI_ASSERT(_cachedHashes[i] != 0);
      TRI_ASSERT(_cachedData[i] != nullptr);
    } else {
      TRI_ASSERT(_cachedHashes[i] == 0);
      TRI_ASSERT(_cachedData[i] == nullptr);
    }
  }
#endif
}

template CachedValue* PlainBucket::find<BinaryKeyHasher>(
    std::uint32_t hash, void const* key, std::size_t keySize,
    bool moveToFront) noexcept;

template CachedValue* PlainBucket::remove<BinaryKeyHasher>(
    std::uint32_t hash, void const* key, std::size_t keySize) noexcept;

template CachedValue* PlainBucket::find<VPackKeyHasher>(
    std::uint32_t hash, void const* key, std::size_t keySize,
    bool moveToFront) noexcept;

template CachedValue* PlainBucket::remove<VPackKeyHasher>(
    std::uint32_t hash, void const* key, std::size_t keySize) noexcept;

}  // namespace arangodb::cache

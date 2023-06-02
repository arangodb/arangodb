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

  // check from the front, so more frequently accessed items are found quicker
  for (std::size_t slot = 0; slot < _slotsUsed; ++slot) {
    TRI_ASSERT(_cachedData[slot] != nullptr);

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

// requires there to be an open slot, otherwise will not be inserted
void PlainBucket::insert(std::uint32_t hash, CachedValue* value) noexcept {
  TRI_ASSERT(isLocked());
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
CachedValue* PlainBucket::remove(std::uint32_t hash, void const* key,
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

std::uint64_t PlainBucket::evictCandidate() noexcept {
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

CachedValue* PlainBucket::evictionCandidate() const noexcept {
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

void PlainBucket::evict(CachedValue* value) noexcept {
  TRI_ASSERT(isLocked());
  for (std::size_t slot = 0; slot < _slotsUsed; ++slot) {
    if (_cachedData[slot] == value) {
      // found a match
      closeGap(slot);
      return;
    }
  }
}

void PlainBucket::clear() noexcept {
  TRI_ASSERT(isLocked());
  _state.clear();  // "clear" will keep the lock!

  _slotsUsed = 0;
  for (std::size_t i = 0; i < kSlotsData; ++i) {
    _cachedHashes[i] = 0;
  }
  for (std::size_t i = 0; i < kSlotsData; ++i) {
    _cachedData[i] = nullptr;
  }
  checkInvariants();

  _state.unlock();
}

void PlainBucket::closeGap(std::size_t slot) noexcept {
  _cachedHashes[slot] = _cachedHashes[_slotsUsed - 1];
  _cachedData[slot] = _cachedData[_slotsUsed - 1];
  _cachedHashes[_slotsUsed - 1] = 0;
  _cachedData[_slotsUsed - 1] = nullptr;
  TRI_ASSERT(_slotsUsed > 0);
  --_slotsUsed;
  checkInvariants();
}

void PlainBucket::moveSlotToFront(std::size_t slot) noexcept {
  TRI_ASSERT(isLocked());
  std::uint32_t hash = _cachedHashes[slot];
  CachedValue* value = _cachedData[slot];
  // move slot to front
  for (; slot >= 1; slot--) {
    TRI_ASSERT(_cachedData[slot - 1] != nullptr);
    _cachedHashes[slot] = _cachedHashes[slot - 1];
    _cachedData[slot] = _cachedData[slot - 1];
  }
  TRI_ASSERT(slot == 0);
  _cachedHashes[0] = hash;
  _cachedData[0] = value;
}

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
void PlainBucket::checkInvariants() const noexcept {
#if 1
  // this invariants check is intentionally here, so it is executed
  // during testing. if it turns out to be too slow, if can be disabled
  // or removed.
  // it is not compiled in non-maintainer mode, so it does not affect
  // the performance of release builds.
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

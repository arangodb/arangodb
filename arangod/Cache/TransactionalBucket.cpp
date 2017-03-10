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
/// @author Daniel H. Larkin
////////////////////////////////////////////////////////////////////////////////

#include "Cache/TransactionalBucket.h"
#include "Basics/Common.h"
#include "Cache/CachedValue.h"
#include "Cache/State.h"

#include <stdint.h>
#include <atomic>

using namespace arangodb::cache;

size_t TransactionalBucket::SLOTS_DATA = 3;
size_t TransactionalBucket::SLOTS_BLACKLIST = 4;

TransactionalBucket::TransactionalBucket() {
  _state.lock();
  clear();
}

bool TransactionalBucket::lock(uint64_t transactionTerm, int64_t maxTries) {
  return _state.lock(maxTries, [this, transactionTerm]() -> void {
    updateBlacklistTerm(transactionTerm);
  });
}

void TransactionalBucket::unlock() {
  TRI_ASSERT(isLocked());
  _state.unlock();
}

bool TransactionalBucket::isLocked() const { return _state.isLocked(); }

bool TransactionalBucket::isMigrated() const {
  TRI_ASSERT(isLocked());
  return _state.isSet(State::Flag::migrated);
}

bool TransactionalBucket::isFullyBlacklisted() const {
  TRI_ASSERT(isLocked());
  return (haveOpenTransaction() && _state.isSet(State::Flag::blacklisted));
}

bool TransactionalBucket::isFull() const {
  TRI_ASSERT(isLocked());
  bool hasEmptySlot = false;
  for (size_t i = 0; i < SLOTS_DATA; i++) {
    size_t slot = SLOTS_DATA - (i + 1);
    if (_cachedHashes[slot] == 0) {
      hasEmptySlot = true;
      break;
    }
  }

  return !hasEmptySlot;
}

CachedValue* TransactionalBucket::find(uint32_t hash, void const* key,
                                       uint32_t keySize, bool moveToFront) {
  TRI_ASSERT(isLocked());
  CachedValue* result = nullptr;

  for (size_t i = 0; i < SLOTS_DATA; i++) {
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

void TransactionalBucket::insert(uint32_t hash, CachedValue* value) {
  TRI_ASSERT(isLocked());
  if (isBlacklisted(hash)) {
    return;
  }

  for (size_t i = 0; i < SLOTS_DATA; i++) {
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

CachedValue* TransactionalBucket::remove(uint32_t hash, void const* key,
                                         uint32_t keySize) {
  TRI_ASSERT(isLocked());
  CachedValue* value = find(hash, key, keySize, false);
  if (value != nullptr) {
    evict(value, false);
  }

  return value;
}

CachedValue* TransactionalBucket::blacklist(uint32_t hash, void const* key,
                                            uint32_t keySize) {
  TRI_ASSERT(isLocked());
  if (!haveOpenTransaction()) {
    return nullptr;
  }

  // remove key if it's here
  CachedValue* value = (keySize == 0) ? nullptr : remove(hash, key, keySize);

  if (isBlacklisted(hash)) {
    return value;
  }

  for (size_t i = 0; i < SLOTS_BLACKLIST; i++) {
    if (_blacklistHashes[i] == 0) {
      // found an empty slot
      _blacklistHashes[i] = hash;
      return value;
    }
  }

  // no empty slot found, fully blacklist
  _state.toggleFlag(State::Flag::blacklisted);
  return value;
}

bool TransactionalBucket::isBlacklisted(uint32_t hash) const {
  TRI_ASSERT(isLocked());
  if (!haveOpenTransaction()) {
    return false;
  }

  if (isFullyBlacklisted()) {
    return true;
  }

  bool blacklisted = false;
  for (size_t i = 0; i < SLOTS_BLACKLIST; i++) {
    if (_blacklistHashes[i] == hash) {
      blacklisted = true;
      break;
    }
  }

  return blacklisted;
}

CachedValue* TransactionalBucket::evictionCandidate(bool ignoreRefCount) const {
  TRI_ASSERT(isLocked());
  for (size_t i = 0; i < SLOTS_DATA; i++) {
    size_t slot = SLOTS_DATA - (i + 1);
    if (_cachedHashes[slot] == 0) {
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
  for (size_t i = 0; i < SLOTS_DATA; i++) {
    size_t slot = SLOTS_DATA - (i + 1);
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
  memset(this, 0, sizeof(TransactionalBucket));
}

void TransactionalBucket::updateBlacklistTerm(uint64_t term) {
  if (term > _blacklistTerm) {
    _blacklistTerm = term;

    if (isFullyBlacklisted()) {
      _state.toggleFlag(State::Flag::blacklisted);
    }

    memset(_blacklistHashes, 0, (SLOTS_BLACKLIST * sizeof(uint32_t)));
  }
}

void TransactionalBucket::moveSlot(size_t slot, bool moveToFront) {
  TRI_ASSERT(isLocked());
  uint32_t hash = _cachedHashes[slot];
  CachedValue* value = _cachedData[slot];
  size_t i = slot;
  if (moveToFront) {
    // move slot to front
    for (; i >= 1; i--) {
      _cachedHashes[i] = _cachedHashes[i - 1];
      _cachedData[i] = _cachedData[i - 1];
    }
  } else {
    // move slot to back
    for (; (i < SLOTS_DATA - 1) && (_cachedHashes[i + 1] != 0); i++) {
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
  return ((_blacklistTerm & 1ULL) > 0);
}

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

#ifndef ARANGODB_CACHE_TRANSACTIONAL_BUCKET_H
#define ARANGODB_CACHE_TRANSACTIONAL_BUCKET_H

#include <atomic>
#include <cstddef>
#include <cstdint>

#include "Cache/BucketState.h"
#include "Cache/CachedValue.h"
#include "Cache/Common.h"

namespace arangodb {
namespace cache {

////////////////////////////////////////////////////////////////////////////////
/// @brief Bucket structure for TransactionalCache.
///
/// Contains, a State variable, three slots each for hashes and data pointers,
/// four slots for blacklisted hashes, and the applicable transaction term. Most
/// querying and manipulation can be handled via the exposed methods. Bucket
/// must be locked before doing anything else to ensure proper synchronization.
/// Data entries are carefully laid out to ensure the structure fits in a single
/// cacheline.
////////////////////////////////////////////////////////////////////////////////
struct TransactionalBucket {
  BucketState _state;

  // blacklist entries for transactional semantics
  static constexpr std::size_t slotsBlacklist = 5;
  std::uint32_t _blacklistHashes[slotsBlacklist];
  std::uint64_t _blacklistTerm;

  // actual cached entries
  static constexpr std::size_t slotsData = 8;
  std::uint32_t _cachedHashes[slotsData];
  CachedValue* _cachedData[slotsData];

// padding, if necessary?
#ifdef TRI_PADDING_32
  uint32_t _padding[slotsData];
#endif

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Initialize an empty bucket.
  //////////////////////////////////////////////////////////////////////////////
  TransactionalBucket();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Attempt to lock bucket (failing after maxTries attempts).
  //////////////////////////////////////////////////////////////////////////////
  bool lock(std::uint64_t maxTries);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Unlock the bucket. Requires bucket to be locked.
  //////////////////////////////////////////////////////////////////////////////
  void unlock();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Checks whether the bucket is locked.
  //////////////////////////////////////////////////////////////////////////////
  bool isLocked() const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Checks whether the bucket has been migrated. Requires state to be
  /// locked.
  //////////////////////////////////////////////////////////////////////////////
  bool isMigrated() const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Checks whether bucket has been fully blacklisted. Requires state to
  /// be locked.
  //////////////////////////////////////////////////////////////////////////////
  bool isFullyBlacklisted() const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Checks whether bucket is full. Requires state to be locked.
  //////////////////////////////////////////////////////////////////////////////
  bool isFull() const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Looks up a given key and returns associated value. Requires state
  /// to be locked.
  ///
  /// Takes an input hash and key (specified by pointer and size), and searches
  /// the bucket for a matching entry. If a matching entry is found, it is
  /// returned. By default, a matching entry will be moved to the front of the
  /// bucket to allow basic LRU semantics. If no matching entry is found,
  /// nothing will be changed and a nullptr will be returned.
  //////////////////////////////////////////////////////////////////////////////
  CachedValue* find(std::uint32_t hash, void const* key, std::size_t keySize,
                    bool moveToFront = true);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Inserts a given value if it is not blacklisted. Requires state to
  /// be locked.
  ///
  /// Requires that the bucket is not full and does not already contain an item
  /// with the same key. If it is full, the item will not be inserted. If an
  /// item with the same key exists, this is not detected but it is likely to
  /// produce bugs later on down the line. If the item's hash has been
  /// blacklisted, or the bucket is fully blacklisted, insertion will simply do
  /// nothing. When inserting, the item is put into the first empty slot, then
  /// moved to the front. If attempting to insert and the bucket is full, the
  /// user should evict an item and specify the optimizeForInsertion flag to be
  /// true.
  //////////////////////////////////////////////////////////////////////////////
  void insert(std::uint32_t hash, CachedValue* value);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Removes an item with the given key if one exists. Requires state to
  /// be locked.
  ///
  /// Search for a matching key. If none exists, do nothing and return a
  /// nullptr. If one exists, remove it from the bucket and return the pointer
  /// to the value. Upon removal, the empty slot generated is moved to the back
  /// of the bucket (to remove the gap).
  //////////////////////////////////////////////////////////////////////////////
  CachedValue* remove(std::uint32_t hash, void const* key, std::size_t keySize);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Blacklists a key and removes it if it exists. Requires state to
  /// be locked.
  ///
  /// Search for a matching key. If one exists, remove it. Then blacklist the
  /// hash associated with the key. If there are no empty blacklist slots, fully
  /// blacklist the bucket.
  //////////////////////////////////////////////////////////////////////////////
  CachedValue* blacklist(std::uint32_t hash, void const* key, std::size_t keySize);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Checks whether a given hash is blacklisted. Requires state to be
  /// locked.
  //////////////////////////////////////////////////////////////////////////////
  bool isBlacklisted(std::uint32_t hash) const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Searches for the best candidate in the bucket to evict. Requires
  /// state to be locked.
  ///
  /// Usually returns a pointer to least recently used freeable value. If the
  /// bucket contains no values or all have outstanding references, then it
  /// returns nullptr. In the case that ignoreRefCount is set to true, then it
  /// simply returns the least recently used value, regardless of freeability.
  //////////////////////////////////////////////////////////////////////////////
  CachedValue* evictionCandidate(bool ignoreRefCount = false) const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Evicts the given value from the bucket. Requires state to be
  /// locked.
  ///
  /// By default, it will move the empty slot to the back of the bucket. If
  /// preparing an empty slot for insertion, specify the second parameter to be
  /// true. This will move the empty slot to the front instead.
  //////////////////////////////////////////////////////////////////////////////
  void evict(CachedValue* value, bool optimizeForInsertion = false);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Updates the bucket's blacklist term. Requires state to be locked.
  //////////////////////////////////////////////////////////////////////////////
  void updateBlacklistTerm(std::uint64_t term);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Reinitializes a bucket to be completely empty and unlocked.
  /// Requires state to be locked.
  //////////////////////////////////////////////////////////////////////////////
  void clear();

 private:
  void moveSlot(std::size_t slot, bool moveToFront);
  bool haveOpenTransaction() const;
};

// ensure that TransactionalBucket is exactly BUCKET_SIZE
static_assert(sizeof(TransactionalBucket) == BUCKET_SIZE,
              "Expected sizeof(TransactionalBucket) == BUCKET_SIZE.");

};  // end namespace cache
};  // end namespace arangodb

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for arangodb::cache::TransactionalBucket
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2017, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include <cstdint>
#include <string>

#include "Basics/debugging.h"
#include "Cache/TransactionalBucket.h"

using namespace arangodb::cache;

TEST(CacheTransactionalBucketTest, test_locking_behavior) {
  auto bucket = std::make_unique<TransactionalBucket>();
  bool success;

  // check lock without contention
  ASSERT_FALSE(bucket->isLocked());
  success = bucket->lock(-1LL);
  ASSERT_TRUE(success);
  ASSERT_TRUE(bucket->isLocked());

  // check lock with contention
  success = bucket->lock(10LL);
  ASSERT_FALSE(success);
  ASSERT_TRUE(bucket->isLocked());

  // check unlock
  bucket->unlock();
  ASSERT_FALSE(bucket->isLocked());

  // check that blacklist term is updated appropriately
  ASSERT_EQ(0ULL, bucket->_blacklistTerm);
  bucket->lock(-1LL);
  bucket->updateBlacklistTerm(1ULL);
  ASSERT_EQ(1ULL, bucket->_blacklistTerm);
  bucket->unlock();
  ASSERT_EQ(1ULL, bucket->_blacklistTerm);
}

TEST(CacheTransactionalBucketTest, verify_that_insertion_works_as_expected) {
  auto bucket = std::make_unique<TransactionalBucket>();
  bool success;

  std::uint32_t hashes[9] = {1, 2, 3, 4, 5,
                             6, 7, 8, 9};  // don't have to be real, but should be unique and non-zero
  std::uint64_t keys[9] = {0, 1, 2, 3, 4, 5, 6, 7, 8};
  std::uint64_t values[9] = {0, 1, 2, 3, 4, 5, 6, 7, 8};
  CachedValue* ptrs[9];
  for (std::size_t i = 0; i < 9; i++) {
    ptrs[i] = CachedValue::construct(&(keys[i]), sizeof(std::uint64_t),
                                     &(values[i]), sizeof(std::uint64_t));
    TRI_ASSERT(ptrs[i] != nullptr);
  }

  success = bucket->lock(-1LL);
  ASSERT_TRUE(success);

  // insert three to fill
  ASSERT_FALSE(bucket->isFull());
  for (std::size_t i = 0; i < 8; i++) {
    bucket->insert(hashes[i], ptrs[i]);
    if (i < 7) {
      ASSERT_FALSE(bucket->isFull());
    } else {
      ASSERT_TRUE(bucket->isFull());
    }
  }
  for (std::size_t i = 0; i < 7; i++) {
    CachedValue* res = bucket->find(hashes[i], ptrs[i]->key(), ptrs[i]->keySize());
    ASSERT_EQ(res, ptrs[i]);
  }

  // check that insert is ignored if full
  bucket->insert(hashes[8], ptrs[8]);
  CachedValue* res = bucket->find(hashes[8], ptrs[8]->key(), ptrs[8]->keySize());
  ASSERT_EQ(nullptr, res);

  bucket->unlock();

  // cleanup
  for (std::size_t i = 0; i < 9; i++) {
    delete ptrs[i];
  }
}

TEST(CacheTransactionalBucketTest, verify_that_removal_works_as_expected) {
  auto bucket = std::make_unique<TransactionalBucket>();
  bool success;

  std::uint32_t hashes[3] = {1, 2, 3};  // don't have to be real, but should be unique and non-zero
  std::uint64_t keys[3] = {0, 1, 2};
  std::uint64_t values[3] = {0, 1, 2};
  CachedValue* ptrs[3];
  for (std::size_t i = 0; i < 3; i++) {
    ptrs[i] = CachedValue::construct(&(keys[i]), sizeof(std::uint64_t),
                                     &(values[i]), sizeof(std::uint64_t));
    TRI_ASSERT(ptrs[i] != nullptr);
  }

  success = bucket->lock(-1LL);
  ASSERT_TRUE(success);

  for (std::size_t i = 0; i < 3; i++) {
    bucket->insert(hashes[i], ptrs[i]);
  }
  for (std::size_t i = 0; i < 3; i++) {
    CachedValue* res = bucket->find(hashes[i], ptrs[i]->key(), ptrs[i]->keySize());
    ASSERT_EQ(res, ptrs[i]);
  }

  CachedValue* res;
  res = bucket->remove(hashes[1], ptrs[1]->key(), ptrs[1]->keySize());
  ASSERT_EQ(res, ptrs[1]);
  res = bucket->find(hashes[1], ptrs[1]->key(), ptrs[1]->keySize());
  ASSERT_EQ(nullptr, res);
  res = bucket->remove(hashes[0], ptrs[0]->key(), ptrs[0]->keySize());
  ASSERT_EQ(res, ptrs[0]);
  res = bucket->find(hashes[0], ptrs[0]->key(), ptrs[0]->keySize());
  ASSERT_EQ(nullptr, res);
  res = bucket->remove(hashes[2], ptrs[2]->key(), ptrs[2]->keySize());
  ASSERT_EQ(res, ptrs[2]);
  res = bucket->find(hashes[2], ptrs[2]->key(), ptrs[2]->keySize());
  ASSERT_EQ(nullptr, res);

  bucket->unlock();

  // cleanup
  for (std::size_t i = 0; i < 3; i++) {
    delete ptrs[i];
  }
}

TEST(CacheTransactionalBucketTest, verify_that_eviction_works_as_expected) {
  auto bucket = std::make_unique<TransactionalBucket>();
  bool success;

  std::uint32_t hashes[9] = {1, 2, 3, 4, 5,
                             6, 7, 8, 9};  // don't have to be real, but should be unique and non-zero
  std::uint64_t keys[9] = {0, 1, 2, 3, 4, 5, 6, 7, 8};
  std::uint64_t values[9] = {0, 1, 2, 3, 4, 5, 6, 7, 8};
  CachedValue* ptrs[9];
  for (std::size_t i = 0; i < 9; i++) {
    ptrs[i] = CachedValue::construct(&(keys[i]), sizeof(std::uint64_t),
                                     &(values[i]), sizeof(std::uint64_t));
    TRI_ASSERT(ptrs[i] != nullptr);
  }

  success = bucket->lock(-1LL);
  ASSERT_TRUE(success);

  // insert three to fill
  ASSERT_FALSE(bucket->isFull());
  for (std::size_t i = 0; i < 8; i++) {
    bucket->insert(hashes[i], ptrs[i]);
    if (i < 7) {
      ASSERT_FALSE(bucket->isFull());
    } else {
      ASSERT_TRUE(bucket->isFull());
    }
  }
  for (std::size_t i = 0; i < 8; i++) {
    CachedValue* res = bucket->find(hashes[i], ptrs[i]->key(), ptrs[i]->keySize());
    ASSERT_EQ(res, ptrs[i]);
  }

  // check that we get proper eviction candidate
  CachedValue* candidate = bucket->evictionCandidate();
  ASSERT_EQ(candidate, ptrs[0]);
  bucket->evict(candidate, false);
  CachedValue* res = bucket->find(hashes[0], ptrs[0]->key(), ptrs[0]->keySize());
  ASSERT_EQ(nullptr, res);
  ASSERT_FALSE(bucket->isFull());

  // check that we still find the right candidate if not full
  candidate = bucket->evictionCandidate();
  ASSERT_EQ(candidate, ptrs[1]);
  bucket->evict(candidate, true);
  res = bucket->find(hashes[1], ptrs[1]->key(), ptrs[1]->keySize());
  ASSERT_EQ(nullptr, res);
  ASSERT_FALSE(bucket->isFull());

  // check that we can insert now after eviction optimized for insertion
  bucket->insert(hashes[8], ptrs[8]);
  res = bucket->find(hashes[8], ptrs[8]->key(), ptrs[8]->keySize());
  ASSERT_EQ(res, ptrs[8]);

  bucket->unlock();

  // cleanup
  for (std::size_t i = 0; i < 9; i++) {
    delete ptrs[i];
  }
}

TEST(CacheTransactionalBucketTest, verify_that_blacklisting_works_as_expected) {
  auto bucket = std::make_unique<TransactionalBucket>();
  bool success;
  CachedValue* res;

  std::uint32_t hashes[8] = {1, 1, 2, 3, 4,
                             5, 6, 7};  // don't have to be real, want some overlap
  std::uint64_t keys[8] = {0, 1, 2, 3, 4, 5, 6, 7};
  std::uint64_t values[8] = {0, 1, 2, 3, 4, 5, 6, 7};
  CachedValue* ptrs[8];
  for (std::size_t i = 0; i < 8; i++) {
    ptrs[i] = CachedValue::construct(&(keys[i]), sizeof(std::uint64_t),
                                     &(values[i]), sizeof(std::uint64_t));
    TRI_ASSERT(ptrs[i] != nullptr);
  }

  success = bucket->lock(-1LL);
  bucket->updateBlacklistTerm(1ULL);
  ASSERT_TRUE(success);

  // insert eight to fill
  ASSERT_FALSE(bucket->isFull());
  for (std::size_t i = 0; i < 8; i++) {
    bucket->insert(hashes[i], ptrs[i]);
    if (i < 7) {
      ASSERT_FALSE(bucket->isFull());
    } else {
      ASSERT_TRUE(bucket->isFull());
    }
  }
  for (std::size_t i = 0; i < 8; i++) {
    res = bucket->find(hashes[i], ptrs[i]->key(), ptrs[i]->keySize());
    ASSERT_EQ(res, ptrs[i]);
  }

  // blacklist 1-5 to fill blacklist
  for (std::size_t i = 1; i < 6; i++) {
    bucket->blacklist(hashes[i], ptrs[i]->key(), ptrs[i]->keySize());
  }
  for (std::size_t i = 1; i < 6; i++) {
    ASSERT_TRUE(bucket->isBlacklisted(hashes[i]));
    res = bucket->find(hashes[i], ptrs[i]->key(), ptrs[i]->keySize());
    ASSERT_EQ(nullptr, res);
  }
  // verify actually not fully blacklisted
  ASSERT_FALSE(bucket->isFullyBlacklisted());
  ASSERT_FALSE(bucket->isBlacklisted(hashes[6]));
  // verify it didn't remove matching hash with non-matching key
  res = bucket->find(hashes[0], ptrs[0]->key(), ptrs[0]->keySize());
  ASSERT_EQ(res, ptrs[0]);

  // proceed to fully blacklist
  bucket->blacklist(hashes[6], ptrs[6]->key(), ptrs[6]->keySize());
  ASSERT_TRUE(bucket->isBlacklisted(hashes[6]));
  res = bucket->find(hashes[6], ptrs[6]->key(), ptrs[6]->keySize());
  ASSERT_EQ(nullptr, res);
  // make sure it still didn't remove non-matching key
  res = bucket->find(hashes[0], ptrs[0]->key(), ptrs[0]->keySize());
  ASSERT_EQ(ptrs[0], res);
  // make sure it's fully blacklisted
  ASSERT_TRUE(bucket->isFullyBlacklisted());
  ASSERT_TRUE(bucket->isBlacklisted(hashes[7]));

  bucket->unlock();

  // check that updating blacklist term clears blacklist
  bucket->lock(-1LL);
  bucket->updateBlacklistTerm(2ULL);
  ASSERT_FALSE(bucket->isFullyBlacklisted());
  for (std::size_t i = 0; i < 7; i++) {
    ASSERT_FALSE(bucket->isBlacklisted(hashes[i]));
  }
  bucket->unlock();

  // cleanup
  for (std::size_t i = 0; i < 8; i++) {
    delete ptrs[i];
  }
}

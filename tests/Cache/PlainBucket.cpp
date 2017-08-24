////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for arangodb::cache::PlainBucket
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
/// @author Daniel H. Larkin
/// @author Copyright 2017, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Cache/PlainBucket.h"
#include "Basics/Common.h"

#include "catch.hpp"

#include <stdint.h>
#include <string>

using namespace arangodb::cache;

TEST_CASE("cache::PlainBucket", "[cache]") {
  SECTION("verify that insertion works correctly") {
    auto bucket = std::make_unique<PlainBucket>();
    bool success;

    uint32_t hashes[11] = {
        1, 2, 3,
        4, 5, 6,
        7, 8, 9,
        10, 11};  // don't have to be real, but should be unique and non-zero
    uint64_t keys[11] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    uint64_t values[11] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    CachedValue* ptrs[11];
    for (size_t i = 0; i < 11; i++) {
      ptrs[i] = CachedValue::construct(&(keys[i]), sizeof(uint64_t),
                                       &(values[i]), sizeof(uint64_t));
      TRI_ASSERT(ptrs[i] != nullptr);
    }

    success = bucket->lock(-1LL);
    REQUIRE(success);

    // insert ten to fill
    REQUIRE(!bucket->isFull());
    for (size_t i = 0; i < 10; i++) {
      bucket->insert(hashes[i], ptrs[i]);
      if (i < 9) {
        REQUIRE(!bucket->isFull());
      } else {
        REQUIRE(bucket->isFull());
      }
    }
    for (size_t i = 0; i < 10; i++) {
      CachedValue* res =
          bucket->find(hashes[i], ptrs[i]->key(), ptrs[i]->keySize());
      REQUIRE(res == ptrs[i]);
    }

    // check that insert is ignored if full
    bucket->insert(hashes[10], ptrs[10]);
    CachedValue* res = bucket->find(hashes[10], ptrs[10]->key(), ptrs[10]->keySize());
    REQUIRE(nullptr == res);

    bucket->unlock();

    // cleanup
    for (size_t i = 0; i < 11; i++) {
      delete ptrs[i];
    }
  }

  SECTION("verify removal works correctly") {
    auto bucket = std::make_unique<PlainBucket>();
    bool success;

    uint32_t hashes[3] = {
        1, 2, 3};  // don't have to be real, but should be unique and non-zero
    uint64_t keys[3] = {0, 1, 2};
    uint64_t values[3] = {0, 1, 2};
    CachedValue* ptrs[3];
    for (size_t i = 0; i < 3; i++) {
      ptrs[i] = CachedValue::construct(&(keys[i]), sizeof(uint64_t),
                                       &(values[i]), sizeof(uint64_t));
      TRI_ASSERT(ptrs[i] != nullptr);
    }

    success = bucket->lock(-1LL);
    REQUIRE(success);

    for (size_t i = 0; i < 3; i++) {
      bucket->insert(hashes[i], ptrs[i]);
    }
    for (size_t i = 0; i < 3; i++) {
      CachedValue* res =
          bucket->find(hashes[i], ptrs[i]->key(), ptrs[i]->keySize());
      REQUIRE(res == ptrs[i]);
    }

    CachedValue* res;
    res = bucket->remove(hashes[1], ptrs[1]->key(), ptrs[1]->keySize());
    REQUIRE(res == ptrs[1]);
    res = bucket->find(hashes[1], ptrs[1]->key(), ptrs[1]->keySize());
    REQUIRE(nullptr == res);
    res = bucket->remove(hashes[0], ptrs[0]->key(), ptrs[0]->keySize());
    REQUIRE(res == ptrs[0]);
    res = bucket->find(hashes[0], ptrs[0]->key(), ptrs[0]->keySize());
    REQUIRE(nullptr == res);
    res = bucket->remove(hashes[2], ptrs[2]->key(), ptrs[2]->keySize());
    REQUIRE(res == ptrs[2]);
    res = bucket->find(hashes[2], ptrs[2]->key(), ptrs[2]->keySize());
    REQUIRE(nullptr == res);

    bucket->unlock();

    // cleanup
    for (size_t i = 0; i < 3; i++) {
      delete ptrs[i];
    }
  }

  SECTION("verify eviction works correctly") {
    auto bucket = std::make_unique<PlainBucket>();
    bool success;

    uint32_t hashes[11] = {
        1, 2, 3,
        4, 5, 6,
        7, 8, 9,
        10, 11};  // don't have to be real, but should be unique and non-zero
    uint64_t keys[11] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    uint64_t values[11] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    CachedValue* ptrs[11];
    for (size_t i = 0; i < 11; i++) {
      ptrs[i] = CachedValue::construct(&(keys[i]), sizeof(uint64_t),
                                       &(values[i]), sizeof(uint64_t));
      TRI_ASSERT(ptrs[i] != nullptr);
    }

    success = bucket->lock(-1LL);
    REQUIRE(success);

    // insert five to fill
    REQUIRE(!bucket->isFull());
    for (size_t i = 0; i < 10; i++) {
      bucket->insert(hashes[i], ptrs[i]);
      if (i < 9) {
        REQUIRE(!bucket->isFull());
      } else {
        REQUIRE(bucket->isFull());
      }
    }
    for (size_t i = 0; i < 10; i++) {
      CachedValue* res =
          bucket->find(hashes[i], ptrs[i]->key(), ptrs[i]->keySize());
      REQUIRE(res == ptrs[i]);
    }

    // check that we get proper eviction candidate
    CachedValue* candidate = bucket->evictionCandidate();
    REQUIRE(candidate == ptrs[0]);
    bucket->evict(candidate, false);
    CachedValue* res = bucket->find(hashes[0], ptrs[0]->key(), ptrs[0]->keySize());
    REQUIRE(nullptr == res);
    REQUIRE(!bucket->isFull());

    // check that we still find the right candidate if not full
    candidate = bucket->evictionCandidate();
    REQUIRE(candidate == ptrs[1]);
    bucket->evict(candidate, true);
    res = bucket->find(hashes[1], ptrs[1]->key(), ptrs[1]->keySize());
    REQUIRE(nullptr == res);
    REQUIRE(!bucket->isFull());

    // check that we can insert now after eviction optimized for insertion
    bucket->insert(hashes[10], ptrs[10]);
    res = bucket->find(hashes[10], ptrs[10]->key(), ptrs[10]->keySize());
    REQUIRE(res == ptrs[10]);

    bucket->unlock();

    // cleanup
    for (size_t i = 0; i < 11; i++) {
      delete ptrs[i];
    }
  }
}

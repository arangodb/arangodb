////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for arangodb::cache::PlainBucket
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2017 triagens GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Daniel H. Larkin
/// @author Copyright 2017, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Basics/Common.h"

#include "catch.hpp"

#include "Cache/PlainBucket.h"

#include <stdint.h>
#include <string>

using namespace arangodb::cache;

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief setup
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("CCachePlainBucketTest", "[cache]") {

////////////////////////////////////////////////////////////////////////////////
/// @brief test insertion to full and fail beyond
////////////////////////////////////////////////////////////////////////////////

SECTION("tst_insertion") {
  PlainBucket bucket;
  bool success;

  uint32_t hashes[6] = {
      1, 2, 3,
      4, 5, 6};  // don't have to be real, but should be unique and non-zero
  uint64_t keys[6] = {0, 1, 2, 3, 4, 5};
  uint64_t values[6] = {0, 1, 2, 3, 4, 5};
  CachedValue* ptrs[6];
  for (size_t i = 0; i < 6; i++) {
    ptrs[i] = CachedValue::construct(&(keys[i]), sizeof(uint64_t), &(values[i]),
                                     sizeof(uint64_t));
  }

  success = bucket.lock(-1LL);
  CHECK(success);

  // insert five to fill
  CHECK(!bucket.isFull());
  for (size_t i = 0; i < 5; i++) {
    bucket.insert(hashes[i], ptrs[i]);
    if (i < 4) {
      CHECK(!bucket.isFull());
    } else {
      CHECK(bucket.isFull());
    }
  }
  for (size_t i = 0; i < 5; i++) {
    CachedValue* res = bucket.find(hashes[i], ptrs[i]->key(), ptrs[i]->keySize);
    CHECK(res == ptrs[i]);
  }

  // check that insert is ignored if full
  bucket.insert(hashes[5], ptrs[5]);
  CachedValue* res = bucket.find(hashes[5], ptrs[5]->key(), ptrs[5]->keySize);
  CHECK(nullptr == res);

  bucket.unlock();

  // cleanup
  for (size_t i = 0; i < 6; i++) {
    delete ptrs[i];
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test removal
////////////////////////////////////////////////////////////////////////////////

SECTION("tst_removal") {
  PlainBucket bucket;
  bool success;

  uint32_t hashes[3] = {
      1, 2, 3};  // don't have to be real, but should be unique and non-zero
  uint64_t keys[3] = {0, 1, 2};
  uint64_t values[3] = {0, 1, 2};
  CachedValue* ptrs[3];
  for (size_t i = 0; i < 3; i++) {
    ptrs[i] = CachedValue::construct(&(keys[i]), sizeof(uint64_t), &(values[i]),
                                     sizeof(uint64_t));
  }

  success = bucket.lock(-1LL);
  CHECK(success);

  for (size_t i = 0; i < 3; i++) {
    bucket.insert(hashes[i], ptrs[i]);
  }
  for (size_t i = 0; i < 3; i++) {
    CachedValue* res = bucket.find(hashes[i], ptrs[i]->key(), ptrs[i]->keySize);
    CHECK(res == ptrs[i]);
  }

  CachedValue* res;
  res = bucket.remove(hashes[1], ptrs[1]->key(), ptrs[1]->keySize);
  CHECK(res == ptrs[1]);
  res = bucket.find(hashes[1], ptrs[1]->key(), ptrs[1]->keySize);
  CHECK(nullptr == res);
  res = bucket.remove(hashes[0], ptrs[0]->key(), ptrs[0]->keySize);
  CHECK(res == ptrs[0]);
  res = bucket.find(hashes[0], ptrs[0]->key(), ptrs[0]->keySize);
  CHECK(nullptr == res);
  res = bucket.remove(hashes[2], ptrs[2]->key(), ptrs[2]->keySize);
  CHECK(res == ptrs[2]);
  res = bucket.find(hashes[2], ptrs[2]->key(), ptrs[2]->keySize);
  CHECK(nullptr == res);

  bucket.unlock();

  // cleanup
  for (size_t i = 0; i < 3; i++) {
    delete ptrs[i];
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test eviction with subsequent insertion
////////////////////////////////////////////////////////////////////////////////

SECTION("tst_eviction") {
  PlainBucket bucket;
  bool success;

  uint32_t hashes[6] = {
      1, 2, 3,
      4, 5, 6};  // don't have to be real, but should be unique and non-zero
  uint64_t keys[6] = {0, 1, 2, 3, 4, 5};
  uint64_t values[6] = {0, 1, 2, 3, 4, 5};
  CachedValue* ptrs[6];
  for (size_t i = 0; i < 6; i++) {
    ptrs[i] = CachedValue::construct(&(keys[i]), sizeof(uint64_t), &(values[i]),
                                     sizeof(uint64_t));
  }

  success = bucket.lock(-1LL);
  CHECK(success);

  // insert five to fill
  CHECK(!bucket.isFull());
  for (size_t i = 0; i < 5; i++) {
    bucket.insert(hashes[i], ptrs[i]);
    if (i < 4) {
      CHECK(!bucket.isFull());
    } else {
      CHECK(bucket.isFull());
    }
  }
  for (size_t i = 0; i < 5; i++) {
    CachedValue* res = bucket.find(hashes[i], ptrs[i]->key(), ptrs[i]->keySize);
    CHECK(res == ptrs[i]);
  }

  // check that we get proper eviction candidate
  CachedValue* candidate = bucket.evictionCandidate();
  CHECK(candidate == ptrs[0]);
  bucket.evict(candidate, false);
  CachedValue* res = bucket.find(hashes[0], ptrs[0]->key(), ptrs[0]->keySize);
  CHECK(nullptr == res);
  CHECK(!bucket.isFull());

  // check that we still find the right candidate if not full
  candidate = bucket.evictionCandidate();
  CHECK(candidate == ptrs[1]);
  bucket.evict(candidate, true);
  res = bucket.find(hashes[1], ptrs[1]->key(), ptrs[1]->keySize);
  CHECK(nullptr == res);
  CHECK(!bucket.isFull());

  // check that we can insert now after eviction optimized for insertion
  bucket.insert(hashes[5], ptrs[5]);
  res = bucket.find(hashes[5], ptrs[5]->key(), ptrs[5]->keySize);
  CHECK(res == ptrs[5]);

  bucket.unlock();

  // cleanup
  for (size_t i = 0; i < 6; i++) {
    delete ptrs[i];
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate tests
////////////////////////////////////////////////////////////////////////////////

}

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|//
// --SECTION--\\|/// @\\}\\)"
// End:

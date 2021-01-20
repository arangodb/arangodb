////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2017, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include <cstdint>

#include "Cache/BucketState.h"

using namespace arangodb::cache;

TEST(CacheBucketStateTest, test_lock_methods) {
  BucketState state;
  bool success;

  std::uint32_t outsideBucketState = 0;

  auto cb1 = [&outsideBucketState]() -> void { outsideBucketState = 1; };

  auto cb2 = [&outsideBucketState]() -> void { outsideBucketState = 2; };

  // check lock without contention
  ASSERT_FALSE(state.isLocked());
  success = state.lock(-1, cb1);
  ASSERT_TRUE(success);
  ASSERT_TRUE(state.isLocked());
  ASSERT_EQ(1UL, outsideBucketState);

  // check lock with contention
  success = state.lock(10LL, cb2);
  ASSERT_FALSE(success);
  ASSERT_TRUE(state.isLocked());
  ASSERT_EQ(1UL, outsideBucketState);

  // check unlock
  state.unlock();
  ASSERT_FALSE(state.isLocked());
}

TEST(CacheBucketStateTest, test_methods_for_nonlock_flags) {
  BucketState state;
  bool success;

  success = state.lock();
  ASSERT_TRUE(success);
  ASSERT_FALSE(state.isSet(BucketState::Flag::migrated));
  state.unlock();

  success = state.lock();
  ASSERT_TRUE(success);
  ASSERT_FALSE(state.isSet(BucketState::Flag::migrated));
  state.toggleFlag(BucketState::Flag::migrated);
  ASSERT_TRUE(state.isSet(BucketState::Flag::migrated));
  state.unlock();

  success = state.lock();
  ASSERT_TRUE(success);
  ASSERT_TRUE(state.isSet(BucketState::Flag::migrated));
  state.unlock();

  success = state.lock();
  ASSERT_TRUE(success);
  ASSERT_TRUE(state.isSet(BucketState::Flag::migrated));
  state.toggleFlag(BucketState::Flag::migrated);
  ASSERT_FALSE(state.isSet(BucketState::Flag::migrated));
  state.unlock();

  success = state.lock();
  ASSERT_TRUE(success);
  ASSERT_FALSE(state.isSet(BucketState::Flag::migrated));
  state.unlock();
}

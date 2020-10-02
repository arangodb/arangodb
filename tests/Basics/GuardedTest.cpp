////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "Utilities/Guarded.h"
#include "Basics/Mutex.h"

#include <mutex>

using namespace arangodb;

namespace arangodb::tests {

template <typename Mutex>
class GuardedTest : public ::testing::Test {
};

TYPED_TEST_CASE_P(GuardedTest);

TYPED_TEST_P(GuardedTest, test_allows_access) {
  struct UnderGuard {
    int val{};
  };
  Guarded<UnderGuard, Mutex> guardedObj{UnderGuard{.val = 1}};
  {
    MutexGuard<UnderGuard, Mutex> guard = guardedObj.getLockedGuard();
    EXPECT_EQ(1, guard.get().val);
    guard.get().val = 2;
    EXPECT_EQ(2, guard.get().val);
  }
  guardedObj.doUnderLock([](UnderGuard& obj) {
    EXPECT_EQ(2, obj.val);
    obj.val = 3;
    EXPECT_EQ(3, obj.val);
  });
  // guardedObj.tryUnderLock([](UnderGuard& obj) { ... });
  {
    MutexGuard<UnderGuard, Mutex> guard = guardedObj.getLockedGuard();
    EXPECT_EQ(3, guard.get().val);
  }
}

TYPED_TEST_P(GuardedTest, test_2) {}

REGISTER_TYPED_TEST_CASE_P(GuardedTest, test_allows_access, test_2);

using TestedMutexes = ::testing::Types<std::mutex, arangodb::Mutex>;

INSTANTIATE_TYPED_TEST_CASE_P(GuardedTestInstantiation, GuardedTest, TestedMutexes);

}  // namespace arangodb::tests

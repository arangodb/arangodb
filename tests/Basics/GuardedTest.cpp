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

#include "Basics/Mutex.h"
#include "Utilities/Guarded.h"

#include <mutex>
#include <thread>

namespace arangodb::tests {

template <typename M>
class GuardedTest : public ::testing::Test {
 protected:
  using Mutex = M;
};

struct UnderGuard {
  int val{};
};

TYPED_TEST_CASE_P(GuardedTest);

TYPED_TEST_P(GuardedTest, test_guard_allows_access) {
  auto guardedObj = Guarded<UnderGuard, typename TestFixture::Mutex>{UnderGuard{1}};
  {
    auto guard = guardedObj.getLockedGuard();
    EXPECT_EQ(1, guard.get().val);
    guard.get().val = 2;
    EXPECT_EQ(2, guard.get().val);
  }
}
TYPED_TEST_P(GuardedTest, test_guard_waits_for_access) {
  auto guardedObj = Guarded<UnderGuard, typename TestFixture::Mutex>{UnderGuard{1}};
  // TODO Get a lock first, then assert that trying to get a guard waits for the
  //      lock to be released.
}

TYPED_TEST_P(GuardedTest, test_do_allows_access) {
  auto guardedObj = Guarded<UnderGuard, typename TestFixture::Mutex>{UnderGuard{1}};
  {
    bool didExecute = false;
    guardedObj.doUnderLock([&didExecute](UnderGuard& obj) {
      EXPECT_EQ(1, obj.val);
      obj.val = 2;
      didExecute = true;
      EXPECT_EQ(2, obj.val);
    });
    EXPECT_TRUE(didExecute);
    {
      auto guard = guardedObj.getLockedGuard();
      EXPECT_EQ(2, guard.get().val);
    }
  }
}

TYPED_TEST_P(GuardedTest, test_do_waits_for_access) {
  auto guardedObj = Guarded<UnderGuard, typename TestFixture::Mutex>{UnderGuard{1}};
  // TODO Get a lock first, then assert that doUnderLock waits for the lock
  //      to be released.
}

TYPED_TEST_P(GuardedTest, test_try_allows_access) {
  auto guardedObj = Guarded<UnderGuard, typename TestFixture::Mutex>{UnderGuard{1}};
  {
    bool didExecute = false;
    auto const res = guardedObj.tryUnderLock([&didExecute](UnderGuard& obj) {
      EXPECT_EQ(1, obj.val);
      obj.val = 2;
      didExecute = true;
      EXPECT_EQ(2, obj.val);
    });
    static_assert(std::is_same_v<std::optional<std::monostate> const, decltype(res)>);
    EXPECT_EQ(didExecute, res.has_value());
    {
      auto guard = guardedObj.getLockedGuard();
      if (didExecute) {
        EXPECT_EQ(guard->val, 2);
      } else {
        EXPECT_EQ(guard->val, 1);
      }
    }
  }
}

TYPED_TEST_P(GuardedTest, test_try_fails_access) {
  auto guardedObj = Guarded<UnderGuard, typename TestFixture::Mutex>{UnderGuard{1}};
  {
    auto guard = guardedObj.getLockedGuard();
    bool threadStarted = false;
    bool threadFinished = false;
    auto thr = std::thread([&] {
      threadStarted = true;
      bool didExecute = false;
      auto const res = guardedObj.tryUnderLock([&didExecute](UnderGuard& obj) {
        EXPECT_EQ(1, obj.val);
        obj.val = 2;
        didExecute = true;
        EXPECT_EQ(2, obj.val);
      });
      static_assert(std::is_same_v<std::optional<std::monostate> const, decltype(res)>);
      EXPECT_FALSE(res.has_value());
      EXPECT_FALSE(didExecute);
      threadFinished = true;
    });
    thr.join();
    EXPECT_EQ(guard->val, 1);
  }
}

REGISTER_TYPED_TEST_CASE_P(GuardedTest, test_guard_allows_access, test_guard_waits_for_access,
                           test_do_allows_access, test_do_waits_for_access,
                           test_try_allows_access, test_try_fails_access);

using TestedMutexes = ::testing::Types<std::mutex, arangodb::Mutex>;

INSTANTIATE_TYPED_TEST_CASE_P(GuardedTestInstantiation, GuardedTest, TestedMutexes);

}  // namespace arangodb::tests

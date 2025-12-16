////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2025 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
////////////////////////////////////////////////////////////////////////////////

#include <velocypack/Value.h>
#include "Basics/ThreadLocalLeaser.h"
#include "Basics/ThreadGuard.h"

#include "gtest/gtest.h"

using namespace arangodb;

struct ThreadLocalLeaserTest : testing::Test {
  ThreadLocalLeaserTest() { ThreadLocalBuilderLeaser::clear(); }
};

TEST_F(ThreadLocalLeaserTest, testLeaseAccess) {
  auto lease = ThreadLocalBuilderLeaser::lease();

  velocypack::Builder* builder{lease.get()};

  {
    auto& a = *lease;
    ASSERT_EQ(&a, builder);
  }

  { ASSERT_EQ(lease->isEmpty(), true); }
}

TEST_F(ThreadLocalLeaserTest, testBuilderIsLeasable) {
  auto b = ThreadLocalBuilderLeaser::lease();
  ASSERT_NE(b.get(), nullptr);
  ASSERT_TRUE(b->isEmpty());
}

TEST_F(ThreadLocalLeaserTest, testLeasedBuilderIsUsable) {
  auto b = ThreadLocalBuilderLeaser::lease();
  ASSERT_NE(b.get(), nullptr);
  ASSERT_TRUE(b->isEmpty());

  b->add(VPackValue("Hello, world"));
}

TEST_F(ThreadLocalLeaserTest, testLeasingAndReturningIncreasesStashSize) {
  { auto b = ThreadLocalBuilderLeaser::lease(); }
  ASSERT_EQ(ThreadLocalBuilderLeaser::stashSize(), 1);
}

TEST_F(ThreadLocalLeaserTest, testReturnedLeasedBuilderIsReturnedAgain) {
  velocypack::Builder* builder{nullptr};

  {
    EXPECT_EQ(ThreadLocalBuilderLeaser::stashSize(), 0);
    auto b = ThreadLocalBuilderLeaser::lease();
    EXPECT_EQ(ThreadLocalBuilderLeaser::stashSize(), 0);
    ASSERT_NE(b.get(), nullptr);
    b->add(VPackValue("Hello, world"));
    ASSERT_FALSE(b->isEmpty());

    builder = b.get();
  }
  // here the lease b has been dropped and the builder is returned to
  // the stash
  EXPECT_EQ(ThreadLocalBuilderLeaser::stashSize(), 1);
  auto c = ThreadLocalBuilderLeaser::lease();
  ASSERT_EQ(c.get(), builder);
  EXPECT_EQ(ThreadLocalBuilderLeaser::stashSize(), 0);
}

TEST_F(ThreadLocalLeaserTest, testLeaseMovedOutOf) {
  EXPECT_EQ(ThreadLocalBuilderLeaser::stashSize(), 0);
  auto b = ThreadLocalBuilderLeaser::lease();
  ASSERT_NE(b.get(), nullptr);
  EXPECT_EQ(ThreadLocalBuilderLeaser::stashSize(), 0);

  auto c = std::move(b);
  ASSERT_EQ(b.get(), nullptr);
  ASSERT_NE(c.get(), nullptr);
  ASSERT_EQ(ThreadLocalBuilderLeaser::stashSize(), 0);
}

TEST_F(ThreadLocalLeaserTest, testLeaseMovedOutOfReturned) {
  EXPECT_EQ(ThreadLocalBuilderLeaser::stashSize(), 0);
  velocypack::Builder* builder{nullptr};
  {
    auto b = ThreadLocalBuilderLeaser::lease();
    ASSERT_NE(b.get(), nullptr);

    auto c = std::move(b);
    // The moved-from lease should not stash anything
    ASSERT_EQ(b.get(), nullptr);
    ASSERT_NE(c.get(), nullptr);
    builder = c.get();
  }
  // Only one object should be returned. In particular
  // the moved-out-of lease must not push anything to
  // the stash
  ASSERT_EQ(ThreadLocalBuilderLeaser::stashSize(), 1);

  auto b = ThreadLocalBuilderLeaser::lease();
  ASSERT_EQ(b.get(), builder);
}

TEST_F(ThreadLocalLeaserTest, testLeaseMovedBetweenThreads) {
  EXPECT_EQ(ThreadLocalBuilderLeaser::stashSize(), 0);
  auto b = ThreadLocalBuilderLeaser::lease();
  ASSERT_NE(b.get(), nullptr);
  auto* builder = b.get();

  auto threads = ThreadGuard(1);
  threads.emplace([&]() {
    auto n = ThreadLocalBuilderLeaser::stashSize();
    ASSERT_EQ(n, 0);
    {
      auto c = std::move(b);
      ASSERT_NE(c.get(), nullptr);
    }
    // Now c is dropped and should end up in this thread's stash

    auto d = ThreadLocalBuilderLeaser::lease();
    ASSERT_EQ(d.get(), builder);
  });

  threads.joinAll();
  ASSERT_EQ(ThreadLocalBuilderLeaser::stashSize(), 0);
}

TEST_F(ThreadLocalLeaserTest, testStashIsLiFo) {
  auto lease_ptrs = std::vector<velocypack::Builder*>{};
  lease_ptrs.reserve(ThreadLocalBuilderLeaser::maxStashedPerThread);
  {
    auto leases = std::vector<ThreadLocalBuilderLeaser::Lease>{};
    leases.reserve(ThreadLocalBuilderLeaser::maxStashedPerThread);
    for (auto i = size_t{0}; i < ThreadLocalBuilderLeaser::maxStashedPerThread;
         ++i) {
      auto& it = leases.emplace_back(ThreadLocalBuilderLeaser::lease());
      lease_ptrs.emplace_back(it.get());
    }

    // enforce defined order of leases being freed
    while (!leases.empty()) {
      leases.pop_back();
    }
  }
  ASSERT_EQ(ThreadLocalBuilderLeaser::maxStashedPerThread,
            ThreadLocalBuilderLeaser::stashSize());

  {
    auto leases = std::vector<ThreadLocalBuilderLeaser::Lease>{};
    leases.reserve(ThreadLocalBuilderLeaser::maxStashedPerThread);
    for (auto i = size_t{0}; i < ThreadLocalBuilderLeaser::maxStashedPerThread;
         ++i) {
      auto& it = leases.emplace_back(ThreadLocalBuilderLeaser::lease());
      ASSERT_EQ(it.get(), lease_ptrs.at(i));
    }
  }
}

TEST_F(ThreadLocalLeaserTest, testStashExhaustion) {
  auto lease_ptrs = std::vector<velocypack::Builder*>{};
  lease_ptrs.reserve(ThreadLocalBuilderLeaser::maxStashedPerThread + 1);
  {
    auto leases = std::vector<ThreadLocalBuilderLeaser::Lease>{};
    leases.reserve(ThreadLocalBuilderLeaser::maxStashedPerThread + 1);
    for (auto i = size_t{0};
         i < ThreadLocalBuilderLeaser::maxStashedPerThread + 1; ++i) {
      auto& it = leases.emplace_back(ThreadLocalBuilderLeaser::lease());
      lease_ptrs.emplace_back(it.get());
    }

    // enforce defined order of leases being freed
    while (!leases.empty()) {
      leases.pop_back();
    }
  }
  ASSERT_EQ(ThreadLocalBuilderLeaser::maxStashedPerThread,
            ThreadLocalBuilderLeaser::stashSize());

  {
    auto leases = std::vector<ThreadLocalBuilderLeaser::Lease>{};
    leases.reserve(ThreadLocalBuilderLeaser::maxStashedPerThread);
    for (auto i = size_t{0}; i < ThreadLocalBuilderLeaser::maxStashedPerThread;
         ++i) {
      auto& it = leases.emplace_back(ThreadLocalBuilderLeaser::lease());
      ASSERT_EQ(it.get(), lease_ptrs.at(i + 1));
    }
  }
}

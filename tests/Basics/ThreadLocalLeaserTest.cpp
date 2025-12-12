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

#include "Basics/ThreadLocalLeaser.h"
#include "Basics/ThreadGuard.h"

#include "gtest/gtest.h"

using namespace arangodb;

namespace {
constexpr size_t numThreads = 4;
constexpr uint64_t numOps = 100000;
}  // namespace

TEST(ThreadLocalLeaserTest, testBuilderIsLeasable) {
  auto n = ThreadLocalBuilderLeaser::stashSize();
  ASSERT_EQ(n, 0);

  {
    auto b = ThreadLocalBuilderLeaser::lease();
    ASSERT_NE(b.get(), nullptr);
    ASSERT_EQ(n, 0);
    ASSERT_TRUE(b->isEmpty());

    b->add(VPackValue(15));
  }

  auto n2 = ThreadLocalBuilderLeaser::stashSize();
  ASSERT_EQ(n2, 1);

  {
    auto b = ThreadLocalBuilderLeaser::lease();
    ASSERT_NE(b.get(), nullptr);
    ASSERT_EQ(ThreadLocalBuilderLeaser::stashSize(), 0);
    ASSERT_TRUE(b->isEmpty());
  }

  auto n3 = ThreadLocalBuilderLeaser::stashSize();
  ASSERT_EQ(n3, 1);
}

TEST(ThreadLocalLeaserTest, testLeaseMovedOutOf) {
  auto n = ThreadLocalBuilderLeaser::stashSize();
  ASSERT_EQ(n, 0);
  {
    auto b = ThreadLocalBuilderLeaser::lease();
    ASSERT_NE(b.get(), nullptr);

    auto c = std::move(b);
    ASSERT_NE(c.get(), nullptr);
    ASSERT_EQ(b.get(), nullptr);
  }
  // b and c are dropped here, but b has been moved out of, hence is a nullptr
  // and as such an invalid lease. Sad fact of how C++ works.
  //
  // Only c is allowed to be returned to the stash.

  auto n2 = ThreadLocalBuilderLeaser::stashSize();
  ASSERT_EQ(n2, 1);
  {
    auto b = ThreadLocalBuilderLeaser::lease();

    // Assert we got the one that was returned above
    auto n3 = ThreadLocalBuilderLeaser::stashSize();
    ASSERT_EQ(n3, 0);
    ASSERT_NE(b.get(), nullptr);
    ASSERT_TRUE(b->isEmpty());
  }
}

TEST(ThreadLocalLeaserTest, testLeaseMovedBetweenThreads) {
  auto b = ThreadLocalBuilderLeaser::lease();
  ASSERT_NE(b.get(), nullptr);

  {
    auto n = ThreadLocalBuilderLeaser::stashSize();
    ASSERT_EQ(n, 0);
  }

  auto threads = ThreadGuard(1);
  threads.emplace([&]() {
    auto n = ThreadLocalBuilderLeaser::stashSize();
    ASSERT_EQ(n, 0);
    {
      // The lease got moved to this thread.
      ASSERT_NE(b.get(), nullptr);
      auto c = std::move(b);
      ASSERT_NE(c.get(), nullptr);
    }
    // Now c is dropped and should end up in this thread's stash

    auto n2 = ThreadLocalBuilderLeaser::stashSize();
    ASSERT_EQ(n2, 1);
  });

  threads.joinAll();

  // Nothing has been returned to the main thread's builder leaser
  {
    auto n = ThreadLocalBuilderLeaser::stashSize();
    ASSERT_EQ(n, 0);
  }
}

TEST(ThreadLocalLeaserTest, testMaximumStashSize) {
  {
    auto leases = std::vector<ThreadLocalBuilderLeaser::Lease>{};
    leases.reserve(ThreadLocalBuilderLeaser::maxStashedPerThread);
    for (auto i = size_t{0}; i < ThreadLocalBuilderLeaser::maxStashedPerThread;
         ++i) {
      leases.emplace_back(ThreadLocalBuilderLeaser::lease());
    }
    ASSERT_EQ(0, ThreadLocalBuilderLeaser::stashSize());
  }

  // leases above is dropped and all builders end up in the stash
  ASSERT_EQ(ThreadLocalBuilderLeaser::maxStashedPerThread,
            ThreadLocalBuilderLeaser::stashSize());

  {
    // Now get more than the stash size of builders
    auto leases = std::vector<ThreadLocalBuilderLeaser::Lease>{};
    leases.reserve(2 * ThreadLocalBuilderLeaser::maxStashedPerThread);
    for (auto i = size_t{0};
         i < 2 * ThreadLocalBuilderLeaser::maxStashedPerThread; ++i) {
      leases.emplace_back(ThreadLocalBuilderLeaser::lease());
    }
    // should have used all our stash
    ASSERT_EQ(0, ThreadLocalBuilderLeaser::stashSize());
  }

  // leases above is dropped and maxStashedPerThread  builders end up in the
  // stash
  ASSERT_EQ(ThreadLocalBuilderLeaser::maxStashedPerThread,
            ThreadLocalBuilderLeaser::stashSize());
}

// Just hammer the leasers on multiple threads a bit.
TEST(ThreadLocalLeaserTest, multiThreadBuilderLeasing) {
  auto run = std::atomic<bool>{false};
  auto threads = ThreadGuard(::numThreads);

  for (auto i = size_t{0}; i < ::numThreads; ++i) {
    threads.emplace([&]() {
      while (!run.load()) {
      }
      auto builders = std::vector<ThreadLocalBuilderLeaser::Lease>{};
      builders.reserve(numOps);
      for (auto j = uint64_t{0}; j < numOps; ++j) {
        auto& b = builders.emplace_back(ThreadLocalBuilderLeaser::lease());
        ASSERT_NE(b.get(), nullptr);
        ASSERT_TRUE(b.get()->isEmpty());
        b.get()->add(VPackValue("Fooooooooo"));
      }
    });
  }
  run.store(true);
  threads.joinAll();
}

TEST(ThreadLocalLeaserTest, testBasicLeaseString) {
  auto b = ThreadLocalStringLeaser::lease();
  ASSERT_NE(b.get(), nullptr);

  *b.get() += "foooooooooooooooooooooooooooooooooooooooooooooooooo";
}

TEST(ThreadLocalLeaserTest, multiThreadStringLeasing) {
  auto run = std::atomic<bool>{false};
  auto threads = ThreadGuard(::numThreads);

  for (auto i = size_t{0}; i < ::numThreads; ++i) {
    threads.emplace([&]() {
      while (!run.load()) {
      }
      auto builders = std::vector<ThreadLocalStringLeaser::Lease>{};
      builders.reserve(numOps);
      for (auto j = uint64_t{0}; j < numOps; ++j) {
        auto& b = builders.emplace_back(ThreadLocalStringLeaser::lease());
        ASSERT_NE(b.get(), nullptr);
        ASSERT_TRUE(b.get()->empty());
        *b.get() += "Fooooooooo";
      }
    });
  }
  run.store(true);
  threads.joinAll();
}

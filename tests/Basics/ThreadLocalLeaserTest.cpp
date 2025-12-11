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

TEST(ThreadLocalLeaserTest, testBasicLeaseBuilder) {
  auto b = ThreadLocalBuilderLeaser::current.lease();
  ASSERT_NE(b.leasee(), nullptr);

  b.leasee()->add(VPackValue(15));
}

TEST(ThreadLocalLeaserTest, testBasicLeaseString) {
  auto b = ThreadLocalStringLeaser::current.lease();
  ASSERT_NE(b.leasee(), nullptr);

  *b.leasee() += "foooooooooooooooooooooooooooooooooooooooooooooooooo";
}

TEST(ThreadLocalLeaserTest, multiThreadBuilderLeasing) {
  auto run = std::atomic<bool>{false};
  auto threads = ThreadGuard(::numThreads);

  for (auto i = size_t{0}; i < ::numThreads; ++i) {
    threads.emplace([&]() {
      while (!run.load()) {
      }
      for (auto j = uint64_t{0}; j < numOps; ++j) {
        auto b = ThreadLocalBuilderLeaser::current.lease();
        ASSERT_NE(b.leasee(), nullptr);
        ASSERT_TRUE(b.leasee()->isEmpty());
        b.leasee()->add(VPackValue("Fooooooooo"));
      }
    });

    run.store(true);
    threads.joinAll();
  }

  auto b = ThreadLocalBuilderLeaser::current.lease();
  ASSERT_NE(b.leasee(), nullptr);

  b.leasee()->add(VPackValue(15));
}

TEST(ThreadLocalLeaserTest, multiThreadStringLeasing) {
  auto run = std::atomic<bool>{false};
  auto threads = ThreadGuard(::numThreads);

  for (auto i = size_t{0}; i < ::numThreads; ++i) {
    threads.emplace([&]() {
      while (!run.load()) {
      }
      for (auto j = uint64_t{0}; j < numOps; ++j) {
        auto b = ThreadLocalStringLeaser::current.lease();
        ASSERT_NE(b.leasee(), nullptr);
        ASSERT_TRUE(b.leasee()->empty());
        *b.leasee() += "Fooooooooo";
      }
    });

    run.store(true);
    threads.joinAll();
  }

  auto b = ThreadLocalBuilderLeaser::current.lease();
  ASSERT_NE(b.leasee(), nullptr);

  b.leasee()->add(VPackValue(15));
}

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
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
/// @author Julia Volmer
////////////////////////////////////////////////////////////////////////////////
#include "Async/Registry/Metrics.h"
#include "Async/Registry/promise.h"
#include "Async/Registry/thread_registry.h"

#include <gtest/gtest.h>
#include <cstdint>
#include <source_location>
#include <thread>

using namespace arangodb::async_registry;

namespace {

auto promises_in_registry(std::shared_ptr<ThreadRegistry> registry)
    -> std::vector<PromiseSnapshot> {
  std::vector<PromiseSnapshot> promises;
  registry->for_promise(
      [&](PromiseSnapshot promise) { promises.push_back(promise); });
  return promises;
}

}  // namespace

struct AsyncThreadRegistryTest : ::testing::Test {};
using AsyncThreadRegistryDeathTest = AsyncThreadRegistryTest;

TEST_F(AsyncThreadRegistryTest, adds_a_promise) {
  auto registry = ThreadRegistry::make(std::make_shared<Metrics>());

  auto* promise_in = registry->add_promise();

  EXPECT_EQ(promises_in_registry(registry),
            (std::vector<PromiseSnapshot>{promise_in->snapshot()}));

  // make sure registry is cleaned up
  promise_in->mark_for_deletion();
}

// TEST_F(AsyncThreadRegistryDeathTest, another_thread_cannot_add_a_promise) {
//   GTEST_FLAG_SET(death_test_style, "threadsafe");
//   auto registry = ThreadRegistry::make(std::make_shared<Metrics>());

//   std::jthread([&]() {
//     EXPECT_DEATH(registry->add_promise(),
//                  "Assertion failed");
//   });
// }

TEST_F(AsyncThreadRegistryTest, iterates_over_all_promises) {
  auto registry = ThreadRegistry::make(std::make_shared<Metrics>());

  auto* first_promise = registry->add_promise();
  auto* second_promise = registry->add_promise();
  auto* third_promise = registry->add_promise();

  EXPECT_EQ(promises_in_registry(registry),
            (std::vector<PromiseSnapshot>{third_promise->snapshot(),
                                          second_promise->snapshot(),
                                          first_promise->snapshot()}));

  // make sure registry is cleaned up
  first_promise->mark_for_deletion();
  second_promise->mark_for_deletion();
  third_promise->mark_for_deletion();
}

TEST_F(AsyncThreadRegistryTest, iterates_in_another_thread_over_all_promises) {
  auto registry = ThreadRegistry::make(std::make_shared<Metrics>());

  auto* first_promise = registry->add_promise();
  auto* second_promise = registry->add_promise();
  auto* third_promise = registry->add_promise();

  std::thread([&]() {
    EXPECT_EQ(promises_in_registry(registry),
              (std::vector<PromiseSnapshot>{third_promise->snapshot(),
                                            second_promise->snapshot(),
                                            first_promise->snapshot()}));
  }).join();

  // make sure registry is cleaned up
  first_promise->mark_for_deletion();
  second_promise->mark_for_deletion();
  third_promise->mark_for_deletion();
}

TEST_F(AsyncThreadRegistryTest,
       marked_promises_are_deleted_in_garbage_collection) {
  auto registry = ThreadRegistry::make(std::make_shared<Metrics>());
  auto* promise_to_delete = registry->add_promise();
  auto* another_promise = registry->add_promise();

  promise_to_delete->mark_for_deletion();
  EXPECT_EQ(promises_in_registry(registry),
            (std::vector<PromiseSnapshot>{another_promise->snapshot(),
                                          promise_to_delete->snapshot()}));

  registry->garbage_collect();
  EXPECT_EQ(promises_in_registry(registry),
            (std::vector<PromiseSnapshot>{another_promise->snapshot()}));

  // make sure registry is cleaned up
  another_promise->mark_for_deletion();
}

TEST_F(AsyncThreadRegistryTest, garbage_collection_deletes_marked_promises) {
  {
    auto registry = ThreadRegistry::make(std::make_shared<Metrics>());
    auto* first_promise = registry->add_promise();
    auto* second_promise = registry->add_promise();
    auto* third_promise = registry->add_promise();

    first_promise->mark_for_deletion();
    registry->garbage_collect();

    EXPECT_EQ(promises_in_registry(registry),
              (std::vector<PromiseSnapshot>{third_promise->snapshot(),
                                            second_promise->snapshot()}));

    // clean up
    second_promise->mark_for_deletion();
    third_promise->mark_for_deletion();
  }
  {
    auto registry = ThreadRegistry::make(std::make_shared<Metrics>());
    auto* first_promise = registry->add_promise();
    auto* second_promise = registry->add_promise();
    auto* third_promise = registry->add_promise();

    second_promise->mark_for_deletion();
    registry->garbage_collect();

    EXPECT_EQ(promises_in_registry(registry),
              (std::vector<PromiseSnapshot>{third_promise->snapshot(),
                                            first_promise->snapshot()}));

    // clean up
    first_promise->mark_for_deletion();
    third_promise->mark_for_deletion();
  }
  {
    auto registry = ThreadRegistry::make(std::make_shared<Metrics>());
    auto* first_promise = registry->add_promise();
    auto* second_promise = registry->add_promise();
    auto* third_promise = registry->add_promise();

    third_promise->mark_for_deletion();
    registry->garbage_collect();

    EXPECT_EQ(promises_in_registry(registry),
              (std::vector<PromiseSnapshot>{second_promise->snapshot(),
                                            first_promise->snapshot()}));

    // clean up
    first_promise->mark_for_deletion();
    second_promise->mark_for_deletion();
  }
}

// TEST_F(AsyncThreadRegistryDeathTest,
//        unrelated_promise_cannot_be_marked_for_deletion) {
//   GTEST_FLAG_SET(death_test_style, "threadsafe");
//   auto registry = ThreadRegistry::make(std::make_shared<Metrics>());
//   auto some_other_registry =
//   ThreadRegistry::make(std::make_shared<Metrics>());

//   auto* promise =
//       some_other_registry->add_promise();

//   EXPECT_DEATH(registry->mark_for_deletion(promise), "Assertion failed");
// }

TEST_F(AsyncThreadRegistryTest,
       another_thread_can_mark_a_promise_for_deletion) {
  auto registry = ThreadRegistry::make(std::make_shared<Metrics>());

  auto* promise_to_delete = registry->add_promise();
  auto* another_promise = registry->add_promise();

  std::thread([&]() { promise_to_delete->mark_for_deletion(); }).join();

  registry->garbage_collect();
  EXPECT_EQ(promises_in_registry(registry),
            (std::vector<PromiseSnapshot>{another_promise->snapshot()}));

  // clean up
  another_promise->mark_for_deletion();
}

// TEST_F(AsyncThreadRegistryDeathTest,
//        garbage_collection_for_last_promises_can_be_called_on_different_thread)
//        {
//   {
//     auto registry = ThreadRegistry::make(std::make_shared<Metrics>());

//     std::jthread(
//         [&] { EXPECT_DEATH(registry->garbage_collect(), "Assertion failed");
//         });
//   }
// }

// TEST_F(AsyncThreadRegistryDeathTest,
//        garbage_collection_cannot_be_called_on_different_thread) {
//   GTEST_FLAG_SET(death_test_style, "threadsafe");

//   auto registry = ThreadRegistry::make(std::make_shared<Metrics>());

//   std::jthread(
//       [&] { EXPECT_DEATH(registry->garbage_collect(), "Assertion failed");
//       });
// }

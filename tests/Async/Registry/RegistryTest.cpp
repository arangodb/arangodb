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
#include "Async/Registry/promise.h"
#include "Async/Registry/registry.h"

#include <gtest/gtest.h>
#include <source_location>
#include <thread>

using namespace arangodb;
using namespace arangodb::async_registry;

namespace {

auto promises_in_registry(Registry& registry) -> std::vector<PromiseSnapshot> {
  std::vector<PromiseSnapshot> promises;
  registry.for_promise(
      [&](PromiseSnapshot promise) { promises.push_back(promise); });
  return promises;
}

}  // namespace

struct AsyncRegistryTest : ::testing::Test {};

TEST_F(AsyncRegistryTest, registers_promise_on_same_thread) {
  Registry registry;
  auto thread_registry = registry.add_thread();

  auto* promise = thread_registry->add_promise();

  EXPECT_EQ(promises_in_registry(registry),
            std::vector<PromiseSnapshot>{promise->snapshot()});

  promise->mark_for_deletion();
  thread_registry->garbage_collect();
}

TEST_F(AsyncRegistryTest, registers_promise_on_different_threads) {
  Registry registry;

  std::thread([&]() {
    auto thread_registry = registry.add_thread();

    auto* promise = thread_registry->add_promise();

    EXPECT_EQ(promises_in_registry(registry),
              std::vector<PromiseSnapshot>{promise->snapshot()});

    promise->mark_for_deletion();
    thread_registry->garbage_collect();
  }).join();
}

TEST_F(AsyncRegistryTest,
       iterates_over_promises_on_same_thread_in_reverse_order) {
  Registry registry;
  auto thread_registry = registry.add_thread();
  auto* first_promise = thread_registry->add_promise();
  auto* second_promise = thread_registry->add_promise();

  EXPECT_EQ(promises_in_registry(registry),
            (std::vector<PromiseSnapshot>{second_promise->snapshot(),
                                          first_promise->snapshot()}));

  first_promise->mark_for_deletion();
  second_promise->mark_for_deletion();
  thread_registry->garbage_collect();
  registry.remove_thread(thread_registry.get());
}

TEST_F(AsyncRegistryTest, iterates_over_promises_on_differen_threads) {
  Registry registry;
  auto thread_registry = registry.add_thread();
  auto* first_promise = thread_registry->add_promise();

  std::thread([&]() {
    auto thread_registry = registry.add_thread();
    auto* second_promise = thread_registry->add_promise();

    EXPECT_EQ(promises_in_registry(registry),
              (std::vector<PromiseSnapshot>{first_promise->snapshot(),
                                            second_promise->snapshot()}));

    second_promise->mark_for_deletion();
    thread_registry->garbage_collect();
  }).join();

  first_promise->mark_for_deletion();
  thread_registry->garbage_collect();
}

TEST_F(AsyncRegistryTest,
       iteration_after_executed_garbage_collection_is_empty) {
  Registry registry;
  auto thread_registry = registry.add_thread();

  auto* promise = thread_registry->add_promise();
  EXPECT_EQ(promises_in_registry(registry),
            (std::vector<PromiseSnapshot>{promise->snapshot()}));

  promise->mark_for_deletion();
  EXPECT_EQ(promises_in_registry(registry),
            (std::vector<PromiseSnapshot>{promise->snapshot()}));

  thread_registry->garbage_collect();
  EXPECT_EQ(promises_in_registry(registry), (std::vector<PromiseSnapshot>{}));
}

TEST_F(AsyncRegistryTest, promises_on_removed_thread_are_still_iterated_over) {
  Registry registry;
  Promise* promise;
  {
    auto thread_registry = registry.add_thread();
    promise = thread_registry->add_promise();
  }
  EXPECT_EQ(promises_in_registry(registry),
            (std::vector<PromiseSnapshot>{promise->snapshot()}));

  promise->mark_for_deletion();
}

TEST_F(
    AsyncRegistryTest,
    different_thread_can_mark_promise_for_deletion_after_thread_already_ended) {
  Registry registry;
  auto thread_registry = registry.add_thread();
  Promise* promise;

  std::thread([&]() {
    auto thread_registry_on_different_thread = registry.add_thread();
    promise = thread_registry_on_different_thread->add_promise(
        std::source_location::current());
  }).join();

  promise->mark_for_deletion();

  EXPECT_EQ(promises_in_registry(registry), (std::vector<PromiseSnapshot>{}));
}

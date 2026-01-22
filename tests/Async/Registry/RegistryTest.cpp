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
#include "Async/Registry/registry_variable.h"
#include "Containers/Concurrent/shared.h"
#include "Containers/Concurrent/thread.h"
#include "Basics/Thread.h"

#include <gtest/gtest.h>
#include <optional>
#include <source_location>
#include <thread>

using namespace arangodb;
using namespace arangodb::async_registry;

namespace {

auto promises_in_registry() -> std::vector<PromiseSnapshot> {
  std::vector<PromiseSnapshot> promises;
  registry.for_node(
      [&](PromiseSnapshot promise) { promises.push_back(promise); });
  return promises;
}

struct MyPromise : public AddToAsyncRegistry {
  basics::SourceLocationSnapshot source_location;
  basics::ThreadId thread_id;
  containers::SharedPtr<basics::ThreadInfo> thread;
  MyPromise(std::source_location loc = std::source_location::current())
      : AddToAsyncRegistry{loc},
        source_location{basics::SourceLocationSnapshot::from(std::move(loc))},
        thread_id{basics::ThreadId::current()},
        thread{basics::ThreadInfo::current()} {}
  auto snapshot(State state = State::Running) -> PromiseSnapshot {
    return PromiseSnapshot{.id = id().value(),
                           .owning_thread = thread.get_ref().value(),
                           .requester = {thread.get_ref().value()},
                           .state = state,
                           .thread = thread_id,
                           .source_location = source_location};
  }
};

}  // namespace

struct AsyncRegistryTest : ::testing::Test {
  void TearDown() override {
    // execute garbage collection on current thread
    get_thread_registry().garbage_collect();
  }
};

TEST_F(AsyncRegistryTest, registers_created_promise) {
  auto promise = MyPromise{};

  EXPECT_EQ(promises_in_registry(),
            (std::vector<PromiseSnapshot>{promise.snapshot()}));
}

TEST_F(AsyncRegistryTest, registers_promise_on_different_threads) {
  std::thread([&]() {
    auto promise = MyPromise{};

    EXPECT_EQ(promises_in_registry(),
              (std::vector<PromiseSnapshot>{promise.snapshot()}));
    // cleans up by itself
  }).join();

  EXPECT_EQ(promises_in_registry(), std::vector<PromiseSnapshot>{});
}

TEST_F(AsyncRegistryTest,
       iterates_over_promises_on_same_thread_in_reverse_order) {
  auto first_promise = MyPromise{};
  auto second_promise = MyPromise{};

  EXPECT_EQ(promises_in_registry(),
            (std::vector<PromiseSnapshot>{second_promise.snapshot(),
                                          first_promise.snapshot()}));
}

TEST_F(AsyncRegistryTest, iterates_over_promises_on_differen_threads) {
  auto outer_thread_promise = MyPromise{};

  std::thread([&]() {
    auto inner_thread_promise = MyPromise{};

    EXPECT_EQ(promises_in_registry(),
              (std::vector<PromiseSnapshot>{outer_thread_promise.snapshot(),
                                            inner_thread_promise.snapshot()}));
  }).join();

  EXPECT_EQ(promises_in_registry(),
            (std::vector<PromiseSnapshot>{outer_thread_promise.snapshot()}));
}

TEST_F(
    AsyncRegistryTest,
    marks_deleted_promise_for_deletion_which_is_deleted_in_garbage_collection) {
  PromiseSnapshot promise_in_registry;
  {
    auto promise = MyPromise{};
    auto promises = promises_in_registry();
    EXPECT_EQ(promises, (std::vector<PromiseSnapshot>{promise.snapshot()}));
    promise_in_registry = promises[0];

    get_thread_registry()
        .garbage_collect();  // does not do anything because nothing
                             // is yet marked for deletion
    EXPECT_EQ(promises_in_registry(),
              (std::vector<PromiseSnapshot>{promise.snapshot()}));
  }  // marks promise for deletion

  promise_in_registry.state = State::Deleted;
  EXPECT_EQ(promises_in_registry(),
            (std::vector<PromiseSnapshot>{promise_in_registry}));

  get_thread_registry().garbage_collect();
  EXPECT_EQ(promises_in_registry(), (std::vector<PromiseSnapshot>{}));
}

TEST_F(AsyncRegistryTest, sets_running_thread_to_current_thread_when_running) {
  auto promise = MyPromise{};
  auto all_promises = promises_in_registry();
  EXPECT_EQ(all_promises.size(), 1);
  EXPECT_EQ(all_promises[0].state, State::Running);
  EXPECT_EQ(all_promises[0].thread, basics::ThreadId::current());

  promise.update_state(State::Suspended);
  all_promises = promises_in_registry();
  EXPECT_EQ(all_promises[0].state, State::Suspended);
  EXPECT_EQ(all_promises[0].thread, std::nullopt);

  promise.update_state(State::Running);
  all_promises = promises_in_registry();
  EXPECT_EQ(all_promises[0].state, State::Running);
  EXPECT_EQ(all_promises[0].thread, basics::ThreadId::current());

  promise.update_state(State::Resolved);
  all_promises = promises_in_registry();
  EXPECT_EQ(all_promises[0].state, State::Resolved);
  EXPECT_EQ(all_promises[0].thread, std::nullopt);

  promise.update_state(State::Running);
  all_promises = promises_in_registry();
  EXPECT_EQ(all_promises[0].state, State::Running);
  EXPECT_EQ(all_promises[0].thread, basics::ThreadId::current());

  promise.update_state(State::Deleted);
  all_promises = promises_in_registry();
  EXPECT_EQ(all_promises[0].state, State::Deleted);
  EXPECT_EQ(all_promises[0].thread, std::nullopt);

  promise.update_state(State::Running);
  all_promises = promises_in_registry();
  EXPECT_EQ(all_promises[0].state, State::Running);
  EXPECT_EQ(all_promises[0].thread, basics::ThreadId::current());
}

TEST_F(AsyncRegistryTest, inpection_works_on_after_thread_was_deleted) {
  PromiseSnapshot promise_snapshot;
  std::ignore = std::jthread([&promise_snapshot]() {
    auto promise = MyPromise{};
    promise_snapshot = promise.snapshot();
  });

  // we just make sure that we can still inspect the promise (and it does not
  // crash the system), although the thread the promise was created on is gone
  EXPECT_NE(fmt::format("{}", inspection::json(promise_snapshot)), "");
}

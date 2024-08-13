#include "Async/Registry/promise.h"
#include "Async/Registry/thread_registry.h"

#include <gtest/gtest.h>
#include <cstdint>
#include <source_location>
#include <thread>

using namespace arangodb::async_registry;

namespace {

struct MyTestPromise : PromiseInList {
  MyTestPromise(uint64_t id,
                std::source_location loc = std::source_location::current())
      : PromiseInList(loc), id{id} {}
  auto destroy() noexcept -> void override { destroyed = true; }
  bool destroyed = false;
  uint64_t id;
};

auto all_ids(ThreadRegistry* registry) -> std::vector<uint64_t> {
  std::vector<uint64_t> ids;
  registry->for_promise([&](PromiseInList* promise) {
    ids.push_back(dynamic_cast<MyTestPromise*>(promise)->id);
  });
  return ids;
}
}  // namespace

struct CoroutineThreadRegistryTest : ::testing::Test {};
using CoroutineThreadRegistryDeathTest = CoroutineThreadRegistryTest;

TEST_F(CoroutineThreadRegistryTest, adds_a_promise) {
  auto registry = ThreadRegistry::make();

  auto promise = MyTestPromise{1};
  registry->add(&promise);

  EXPECT_EQ(all_ids(registry), std::vector<uint64_t>{promise.id});

  // make sure registry is cleaned up
  registry->mark_for_deletion(&promise);
}

// TEST_F(CoroutineThreadRegistryDeathTest, another_thread_cannot_add_a_promise)
// {
//   GTEST_FLAG_SET(death_test_style, "threadsafe");
//   auto registry = ThreadRegistry::make();

//   std::jthread([&]() {
//     auto promise = MyTestPromise{1};

//     EXPECT_DEATH(registry->add(&promise), "Assertion failed");
//   });
// }

TEST_F(CoroutineThreadRegistryTest, iterates_over_all_promises) {
  auto registry = ThreadRegistry::make();
  auto first_promise = MyTestPromise{1};
  registry->add(&first_promise);
  auto second_promise = MyTestPromise{2};
  registry->add(&second_promise);
  auto third_promise = MyTestPromise{3};
  registry->add(&third_promise);

  std::vector<uint64_t> promise_ids;
  registry->for_promise([&](PromiseInList* promise) {
    promise_ids.push_back(dynamic_cast<MyTestPromise*>(promise)->id);
  });

  EXPECT_EQ(promise_ids,
            (std::vector<uint64_t>{third_promise.id, second_promise.id,
                                   first_promise.id}));

  // make sure registry is cleaned up
  registry->mark_for_deletion(&first_promise);
  registry->mark_for_deletion(&second_promise);
  registry->mark_for_deletion(&third_promise);
}

TEST_F(CoroutineThreadRegistryTest,
       iterates_in_another_thread_over_all_promises) {
  auto registry = ThreadRegistry::make();
  auto first_promise = MyTestPromise{1};
  registry->add(&first_promise);
  auto second_promise = MyTestPromise{2};
  registry->add(&second_promise);
  auto third_promise = MyTestPromise{3};
  registry->add(&third_promise);

  std::jthread([&]() {
    std::vector<uint64_t> promise_ids;
    registry->for_promise([&](PromiseInList* promise) {
      promise_ids.push_back(dynamic_cast<MyTestPromise*>(promise)->id);
    });
    EXPECT_EQ(promise_ids,
              (std::vector<uint64_t>{third_promise.id, second_promise.id,
                                     first_promise.id}));
  });

  // make sure registry is cleaned up
  registry->mark_for_deletion(&first_promise);
  registry->mark_for_deletion(&second_promise);
  registry->mark_for_deletion(&third_promise);
}

TEST_F(CoroutineThreadRegistryTest,
       marked_promises_are_deleted_in_garbage_collection) {
  auto registry = ThreadRegistry::make();
  auto promise_to_delete = MyTestPromise{1};
  registry->add(&promise_to_delete);
  auto another_promise = MyTestPromise{2};
  registry->add(&another_promise);

  registry->mark_for_deletion(&promise_to_delete);

  EXPECT_FALSE(promise_to_delete.destroyed);
  EXPECT_EQ(all_ids(registry),
            (std::vector<uint64_t>{another_promise.id, promise_to_delete.id}));

  registry->garbage_collect();
  EXPECT_TRUE(promise_to_delete.destroyed);
  EXPECT_EQ(all_ids(registry), (std::vector<uint64_t>{another_promise.id}));

  // make sure registry is cleaned up
  registry->mark_for_deletion(&another_promise);
}

TEST_F(CoroutineThreadRegistryTest,
       last_marked_promise_runs_garbage_collection_and_deletes_registry) {
  auto registry = ThreadRegistry::make();
  auto promise = MyTestPromise{1};
  registry->add(&promise);

  registry->mark_for_deletion(&promise);

  EXPECT_TRUE(promise.destroyed);

  // now registry is deleted
}

TEST_F(CoroutineThreadRegistryTest,
       garbage_collection_deletes_marked_promises) {
  {
    auto registry = ThreadRegistry::make();
    auto first_promise = MyTestPromise{1};
    registry->add(&first_promise);
    auto second_promise = MyTestPromise{2};
    registry->add(&second_promise);
    auto third_promise = MyTestPromise{3};
    registry->add(&third_promise);
    EXPECT_EQ(all_ids(registry),
              (std::vector<uint64_t>{third_promise.id, second_promise.id,
                                     first_promise.id}));

    registry->mark_for_deletion(&first_promise);
    registry->garbage_collect();

    EXPECT_EQ(all_ids(registry),
              (std::vector<uint64_t>{third_promise.id, second_promise.id}));

    // clean up
    registry->mark_for_deletion(&second_promise);
    registry->mark_for_deletion(&third_promise);
  }
  {
    auto registry = ThreadRegistry::make();
    auto first_promise = MyTestPromise{1};
    registry->add(&first_promise);
    auto second_promise = MyTestPromise{2};
    registry->add(&second_promise);
    auto third_promise = MyTestPromise{3};
    registry->add(&third_promise);
    EXPECT_EQ(all_ids(registry),
              (std::vector<uint64_t>{third_promise.id, second_promise.id,
                                     first_promise.id}));

    registry->mark_for_deletion(&second_promise);
    registry->garbage_collect();

    EXPECT_EQ(all_ids(registry),
              (std::vector<uint64_t>{third_promise.id, first_promise.id}));

    // clean up
    registry->mark_for_deletion(&first_promise);
    registry->mark_for_deletion(&third_promise);
  }
  {
    auto registry = ThreadRegistry::make();
    auto first_promise = MyTestPromise{1};
    registry->add(&first_promise);
    auto second_promise = MyTestPromise{2};
    registry->add(&second_promise);
    auto third_promise = MyTestPromise{3};
    registry->add(&third_promise);
    EXPECT_EQ(all_ids(registry),
              (std::vector<uint64_t>{third_promise.id, second_promise.id,
                                     first_promise.id}));

    registry->mark_for_deletion(&third_promise);
    registry->garbage_collect();

    EXPECT_EQ(all_ids(registry),
              (std::vector<uint64_t>{second_promise.id, first_promise.id}));

    // clean up
    registry->mark_for_deletion(&first_promise);
    registry->mark_for_deletion(&second_promise);
  }
}

// TEST_F(CoroutineThreadRegistryDeathTest,
//        unrelated_promise_cannot_be_marked_for_deletion) {
//   GTEST_FLAG_SET(death_test_style, "threadsafe");

//   auto registry = ThreadRegistry::make();
//   auto promise = MyTestPromise{1};

//   EXPECT_DEATH(registry->mark_for_deletion(&promise), "Assertion failed");
// }

TEST_F(CoroutineThreadRegistryTest,
       another_thread_can_mark_a_promise_for_deletion) {
  auto registry = ThreadRegistry::make();
  auto promise_to_delete = MyTestPromise{1};
  registry->add(&promise_to_delete);
  auto another_promise = MyTestPromise{2};
  registry->add(&another_promise);

  std::jthread([&]() {
    registry->mark_for_deletion(&promise_to_delete);
  }).join();
  registry->garbage_collect();

  EXPECT_EQ(all_ids(registry), (std::vector<uint64_t>{another_promise.id}));

  // clean up
  registry->mark_for_deletion(&another_promise);
}

// TEST_F(CoroutineThreadRegistryDeathTest,
//        garbage_collection_for_last_promises_can_be_called_on_different_thread)
//        {
//   GTEST_FLAG_SET(death_test_style, "threadsafe");

//   {
//     auto registry = ThreadRegistry::make();

//     std::jthread([&] { registry->garbage_collect(); });
//   }
// }

// TEST_F(CoroutineThreadRegistryDeathTest,
//        garbage_collection_cannot_be_called_on_different_thread) {
//   GTEST_FLAG_SET(death_test_style, "threadsafe");

//   auto registry = ThreadRegistry::make();
//   auto promise = MyTestPromise{1};
//   registry->add(&promise);

//   std::jthread(
//       [&] { EXPECT_DEATH(registry->garbage_collect(), "Assertion failed");
//       });
// }

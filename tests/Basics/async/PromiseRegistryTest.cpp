#include "Basics/async/promise.hpp"
#include "Basics/async/promise_registry.hpp"
#include "Basics/Result.h"

#include <gtest/gtest.h>
#include <cstdint>
#include <source_location>
#include <thread>

using namespace arangodb::coroutine;

namespace {
struct MyTestPromise : PromiseInList {
  MyTestPromise(uint64_t id,
                std::source_location loc = std::source_location::current())
      : PromiseInList(loc), id{id} {}
  auto destroy() -> void override { destroyed = true; }
  bool destroyed = false;
  uint64_t id;
};

auto all_ids(PromiseRegistry& registry) -> std::vector<uint64_t> {
  std::vector<uint64_t> ids;
  registry.for_promise([&](PromiseInList* promise) {
    ids.push_back(dynamic_cast<MyTestPromise*>(promise)->id);
  });
  return ids;
}
}  // namespace

TEST(PromiseRegistryTest, adds_a_promise) {
  auto registry = PromiseRegistry{};

  auto promise = MyTestPromise{1};
  registry.add(&promise);

  EXPECT_EQ(all_ids(registry), std::vector<uint64_t>{promise.id});
}

TEST(PromiseRegistryDeathTest, another_thread_cannot_add_a_promise) {
  GTEST_FLAG_SET(death_test_style, "threadsafe");
  auto registry = PromiseRegistry{};

  std::jthread([&]() {
    auto promise = MyTestPromise{1};

    EXPECT_DEATH(registry.add(&promise), "Assertion failed");
  });
}

TEST(PromiseRegistryTest, iterates_over_all_promises) {
  auto registry = PromiseRegistry{};
  auto first_promise = MyTestPromise{1};
  registry.add(&first_promise);
  auto second_promise = MyTestPromise{2};
  registry.add(&second_promise);
  auto third_promise = MyTestPromise{3};
  registry.add(&third_promise);

  std::vector<uint64_t> promise_ids;
  registry.for_promise([&](PromiseInList* promise) {
    promise_ids.push_back(dynamic_cast<MyTestPromise*>(promise)->id);
  });

  EXPECT_EQ(promise_ids,
            (std::vector<uint64_t>{third_promise.id, second_promise.id,
                                   first_promise.id}));
}

TEST(PromiseRegistryTest, iterates_in_another_thread_over_all_promises) {
  auto registry = PromiseRegistry{};
  auto first_promise = MyTestPromise{1};
  registry.add(&first_promise);
  auto second_promise = MyTestPromise{2};
  registry.add(&second_promise);
  auto third_promise = MyTestPromise{3};
  registry.add(&third_promise);

  std::jthread([&]() {
    std::vector<uint64_t> promise_ids;
    registry.for_promise([&](PromiseInList* promise) {
      promise_ids.push_back(dynamic_cast<MyTestPromise*>(promise)->id);
    });
    EXPECT_EQ(promise_ids,
              (std::vector<uint64_t>{third_promise.id, second_promise.id,
                                     first_promise.id}));
  });
}

TEST(PromiseRegistryTest, mark_for_deletion_does_not_delete_promise) {
  auto registry = PromiseRegistry{};
  auto promise = MyTestPromise{1};
  registry.add(&promise);

  registry.mark_for_deletion(&promise);

  EXPECT_EQ(all_ids(registry).size(), 1);
}

TEST(PromiseRegistryTest, garbage_collection_deletes_marked_promises) {
  auto registry = PromiseRegistry{};
  auto promise = MyTestPromise{1};
  registry.add(&promise);

  registry.mark_for_deletion(&promise);
  registry.garbage_collect();

  EXPECT_EQ(all_ids(registry).size(), 0);
}

TEST(PromiseRegistryTest,
     garbage_collection_does_not_delete_unmarked_promises) {
  auto registry = PromiseRegistry{};
  auto promise = MyTestPromise{1};
  registry.add(&promise);

  registry.garbage_collect();

  EXPECT_EQ(all_ids(registry).size(), 1);
}

TEST(PromiseRegistryDeathTest,
     unrelated_promise_cannot_be_marked_for_deletion) {
  GTEST_FLAG_SET(death_test_style, "threadsafe");

  auto registry = PromiseRegistry{};
  auto promise = MyTestPromise{1};

  EXPECT_DEATH(registry.mark_for_deletion(&promise), "Assertion failed");
}

TEST(PromiseRegistryTest, another_thread_can_mark_a_promise_for_deletion) {
  auto registry = PromiseRegistry{};
  auto promise = MyTestPromise{1};
  registry.add(&promise);

  std::jthread([&]() { registry.mark_for_deletion(&promise); }).join();
  registry.garbage_collect();

  EXPECT_EQ(all_ids(registry).size(), 0);
}

TEST(PromiseRegistryDeathTest,
     garbage_collection_cannot_be_called_on_different_thread) {
  GTEST_FLAG_SET(death_test_style, "threadsafe");

  auto registry = PromiseRegistry{};

  std::jthread(
      [&] { EXPECT_DEATH(registry.garbage_collect(), "Assertion failed"); });
}

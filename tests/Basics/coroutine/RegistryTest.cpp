#include "Basics/coroutine/promise.h"
#include "Basics/coroutine/registry.h"

#include <gtest/gtest.h>
#include <thread>

using namespace arangodb;
using namespace arangodb::coroutine;

namespace {

struct MyTestPromise : PromiseInList {
  MyTestPromise(uint64_t id,
                std::source_location loc = std::source_location::current())
      : PromiseInList(loc), id{id} {}
  auto destroy() noexcept -> void override { destroyed = true; }
  bool destroyed = false;
  uint64_t id;
};

auto all_ids(Registry& registry) -> std::vector<uint64_t> {
  std::vector<uint64_t> ids;
  registry.for_promise([&](PromiseInList* promise) {
    ids.push_back(dynamic_cast<MyTestPromise*>(promise)->id);
  });
  return ids;
}

}  // namespace


struct CoroutineRegistryTest : ::testing::Test {
  void SetUp() override {}

  void TearDown() override {
  }
};
using CoroutineRegistryDeathTest = CoroutineRegistryTest;

TEST_F(CoroutineRegistryTest, registers_promise_on_same_thread) {
  Registry registry;
  auto thread_registry = registry.add_thread();

  auto promise = MyTestPromise{1};
  thread_registry->add(&promise);

  EXPECT_EQ(all_ids(registry), std::vector<uint64_t>{1});

  thread_registry->mark_for_deletion(&promise);
  thread_registry->garbage_collect();
  registry.remove_thread(thread_registry);
}

TEST_F(CoroutineRegistryTest, registers_promise_on_different_threads) {
  Registry registry;

  std::jthread([&]() {
    auto thread_registry = registry.add_thread();

    auto promise = MyTestPromise{1};
    thread_registry->add(&promise);

    EXPECT_EQ(all_ids(registry), std::vector<uint64_t>{1});

    thread_registry->mark_for_deletion(&promise);
    thread_registry->garbage_collect();
    registry.remove_thread(thread_registry);
  });
}

TEST_F(CoroutineRegistryTest,
       iterates_over_promises_on_same_thread_in_reverse_order) {
  Registry registry;
  auto thread_registry = registry.add_thread();
  auto first_promise = MyTestPromise{1};
  thread_registry->add(&first_promise);
  auto second_promise = MyTestPromise{2};
  thread_registry->add(&second_promise);

  EXPECT_EQ(all_ids(registry), (std::vector<uint64_t>{2, 1}));

  thread_registry->mark_for_deletion(&first_promise);
  thread_registry->mark_for_deletion(&second_promise);
  thread_registry->garbage_collect();
  registry.remove_thread(thread_registry);
}

TEST_F(CoroutineRegistryTest, iterates_over_promises_on_differen_threads) {
  Registry registry;
  auto thread_registry = registry.add_thread();
  auto first_promise = MyTestPromise{1};
  thread_registry->add(&first_promise);

  std::jthread([&]() {
    auto thread_registry = registry.add_thread();
    auto second_promise = MyTestPromise{2};
    thread_registry->add(&second_promise);

    EXPECT_EQ(all_ids(registry), (std::vector<uint64_t>{1, 2}));

    thread_registry->mark_for_deletion(&second_promise);
    thread_registry->garbage_collect();
    registry.remove_thread(thread_registry);
  });
  thread_registry->mark_for_deletion(&first_promise);
  thread_registry->garbage_collect();
  registry.remove_thread(thread_registry);
}

TEST_F(CoroutineRegistryTest,
       iteration_after_executed_garbage_collection_is_empty) {
  Registry registry;
  auto thread_registry = registry.add_thread();
  auto promise = MyTestPromise{1};
  thread_registry->add(&promise);

  EXPECT_EQ(all_ids(registry), (std::vector<uint64_t>{1}));

  thread_registry->mark_for_deletion(&promise);

  EXPECT_FALSE(promise.destroyed);
  EXPECT_EQ(all_ids(registry), (std::vector<uint64_t>{1}));

  thread_registry->garbage_collect();

  EXPECT_TRUE(promise.destroyed);
  EXPECT_EQ(all_ids(registry).size(), 0);

  registry.remove_thread(thread_registry);
}

TEST_F(CoroutineRegistryTest,
       promises_on_removed_thread_dont_show_in_iteration_but_are_not_destroyed_automatically) {
  Registry registry;
  auto thread_registry = registry.add_thread();
  auto promise = MyTestPromise{1};
  thread_registry->add(&promise);

  EXPECT_EQ(all_ids(registry), (std::vector<uint64_t>{1}));

  registry.remove_thread(thread_registry);

  EXPECT_FALSE(promise.destroyed);
  EXPECT_EQ(all_ids(registry), (std::vector<uint64_t>{}));

  thread_registry->mark_for_deletion(&promise);
  thread_registry->garbage_collect();
}

// TODO necessary?
TEST_F(CoroutineRegistryTest,
       removing_a_thread_does_not_invalidate_other_threads) {
  Registry registry;
  auto first_thread_registry = registry.add_thread();
  auto second_thread_registry = registry.add_thread();
  auto first_promise = MyTestPromise{1};
  second_thread_registry->add(&first_promise);

  registry.remove_thread(first_thread_registry);

  auto second_promise = MyTestPromise{2};
  second_thread_registry->add(&second_promise);
  EXPECT_EQ(all_ids(registry), (std::vector<uint64_t>{2, 1}));

  second_thread_registry->mark_for_deletion(&first_promise);
  second_thread_registry->mark_for_deletion(&second_promise);
  second_thread_registry->garbage_collect();
  registry.remove_thread(second_thread_registry);
}

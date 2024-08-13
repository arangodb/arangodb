#include "Async/Registry/promise.h"
#include "Async/Registry/registry.h"

#include <gtest/gtest.h>
#include <thread>

using namespace arangodb;
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

  void TearDown() override {}
};

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

TEST_F(
    CoroutineRegistryTest,
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
  EXPECT_TRUE(promise.destroyed);
}

TEST_F(CoroutineRegistryTest,
       different_thread_deletes_promise_after_thread_already_ended) {
  Registry registry;
  ThreadRegistry* thread_registry_on_different_thread;
  auto promise = MyTestPromise{1};

  std::jthread([&]() {
    thread_registry_on_different_thread = registry.add_thread();
    thread_registry_on_different_thread->add(&promise);
    registry.remove_thread(thread_registry_on_different_thread);
  });

  EXPECT_EQ(all_ids(registry).size(), 0);
  EXPECT_FALSE(promise.destroyed);

  thread_registry_on_different_thread->mark_for_deletion(&promise);
  EXPECT_TRUE(promise.destroyed);
}

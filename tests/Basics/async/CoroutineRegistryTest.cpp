#include "Basics/async/async.h"
#include "Basics/async/promise.hpp"
#include "Basics/async/thread_registry.hpp"
#include <gtest/gtest.h>
#include <thread>

using namespace arangodb;
using namespace arangodb::coroutine;

namespace {

auto foo() -> async<int> { co_return 1; }
auto bar() -> async<int> { co_return 4; }
auto baz() -> async<int> { co_return 2; }

}  // namespace

TEST(CoroutineRegistryTest, includes_current_coroutine) {
  ThreadRegistryForPromises thread_registry;
  thread_registry.create();
  auto coro = foo();
  std::vector<uint> promise_lines;
  thread_registry.for_promise([&](coroutine::PromiseInList* promise) {
    promise_lines.push_back(promise->_where.line());
  });
  EXPECT_EQ(promise_lines, std::vector<uint>{12});
}

TEST(CoroutineRegistryTest, includes_several_coroutines) {
  ThreadRegistryForPromises thread_registry;
  thread_registry.create();
  auto f = foo();
  auto b = bar();
  std::vector<uint> promise_lines;
  thread_registry.for_promise([&](coroutine::PromiseInList* promise) {
    promise_lines.push_back(promise->_where.line());
  });
  EXPECT_EQ(promise_lines, std::vector<uint>({13, 12}));
}

TEST(CoroutineRegistryTest, includes_coroutines_running_on_differen_threads) {
  ThreadRegistryForPromises thread_registry;
  thread_registry.create();

  auto f = foo();
  auto b = bar();

  std::jthread([&]() {
    thread_registry.create();

    auto z = baz();

    // we are sure that all threads still exist
    std::vector<uint> promise_lines;
    thread_registry.for_promise([&](coroutine::PromiseInList* promise) {
      promise_lines.push_back(promise->_where.line());
    });

    EXPECT_EQ(promise_lines, std::vector<uint>({14, 13, 12}));
  });
}

TEST(CoroutineRegistryTest,
     includes_coroutines_of_deleted_threads_before_garbage_collection) {
  ThreadRegistryForPromises thread_registry;
  thread_registry.create();

  auto f = foo();
  auto b = bar();

  std::jthread([&]() {
    thread_registry.create();

    auto z = baz();
  }).join();

  std::vector<uint> promise_lines;
  thread_registry.for_promise([&](coroutine::PromiseInList* promise) {
    promise_lines.push_back(promise->_where.line());
  });

  EXPECT_EQ(promise_lines.size(), 3);
}

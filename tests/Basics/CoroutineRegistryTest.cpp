#include "Basics/async/async.h"
#include "Basics/async/promise.hpp"
#include "Basics/async/thread_registry.hpp"
#include <gtest/gtest.h>
#include <thread>

using namespace arangodb;

std::vector<uint> promise_lines;
auto collect_promise_line(coroutine::PromiseInList* promise) -> void {
  promise_lines.push_back(promise->_where.line());
}

namespace {

struct CoroutineRegistryTest : ::testing::Test {
  coroutine::ThreadRegistryForPromises thread_registry;
  void SetUp() override { promise_lines = std::vector<uint>(); }
};

auto foo() -> async<int> { co_return 1; }
auto bar() -> async<int> { co_return 4; }
auto baz() -> async<int> { co_return 2; }

}  // namespace

TEST_F(CoroutineRegistryTest, includes_current_coroutine) {
  thread_registry.create();
  // auto coro = []() -> async<int> { co_return 1; }();
  auto coro = foo();
  thread_registry.for_promise(collect_promise_line);
  EXPECT_EQ(promise_lines, std::vector<uint>{21});
}

TEST_F(CoroutineRegistryTest, includes_several_coroutines) {
  thread_registry.create();
  // auto coro = []() -> async<int> { co_return 1; }();
  auto f = foo();
  auto b = bar();
  thread_registry.for_promise(collect_promise_line);
  EXPECT_EQ(promise_lines, std::vector<uint>({22, 21}));
}

TEST_F(CoroutineRegistryTest, includes_coroutines_running_on_differen_threads) {
  thread_registry.create();

  auto f = foo();
  auto b = bar();

  std::jthread([this]() {
    thread_registry.create();

    auto z = baz();

    // we are sure that all threads still exist
    thread_registry.for_promise(collect_promise_line);

    EXPECT_EQ(promise_lines, std::vector<uint>({23, 22, 21}));
  });
}

TEST_F(CoroutineRegistryTest,
       includes_coroutines_of_deleted_threads_before_garbage_collection) {
  thread_registry.create();

  auto f = foo();
  auto b = bar();

  std::jthread([this]() {
    thread_registry.create();

    auto z = baz();
  }).join();

  thread_registry.for_promise(collect_promise_line);

  EXPECT_EQ(promise_lines.size(), 3)
}

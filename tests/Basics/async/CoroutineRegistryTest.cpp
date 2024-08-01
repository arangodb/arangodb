#include "Basics/async/async.h"
#include "Basics/async/promise.hpp"
#include "Basics/async/thread_registry.hpp"
#include <gtest/gtest.h>
#include <thread>

using namespace arangodb;
using namespace arangodb::coroutine;

namespace coroutine_test {

auto foo() -> async<int> { co_return 1; }
auto bar() -> async<int> { co_return 4; }
auto baz() -> async<int> { co_return 2; }

}  // namespace coroutine_test

namespace {

auto all_function_names(ThreadRegistryForPromises& registry)
    -> std::vector<std::string> {
  std::vector<std::string> names;
  registry.for_promise([&](PromiseInList* promise) {
    names.push_back(promise->_where.function_name());
  });
  return names;
}

}  // namespace

TEST(CoroutineRegistryTest, registers_one_coroutine) {
  ThreadRegistryForPromises registry;
  registry.add_thread();

  auto coro = coroutine_test::foo();

  EXPECT_EQ(all_function_names(registry),
            std::vector<std::string>{"async<int> coroutine_test::foo()"});
}

TEST(CoroutineRegistryTest, registers_coroutines_running_on_differen_threads) {
  ThreadRegistryForPromises registry;
  registry.add_thread();

  std::jthread([&]() {
    registry.add_thread();

    auto z = coroutine_test::foo();

    EXPECT_EQ(all_function_names(registry),
              std::vector<std::string>({"async<int> coroutine_test::foo()"}));
  });
}

TEST(CoroutineRegistryTest,
     iterates_over_coroutines_on_same_thread_in_reverse_order) {
  ThreadRegistryForPromises registry;
  registry.add_thread();
  auto f = coroutine_test::foo();
  auto b = coroutine_test::bar();

  std::vector<std::string> function_names;
  registry.for_promise([&](PromiseInList* promise) {
    function_names.push_back(promise->_where.function_name());
  });

  EXPECT_EQ(function_names,
            std::vector<std::string>({"async<int> coroutine_test::bar()",
                                      "async<int> coroutine_test::foo()"}));
}

TEST(CoroutineRegistryTest, iterates_over_coroutines_on_differen_threads) {
  ThreadRegistryForPromises registry;
  registry.add_thread();
  auto f = coroutine_test::foo();

  std::jthread([&]() {
    registry.add_thread();
    auto b = coroutine_test::bar();

    std::vector<std::string> function_names;
    registry.for_promise([&](PromiseInList* promise) {
      function_names.push_back(promise->_where.function_name());
    });

    EXPECT_EQ(function_names,
              std::vector<std::string>({"async<int> coroutine_test::foo()",
                                        "async<int> coroutine_test::bar()"}));
  });
}

// TODO currently we cannot delete threads
// TEST(CoroutineRegistryTest,
//      includes_coroutines_of_deleted_threads_before_garbage_collection) {
//   ThreadRegistryForPromises registry;
//   registry.add_thread();

//   auto f = coroutine_test::foo();
//   auto b = coroutine_test::bar();

//   std::jthread([&]() {
//     registry.add_thread();

//     auto z = coroutine_test::baz();
//   }).join();

//   EXPECT_EQ(all_function_names(registry).size(), 3);
// }

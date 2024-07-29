#include <gtest/gtest.h>
#include <source_location>
#include "Basics/async/promise.hpp"
#include "Basics/async/promise_registry.hpp"

using namespace arangodb::coroutine;

TEST(PromiseRegistryTest, adds_a_promise_to_an_empty_list) {
  auto promise = PromiseInList(std::source_location::current());
  auto promise_registry = PromiseRegistryOnThread{};

  promise_registry.add(&promise);

  // thread_registry.create();
  // auto coro = foo();
  // thread_registry.for_promise(collect_promise_line);
  // EXPECT_EQ(promise_lines, std::vector<uint>{21});
}

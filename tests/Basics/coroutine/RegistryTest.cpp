#include "Basics/async.h"
#include "Basics/coroutine/promise.h"
#include "Basics/coroutine/registry.h"
#include "gtest/gtest.h"
#include <gtest/gtest.h>
#include <thread>

using namespace arangodb;
using namespace arangodb::coroutine;

namespace {

struct InstanceCounterValue {
  InstanceCounterValue() { instanceCounter += 1; }
  InstanceCounterValue(InstanceCounterValue const& o) { instanceCounter += 1; }
  InstanceCounterValue(InstanceCounterValue&& o) noexcept {
    instanceCounter += 1;
  }

  ~InstanceCounterValue() {
    if (instanceCounter == 0) {
      abort();
    }
    instanceCounter -= 1;
  }
  static std::size_t instanceCounter;
};

std::size_t InstanceCounterValue::instanceCounter = 0;

struct MyInt : InstanceCounterValue {
  MyInt(int x) : x(x) {}
  MyInt(MyInt const& x) : x(x) {}

  auto operator<=>(int y) const noexcept { return x <=> y; }
  operator int() const noexcept { return x; }
  int x;
};

auto all_function_names(Registry& registry) -> std::vector<std::string> {
  std::vector<std::string> names;
  registry.for_promise([&](PromiseInList* promise) {
    names.push_back(promise->_where.function_name());
  });
  return names;
}

}  // namespace

namespace coroutine_test {

auto foo() -> async<MyInt> { co_return 1; }
auto bar() -> async<MyInt> { co_return 4; }

}  // namespace coroutine_test

struct CoroutineRegistryTest : ::testing::Test {
  void SetUp() override { InstanceCounterValue::instanceCounter = 0; }

  void TearDown() override {
    EXPECT_EQ(InstanceCounterValue::instanceCounter, 0);
  }
};
using CoroutineRegistryDeathTest = CoroutineRegistryTest;

TEST_F(CoroutineRegistryDeathTest, thread_requires_a_coroutine_registry) {
  Registry registry;

  EXPECT_EXIT(coroutine_test::foo(), testing::KilledBySignal(SIGSEGV), "");
}

TEST_F(CoroutineRegistryTest, registers_one_coroutine) {
  Registry registry;
  registry.add_thread();

  auto coro = coroutine_test::foo();

  EXPECT_EQ(all_function_names(registry),
            std::vector<std::string>{"async<MyInt> coroutine_test::foo()"});

  coro.reset();
  arangodb::coroutine::thread_registry->garbage_collect();
}

TEST_F(CoroutineRegistryTest, registers_coroutines_running_on_other_threads) {
  Registry registry;
  registry.add_thread();

  std::jthread([&]() {
    registry.add_thread();

    auto coro = coroutine_test::foo();

    EXPECT_EQ(all_function_names(registry),
              std::vector<std::string>({"async<MyInt> coroutine_test::foo()"}));

    coro.reset();
    arangodb::coroutine::thread_registry->garbage_collect();
  });
}

TEST_F(CoroutineRegistryTest,
       iterates_over_coroutines_on_same_thread_in_reverse_order) {
  Registry registry;
  registry.add_thread();
  auto foo = coroutine_test::foo();
  auto bar = coroutine_test::bar();

  std::vector<std::string> function_names;
  registry.for_promise([&](PromiseInList* promise) {
    function_names.push_back(promise->_where.function_name());
  });

  EXPECT_EQ(function_names,
            std::vector<std::string>({"async<MyInt> coroutine_test::bar()",
                                      "async<MyInt> coroutine_test::foo()"}));
  foo.reset();
  bar.reset();
  arangodb::coroutine::thread_registry->garbage_collect();
}

TEST_F(CoroutineRegistryTest, iterates_over_coroutines_on_differen_threads) {
  Registry registry;
  registry.add_thread();
  { coroutine_test::foo(); }

  std::jthread([&]() {
    registry.add_thread();
    auto coro = coroutine_test::bar();

    std::vector<std::string> function_names;
    registry.for_promise([&](PromiseInList* promise) {
      function_names.push_back(promise->_where.function_name());
    });

    EXPECT_EQ(function_names,
              std::vector<std::string>({"async<MyInt> coroutine_test::foo()",
                                        "async<MyInt> coroutine_test::bar()"}));

    coro.reset();
    arangodb::coroutine::thread_registry->garbage_collect();
  });

  arangodb::coroutine::thread_registry->garbage_collect();
}

TEST_F(CoroutineRegistryTest,
       iteration_after_executed_garbage_collection_is_empty) {
  Registry registry;
  registry.add_thread();

  auto coro = coroutine_test::foo();

  EXPECT_EQ(all_function_names(registry),
            std::vector<std::string>{"async<MyInt> coroutine_test::foo()"});

  coro.reset();

  EXPECT_EQ(InstanceCounterValue::instanceCounter, 1);
  EXPECT_EQ(all_function_names(registry),
            std::vector<std::string>{"async<MyInt> coroutine_test::foo()"});

  arangodb::coroutine::thread_registry->garbage_collect();

  EXPECT_EQ(InstanceCounterValue::instanceCounter, 0);
  EXPECT_EQ(all_function_names(registry), std::vector<std::string>{});
}

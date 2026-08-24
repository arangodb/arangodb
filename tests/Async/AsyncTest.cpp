#include "Async/async.h"
#include "Async/Registry/promise.h"
#include "Async/Registry/registry_variable.h"
#include "Containers/Concurrent/shared.h"
#include "Inspection/Format.h"
#include "Inspection/JsonPrintInspector.h"
#include "Mocks/ExecContextFactory.h"
#include "Utils/ExecContext.h"

#include "WaitTypes.h"

#include <gtest/gtest.h>
#include <coroutine>
#include <thread>
#include <deque>
#include <variant>

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

struct CopyOnlyValue : InstanceCounterValue {
  CopyOnlyValue(int x) : x(x) {}
  CopyOnlyValue(CopyOnlyValue const& x) : x(x) {}

  auto operator<=>(int y) const noexcept { return x <=> y; }
  operator int() const noexcept { return x; }
  int x;
};

struct MoveOnlyValue : InstanceCounterValue {
  MoveOnlyValue(int x) : x(x) {}
  MoveOnlyValue(MoveOnlyValue&& x) : x(x) {}
  MoveOnlyValue(MoveOnlyValue const& x) = delete;

  auto operator<=>(int y) const noexcept { return x <=> y; }
  operator int() const noexcept { return x; }
  int x;
};

}  // namespace

template<typename>
struct AsyncTest;

template<typename WaitType, typename ValueType>
struct AsyncTest<std::pair<WaitType, ValueType>> : ::testing::Test {
  void SetUp() override { InstanceCounterValue::instanceCounter = 0; }

  void TearDown() override {
    arangodb::async_registry::get_thread_registry().garbage_collect();
    wait.stop();
    EXPECT_EQ(InstanceCounterValue::instanceCounter, 0);
    EXPECT_EQ(arangodb::async_registry::registry.size(), 0);
    EXPECT_TRUE(std::holds_alternative<
                arangodb::containers::SharedPtr<arangodb::basics::ThreadInfo>>(
        *arangodb::async_registry::get_current_coroutine()));
  }

  WaitType wait;
};

using namespace arangodb;
using MyTypes = ::testing::Types<
    std::pair<async_tests::NoWait, CopyOnlyValue>,            //
    std::pair<async_tests::NoWait, MoveOnlyValue>,            //
    std::pair<async_tests::WaitSlot, CopyOnlyValue>,          //
    std::pair<async_tests::WaitSlot, MoveOnlyValue>,          //
    std::pair<async_tests::ConcurrentNoWait, CopyOnlyValue>,  //
    std::pair<async_tests::ConcurrentNoWait, MoveOnlyValue>>;
TYPED_TEST_SUITE(AsyncTest, MyTypes);

TYPED_TEST(AsyncTest, async_return) {
  using ValueType = TypeParam::second_type;

  auto fn = [&]() -> async<ValueType> {
    co_await this->wait;
    co_return 12;
  };
  auto coro = fn();

  this->wait.resume();
  EXPECT_TRUE(coro.valid());
  auto awaitable = std::move(coro).operator co_await();
  EXPECT_FALSE(coro.valid());
  this->wait.await();
  EXPECT_TRUE(awaitable.await_ready());
  EXPECT_EQ(awaitable.await_resume(), 12);
}

TYPED_TEST(AsyncTest, async_return_move) {
  using ValueType = TypeParam::second_type;

  auto fn = [&]() -> async<ValueType> {
    co_await this->wait;
    co_return 12;
  };
  auto coro = fn();

  EXPECT_TRUE(coro.valid());

  auto moved_coro = std::move(coro);
  EXPECT_TRUE(moved_coro.valid());
  EXPECT_FALSE(coro.valid());

  coro = std::move(moved_coro);
  EXPECT_TRUE(coro.valid());
  EXPECT_FALSE(moved_coro.valid());

  this->wait.resume();
  this->wait.await();
}

TYPED_TEST(AsyncTest, async_return_destroy) {
  using ValueType = TypeParam::second_type;

  auto fn = [&]() -> async<ValueType> {
    co_await this->wait;
    co_return 12;
  };
  auto coro = fn();

  this->wait.resume();
  EXPECT_TRUE(coro.valid());
  coro.reset();
  EXPECT_FALSE(coro.valid());

  this->wait.await();
}

TYPED_TEST(AsyncTest, await_ready_async) {
  using ValueType = TypeParam::second_type;

  auto fn_a = [&]() -> async<ValueType> {
    co_await this->wait;
    co_return 12;
  };
  auto coro_a = fn_a();

  auto fn_b = [&]() -> async<ValueType> {
    co_return 2 * co_await std::move(coro_a);
  };
  auto coro_b = fn_b();

  this->wait.resume();
  EXPECT_TRUE(coro_b.valid());
  EXPECT_FALSE(coro_a.valid());
  auto awaitable = std::move(coro_b).operator co_await();
  this->wait.await();
  EXPECT_TRUE(awaitable.await_ready());
  EXPECT_EQ(awaitable.await_resume(), 24);
}

TYPED_TEST(AsyncTest, async_throw) {
  using ValueType = TypeParam::second_type;

  auto fn = [&]() -> async<ValueType> {
    co_await this->wait;
    throw std::runtime_error("TEST!");
  };
  auto coro = fn();

  this->wait.resume();
  EXPECT_TRUE(coro.valid());
  auto awaitable = std::move(coro).operator co_await();
  this->wait.await();
  EXPECT_TRUE(awaitable.await_ready());
  EXPECT_THROW(awaitable.await_resume(), std::runtime_error);
}

TYPED_TEST(AsyncTest, await_throw_async) {
  using ValueType = TypeParam::second_type;

  auto fn_a = [&]() -> async<ValueType> {
    co_await this->wait;
    throw std::runtime_error("TEST!");
  };
  auto coro_a = fn_a();

  auto fn_b = [&]() -> async<ValueType> {
    try {
      co_return 2 * co_await std::move(coro_a);
    } catch (std::runtime_error const&) {
      co_return 0;
    }
  };
  auto coro_b = fn_b();

  this->wait.resume();
  EXPECT_TRUE(coro_b.valid());
  EXPECT_FALSE(coro_a.valid());
  auto awaitable = std::move(coro_b).operator co_await();
  this->wait.await();
  EXPECT_TRUE(awaitable.await_ready());
  EXPECT_EQ(awaitable.await_resume(), 0);
}

TYPED_TEST(AsyncTest, await_async_void) {
  using ValueType = TypeParam::second_type;

  auto fn_a = [&]() -> async<void> {
    co_await this->wait;
    co_return;
  };
  auto coro_a = fn_a();

  auto fn_b = [&]() -> async<ValueType> {
    co_await std::move(coro_a);
    co_return 2;
  };
  auto coro_b = fn_b();

  this->wait.resume();
  EXPECT_TRUE(coro_b.valid());
  EXPECT_FALSE(coro_a.valid());
  auto awaitable = std::move(coro_b).operator co_await();
  this->wait.await();
  EXPECT_TRUE(awaitable.await_ready());
  EXPECT_EQ(awaitable.await_resume(), 2);
}

TYPED_TEST(AsyncTest, await_async_void_exception) {
  using ValueType = TypeParam::second_type;

  auto fn_a = [&]() -> async<void> {
    co_await this->wait;
    throw std::runtime_error("TEST!");
  };
  auto coro_a = fn_a();

  auto fn_b = [&]() -> async<ValueType> {
    try {
      co_await std::move(coro_a);
      co_return 2;
    } catch (std::runtime_error const&) {
      co_return 0;
    }
  };
  auto coro_b = fn_b();

  this->wait.resume();
  EXPECT_TRUE(coro_b.valid());
  EXPECT_FALSE(coro_a.valid());
  auto awaitable = std::move(coro_b).operator co_await();
  this->wait.await();
  EXPECT_TRUE(awaitable.await_ready());
  EXPECT_EQ(awaitable.await_resume(), 0);
}

TYPED_TEST(AsyncTest, multiple_suspension_points) {
  using ValueType = TypeParam::second_type;

  auto fn_a = [&]() -> async<ValueType> {
    co_await this->wait;
    co_return 12;
  };

  auto fn_b = [&]() -> async<ValueType> {
    for (int i = 0; i < 10; i++) {
      co_await fn_a();
    }

    co_return 0;
  };

  auto coro_b = fn_b();

  this->wait.resume();
  EXPECT_TRUE(coro_b.valid());
  auto awaitable = std::move(coro_b).operator co_await();
  this->wait.await();
  EXPECT_TRUE(awaitable.await_ready());
  EXPECT_EQ(awaitable.await_resume(), 0);
}

auto makeExecContext(std::string username) {
  return tests::mocks::makeClassicExecContext(
      std::move(username), "", auth::Level::RW, auth::Level::NONE);
}

TYPED_TEST(AsyncTest, execution_context_is_local_to_coroutine) {
  auto ctxBegin = makeExecContext("Begin");
  ExecContextScope exec(ctxBegin.execContext);
  EXPECT_EQ(ExecContext::current().user(), "Begin");

  auto waiting_fn = [&]() -> async<void> {
    EXPECT_EQ(ExecContext::current().user(), "Begin");
    auto ctxWaiting = makeExecContext("Waiting");
    ExecContextScope exec(ctxWaiting.execContext);
    EXPECT_EQ(ExecContext::current().user(), "Waiting");
    co_await this->wait;
    EXPECT_EQ(ExecContext::current().user(), "Waiting");
    co_return;
  };
  auto waiting_coro = waiting_fn();
  EXPECT_EQ(ExecContext::current().user(), "Begin");

  auto trivial_fn = []() -> async<void> {
    EXPECT_EQ(ExecContext::current().user(), "Begin");
    co_return;
  };
  auto trivial_coro = trivial_fn();

  auto calling_coro = [&]() -> async<void> {
    EXPECT_EQ(ExecContext::current().user(), "Begin");
    auto ctxCalling = makeExecContext("Calling");
    ExecContextScope exec(ctxCalling.execContext);
    EXPECT_EQ(ExecContext::current().user(), "Calling");
    co_await std::move(waiting_coro);
    EXPECT_EQ(ExecContext::current().user(), "Calling");
    co_await std::move(trivial_coro);
    EXPECT_EQ(ExecContext::current().user(), "Calling");
    co_return;
  };
  EXPECT_EQ(ExecContext::current().user(), "Begin");

  std::ignore = calling_coro();
  EXPECT_EQ(ExecContext::current().user(), "Begin");

  auto ctxEnd = makeExecContext("End");
  ExecContextScope new_exec(ctxEnd.execContext);
  EXPECT_EQ(ExecContext::current().user(), "End");

  this->wait.resume();
  this->wait.await();
  EXPECT_EQ(ExecContext::current().user(), "End");
}

namespace {
auto foo() -> async<void> { co_return; }
auto bar() -> async<void> { co_return; }
auto baz() -> async<void> { co_return; }
}  // namespace
TYPED_TEST(AsyncTest, promises_are_registered_in_global_async_registry) {
  auto coro_foo = foo();
  EXPECT_EQ(arangodb::async_registry::registry.size(), 1);

  std::jthread([&]() {
    auto coro_bar = bar();
    auto coro_baz = baz();

    std::vector<std::string_view> names;
    arangodb::async_registry::registry.for_node(
        [&](arangodb::async_registry::PromiseSnapshot promise) {
          names.push_back(promise.source_location.function_name);
        });
    EXPECT_EQ(names.size(), 3);
    EXPECT_TRUE(names[0].find("foo") != std::string::npos);
    EXPECT_TRUE(names[1].find("baz") != std::string::npos);
    EXPECT_TRUE(names[2].find("bar") != std::string::npos);
  }).join();
}

TYPED_TEST(AsyncTest, coroutine_is_deleted_earlier_than_registry_entry) {
  using ValueType = TypeParam::second_type;

  auto coro = []() -> async<ValueType> { co_return 12; };

  {
    coro().reset();
    EXPECT_EQ(InstanceCounterValue::instanceCounter, 0);
    EXPECT_EQ(arangodb::async_registry::registry.size(), 1);
  }
  {
    std::move(coro()).operator co_await().await_resume();
    EXPECT_EQ(InstanceCounterValue::instanceCounter, 0);
    EXPECT_EQ(arangodb::async_registry::registry.size(), 2);
  }
  {
    { std::ignore = coro(); }

    EXPECT_EQ(InstanceCounterValue::instanceCounter, 0);
    EXPECT_EQ(arangodb::async_registry::registry.size(), 3);
  }
}

namespace {
auto find_promise_by_name(std::string_view name)
    -> std::optional<async_registry::PromiseSnapshot> {
  std::optional<async_registry::PromiseSnapshot> requested_promise =
      std::nullopt;
  arangodb::async_registry::registry.for_node(
      [&](arangodb::async_registry::PromiseSnapshot promise) {
        if (promise.source_location.function_name.find(name) !=
            std::string::npos) {
          requested_promise = promise;
        }
      });
  return requested_promise;
}
}  // namespace
TYPED_TEST(
    AsyncTest,
    async_promises_in_async_registry_know_their_requester_with_nested_coroutines) {
  using TestType = decltype(this);
  arangodb::async_registry::get_thread_registry().garbage_collect();
  struct Functions {
    static auto awaited_by_awaited_fn(TestType test)
        -> async<async_registry::PromiseSnapshot> {
      auto promise = find_promise_by_name("awaited_by_awaited_fn");
      EXPECT_TRUE(promise.has_value());
      EXPECT_TRUE(std::holds_alternative<async_registry::PromiseId>(
          promise->requester));
      co_await test->wait;

      co_return promise.value();
    };
    static auto awaited_fn(TestType test)
        -> async<async_registry::PromiseSnapshot> {
      auto promise = find_promise_by_name("awaited_fn");
      EXPECT_TRUE(promise.has_value());
      EXPECT_TRUE(std::holds_alternative<async_registry::PromiseId>(
          promise->requester));

      auto fn = Functions::awaited_by_awaited_fn(test);
      auto awaited_promise = find_promise_by_name("awaited_by_awaited_fn");
      EXPECT_TRUE(awaited_promise.has_value());
      EXPECT_EQ(awaited_promise->requester,
                async_registry::Requester{promise->id});

      auto awaited_promise_coawaited = co_await std::move(fn);
      EXPECT_EQ(awaited_promise_coawaited.requester,
                async_registry::Requester{promise->id});

      co_return promise.value();
    };
    static auto waiter_fn(TestType test) -> async<void> {
      auto waiter_promise = find_promise_by_name("waiter_fn");
      EXPECT_TRUE(waiter_promise.has_value());
      EXPECT_TRUE(std::holds_alternative<basics::ThreadInfo>(
          waiter_promise->requester));

      auto fn = Functions::awaited_fn(test);

      auto awaited_promise = find_promise_by_name("awaited_fn");
      EXPECT_TRUE(awaited_promise.has_value());
      EXPECT_EQ(awaited_promise->requester,
                async_registry::Requester{waiter_promise->id});

      auto awaited_promise_coawaited = co_await std::move(fn);
      EXPECT_EQ(awaited_promise_coawaited.requester,
                async_registry::Requester{waiter_promise->id});

      // waiter did not change
      waiter_promise = find_promise_by_name("waiter_fn");
      EXPECT_TRUE(waiter_promise.has_value());
      EXPECT_TRUE(std::holds_alternative<basics::ThreadInfo>(
          waiter_promise->requester));

      co_return;
    };
  };

  std::ignore = Functions::waiter_fn(this);

  this->wait.resume();
  this->wait.await();
}

TYPED_TEST(
    AsyncTest,
    async_promises_in_async_registry_know_their_requester_with_two_requests_after_each_other) {
  using TestType = decltype(this);
  arangodb::async_registry::get_thread_registry().garbage_collect();
  struct Functions {
    static auto awaited_2_fn() -> async<async_registry::PromiseSnapshot> {
      auto promise = find_promise_by_name("awaited_2_fn");
      EXPECT_TRUE(promise.has_value());
      EXPECT_TRUE(std::holds_alternative<async_registry::PromiseId>(
          promise->requester));

      co_return promise.value();
    };
    static auto awaited_fn(TestType test)
        -> async<async_registry::PromiseSnapshot> {
      auto promise = find_promise_by_name("awaited_fn");
      EXPECT_TRUE(promise.has_value());
      EXPECT_TRUE(std::holds_alternative<async_registry::PromiseId>(
          promise->requester));

      co_await test->wait;

      co_return promise.value();
    };
    static auto waiter_fn(TestType test) -> async<void> {
      auto waiter_promise = find_promise_by_name("waiter_fn");
      EXPECT_TRUE(waiter_promise.has_value());
      EXPECT_TRUE(std::holds_alternative<basics::ThreadInfo>(
          waiter_promise->requester));

      auto fn = Functions::awaited_fn(test);
      auto fn_2 = Functions::awaited_2_fn();

      auto awaited_promise = find_promise_by_name("awaited_fn");
      EXPECT_TRUE(awaited_promise.has_value());
      EXPECT_EQ(awaited_promise->requester,
                async_registry::Requester{waiter_promise->id});
      auto awaited_2_promise = find_promise_by_name("awaited_2_fn");
      if (not std::is_same<TypeParam, arangodb::async_tests::NoWait>::value) {
        EXPECT_TRUE(awaited_2_promise.has_value());
        EXPECT_EQ(awaited_2_promise->requester,
                  async_registry::Requester{waiter_promise->id});
      }

      auto awaited_promise_coawaited = co_await std::move(fn);
      EXPECT_EQ(awaited_promise_coawaited.requester,
                async_registry::Requester{waiter_promise->id});

      auto awaited_2_promise_coawaited = co_await std::move(fn_2);
      EXPECT_EQ(awaited_2_promise_coawaited.requester,
                async_registry::Requester{waiter_promise->id});

      // waiter did not change
      waiter_promise = find_promise_by_name("waiter_fn");
      EXPECT_TRUE(waiter_promise.has_value());
      EXPECT_TRUE(std::holds_alternative<basics::ThreadInfo>(
          waiter_promise->requester));

      co_return;
    };
  };

  std::ignore = Functions::waiter_fn(this);

  this->wait.resume();
  this->wait.await();
}

TYPED_TEST(AsyncTest,
           async_promises_in_async_registry_know_their_requester_with_move) {
  arangodb::async_registry::get_thread_registry().garbage_collect();
  using TestType = decltype(this);
  struct Functions {
    static auto awaited_fn(TestType test)
        -> async<async_registry::PromiseSnapshot> {
      auto promise = find_promise_by_name("awaited_fn");
      EXPECT_TRUE(promise.has_value());
      EXPECT_TRUE(
          std::holds_alternative<basics::ThreadInfo>(promise->requester));

      co_await test->wait;

      promise = find_promise_by_name("awaited_fn");
      EXPECT_TRUE(promise.has_value());
      co_return promise.value();
    };
    static auto waiter_fn(async<async_registry::PromiseSnapshot>&& fn)
        -> async<void> {
      auto waiter_promise = find_promise_by_name("waiter_fn");
      EXPECT_TRUE(waiter_promise.has_value());
      EXPECT_TRUE(std::holds_alternative<basics::ThreadInfo>(
          waiter_promise->requester));

      auto awaited_promise = find_promise_by_name("awaited_fn");
      EXPECT_TRUE(awaited_promise.has_value());
      EXPECT_TRUE(std::holds_alternative<basics::ThreadInfo>(
          waiter_promise->requester));

      auto awaited_promise_coawaited = co_await std::move(fn);
      // NoWait: awaited_fn promise is already marked for deletion, therefore
      // irrelevant
      if (not std::is_same<typename TypeParam::first_type,
                           arangodb::async_tests::NoWait>::value) {
        EXPECT_EQ(awaited_promise_coawaited.requester,
                  async_registry::Requester{waiter_promise->id});
      }

      // waiter did not change
      waiter_promise = find_promise_by_name("waiter_fn");
      EXPECT_TRUE(waiter_promise.has_value());
      EXPECT_TRUE(std::holds_alternative<basics::ThreadInfo>(
          waiter_promise->requester));

      co_return;
    };
  };

  auto awaited_coro = Functions::awaited_fn(this);
  std::ignore = Functions::waiter_fn(std::move(awaited_coro));

  this->wait.resume();
  this->wait.await();
}

TYPED_TEST(
    AsyncTest,
    async_promises_in_async_registry_know_their_requester_with_move_and_call_without_await) {
  arangodb::async_registry::get_thread_registry().garbage_collect();
  using TestType = decltype(this);
  struct Functions {
    static auto awaited_2_fn(TestType test) -> async<void> {
      co_await test->wait;
      co_return;
    };
    static auto awaited_fn(TestType test)
        -> async<async_registry::PromiseSnapshot> {
      auto promise = find_promise_by_name("awaited_fn");
      EXPECT_TRUE(promise.has_value());

      co_await test->wait;

      promise = find_promise_by_name("awaited_fn");
      EXPECT_TRUE(promise.has_value());
      co_return promise.value();
    };
    static auto waiter_fn(async<async_registry::PromiseSnapshot>&& fn,
                          TestType test) -> async<void> {
      auto awaited_promise_coawaited = co_await std::move(fn);

      auto a = awaited_2_fn(test);

      auto waiter_promise = find_promise_by_name("waiter_fn");
      EXPECT_TRUE(waiter_promise.has_value());
      EXPECT_TRUE(std::holds_alternative<basics::ThreadInfo>(
          waiter_promise->requester));

      // NoWait: awaited_fn promise is already marked for deletion, therefore
      // irrelevant
      if (not std::is_same<typename TypeParam::first_type,
                           arangodb::async_tests::NoWait>::value) {
        EXPECT_EQ(awaited_promise_coawaited.requester,
                  async_registry::Requester{waiter_promise->id});
      }

      auto awaited_2_promise = find_promise_by_name("awaited_2_fn");
      EXPECT_TRUE(awaited_2_promise.has_value());
      EXPECT_EQ(awaited_2_promise->requester,
                async_registry::Requester{waiter_promise->id});

      co_return;
    };
  };

  auto awaited_coro = Functions::awaited_fn(this);
  std::ignore = Functions::waiter_fn(std::move(awaited_coro), this);

  this->wait.resume();
  this->wait.await();
}

namespace {
auto expect_all_promises_in_state(arangodb::async_registry::State state,
                                  uint number_of_promises) {
  uint count = 0;
  arangodb::async_registry::registry.for_node(
      [&](arangodb::async_registry::PromiseSnapshot promise) {
        count++;
        EXPECT_EQ(promise.state, state);
      });
  EXPECT_EQ(count, number_of_promises);
}
}  // namespace
TYPED_TEST(AsyncTest, async_promises_in_async_registry_know_their_state) {
  {
    auto fn = [&]() -> async<int> {
      co_await this->wait;
      co_return 12;
    };
    auto coro = fn();

    if (std::is_same<decltype(this->wait), async_tests::WaitSlot>()) {
      expect_all_promises_in_state(arangodb::async_registry::State::Suspended,
                                   1);
    }

    this->wait.resume();
    this->wait.await();

    expect_all_promises_in_state(arangodb::async_registry::State::Resolved, 1);
  }

  expect_all_promises_in_state(arangodb::async_registry::State::Resolved, 0);
}

#include "AsyncTestLineNumbers.tpp"

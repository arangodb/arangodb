#include "Async/Registry/promise.h"
#include "Async/Registry/registry_variable.h"
#include "Futures/Future.h"

#include <condition_variable>
#include <coroutine>
#include <mutex>
#include <thread>
#include <deque>

#include <gtest/gtest.h>

using namespace arangodb::futures;

namespace {
struct WaitSlot {
  void resume() {
    ready = true;
    _continuation.resume();
  }

  void await() {}

  std::coroutine_handle<> _continuation;

  bool await_ready() { return ready; }
  void await_resume() {}
  void await_suspend(std::coroutine_handle<> continuation) {
    _continuation = continuation;
  }

  void stop() {}

  bool ready = false;
};

struct NoWait {
  void resume() {}
  void await() {}

  auto operator co_await() { return std::suspend_never{}; }
  void stop() {}
};

struct ConcurrentNoWait {
  void resume() {}
  void await() {
    await_suspend(std::noop_coroutine());
    _thread.join();
  }

  bool await_ready() { return false; }
  void await_resume() {}
  void await_suspend(std::coroutine_handle<> handle) {
    {
      std::unique_lock guard(_mutex);
      _coro.emplace_back(handle);
    }
    _cv.notify_one();
  }
  ConcurrentNoWait()
      : _thread([&] {
          bool stopping = false;
          while (true) {
            std::coroutine_handle<> handle;
            {
              std::unique_lock guard(_mutex);
              if (_coro.empty() && stopping) {
                break;
              }
              _cv.wait(guard, [&] { return !_coro.empty(); });
              handle = _coro.front();
              _coro.pop_front();
            }
            if (handle == std::noop_coroutine()) {
              stopping = true;
            }
            handle.resume();
          }
        }) {}

  auto stop() -> void {
    if (_thread.joinable()) {
      await_suspend(std::noop_coroutine());
      _thread.join();
    }
  }

  std::mutex _mutex;
  std::condition_variable_any _cv;
  std::deque<std::coroutine_handle<>> _coro;

  std::jthread _thread;
};

auto expect_all_promises_in_state(arangodb::async_registry::State state,
                                  uint number_of_promises) {
  uint count = 0;
  arangodb::async_registry::registry.for_promise(
      [&](arangodb::async_registry::PromiseSnapshot promise) {
        count++;
        EXPECT_EQ(promise.state, state);
      });
  EXPECT_EQ(count, number_of_promises);
}

}  // namespace

template<typename WaitType>
struct FutureCoroutineTest : ::testing::Test {
  void SetUp() override {
    arangodb::async_registry::get_thread_registry().garbage_collect();
    EXPECT_TRUE(std::holds_alternative<arangodb::async_registry::ThreadId>(
        *arangodb::async_registry::get_current_coroutine()));
  }

  void TearDown() override {
    arangodb::async_registry::get_thread_registry().garbage_collect();
    wait.stop();
  }

  WaitType wait;
};

using MyTypes = ::testing::Types<NoWait, WaitSlot, ConcurrentNoWait>;
TYPED_TEST_SUITE(FutureCoroutineTest, MyTypes);

TYPED_TEST(FutureCoroutineTest, promises_in_async_registry_know_their_state) {
  {
    auto coro = [&]() -> Future<int> {
      co_await this->wait;
      co_return 12;
    }();

    if (std::is_same<decltype(this->wait), WaitSlot>()) {
      expect_all_promises_in_state(arangodb::async_registry::State::Suspended,
                                   1);
    }

    this->wait.resume();
    this->wait.await();

    expect_all_promises_in_state(arangodb::async_registry::State::Resolved, 1);
  }
  expect_all_promises_in_state(arangodb::async_registry::State::Deleted, 1);
}

namespace {
auto find_promise_by_name(std::string_view name)
    -> std::optional<arangodb::async_registry::PromiseSnapshot> {
  std::optional<arangodb::async_registry::PromiseSnapshot> requested_promise =
      std::nullopt;
  arangodb::async_registry::registry.for_promise(
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
    FutureCoroutineTest,
    promises_in_async_registry_know_their_requester_with_nested_coroutines) {
  using TestType = decltype(this);
  struct Functions {
    static auto awaited_by_awaited_fn(TestType test) -> Future<Unit> {
      auto promise = find_promise_by_name("awaited_by_awaited_fn");
      EXPECT_TRUE(promise.has_value());
      EXPECT_TRUE(std::holds_alternative<arangodb::async_registry::PromiseId>(
          promise->requester));
      co_await test->wait;

      co_return;
    };
    static auto awaited_fn(TestType test) -> Future<Unit> {
      auto promise = find_promise_by_name("awaited_fn");
      EXPECT_TRUE(promise.has_value());
      EXPECT_TRUE(std::holds_alternative<arangodb::async_registry::PromiseId>(
          promise->requester));

      auto fn = Functions::awaited_by_awaited_fn(test);
      auto awaited_promise = find_promise_by_name("awaited_by_awaited_fn");
      EXPECT_TRUE(awaited_promise.has_value());
      EXPECT_EQ(awaited_promise->requester,
                arangodb::async_registry::Requester{promise->id});

      co_await std::move(fn);
      awaited_promise = find_promise_by_name("awaited_by_awaited_fn");
      EXPECT_TRUE(awaited_promise.has_value());
      EXPECT_EQ(awaited_promise->requester,
                arangodb::async_registry::Requester{promise->id});

      co_return;
    };
    static auto waiter_fn(TestType test) -> Future<Unit> {
      auto waiter_promise = find_promise_by_name("waiter_fn");
      EXPECT_TRUE(waiter_promise.has_value());
      EXPECT_TRUE(std::holds_alternative<arangodb::async_registry::ThreadId>(
          waiter_promise->requester));

      auto fn = Functions::awaited_fn(test);

      auto awaited_promise = find_promise_by_name("awaited_fn");
      EXPECT_TRUE(awaited_promise.has_value());
      EXPECT_EQ(awaited_promise->requester,
                arangodb::async_registry::Requester{waiter_promise->id});

      co_await std::move(fn);

      awaited_promise = find_promise_by_name("awaited_fn");
      EXPECT_TRUE(awaited_promise.has_value());
      EXPECT_EQ(awaited_promise->requester,
                arangodb::async_registry::Requester{waiter_promise->id});

      // waiter did not change
      waiter_promise = find_promise_by_name("waiter_fn");
      EXPECT_TRUE(waiter_promise.has_value());
      EXPECT_TRUE(std::holds_alternative<arangodb::async_registry::ThreadId>(
          waiter_promise->requester));

      co_return;
    };
  };

  auto waiter = Functions::waiter_fn(this);

  this->wait.resume();
  this->wait.await();
}

TYPED_TEST(FutureCoroutineTest,
           promises_in_async_registry_know_their_requester_with_move) {
  using TestType = decltype(this);
  struct Functions {
    static auto awaited_fn(TestType test) -> Future<Unit> {
      auto promise = find_promise_by_name("awaited_fn");
      EXPECT_TRUE(promise.has_value());
      EXPECT_TRUE(std::holds_alternative<arangodb::async_registry::ThreadId>(
          promise->requester));

      co_await test->wait;

      co_return;
    };
    static auto waiter_fn(Future<Unit>&& fn) -> Future<Unit> {
      auto waiter_promise = find_promise_by_name("waiter_fn");
      EXPECT_TRUE(waiter_promise.has_value());
      EXPECT_TRUE(std::holds_alternative<arangodb::async_registry::ThreadId>(
          waiter_promise->requester));

      auto awaited_promise = find_promise_by_name("awaited_fn");
      EXPECT_TRUE(awaited_promise.has_value());
      EXPECT_TRUE(std::holds_alternative<arangodb::async_registry::ThreadId>(
          waiter_promise->requester));

      co_await std::move(fn);

      awaited_promise = find_promise_by_name("awaited_fn");
      EXPECT_TRUE(awaited_promise.has_value());
      EXPECT_EQ(awaited_promise->requester,
                arangodb::async_registry::Requester{waiter_promise->id});

      // waiter did not change
      waiter_promise = find_promise_by_name("waiter_fn");
      EXPECT_TRUE(waiter_promise.has_value());
      EXPECT_TRUE(std::holds_alternative<arangodb::async_registry::ThreadId>(
          waiter_promise->requester));

      co_return;
    };
  };

  auto awaited_coro = Functions::awaited_fn(this);
  auto waiter = Functions::waiter_fn(std::move(awaited_coro));

  this->wait.resume();
  this->wait.await();
}

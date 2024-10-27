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

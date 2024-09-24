#include "Async/async.h"
#include "Async/Registry/registry.h"
#include "Async/Registry/thread_registry.h"

#include <gtest/gtest.h>
#include <thread>
#include <deque>

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
          arangodb::async_registry::get_thread_registry().garbage_collect();
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
  }

  WaitType wait;
};

using MyTypes = ::testing::Types<
    std::pair<NoWait, CopyOnlyValue>, std::pair<WaitSlot, MoveOnlyValue>,
    std::pair<ConcurrentNoWait, MoveOnlyValue>,
    std::pair<NoWait, CopyOnlyValue>, std::pair<WaitSlot, MoveOnlyValue>,
    std::pair<ConcurrentNoWait, CopyOnlyValue>>;
TYPED_TEST_SUITE(AsyncTest, MyTypes);

using namespace arangodb;

TYPED_TEST(AsyncTest, async_return) {
  using ValueType = TypeParam::second_type;

  auto a = [&]() -> async<ValueType> {
    co_await this->wait;
    co_return 12;
  }();

  this->wait.resume();
  EXPECT_TRUE(a.valid());
  auto awaitable = std::move(a).operator co_await();
  EXPECT_FALSE(a.valid());
  this->wait.await();
  EXPECT_TRUE(awaitable.await_ready());
  EXPECT_EQ(awaitable.await_resume(), 12);
}

TYPED_TEST(AsyncTest, async_return_move) {
  using ValueType = TypeParam::second_type;

  auto a = [&]() -> async<ValueType> {
    co_await this->wait;
    co_return 12;
  }();

  EXPECT_TRUE(a.valid());

  auto b = std::move(a);
  EXPECT_TRUE(b.valid());
  EXPECT_FALSE(a.valid());

  a = std::move(b);
  EXPECT_TRUE(a.valid());
  EXPECT_FALSE(b.valid());

  this->wait.resume();
  this->wait.await();
}

TYPED_TEST(AsyncTest, async_return_destroy) {
  using ValueType = TypeParam::second_type;

  auto a = [&]() -> async<ValueType> {
    co_await this->wait;
    co_return 12;
  }();

  this->wait.resume();
  EXPECT_TRUE(a.valid());
  a.reset();
  EXPECT_FALSE(a.valid());

  this->wait.await();
}

TYPED_TEST(AsyncTest, await_ready_async) {
  using ValueType = TypeParam::second_type;

  auto a = [&]() -> async<ValueType> {
    co_await this->wait;
    co_return 12;
  }();

  auto b = [&]() -> async<ValueType> { co_return 2 * co_await std::move(a); }();

  this->wait.resume();
  EXPECT_TRUE(b.valid());
  EXPECT_FALSE(a.valid());
  auto awaitable = std::move(b).operator co_await();
  this->wait.await();
  EXPECT_TRUE(awaitable.await_ready());
  EXPECT_EQ(awaitable.await_resume(), 24);
}

TYPED_TEST(AsyncTest, async_throw) {
  using ValueType = TypeParam::second_type;

  auto a = [&]() -> async<ValueType> {
    co_await this->wait;
    throw std::runtime_error("TEST!");
  }();

  this->wait.resume();
  EXPECT_TRUE(a.valid());
  auto awaitable = std::move(a).operator co_await();
  this->wait.await();
  EXPECT_TRUE(awaitable.await_ready());
  EXPECT_THROW(awaitable.await_resume(), std::runtime_error);
}

TYPED_TEST(AsyncTest, await_throw_async) {
  using ValueType = TypeParam::second_type;

  auto a = [&]() -> async<ValueType> {
    co_await this->wait;
    throw std::runtime_error("TEST!");
  }();

  auto b = [&]() -> async<ValueType> {
    try {
      co_return 2 * co_await std::move(a);
    } catch (std::runtime_error const&) {
      co_return 0;
    }
  }();

  this->wait.resume();
  EXPECT_TRUE(b.valid());
  EXPECT_FALSE(a.valid());
  auto awaitable = std::move(b).operator co_await();
  this->wait.await();
  EXPECT_TRUE(awaitable.await_ready());
  EXPECT_EQ(awaitable.await_resume(), 0);
}

TYPED_TEST(AsyncTest, await_async_void) {
  using ValueType = TypeParam::second_type;

  auto a = [&]() -> async<void> {
    co_await this->wait;
    co_return;
  }();

  auto b = [&]() -> async<ValueType> {
    co_await std::move(a);
    co_return 2;
  }();

  this->wait.resume();
  EXPECT_TRUE(b.valid());
  EXPECT_FALSE(a.valid());
  auto awaitable = std::move(b).operator co_await();
  this->wait.await();
  EXPECT_TRUE(awaitable.await_ready());
  EXPECT_EQ(awaitable.await_resume(), 2);
}

TYPED_TEST(AsyncTest, await_async_void_exception) {
  using ValueType = TypeParam::second_type;

  auto a = [&]() -> async<void> {
    co_await this->wait;
    throw std::runtime_error("TEST!");
  }();

  auto b = [&]() -> async<ValueType> {
    try {
      co_await std::move(a);
      co_return 2;
    } catch (std::runtime_error const&) {
      co_return 0;
    }
  }();

  this->wait.resume();
  EXPECT_TRUE(b.valid());
  EXPECT_FALSE(a.valid());
  auto awaitable = std::move(b).operator co_await();
  this->wait.await();
  EXPECT_TRUE(awaitable.await_ready());
  EXPECT_EQ(awaitable.await_resume(), 0);
}

TYPED_TEST(AsyncTest, multiple_suspension_points) {
  using ValueType = TypeParam::second_type;

  auto a = [&]() -> async<ValueType> {
    co_await this->wait;
    co_return 12;
  };

  auto lambda = [&]() -> async<ValueType> {
    for (int i = 0; i < 10; i++) {
      co_await a();
    }

    co_return 0;
  };

  auto b = lambda();

  this->wait.resume();
  EXPECT_TRUE(b.valid());
  auto awaitable = std::move(b).operator co_await();
  this->wait.await();
  EXPECT_TRUE(awaitable.await_ready());
  EXPECT_EQ(awaitable.await_resume(), 0);
}

TYPED_TEST(AsyncTest, promises_are_only_removed_after_garbage_collection) {
  using ValueType = TypeParam::second_type;

  auto coro = []() -> async<ValueType> { co_return 12; };

  {
    coro().reset();

    EXPECT_EQ(InstanceCounterValue::instanceCounter, 1);
    arangodb::async_registry::get_thread_registry().garbage_collect();
    EXPECT_EQ(InstanceCounterValue::instanceCounter, 0);
  }
  {
    std::move(coro()).operator co_await().await_resume();

    EXPECT_EQ(InstanceCounterValue::instanceCounter, 1);
    arangodb::async_registry::get_thread_registry().garbage_collect();
    EXPECT_EQ(InstanceCounterValue::instanceCounter, 0);
  }
  {
    { coro(); }

    EXPECT_EQ(InstanceCounterValue::instanceCounter, 1);
    arangodb::async_registry::get_thread_registry().garbage_collect();
    EXPECT_EQ(InstanceCounterValue::instanceCounter, 0);
  }
}

auto foo() -> async<CopyOnlyValue> { co_return 1; }
auto bar() -> async<CopyOnlyValue> { co_return 4; }
auto baz() -> async<CopyOnlyValue> { co_return 2; }
TEST(AsyncTest, promises_are_registered) {
  auto coro_foo = foo();

  std::jthread([&]() {
    auto coro_bar = bar();
    auto coro_baz = baz();

    std::vector<std::string> names;
    arangodb::async_registry::coroutine_registry.for_promise(
        [&](arangodb::async_registry::PromiseInList* promise) {
          names.push_back(promise->_where.function_name());
        });
    EXPECT_EQ(names.size(), 3);
    EXPECT_TRUE(names[0].find("foo") != std::string::npos);
    EXPECT_TRUE(names[1].find("baz") != std::string::npos);
    EXPECT_TRUE(names[2].find("bar") != std::string::npos);

    coro_bar.reset();
    coro_baz.reset();
    arangodb::async_registry::get_thread_registry().garbage_collect();
  });

  coro_foo.reset();
  arangodb::async_registry::get_thread_registry().garbage_collect();
}

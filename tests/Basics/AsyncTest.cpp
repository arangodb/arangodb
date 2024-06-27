#include "Basics/async.h"
#include <gtest/gtest.h>
#include <thread>
#include <deque>

namespace {

struct WaitSlot {
  void resume() { _continuation.resume(); }

  void await(auto&) {}

  std::coroutine_handle<> _continuation;

  bool await_ready() { return false; }
  void await_resume() {}
  void await_suspend(std::coroutine_handle<> continuation) {
    _continuation = continuation;
  }
};

struct NoWait {
  void resume() {}
  void await(auto&) {}

  auto operator co_await() { return std::suspend_never{}; }
};

struct ConcurrentNoWait {
  void resume() {}
  void await(auto&) { _thread.join(); }

  bool await_ready() { return false; }
  void await_resume() {}
  void await_suspend(std::coroutine_handle<> handle) {
    {
      std::unique_lock guard(_mutex);
      _coro = handle;
    }
    _cv.notify_one();
  }

  ConcurrentNoWait()
      : _thread([&] {
          {
            std::unique_lock guard(_mutex);
            _cv.wait(guard, [&] { return _coro != nullptr; });
          }

          _coro.resume();
        }) {}

  std::mutex _mutex;
  std::condition_variable_any _cv;
  std::coroutine_handle<> _coro;

  std::jthread _thread;
};

struct InstanceCounterValue {
  InstanceCounterValue() { instanceCounter += 1; }
  InstanceCounterValue(InstanceCounterValue const& o) { instanceCounter += 1; }
  InstanceCounterValue(InstanceCounterValue&& o) { instanceCounter += 1; }

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
    EXPECT_EQ(InstanceCounterValue::instanceCounter, 0);
  }

  WaitType wait;
};

using MyTypes = ::testing::Types<
    std::pair<NoWait, CopyOnlyValue>, std::pair<WaitSlot, MoveOnlyValue>,
    std::pair<ConcurrentNoWait, MoveOnlyValue>,
    std::pair<NoWait, CopyOnlyValue>, std::pair<WaitSlot, MoveOnlyValue>,
    std::pair<ConcurrentNoWait, MoveOnlyValue>>;
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
  this->wait.await(a);
  EXPECT_TRUE(awaitable.await_ready());
  EXPECT_EQ(awaitable.await_resume(), 12);
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
  this->wait.await(b);
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
  this->wait.await(a);
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
  this->wait.await(b);
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
  this->wait.await(b);
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
  this->wait.await(b);
  EXPECT_TRUE(awaitable.await_ready());
  EXPECT_EQ(awaitable.await_resume(), 0);
}

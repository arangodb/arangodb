////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2024-2024 ArangoDB GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#include "Coroutines/EagerCoro.h"

#include <thread>
#include <variant>

#include <gtest/gtest.h>

using namespace arangodb;
using namespace arangodb::runtime;

struct CoroRunner {
  struct promise_type {
    CoroRunner get_return_object() {
      return {std::coroutine_handle<promise_type>::from_promise(*this)};
    }

    void unhandled_exception() { throw; }
    void return_void() noexcept {}

    std::suspend_never initial_suspend() noexcept { return {}; }
    std::suspend_never final_suspend() noexcept { return {}; }
  };
  CoroRunner(std::coroutine_handle<promise_type> handle) {
    LOG_DEVEL << ADB_HERE << " " << handle.address();
  }
};

template<auto rv = std::monostate{}>
struct Suspension {
  std::mutex _mutex;
  std::coroutine_handle<> _caller{};
  std::atomic<bool> _resolved = false;

  bool await_suspend(std::coroutine_handle<> caller) {
    auto lock = std::lock_guard(_mutex);
    TRI_ASSERT(_caller == nullptr) << "await_suspend called twice";
    _caller = caller;
    return !_resolved;
  }
  bool await_ready() const noexcept { return _resolved.load(); }
  auto await_resume() const noexcept {
    TRI_ASSERT(_resolved) << "return value read before it is ready";
    return rv;
  }

  void resolve() {
    auto lock = std::lock_guard(_mutex);
    TRI_ASSERT(_resolved == false);
    _resolved = true;
    if (_caller != nullptr) {  // caller already waiting, resume
      LOG_DEVEL << "suspension resuming " << _caller.address();
      _caller.resume();
    }
  }
};

TEST(EagerCoroTest, test1) {
  auto g = []() -> Task<unsigned> {
    LOG_DEVEL << "g()";
    co_return 3;
  };

  auto f = [&g]() -> Task<int> {
    LOG_DEVEL << "f()";
    auto x = co_await g();
    LOG_DEVEL << "f() 2";
    co_return 7 + x;
  };

  std::optional<int> res;
  auto runner = [&]() -> CoroRunner { res = co_await f(); };
  runner();

  ASSERT_EQ(res, 10);
}

TEST(EagerCoroTest, test2) {
  auto suspension = Suspension{};

  auto g = [&]() -> Task<unsigned> {
    co_await suspension;
    co_return 3;
  };

  auto f = [&g]() -> Task<int> {
    auto x = co_await g();
    co_return 7 + x;
  };

  std::optional<int> res;
  auto runner = [&]() -> CoroRunner { res = co_await f(); };
  runner();

  ASSERT_EQ(res, std::nullopt);
  suspension.resolve();
  ASSERT_EQ(res, 10);
}

TEST(EagerCoroTest, test3) {
  auto suspension1 = Suspension<2>{};
  auto suspension2 = Suspension<4>{};
  auto suspension3 = Suspension<8>{};

  auto g = [&]() -> Task<unsigned> {
    auto res = 0;
    LOG_DEVEL << ADB_HERE << " res=" << res;
    res += co_await suspension1;
    LOG_DEVEL << ADB_HERE << " res=" << res;
    res += co_await suspension2;
    LOG_DEVEL << ADB_HERE << " res=" << res;
    res += co_await suspension3;
    LOG_DEVEL << ADB_HERE << " res=" << res;
    co_return res + 3;
  };

  auto f = [&g]() -> Task<int> {
    auto x = co_await g();
    co_return 7 + x;
  };

  std::optional<int> res;
  // note: keep the lambda alive until the coroutines are done!
  auto runner = [&]() -> CoroRunner { res = co_await f(); };
  runner();

  ASSERT_EQ(res, std::nullopt);
  suspension1.resolve();
  ASSERT_EQ(res, std::nullopt);
  suspension2.resolve();
  ASSERT_EQ(res, std::nullopt);
  suspension3.resolve();
  ASSERT_EQ(res, 24);
}

TEST(EagerCoroTest, test4) {
  auto suspension1 = Suspension<2>{};
  auto suspension2 = Suspension<4>{};
  auto suspension3 = Suspension<8>{};

  suspension2.resolve();

  auto g = [&]() -> Task<unsigned> {
    auto res = 0;
    LOG_DEVEL << ADB_HERE << " res=" << res;
    res += co_await suspension1;
    LOG_DEVEL << ADB_HERE << " res=" << res;
    res += co_await suspension2;
    LOG_DEVEL << ADB_HERE << " res=" << res;
    res += co_await suspension3;
    LOG_DEVEL << ADB_HERE << " res=" << res;
    co_return res + 3;
  };

  auto f = [&g]() -> Task<int> {
    auto x = co_await g();
    co_return 7 + x;
  };

  std::optional<int> res;
  // note: keep the lambda alive until the coroutines are done!
  auto runner = [&]() -> CoroRunner { res = co_await f(); };
  runner();

  ASSERT_EQ(res, std::nullopt);
  suspension1.resolve();
  ASSERT_EQ(res, std::nullopt);
  suspension3.resolve();
  ASSERT_EQ(res, 24);
}

struct TestException : std::exception {
  std::string what;
  explicit TestException(std::string what_) : what(std::move(what_)) {}
};

TEST(EagerCoroTest, test5) {
  auto g = [&]() -> Task<unsigned> {
    throw TestException("quokka");
    co_return 3;
  };

  auto f = [&g]() -> Task<int> {
    auto x = co_await g();
    co_return 7 + x;
  };

  std::optional<int> res;
  std::optional<TestException> ex;

  // note: keep the lambda alive until the coroutines are done!
  auto runner = [&]() -> CoroRunner {
    try {
      res = co_await f();
    } catch (TestException e) {
      ex = std::move(e);
    }
  };
  runner();

  ASSERT_EQ(res, std::nullopt);
  ASSERT_NE(ex, std::nullopt);
  ASSERT_EQ(ex->what, "quokka");
}

TEST(EagerCoroTest, threaded_test1) {
  auto suspension = Suspension<3>{};

  auto g = [&]() -> Task<unsigned> { co_return co_await suspension; };

  auto f = [&g]() -> Task<int> { co_return 7 + co_await g(); };

  std::optional<int> res;
  auto runner = [&]() -> CoroRunner { res = co_await f(); };

  std::atomic<bool> go = false;

  auto coroThread = std::jthread([&]() {
    go.wait(false);
    runner();
  });
  auto resolverThread = std::jthread([&]() {
    go.wait(false);
    suspension.resolve();
  });

  ASSERT_EQ(res, std::nullopt);

  go = true;
  go.notify_all();

  coroThread.join();
  resolverThread.join();

  ASSERT_EQ(res, 10);
}

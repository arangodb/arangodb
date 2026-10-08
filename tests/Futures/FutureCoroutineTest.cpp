#include "Async/Registry/promise.h"
#include "Async/Registry/registry_variable.h"
#include "Async/WaitTypes.h"
#include "Auth/Common.h"
#include "Basics/Result.h"
#include "Basics/voc-errors.h"
#include "Futures/Future.h"
#include "Mocks/ExecContextFactory.h"

#include <condition_variable>
#include <coroutine>
#include <mutex>
#include <thread>
#include <deque>

#include <gtest/gtest.h>

using namespace arangodb;
using namespace arangodb::futures;

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

template<typename WaitType>
struct FutureTest : ::testing::Test {
  void SetUp() override {
    arangodb::async_registry::get_thread_registry().garbage_collect();
    EXPECT_TRUE(std::holds_alternative<
                arangodb::containers::SharedPtr<arangodb::basics::ThreadInfo>>(
        *arangodb::async_registry::get_current_coroutine()));
  }

  void TearDown() override {
    arangodb::async_registry::get_thread_registry().garbage_collect();
    wait.stop();
  }

  WaitType wait;
};

using MyTypes = ::testing::Types<async_tests::NoWait, async_tests::WaitSlot,
                                 async_tests::ConcurrentNoWait>;
TYPED_TEST_SUITE(FutureTest, MyTypes);

TYPED_TEST(FutureTest, promises_in_async_registry_know_their_state) {
  {
    auto coro = [&]() -> Future<int> {
      co_await this->wait;
      co_return 12;
    }();

    if constexpr (std::is_same<decltype(this->wait), async_tests::WaitSlot>()) {
      // for WaitSlot fn is currently suspended
      expect_all_promises_in_state(arangodb::async_registry::State::Suspended,
                                   1);
    } else if constexpr (std::is_same<decltype(this->wait),
                                      async_tests::NoWait>()) {
      // for NoWait fn already finished
      expect_all_promises_in_state(arangodb::async_registry::State::Resolved,
                                   1);
    } else {
      // for ConcurrentNoWait both can happen, we don't know for sure here
    }

    this->wait.resume();
    this->wait.await();

    expect_all_promises_in_state(arangodb::async_registry::State::Resolved, 1);
  }
  expect_all_promises_in_state(arangodb::async_registry::State::Resolved, 0);
}

namespace {
auto find_promise_by_name(std::string_view name)
    -> std::optional<arangodb::async_registry::PromiseSnapshot> {
  std::optional<arangodb::async_registry::PromiseSnapshot> requested_promise =
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
    FutureTest,
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

      co_return;
    };
    static auto waiter_fn(TestType test) -> Future<Unit> {
      auto waiter_promise = find_promise_by_name("waiter_fn");
      EXPECT_TRUE(waiter_promise.has_value());
      EXPECT_TRUE(std::holds_alternative<arangodb::basics::ThreadInfo>(
          waiter_promise->requester));

      auto fn = Functions::awaited_fn(test);

      auto awaited_promise = find_promise_by_name("awaited_fn");
      EXPECT_TRUE(awaited_promise.has_value());
      EXPECT_EQ(awaited_promise->requester,
                arangodb::async_registry::Requester{waiter_promise->id});

      co_await std::move(fn);
    };
  };

  auto waiter = Functions::waiter_fn(this);

  this->wait.resume();
  this->wait.await();
}

TYPED_TEST(FutureTest,  // HERE
           promises_in_async_registry_know_their_requester_with_move) {
  using TestType = decltype(this);
  struct Functions {
    static auto awaited_fn(TestType test) -> Future<Unit> {
      auto promise = find_promise_by_name("awaited_fn");
      EXPECT_TRUE(promise.has_value());
      EXPECT_TRUE(std::holds_alternative<arangodb::basics::ThreadInfo>(
          promise->requester));

      co_await test->wait;

      co_return;
    };
    static auto waiter_fn(Future<Unit>&& fn) -> Future<Unit> {
      auto waiter_promise = find_promise_by_name("waiter_fn");
      EXPECT_TRUE(waiter_promise.has_value());
      EXPECT_TRUE(std::holds_alternative<arangodb::basics::ThreadInfo>(
          waiter_promise->requester));

      auto awaited_promise = find_promise_by_name("awaited_fn");
      EXPECT_TRUE(awaited_promise.has_value());
      if (awaited_promise.has_value()) {
        // nobody has co_awaited fn yet, so its requester is still the thread
        EXPECT_TRUE(std::holds_alternative<arangodb::basics::ThreadInfo>(
            awaited_promise->requester));
      }

      co_await std::move(fn);

      // The registry entry of fn is owned by its SharedState, which is deleted
      // as soon as the coroutine frame (Promise side) and the Future inside
      // the awaitable temporary (consumer side) have both detached.
      awaited_promise = find_promise_by_name("awaited_fn");
      if constexpr (std::is_same<TypeParam, async_tests::WaitSlot>::value) {
        // fn resolves inside its own callback, which resumes this coroutine
        // synchronously: the SharedState is still attached while we run here
        EXPECT_TRUE(awaited_promise.has_value());
      } else if constexpr (std::is_same<TypeParam,
                                        async_tests::NoWait>::value) {
        // fn already finished before this coroutine started, so only the
        // Future kept its SharedState alive. That Future died with the
        // awaitable temporary at the end of the co_await expression.
        EXPECT_FALSE(awaited_promise.has_value());
      }
      // ConcurrentNoWait: either of the above, depending on whether the worker
      // thread finished fn before the co_await. Whenever the entry is still
      // visible, its requester must have been updated by the co_await.
      if (awaited_promise.has_value()) {
        EXPECT_EQ(awaited_promise->requester,
                  arangodb::async_registry::Requester{waiter_promise->id});
      }

      // waiter did not change
      waiter_promise = find_promise_by_name("waiter_fn");
      EXPECT_TRUE(waiter_promise.has_value());
      EXPECT_TRUE(std::holds_alternative<arangodb::basics::ThreadInfo>(
          waiter_promise->requester));

      co_return;
    };
  };

  auto awaited_coro = Functions::awaited_fn(this);
  auto waiter = Functions::waiter_fn(std::move(awaited_coro));

  this->wait.resume();
  this->wait.await();
}

// COR-822: transaction::Methods::replicateOperations co_awaits the follower
// responses (whose futures are fulfilled on a network or scheduler thread)
// and performs an intermediate commit after resuming. The commit guard in
// RocksDBTrxBaseMethods::doCommitImpl reads
// ExecContext::current().isCanceled(); with a plain .thenValue continuation
// it would run on the fulfilling thread and observe the Superuser fallback
// (whose _canceled is always false) instead of the initiator's context. This
// test asserts the property the coroutine conversion relies on: resumption
// replays the initiator's ExecContext -- in particular for ConcurrentNoWait,
// where the resumption happens on a foreign thread without any ExecContext --
// so a cancellation of the initiating request is observed by the commit
// guard.
TYPED_TEST(FutureTest, canceled_initiator_context_is_replayed_after_resume) {
  auto ctx = arangodb::tests::mocks::makeClassicExecContext(
      "initiator", "", arangodb::auth::Level::RW, arangodb::auth::Level::NONE);
  ExecContextScope scope(ctx.execContext);
  ctx.execContext->cancel();

  auto guardResult = [&]() -> Future<Result> {
    co_await this->wait;
    // this mirrors the commit guard in RocksDBTrxBaseMethods::doCommitImpl
    auto const& exec = ExecContext::current();
    EXPECT_EQ(exec.user(), "initiator");
    if (exec.isCanceled()) {
      co_return Result(TRI_ERROR_ARANGO_READ_ONLY);
    }
    co_return Result();
  }();

  this->wait.resume();
  this->wait.await();

  ASSERT_TRUE(guardResult.isReady());
  EXPECT_EQ(std::move(guardResult).waitAndGet().errorNumber(),
            TRI_ERROR_ARANGO_READ_ONLY);
}

TYPED_TEST(FutureTest, execution_context_is_local_to_coroutine) {
  auto ctxBegin = arangodb::tests::mocks::makeClassicExecContext(
      "Begin", "", arangodb::auth::Level::RW, arangodb::auth::Level::NONE);
  ExecContextScope exec(ctxBegin.execContext);
  EXPECT_EQ(ExecContext::current().user(), "Begin");

  auto waiting_coro = [&]() -> Future<Unit> {
    EXPECT_EQ(ExecContext::current().user(), "Begin");
    auto ctxWaiting = arangodb::tests::mocks::makeClassicExecContext(
        "Waiting", "", arangodb::auth::Level::RW, arangodb::auth::Level::NONE);
    ExecContextScope exec(ctxWaiting.execContext);
    EXPECT_EQ(ExecContext::current().user(), "Waiting");
    co_await this->wait;
    EXPECT_EQ(ExecContext::current().user(), "Waiting");
    co_return;
  }();
  EXPECT_EQ(ExecContext::current().user(), "Begin");

  auto trivial_coro = []() -> Future<Unit> {
    EXPECT_EQ(ExecContext::current().user(), "Begin");
    co_return;
  }();

  auto calling_coro = [&]() -> Future<Unit> {
    EXPECT_EQ(ExecContext::current().user(), "Begin");
    auto ctxCalling = arangodb::tests::mocks::makeClassicExecContext(
        "Calling", "", arangodb::auth::Level::RW, arangodb::auth::Level::NONE);
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

  auto ctxEnd = arangodb::tests::mocks::makeClassicExecContext(
      "End", "", arangodb::auth::Level::RW, arangodb::auth::Level::NONE);
  ExecContextScope new_exec(ctxEnd.execContext);
  EXPECT_EQ(ExecContext::current().user(), "End");

  this->wait.resume();
  this->wait.await();
  EXPECT_EQ(ExecContext::current().user(), "End");
}

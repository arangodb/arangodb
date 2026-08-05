////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
////////////////////////////////////////////////////////////////////////////////

#include <gtest/gtest.h>

#include <chrono>
#include <cstddef>
#include <future>
#include <memory>
#include <stdexcept>
#include <vector>

#include "Basics/SharedPRNG.h"
#include "GeneralServer/RequestLane.h"
#include "Mocks/ExecContextFactory.h"
#include "Mocks/FakeRegistry.h"
#include "Mocks/FakeScheduler.h"
#include "Mocks/Servers.h"
#include "Scheduler/SupervisedScheduler.h"
#include "Utils/ExecContext.h"

using namespace arangodb;
using namespace arangodb::tests;

namespace {
auto makeUserContext(std::string username) {
  return mocks::makeClassicExecContext(std::move(username), "testdb",
                                       auth::Level::RW, auth::Level::RW);
}
}  // namespace

// Tests for COR-821: the scheduler captures the ExecContext installed on the
// enqueuing thread, installs it for the duration of the work item, and
// restores the executing thread's previous ExecContext afterwards. On real
// scheduler workers the previous state is always nullptr, so no context can
// leak into the next job; inline execution (e.g. FakeScheduler::runOnce())
// leaves the caller's context untouched.
//
// These tests use the FakeScheduler, which executes queued work items
// (Scheduler::WorkItem, i.e. exactly what real schedulers execute on their
// workers) synchronously on the calling thread via runOnce(); the
// WorkItem::invoke() semantics themselves are tested on directly created
// work items (FakeScheduler::makeWorkItem).
struct SchedulerExecContextTest : ::testing::Test {
  SchedulerExecContextTest() : scheduler(mockApplicationServer.server()) {}

  mocks::MockRestServer mockApplicationServer;
  // Make sure this thread starts without an ExecContext, no matter what
  // previous tests left behind; restores the previous state on destruction.
  ExecContextScope clearScope{nullptr};
  FakeScheduler scheduler;
};

TEST_F(SchedulerExecContextTest, queue_captures_exec_context_at_enqueue_time) {
  auto classic = makeUserContext("testuser");
  std::shared_ptr<ExecContext const> seenByJob;

  {
    ExecContextScope scope(classic.execContext);
    scheduler.queue(RequestLane::CLIENT_FAST, [&seenByJob]() {
      seenByJob = ExecContext::currentAsShared();
    });
  }

  ASSERT_EQ(ExecContext::currentAsShared(), nullptr);
  scheduler.runOnce();

  EXPECT_EQ(seenByJob.get(), classic.execContext.get());
  ASSERT_NE(seenByJob, nullptr);
  EXPECT_EQ(seenByJob->user(), "testuser");
}

TEST_F(SchedulerExecContextTest,
       work_item_installs_captured_context_and_restores_previous) {
  auto ctxA = makeUserContext("userA");
  auto ctxB = makeUserContext("userB");
  std::shared_ptr<ExecContext const> seenByJob;

  std::unique_ptr<Scheduler::WorkItemBase> item;
  {
    ExecContextScope scope(ctxA.execContext);
    item = FakeScheduler::makeWorkItem([&seenByJob]() {
      seenByJob = ExecContext::currentAsShared();
    });
  }

  {
    ExecContextScope scope(ctxB.execContext);
    item->invoke();
    // The job must see the context captured at creation time, not the
    // context installed on the executing thread.
    EXPECT_EQ(seenByJob.get(), ctxA.execContext.get());
    // Afterwards, the executing thread's previous context must have been
    // restored; the captured context must not stick around.
    EXPECT_EQ(ExecContext::currentAsShared(), ctxB.execContext);
  }
}

TEST_F(SchedulerExecContextTest,
       work_item_leaves_context_less_thread_without_exec_context) {
  auto ctxA = makeUserContext("userA");
  std::shared_ptr<ExecContext const> seenByJob;

  std::unique_ptr<Scheduler::WorkItemBase> item;
  {
    ExecContextScope scope(ctxA.execContext);
    item = FakeScheduler::makeWorkItem([&seenByJob]() {
      seenByJob = ExecContext::currentAsShared();
    });
  }

  // A real scheduler worker never has an ExecContext of its own; restoring
  // that (null) state after the job is what guarantees no context can leak
  // into the next job that runs on the same thread.
  ASSERT_EQ(ExecContext::currentAsShared(), nullptr);
  item->invoke();
  EXPECT_EQ(seenByJob.get(), ctxA.execContext.get());
  EXPECT_EQ(ExecContext::currentAsShared(), nullptr);
}

TEST_F(SchedulerExecContextTest, run_once_leaves_callers_exec_context_intact) {
  auto ctxA = makeUserContext("userA");
  auto ctxB = makeUserContext("userB");

  {
    ExecContextScope scope(ctxA.execContext);
    scheduler.queue(RequestLane::CLIENT_FAST, []() {});
  }

  {
    ExecContextScope scope(ctxB.execContext);
    scheduler.runOnce();
    // Running a work item inline must not clobber the caller's context.
    EXPECT_EQ(ExecContext::currentAsShared(), ctxB.execContext);
  }
}

TEST_F(SchedulerExecContextTest,
       queue_from_context_less_thread_runs_without_exec_context) {
  bool ran = false;
  ASSERT_EQ(ExecContext::currentAsShared(), nullptr);

  scheduler.queue(RequestLane::CLIENT_FAST, [&ran]() {
    ran = true;
    EXPECT_EQ(ExecContext::currentAsShared(), nullptr);
  });
  scheduler.runOnce();

  EXPECT_TRUE(ran);
  EXPECT_EQ(ExecContext::currentAsShared(), nullptr);
}

TEST_F(SchedulerExecContextTest,
       previous_context_is_restored_even_when_the_job_throws) {
  auto classic = makeUserContext("testuser");

  std::unique_ptr<Scheduler::WorkItemBase> item;
  {
    ExecContextScope scope(classic.execContext);
    item = FakeScheduler::makeWorkItem([]() {
      ASSERT_NE(ExecContext::currentAsShared(), nullptr);
      throw std::runtime_error("job failed");
    });
  }

  ASSERT_EQ(ExecContext::currentAsShared(), nullptr);
  EXPECT_THROW(item->invoke(), std::runtime_error);
  EXPECT_EQ(ExecContext::currentAsShared(), nullptr);
}

TEST_F(SchedulerExecContextTest, queue_delayed_captures_exec_context) {
  auto classic = makeUserContext("testuser");
  std::shared_ptr<ExecContext const> seenByHandler;
  bool wasCanceled = true;
  Scheduler::WorkHandle handle;

  {
    ExecContextScope scope(classic.execContext);
    handle = scheduler.queueDelayed("test-delayed", RequestLane::CLIENT_FAST,
                                    std::chrono::hours(1), [&](bool canceled) {
                                      wasCanceled = canceled;
                                      seenByHandler =
                                          ExecContext::currentAsShared();
                                    });
  }
  ASSERT_NE(handle, nullptr);
  ASSERT_TRUE(scheduler.queueEmpty());

  // Simulate the cron thread firing the item; this pushes the handler onto
  // the scheduler's queue. The (context-less) calling thread's ExecContext
  // must be left untouched.
  handle->run();
  ASSERT_EQ(scheduler.queueSize(), 1);
  EXPECT_EQ(ExecContext::currentAsShared(), nullptr);

  scheduler.runOnce();

  EXPECT_FALSE(wasCanceled);
  EXPECT_EQ(seenByHandler.get(), classic.execContext.get());
  EXPECT_EQ(ExecContext::currentAsShared(), nullptr);
}

TEST_F(SchedulerExecContextTest,
       canceled_delayed_item_still_installs_exec_context) {
  auto classic = makeUserContext("testuser");
  std::shared_ptr<ExecContext const> seenByHandler;
  bool wasCanceled = false;
  Scheduler::WorkHandle handle;

  {
    ExecContextScope scope(classic.execContext);
    handle = scheduler.queueDelayed("test-delayed", RequestLane::CLIENT_FAST,
                                    std::chrono::hours(1), [&](bool canceled) {
                                      wasCanceled = canceled;
                                      seenByHandler =
                                          ExecContext::currentAsShared();
                                    });
  }
  ASSERT_NE(handle, nullptr);

  // Cancellation still runs the handler body (with canceled == true), so it
  // must get the captured context installed like any other job.
  handle->cancel();
  ASSERT_EQ(scheduler.queueSize(), 1);
  scheduler.runOnce();

  EXPECT_TRUE(wasCanceled);
  EXPECT_EQ(seenByHandler.get(), classic.execContext.get());
  EXPECT_EQ(ExecContext::currentAsShared(), nullptr);
}

// End-to-end test against the real SupervisedScheduler (which requires at
// least 4 workers): jobs run under the enqueuer's ExecContext, and follow-up
// jobs queued without a context must not see any leftovers -- whichever
// worker picks them up either never had a context installed or must have
// restored its context-less state after its previous job.
// NOTE for all tests in this file: synchronization between the test thread
// and the scheduler/work items must use std::future/std::promise, never
// futures::Future/futures::Promise -- the std types obviously cannot, and
// never will, touch ExecContext::CURRENT, while the arangodb futures and
// coroutine machinery replays exactly the thread-local under test here.
struct SupervisedSchedulerExecContextTest : ::testing::Test {
  SupervisedSchedulerExecContextTest()
      : metrics(std::make_shared<arangodb::SchedulerMetrics>(metricsRegistry)),
        scheduler(mockApplicationServer.server(), 4, 4, 16, 16, 16, 16, 16,
                  0.33, metrics, sharedPRNG) {
    scheduler.start();
  }

  arangodb::tests::mocks::MockRestServer mockApplicationServer;
  // Make sure this thread starts without an ExecContext, no matter what
  // previous tests left behind; restores the previous state on destruction.
  ExecContextScope clearScope{nullptr};
  // must outlive `metrics`, which holds references into the registry
  arangodb::metrics::FakeRegistry metricsRegistry;
  std::shared_ptr<arangodb::SchedulerMetrics> metrics;
  basics::SharedPRNG sharedPRNG;
  SupervisedScheduler scheduler;
};

TEST_F(SupervisedSchedulerExecContextTest,
       worker_installs_captured_context_and_clears_it_between_jobs) {
  auto classic = makeUserContext("testuser");
  constexpr std::size_t numJobs = 16;

  // First wave: jobs queued under a user's ExecContext must see exactly that
  // context on the worker threads.
  {
    std::vector<std::promise<std::shared_ptr<ExecContext const>>> promises(
        numJobs);
    ExecContextScope scope(classic.execContext);
    for (auto& promise : promises) {
      scheduler.queue(RequestLane::CLIENT_FAST, [&promise]() {
        promise.set_value(ExecContext::currentAsShared());
      });
    }
    for (auto& promise : promises) {
      auto future = promise.get_future();
      ASSERT_EQ(future.wait_for(std::chrono::seconds(60)),
                std::future_status::ready);
      EXPECT_EQ(future.get().get(), classic.execContext.get());
    }
  }

  // Second wave: jobs queued from a context-less thread must all see a
  // cleared thread-local, no matter which worker (that just ran a job under
  // "testuser") picks them up.
  {
    std::vector<std::promise<std::shared_ptr<ExecContext const>>> promises(
        numJobs);
    for (auto& promise : promises) {
      scheduler.queue(RequestLane::CLIENT_FAST, [&promise]() {
        promise.set_value(ExecContext::currentAsShared());
      });
    }
    for (auto& promise : promises) {
      auto future = promise.get_future();
      ASSERT_EQ(future.wait_for(std::chrono::seconds(60)),
                std::future_status::ready);
      EXPECT_EQ(future.get(), nullptr);
    }
  }

  scheduler.shutdown();
}

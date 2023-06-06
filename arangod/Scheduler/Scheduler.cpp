////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// @author Dr. Frank Celler
/// @author Achim Brandt
////////////////////////////////////////////////////////////////////////////////

#include "Scheduler.h"

#include <velocypack/Builder.h>

#include <thread>

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/StringUtils.h"
#include "Basics/Thread.h"
#include "GeneralServer/Acceptor.h"
#include "GeneralServer/RestHandler.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "Metrics/CounterBuilder.h"
#include "Metrics/GaugeBuilder.h"
#include "Metrics/MetricsFeature.h"
#include "Random/RandomGenerator.h"
#include "Rest/GeneralResponse.h"
#include "RestServer/SharedPRNGFeature.h"
#include "Scheduler/SchedulerFeature.h"
#include "Statistics/RequestStatistics.h"

using namespace arangodb;
using namespace arangodb::basics;

namespace arangodb {

class SchedulerThread : public ServerThread<ArangodServer> {
 public:
  explicit SchedulerThread(Server& server, Scheduler& scheduler,
                           std::string const& name = "Scheduler")
      : ServerThread<ArangodServer>(server, name), _scheduler(scheduler) {}

  // shutdown is called by derived implementation!
  ~SchedulerThread() = default;

 protected:
  Scheduler& _scheduler;
};

class SchedulerCronThread : public SchedulerThread {
 public:
  explicit SchedulerCronThread(ArangodServer& server, Scheduler& scheduler)
      : SchedulerThread(server, scheduler, "SchedCron") {}

  ~SchedulerCronThread() { shutdown(); }

  void run() override { _scheduler.runCronThread(); }
};

}  // namespace arangodb

DECLARE_COUNTER(arangodb_scheduler_handler_tasks_created_total,
                "Number of scheduler tasks created");
DECLARE_COUNTER(arangodb_scheduler_queue_time_violations_total,
                "Tasks dropped because the client-requested queue time "
                "restriction would be violated");
DECLARE_GAUGE(
    arangodb_scheduler_ongoing_low_prio, uint64_t,
    "Total number of ongoing RestHandlers coming from the low prio queue");
DECLARE_GAUGE(arangodb_scheduler_low_prio_queue_last_dequeue_time, uint64_t,
              "Last recorded dequeue time for a low priority queue item [ms]");
DECLARE_GAUGE(arangodb_scheduler_stack_memory, uint64_t,
              "Approximate stack memory usage of worker threads");
DECLARE_GAUGE(arangodb_scheduler_queue_memory, int64_t,
              "Number of bytes allocated for tasks in the scheduler queue");

Scheduler::Scheduler(ArangodServer& server)
    : _server(server),
      _sharedPRNG(server.getFeature<SharedPRNGFeature>()),
      _metricsHandlerTasksCreated(
          server.getFeature<metrics::MetricsFeature>().add(
              arangodb_scheduler_handler_tasks_created_total{})),
      _metricsQueueTimeViolations(
          server.getFeature<metrics::MetricsFeature>().add(
              arangodb_scheduler_queue_time_violations_total{})),
      _ongoingLowPriorityGauge(
          _server.getFeature<metrics::MetricsFeature>().add(
              arangodb_scheduler_ongoing_low_prio{})),
      _metricsLastLowPriorityDequeueTime(
          _server.getFeature<metrics::MetricsFeature>().add(
              arangodb_scheduler_low_prio_queue_last_dequeue_time{})),
      _metricsStackMemoryWorkerThreads(
          server.getFeature<metrics::MetricsFeature>().add(
              arangodb_scheduler_stack_memory{})),
      _schedulerQueueMemory(server.getFeature<metrics::MetricsFeature>().add(
          arangodb_scheduler_queue_memory{})) {
  // Move this into the Feature and then move it else where
}

Scheduler::~Scheduler() = default;

bool Scheduler::start() {
  _cronThread.reset(new SchedulerCronThread(_server, *this));
  return _cronThread->start();
}

void Scheduler::trackQueueItemSize(std::int64_t x) noexcept {
  _schedulerQueueMemory += x;
}

void Scheduler::schedulerJobMemoryAccounting(std::int64_t x) noexcept {
  SchedulerFeature::SCHEDULER->trackQueueItemSize(x);
}

void Scheduler::shutdown() {
  TRI_ASSERT(isStopping());

  std::unique_lock<std::mutex> guard(_cronQueueMutex);
  guard.unlock();

  _croncv.notify_one();
  _cronThread.reset();

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  // At this point the cron thread has been stopped
  // And there will be no other people posting on the queue
  // Lets make sure that all items on the queue are disabled
  while (!_cronQueue.empty()) {
    auto const& top = _cronQueue.top();
    auto item = top.second.lock();
    if (item) {
      TRI_ASSERT(item->isDisabled()) << item->name();
    }
    _cronQueue.pop();
  }
#endif
}

void Scheduler::runCronThread() {
  std::unique_lock<std::mutex> guard(_cronQueueMutex);

  while (!isStopping()) {
    auto now = clock::now();
    clock::duration sleepTime = std::chrono::milliseconds(50);

    while (!_cronQueue.empty()) {
      // top is a reference to a tuple containing the timepoint and a shared_ptr
      // to the work item
      auto top = _cronQueue.top();
      if (top.first < now) {
        _cronQueue.pop();
        guard.unlock();

        // It is time to schedule this task, try to get the lock and obtain a
        // shared_ptr If this fails a default DelayedWorkItem is constructed
        // which has disabled == true
        try {
          auto item = top.second.lock();
          if (item) {
            item->run();
          }
        } catch (std::exception const& ex) {
          LOG_TOPIC("6d997", WARN, Logger::THREADS)
              << "caught exception in runCronThread: " << ex.what();
        }

        // always lock again, as we are going into the wait_for below
        guard.lock();

      } else {
        auto then = (top.first - now);

        sleepTime = (sleepTime > then ? then : sleepTime);
        break;
      }
    }

    _croncv.wait_for(guard, sleepTime);
  }
}

Scheduler::WorkHandle Scheduler::queueDelayed(
    std::string_view name, RequestLane lane, clock::duration delay,
    fu2::unique_function<void(bool cancelled)> handler) noexcept {
  TRI_ASSERT(!isStopping());

  if (delay < std::chrono::milliseconds(1)) {
    // execute directly
    queue(lane, [handler = std::move(handler)]() mutable { handler(false); });
    return nullptr;
  }

  auto item =
      std::make_shared<DelayedWorkItem>(name, std::move(handler), lane, this);
  {
    std::unique_lock<std::mutex> guard(_cronQueueMutex);
    _cronQueue.emplace(clock::now() + delay, item);

    if (delay < std::chrono::milliseconds(50)) {
      guard.unlock();
      // wakeup thread
      _croncv.notify_one();
    }
  }

  return item;
}

void Scheduler::trackCreateHandlerTask() noexcept {
  ++_metricsHandlerTasksCreated;
}

void Scheduler::trackBeginOngoingLowPriorityTask() noexcept {
  ++_ongoingLowPriorityGauge;
}

void Scheduler::trackEndOngoingLowPriorityTask() noexcept {
  --_ongoingLowPriorityGauge;
}

void Scheduler::trackQueueTimeViolation() { ++_metricsQueueTimeViolations; }

/// @brief returns the last stored dequeue time [ms]
uint64_t Scheduler::getLastLowPriorityDequeueTime() const noexcept {
  return _metricsLastLowPriorityDequeueTime.load();
}

void Scheduler::setLastLowPriorityDequeueTime(uint64_t time) noexcept {
#ifdef ARANGODB_ENABLE_FAILURE_TESTS
  bool setDequeueTime = false;
  TRI_IF_FAILURE("Scheduler::alwaysSetDequeueTime") { setDequeueTime = true; }
#else
  constexpr bool setDequeueTime = false;
#endif

  // update only probabilistically, in order to reduce contention on the
  // gauge
  if (setDequeueTime || (_sharedPRNG.rand() & 7) == 0) {
    _metricsLastLowPriorityDequeueTime.operator=(time);
  }
}

/*
void Scheduler::cancelAllTasks() {
  //std::unique_lock<std::mutex> guard(_cronQueueMutex);
  LOG_TOPIC("1da98", ERR, Logger::FIXME) << "cancelAllTasks";
  while (_cronQueue.size() > 0) {
    auto const& top = _cronQueue.top();
    auto item = top.second.lock();
    if (item) {
      item->cancel();
    }
    _cronQueue.pop();
  }
}
*/

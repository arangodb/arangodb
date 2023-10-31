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
#include "Basics/Exceptions.h"
#include "Basics/StringUtils.h"
#include "Basics/Thread.h"
#include "GeneralServer/Acceptor.h"
#include "GeneralServer/RestHandler.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "Random/RandomGenerator.h"
#include "Rest/GeneralResponse.h"
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

Scheduler::Scheduler(ArangodServer& server)
    : _server(server) /*: _stopping(false)*/
{
  // Move this into the Feature and then move it else where
}

Scheduler::~Scheduler() = default;

bool Scheduler::start() {
  _cronThread = std::make_unique<SchedulerCronThread>(_server, *this);
  return _cronThread->start();
}

void Scheduler::schedulerJobMemoryAccounting(std::int64_t x) noexcept {
  if (SchedulerFeature::SCHEDULER) {
    SchedulerFeature::SCHEDULER->trackQueueItemSize(x);
  }
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
    fu2::unique_function<void(bool cancelled)> handler) noexcept try {
  std::shared_ptr<DelayedWorkItem> item;

  try {
    TRI_IF_FAILURE("Scheduler::queueDelayedFail1") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }

    item =
        std::make_shared<DelayedWorkItem>(name, std::move(handler), lane, this);
  } catch (...) {
    return nullptr;
  }

  {
    try {
      TRI_IF_FAILURE("Scheduler::queueDelayedFail2") {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
      }
      std::unique_lock<std::mutex> guard(_cronQueueMutex);
      _cronQueue.emplace(clock::now() + delay, item);
    } catch (...) {
      // if emplacing throws, we can cancel the item directly
      item->cancel();
      return nullptr;
    }

    if (delay < std::chrono::milliseconds(50)) {
      // wakeup thread
      _croncv.notify_one();
    }
  }

  return item;
} catch (...) {
  return nullptr;
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

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
#include <velocypack/velocypack-aliases.h>

#include <thread>

#include "Basics/MutexLocker.h"
#include "Basics/StringUtils.h"
#include "Basics/Thread.h"
#include "Basics/cpu-relax.h"
#include "GeneralServer/Acceptor.h"
#include "GeneralServer/RestHandler.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "Random/RandomGenerator.h"
#include "Rest/GeneralResponse.h"
#include "Statistics/RequestStatistics.h"

using namespace arangodb;
using namespace arangodb::basics;

namespace arangodb {

class SchedulerThread : virtual public Thread {
 public:
  explicit SchedulerThread(application_features::ApplicationServer& server, Scheduler& scheduler)
      : Thread(server, "Scheduler"), _scheduler(scheduler) {}
  ~SchedulerThread() = default; // shutdown is called by derived implementation!

 protected:
  Scheduler& _scheduler;
};

class SchedulerCronThread : public SchedulerThread {
 public:
  explicit SchedulerCronThread(application_features::ApplicationServer& server,
                               Scheduler& scheduler)
      : Thread(server, "SchedCron"), SchedulerThread(server, scheduler) {}

  ~SchedulerCronThread() { shutdown(); }

  void run() override { _scheduler.runCronThread(); }
};

}  // namespace arangodb

Scheduler::Scheduler(application_features::ApplicationServer& server)
    : _server(server) /*: _stopping(false)*/
{
  // Move this into the Feature and then move it else where
}

Scheduler::~Scheduler() = default;

bool Scheduler::start() {
  _cronThread.reset(new SchedulerCronThread(_server, *this));
  return _cronThread->start();
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
      TRI_ASSERT(item->isDisabled());
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
      // top is a reference to a tuple containing the timepoint and a shared_ptr to the work item
      auto top = _cronQueue.top();
      if (top.first < now) {
        _cronQueue.pop();
        guard.unlock();

        // It is time to schedule this task, try to get the lock and obtain a shared_ptr
        // If this fails a default DelayedWorkItem is constructed which has disabled == true
        try {
          auto item = top.second.lock();
          if (item) {
            item->run();
          }
        } catch (std::exception const& ex) {
          LOG_TOPIC("6d997", WARN, Logger::THREADS) << "caught exception in runCronThread: " << ex.what();
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

std::pair<bool, Scheduler::WorkHandle> Scheduler::queueDelay(
    RequestLane lane, clock::duration delay, fu2::unique_function<void(bool cancelled)> handler) {
  TRI_ASSERT(!isStopping());

  if (delay < std::chrono::milliseconds(1)) {
    // execute directly
    bool queued =
        queue(lane, [handler = std::move(handler)]() mutable { handler(false); });
    return std::make_pair(queued, nullptr);
  }

  auto item = std::make_shared<DelayedWorkItem>(std::move(handler), lane, this);
  {
    std::unique_lock<std::mutex> guard(_cronQueueMutex);
    _cronQueue.emplace(clock::now() + delay, item);

    if (delay < std::chrono::milliseconds(50)) {
      guard.unlock();
      // wakeup thread
      _croncv.notify_one();
    }
  }

  return std::make_pair(true, item);
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

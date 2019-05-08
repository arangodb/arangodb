////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2018 ArangoDB GmbH, Cologne, Germany
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
#include "GeneralServer/Task.h"
#include "Logger/Logger.h"
#include "Random/RandomGenerator.h"
#include "Rest/GeneralResponse.h"
#include "Statistics/RequestStatistics.h"

using namespace arangodb;
using namespace arangodb::basics;

namespace arangodb {

class SchedulerThread : virtual public Thread {
 public:
  explicit SchedulerThread(Scheduler& scheduler)
      : Thread("Scheduler"), _scheduler(scheduler) {}
  ~SchedulerThread() {} // shutdown is called by derived implementation!

 protected:
  Scheduler& _scheduler;
};

class SchedulerCronThread : public SchedulerThread {
 public:
  explicit SchedulerCronThread(Scheduler& scheduler)
      : Thread("SchedCron"), SchedulerThread(scheduler) {}

  ~SchedulerCronThread() { shutdown(); }

  void run() override { _scheduler.runCronThread(); }
};

}  // namespace arangodb

Scheduler::Scheduler() /*: _stopping(false)*/
{
  // Move this into the Feature and then move it else where
}

Scheduler::~Scheduler() {}

bool Scheduler::start() {
  _cronThread.reset(new SchedulerCronThread(*this));
  return _cronThread->start();
}

void Scheduler::shutdown() {
  TRI_ASSERT(isStopping());

  {
    std::unique_lock<std::mutex> guard(_cronQueueMutex);
    _croncv.notify_one();
  }
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
      auto const& top = _cronQueue.top();

      if (top.first < now) {
        // It is time to scheduler this task, try to get the lock and obtain a shared_ptr
        // If this fails a default WorkItem is constructed which has disabled == true
        auto item = top.second.lock();
        if (item) {
          try {
            item->run();
          } catch (std::exception const& ex) {
            LOG_TOPIC("6d997", WARN, Logger::THREADS) << "caught exception in runCronThread: " << ex.what();
          }
        }
        _cronQueue.pop();
      } else {
        auto then = (top.first - now);

        sleepTime = (sleepTime > then ? then : sleepTime);
        break;
      }
    }

    _croncv.wait_for(guard, sleepTime);
  }
}

Scheduler::WorkHandle Scheduler::queueDelay(RequestLane lane, clock::duration delay,
                                            std::function<void(bool cancelled)> handler) {
  TRI_ASSERT(!isStopping());

  if (delay < std::chrono::milliseconds(1)) {
    // execute directly
    queue(lane, [handler = std::move(handler)]() { handler(false); });
    return nullptr;
  }

  auto item = std::make_shared<WorkItem>(std::move(handler), lane, this);
  {
    std::unique_lock<std::mutex> guard(_cronQueueMutex);
    _cronQueue.emplace(clock::now() + delay, item);

    if (delay < std::chrono::milliseconds(50)) {
      // wakeup thread
      _croncv.notify_one();
    }
  }

  return item;
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

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
#include "GeneralServer/RestHandler.h"
#include "Logger/Logger.h"
#include "Random/RandomGenerator.h"
#include "Rest/GeneralResponse.h"
#include "GeneralServer/Acceptor.h"
#include "GeneralServer/Task.h"
#include "Statistics/RequestStatistics.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

namespace arangodb {
namespace rest {
class SchedulerThread : virtual public Thread {
public:
  SchedulerThread(Scheduler &scheduler) : Thread("Scheduler"), _scheduler(scheduler) {}
  ~SchedulerThread() { shutdown(); }
protected:
  Scheduler &_scheduler;
};

class SchedulerCronThread : public SchedulerThread {
public:
  SchedulerCronThread(Scheduler &scheduler) : Thread("SchedCron"), SchedulerThread(scheduler) {}
  void run() { _scheduler.runCron(); };
};

}
}


Scheduler::Scheduler()
{
#ifdef _WIN32
// Windows does not support POSIX signal handling
#else
  struct sigaction action;
  memset(&action, 0, sizeof(action));
  sigfillset(&action.sa_mask);

  // ignore broken pipes
  action.sa_handler = SIG_IGN;

  int res = sigaction(SIGPIPE, &action, nullptr);

  if (res < 0) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME)
        << "cannot initialize signal handlers for pipe";
  }
#endif
}

Scheduler::~Scheduler() {}


bool Scheduler::start() {

  _cronThread.reset(new SchedulerCronThread(*this));
  _cronThread->start();

  return true;
}

void Scheduler::beginShutdown() {
  std::unique_lock<std::mutex> guard2(_cronQueueMutex);
  _croncv.notify_one();
}

void Scheduler::shutdown () {
  // call the destructor of all threads
  _cronThread.reset();
}

void Scheduler::runCron()
{
  std::unique_lock<std::mutex> guard(_cronQueueMutex);

  while (!isStopping()) {

    auto now = clock::now();
    clock::duration sleepTime = std::chrono::milliseconds(50);

    while (_cronQueue.size() > 0) {
      // top is a reference to a tuple containing the timepoint and a shared_ptr to the work item
      auto const& top = _cronQueue.top();

      if (top.first < now) {
        // It is time to scheduler this task, try to get the lock and obtain a shared_ptr
        // If this fails a default WorkItem is constructed which has disabled == true
        auto item = top.second.lock();
        if (item) {
          item->run();
        }
        _cronQueue.pop();
      } else {
        auto then = (top.first - now);

        sleepTime = (sleepTime > then ? then : sleepTime);
        break ;
      }
    }

    _croncv.wait_for(guard, sleepTime);

  }
}

Scheduler::WorkHandle Scheduler::queueDelay(
    RequestLane lane,
    clock::duration delay,
    std::function<void(bool cancelled)> && handler
) {
  if (delay < std::chrono::milliseconds(1)) {
    // execute directly
    queue(lane, [handler](){ handler(false); });
  }

  auto item = std::make_shared<WorkItem>(std::move(handler), lane, this);
  std::unique_lock<std::mutex> guard(_cronQueueMutex);
  _cronQueue.emplace(clock::now() + delay, item);

  if (delay < std::chrono::milliseconds(50)) {
    // wakeup thread
    _croncv.notify_one();
  }

  return item;
}

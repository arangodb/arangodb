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
  std::unique_lock<std::mutex> guard2(_priorityQueueMutex);
  _conditionCron.notify_one();
}

void Scheduler::shutdown () {
  // call the destructor of all threads
  _cronThread.reset();
}

void Scheduler::runCron() {

  std::unique_lock<std::mutex> guard(_priorityQueueMutex);

  uint64_t tick = 0;

  while (!isStopping()) {

    tick++;

    auto now = clock::now();


    clock::duration sleepTime = std::chrono::milliseconds(50);

    while (_priorityQueue.size() > 0) {
      auto &top = _priorityQueue.top();

      if (top->_cancelled || top->_due < now) {
        post([top]() { top->_handler(top->_cancelled); });
        _priorityQueue.pop();
      } else {
        auto then = (top->_due - now);

        sleepTime = (sleepTime > then ? then : sleepTime);
        break ;
      }
    }

    _conditionCron.wait_for(guard, sleepTime);

  }

}

Scheduler::WorkHandle Scheduler::postDelay(clock::duration delay,
  std::function<void(bool cancelled)> const& callback) {

  if (delay < std::chrono::milliseconds(1)) {
    post([callback]() {callback(false);});
    return WorkHandle{};
  }

  std::unique_lock<std::mutex> guard(_priorityQueueMutex);
  auto handle = std::make_shared<Scheduler::DelayedWorkItem>(callback, delay);

  _priorityQueue.push(handle);

  if (delay < std::chrono::milliseconds(50)) {
    // wakeup thread
    _conditionCron.notify_one();
  }

  return handle;
}


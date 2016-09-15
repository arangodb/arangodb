////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Martin Schoenert
////////////////////////////////////////////////////////////////////////////////

#include "SchedulerThread.h"

#include "Logger/Logger.h"
#include "Basics/MutexLocker.h"

#ifdef _WIN32
#include "Basics/win-utils.h"
#endif

#include "Scheduler/Scheduler.h"
#include "Scheduler/Task.h"

#include <velocypack/Value.h>
#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb::basics;
using namespace arangodb::rest;

SchedulerThread::SchedulerThread(Scheduler* scheduler, EventLoop loop,
                                 bool defaultLoop)
    : Thread("Scheduler"),
      _scheduler(scheduler),
      _defaultLoop(defaultLoop),
      _loop(loop),
      _numberTasks(0),
      _taskData(100) {}

////////////////////////////////////////////////////////////////////////////////
/// @brief begin shutdown sequence
////////////////////////////////////////////////////////////////////////////////

void SchedulerThread::beginShutdown() {
  Thread::beginShutdown();

  LOG(TRACE) << "beginning shutdown sequence of scheduler thread ("
             << threadId() << ")";

  _scheduler->wakeupLoop(_loop);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief registers a task
////////////////////////////////////////////////////////////////////////////////

bool SchedulerThread::registerTask(Scheduler* scheduler, Task* task) {
  // thread has already been stopped
  if (isStopping()) {
    // do nothing
    deleteTask(task);
    return false;
  }

  TRI_ASSERT(scheduler != nullptr);
  TRI_ASSERT(task != nullptr);

  // same thread, in this case it does not matter if we are inside the loop
  if (threadId() == currentThreadId()) {
    bool ok = setupTask(task, scheduler, _loop);

    if (ok) {
      ++_numberTasks;
    } else {
      LOG(WARN) << "In SchedulerThread::registerTask setupTask has failed";
      cleanupTask(task);
      deleteTask(task);
    }

    return ok;
  }

  Work w(SETUP, scheduler, task);

  // different thread, be careful - we have to stop the event loop
  // put the register request onto the queue
  MUTEX_LOCKER(mutexLocker, _queueLock);

  _queue.push_back(w);

  scheduler->wakeupLoop(_loop);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief unregisters a task
////////////////////////////////////////////////////////////////////////////////

void SchedulerThread::unregisterTask(Task* task) {
  // thread has already been stopped
  if (isStopping()) {
    return;
  }

  // same thread, in this case it does not matter if we are inside the loop
  if (threadId() == currentThreadId()) {
    cleanupTask(task);
    --_numberTasks;
  }

  // different thread, be careful - we have to stop the event loop
  else {
    Work w(CLEANUP, nullptr, task);

    // put the unregister request into the queue
    MUTEX_LOCKER(mutexLocker, _queueLock);

    _queue.push_back(w);

    _scheduler->wakeupLoop(_loop);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief unregisters a task
////////////////////////////////////////////////////////////////////////////////

void SchedulerThread::destroyTask(Task* task) {
  // thread has already been stopped
  if (isStopping()) {
    deleteTask(task);
    return;
  }

  // same thread, in this case it does not matter if we are inside the loop
  if (threadId() == currentThreadId()) {
    cleanupTask(task);
    deleteTask(task);
    --_numberTasks;
  }

  // different thread, be careful - we have to stop the event loop
  else {
    // put the unregister request into the queue
    Work w(DESTROY, nullptr, task);

    MUTEX_LOCKER(mutexLocker, _queueLock);

    _queue.push_back(w);

    _scheduler->wakeupLoop(_loop);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sends data to a task
////////////////////////////////////////////////////////////////////////////////

void SchedulerThread::signalTask(std::unique_ptr<TaskData>& data) {
  bool result = _taskData.push(data.get());
  if (result) {
    data.release();
    _scheduler->wakeupLoop(_loop);
  }
}

void SchedulerThread::run() {
  LOG(TRACE) << "scheduler thread started (" << threadId() << ")";

  if (_defaultLoop) {
#ifdef TRI_HAVE_POSIX_THREADS
    sigset_t all;
    sigemptyset(&all);
    pthread_sigmask(SIG_SETMASK, &all, 0);
#endif
  }

  while (!isStopping()) {
    // handle the returned data
    TaskData* data;

    while (_taskData.pop(data)) {
      Task* task = _scheduler->lookupTaskById(data->_taskId);

      if (task != nullptr) {
        task->signalTask(data);
      }

      delete data;
    }

    // handle the events
    try {
      _scheduler->eventLoop(_loop);
    } catch (std::exception const& e) {
#ifdef TRI_HAVE_POSIX_THREADS
      if (isStopping()) {
        LOG(WARN) << "caught cancelation exception during work";
        throw;
      }
#endif
      LOG(WARN) << "caught exception from ev_loop: " << e.what();
    } catch (...) {
#ifdef TRI_HAVE_POSIX_THREADS
      if (isStopping()) {
        LOG(WARN) << "caught cancelation exception during work";
        throw;
      }
#endif
      LOG(WARN) << "caught exception from ev_loop";
    }

#if defined(DEBUG_SCHEDULER_THREAD)
    LOG(TRACE) << "left scheduler loop " << threadId();
#endif

    while (true) {
      Work w;

      {
        MUTEX_LOCKER(mutexLocker,
                     _queueLock);  // TODO(fc) XXX goto boost lockfree

        if (_queue.empty()) {
          break;
        }

        w = _queue.front();
        _queue.pop_front();
      }

      // will only get here if there is something to do
      switch (w.work) {
        case CLEANUP: {
          cleanupTask(w.task);
          --_numberTasks;
          break;
        }

        case SETUP: {
          bool ok = setupTask(w.task, w.scheduler, _loop);

          if (ok) {
            ++_numberTasks;
          } else {
            cleanupTask(w.task);
            deleteTask(w.task);
          }

          break;
        }

        case DESTROY: {
          cleanupTask(w.task);
          deleteTask(w.task);
          --_numberTasks;
          break;
        }

        case INVALID: {
          LOG(ERR) << "logic error. got invalid Work item";
          break;
        }
      }
    }
  }

  LOG(TRACE) << "scheduler thread stopped (" << threadId() << ")";

  // pop all undeliviered task data
  {
    TaskData* data;

    while (_taskData.pop(data)) {
      delete data;
    }
  }

  // pop all elements from the queue and delete them
  while (true) {
    Work w;

    {
      MUTEX_LOCKER(mutexLocker, _queueLock);

      if (_queue.empty()) {
        break;
      }

      w = _queue.front();
      _queue.pop_front();
    }

    // will only get here if there is something to do
    switch (w.work) {
      case CLEANUP:
        break;

      case SETUP:
        break;

      case DESTROY:
        deleteTask(w.task);
        break;

      case INVALID:
        LOG(ERR) << "logic error. got invalid Work item";
        break;
    }
  }
}

void SchedulerThread::addStatus(VPackBuilder* b) {
  Thread::addStatus(b);
  b->add("numberTasks", VPackValue(_numberTasks.load()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief input-output scheduler
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2011 triagens GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
/// @author Achim Brandt
/// @author Copyright 2008-2011, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "SchedulerImpl.h"

#include <Logger/Logger.h>
#include <Basics/MutexLocker.h>
#include <Basics/StringUtils.h>
#include <Basics/Thread.h>

#include "Scheduler/Task.h"
#include "Scheduler/SchedulerThread.h"

using namespace triagens::basics;

namespace triagens {
  namespace rest {

    // -----------------------------------------------------------------------------
    // constructors and destructors
    // -----------------------------------------------------------------------------

    SchedulerImpl::SchedulerImpl (size_t nrThreads)
      : nrThreads(nrThreads),
        stopping(0),
        nextLoop(0) {

      // check for multi-threading scheduler
      multiThreading = (nrThreads > 1);

      if (! multiThreading) {
        nrThreads = 1;
      }

      // report status
      LOGGER_TRACE << "scheduler is " << (multiThreading ? "multi" : "single") << "-threaded, number of threads = " << nrThreads;

      // setup signal handlers
      initialiseSignalHandlers();
    }



    SchedulerImpl::~SchedulerImpl () {

      // delete the open tasks
      for (set<Task*>::iterator i = taskRegistered.begin();  i != taskRegistered.end();  ++i) {
        deleteTask(*i);
      }
    }

    // -----------------------------------------------------------------------------
    // public methods
    // -----------------------------------------------------------------------------

    bool SchedulerImpl::start (ConditionVariable* cv) {
      MUTEX_LOCKER(schedulerLock);

      // start the schedulers threads
      for (size_t i = 0;  i < nrThreads;  ++i) {
        bool ok = threads[i]->start(cv);

        if (! ok) {
          LOGGER_FATAL << "cannot start threads";
          return false;
        }
      }

      // and wait until the threads are started
      bool waiting = true;

      while (waiting) {
        waiting = false;

        usleep(1000);

        for (size_t i = 0;  i < nrThreads;  ++i) {
          if (! threads[i]->isRunning()) {
            waiting = true;
            LOGGER_TRACE << "waiting for thread #" << i << " to start";
          }
        }
      }

      LOGGER_TRACE << "all scheduler threads are up and running";
      return true;
    }



    bool SchedulerImpl::isShutdownInProgress () {
      return stopping != 0;
    }



    bool SchedulerImpl::isRunning () {
      MUTEX_LOCKER(schedulerLock);

      for (size_t i = 0;  i < nrThreads;  ++i) {
        if (threads[i]->isRunning()) {
          return true;
        }
      }

      return false;
    }



    void SchedulerImpl::registerTask (Task* task) {
      if (stopping != 0) {
        return;
      }

      SchedulerThread* thread = 0;

      {
        MUTEX_LOCKER(schedulerLock);

        LOGGER_TRACE << "registerTask for task " << task;

        size_t n = 0;

        if (multiThreading && ! task->needsMainEventLoop()) {
          n = (++nextLoop) % nrThreads;
        }

        thread = threads[n];

        task2thread[task] = thread;
        taskRegistered.insert(task);
        ++current[task->getName()];
      }

      thread->registerTask(this, task);
    }



    void SchedulerImpl::unregisterTask (Task* task) {
      if (stopping != 0) {
        return;
      }

      SchedulerThread* thread = 0;

      {
        MUTEX_LOCKER(schedulerLock);

        map<Task*, SchedulerThread*>::iterator i = task2thread.find(task);

        if (i == task2thread.end()) {
          LOGGER_WARNING << "unregisterTask called for a unknown task " << task;

          return;
        }
        else {
          LOGGER_TRACE << "unregisterTask for task " << task;

          thread = i->second;

          if (taskRegistered.count(task) > 0) {
            taskRegistered.erase(task);
            --current[task->getName()];
          }
        }
      }

      thread->unregisterTask(task);
    }



    void SchedulerImpl::destroyTask (Task* task) {
      if (stopping != 0) {
        return;
      }

      SchedulerThread* thread = 0;

      {
        MUTEX_LOCKER(schedulerLock);

        map<Task*, SchedulerThread*>::iterator i = task2thread.find(task);

        if (i == task2thread.end()) {
          LOGGER_WARNING << "destroyTask called for a unknown task " << task;

          return;
        }
        else {
          LOGGER_TRACE << "destroyTask for task " << task;

          thread = i->second;

          if (taskRegistered.count(task) > 0) {
            taskRegistered.erase(task);
            --current[task->getName()];
          }

          task2thread.erase(i);
        }
      }

      thread->destroyTask(task);
    }



    void SchedulerImpl::beginShutdown () {
      if (stopping != 0) {
        return;
      }

      MUTEX_LOCKER(schedulerLock);

      if (stopping == 0) {
        LOGGER_DEBUG << "beginning shutdown sequence of scheduler";

        stopping = 1;

        for (size_t i = 0;  i < nrThreads;  ++i) {
          threads[i]->beginShutdown();
        }
      }
    }



    void SchedulerImpl::reportStatus () {
      if (TRI_IsDebugLogging(__FILE__)) {
        MUTEX_LOCKER(schedulerLock);

        string status = "scheduler: ";

        LoggerInfo info;
        info << LoggerData::Task("scheduler status");

        for (map<string, int>::const_iterator i = current.begin();  i != current.end();  ++i) {
          string val = StringUtils::itoa(i->second);

          status += i->first + " = " + val + " ";

          info
          << LoggerData::Extra(i->first)
          << LoggerData::Extra(val);
        }

        LOGGER_DEBUG_I(info)
        << status;

        LOGGER_HEARTBEAT_I(info);
      }
    }

    // -----------------------------------------------------------------------------
    // private methods (signals)
    // -----------------------------------------------------------------------------

    void SchedulerImpl::initialiseSignalHandlers () {
      struct sigaction action;
      memset(&action, 0, sizeof(action));
      sigfillset(&action.sa_mask);

      // ignore broken pipes
      action.sa_handler = SIG_IGN;

      int res = sigaction(SIGPIPE, &action, 0);

      if (res < 0) {
        LOGGER_ERROR << "cannot initialise signal handlers for pipe";
      }
    }
  }
}

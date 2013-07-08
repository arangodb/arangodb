////////////////////////////////////////////////////////////////////////////////
/// @brief input-output scheduler
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2013 triAGENS GmbH, Cologne, Germany
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
/// @author Copyright 2008-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifdef _WIN32
#include "BasicsC/win-utils.h"
#endif

#include "Scheduler.h"

#include "Basics/MutexLocker.h"
#include "Basics/StringUtils.h"
#include "Basics/Thread.h"
#include "Logger/Logger.h"
#include "Scheduler/SchedulerThread.h"
#include "Scheduler/Task.h"

using namespace triagens::basics;
using namespace triagens::rest;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Scheduler
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

Scheduler::Scheduler (size_t nrThreads)
  : nrThreads(nrThreads),
    stopping(0),
    nextLoop(0),
    _active(true) {

  // check for multi-threading scheduler
  multiThreading = (nrThreads > 1);

  if (! multiThreading) {
    nrThreads = 1;
  }

  // report status
  if (multiThreading) {
    LOGGER_TRACE("scheduler is multi-threaded, number of threads = " << nrThreads);
  }
  else {
    LOGGER_TRACE("scheduler is single-threaded");
  }

  // setup signal handlers
  initialiseSignalHandlers();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

Scheduler::~Scheduler () {
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Threading
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief starts scheduler, keeps running
////////////////////////////////////////////////////////////////////////////////

bool Scheduler::start (ConditionVariable* cv) {
  MUTEX_LOCKER(schedulerLock);

  // start the schedulers threads
  for (size_t i = 0;  i < nrThreads;  ++i) {
    bool ok = threads[i]->start(cv);

    if (! ok) {
      LOGGER_FATAL_AND_EXIT("cannot start threads");
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
        LOGGER_TRACE("waiting for thread #" << i << " to start");
      }
    }
  }

  LOGGER_TRACE("all scheduler threads are up and running");
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if the scheduler threads are up and running
////////////////////////////////////////////////////////////////////////////////

bool Scheduler::isStarted () {
  MUTEX_LOCKER(schedulerLock);

  for (size_t i = 0;  i < nrThreads;  ++i) {
    if (! threads[i]->isStarted()) {
      return false;
    }
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief opens the scheduler for business
////////////////////////////////////////////////////////////////////////////////

bool Scheduler::open () {
  MUTEX_LOCKER(schedulerLock);

  for (size_t i = 0;  i < nrThreads;  ++i) {
    if (! threads[i]->open()) {
      return false;
    }
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if scheduler is still running
////////////////////////////////////////////////////////////////////////////////

bool Scheduler::isRunning () {
  MUTEX_LOCKER(schedulerLock);

  for (size_t i = 0;  i < nrThreads;  ++i) {
    if (threads[i]->isRunning()) {
      return true;
    }
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief starts shutdown sequence
////////////////////////////////////////////////////////////////////////////////

void Scheduler::beginShutdown () {
  if (stopping != 0) {
    return;
  }

  MUTEX_LOCKER(schedulerLock);

  LOGGER_DEBUG("beginning shutdown sequence of scheduler");

  for (size_t i = 0;  i < nrThreads;  ++i) {
    threads[i]->beginShutdown();
  }

  // set the flag AFTER stopping the threads
  stopping = 1;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if scheduler is shuting down
////////////////////////////////////////////////////////////////////////////////

bool Scheduler::isShutdownInProgress () {
  return stopping != 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief shuts down the scheduler
////////////////////////////////////////////////////////////////////////////////

void Scheduler::shutdown () {
  for (set<Task*>::iterator i = taskRegistered.begin();
       i != taskRegistered.end();
       ++i) {
    LOGGER_WARNING("forcefully removing task '" << (*i)->getName() << "'");

    deleteTask(*i);
  }

  taskRegistered.clear();
  task2thread.clear();
  current.clear();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief registers a new task
////////////////////////////////////////////////////////////////////////////////

void Scheduler::registerTask (Task* task) {
  SchedulerThread* thread = 0;

  {
    MUTEX_LOCKER(schedulerLock);

    LOGGER_TRACE("registerTask for task " << task << " (" << task->getName() << ")");

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

////////////////////////////////////////////////////////////////////////////////
/// @brief unregisters a task
////////////////////////////////////////////////////////////////////////////////

void Scheduler::unregisterTask (Task* task) {
  SchedulerThread* thread = 0;

  {
    MUTEX_LOCKER(schedulerLock);

    map<Task*, SchedulerThread*>::iterator i = task2thread.find(task);

    if (i == task2thread.end()) {
      LOGGER_WARNING("unregisterTask called for an unknown task " << task << " (" << task->getName() << ")");

      return;
    }
    else {
      LOGGER_TRACE("unregisterTask for task " << task << " (" << task->getName() << ")");

      thread = i->second;

      if (taskRegistered.count(task) > 0) {
        taskRegistered.erase(task);
        --current[task->getName()];
      }

      task2thread.erase(i);
    }
  }

  thread->unregisterTask(task);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys task
////////////////////////////////////////////////////////////////////////////////

void Scheduler::destroyTask (Task* task) {
  SchedulerThread* thread = 0;

  {
    MUTEX_LOCKER(schedulerLock);

    map<Task*, SchedulerThread*>::iterator i = task2thread.find(task);

    if (i == task2thread.end()) {
      LOGGER_WARNING("destroyTask called for an unknown task " << task << " (" << task->getName() << ")");

      return;
    }
    else {
      LOGGER_TRACE("destroyTask for task " << task << " (" << task->getName() << ")");

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

////////////////////////////////////////////////////////////////////////////////
/// @brief called to display current status
////////////////////////////////////////////////////////////////////////////////

void Scheduler::reportStatus () {
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

    LOGGER_DEBUG_I(info, status);
    LOGGER_HEARTBEAT_I(info, "");
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                            static private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Scheduler
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief inialises the signal handlers for the scheduler
////////////////////////////////////////////////////////////////////////////////

void Scheduler::initialiseSignalHandlers () {

#ifdef _WIN32
  // Windows does not support POSIX signal handling
#else
  struct sigaction action;
  memset(&action, 0, sizeof(action));
  sigfillset(&action.sa_mask);

  // ignore broken pipes
  action.sa_handler = SIG_IGN;

  int res = sigaction(SIGPIPE, &action, 0);

  if (res < 0) {
    LOGGER_ERROR("cannot initialise signal handlers for pipe");
  }
#endif

}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:

////////////////////////////////////////////////////////////////////////////////
/// @brief input-output scheduler
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2008-2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifdef _WIN32
#include "Basics/win-utils.h"
#endif

#include "Scheduler.h"

#include "Basics/MutexLocker.h"
#include "Basics/StringUtils.h"
#include "Basics/Thread.h"
#include "Basics/json.h"
#include "Basics/logging.h"
#include "Scheduler/SchedulerThread.h"
#include "Scheduler/Task.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::rest;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

Scheduler::Scheduler (size_t nrThreads)
  : nrThreads(nrThreads),
    threads(0),
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
    LOG_TRACE("scheduler is multi-threaded, number of threads: %d", (int) nrThreads);
  }
  else {
    LOG_TRACE("scheduler is single-threaded");
  }

  // setup signal handlers
  initialiseSignalHandlers();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

Scheduler::~Scheduler () {
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief starts scheduler, keeps running
////////////////////////////////////////////////////////////////////////////////

bool Scheduler::start (ConditionVariable* cv) {
  MUTEX_LOCKER(schedulerLock);

  // start the schedulers threads
  for (size_t i = 0;  i < nrThreads;  ++i) {
    bool ok = threads[i]->start(cv);

    if (! ok) {
      LOG_FATAL_AND_EXIT("cannot start threads");
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
        LOG_TRACE("waiting for thread #%d to start", (int) i);
      }
    }
  }

  LOG_TRACE("all scheduler threads are up and running");
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

  LOG_DEBUG("beginning shutdown sequence of scheduler");

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
    LOG_DEBUG("forcefully removing task '%s'", (*i)->name().c_str());

    deleteTask(*i);
  }

  taskRegistered.clear();
  task2thread.clear();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief list user tasks
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* Scheduler::getUserTasks () {
  TRI_json_t* json = TRI_CreateListJson(TRI_UNKNOWN_MEM_ZONE);

  if (json == nullptr) {
    return nullptr;
  }

  {
    MUTEX_LOCKER(schedulerLock);

    map<Task*, SchedulerThread*>::iterator i = task2thread.begin();
    while (i != task2thread.end()) {
      Task* task = (*i).first;

      if (task->isUserDefined()) {
        TRI_json_t* obj = task->toJson();

        if (obj != nullptr) {
          TRI_PushBack3ListJson(TRI_UNKNOWN_MEM_ZONE, json, obj);
        }
      }

      ++i;
    }
  }

  return json;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get a single user task
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* Scheduler::getUserTask (string const& id) {
  MUTEX_LOCKER(schedulerLock);

  map<Task*, SchedulerThread*>::iterator i = task2thread.begin();
  while (i != task2thread.end()) {
    Task* task = (*i).first;

    if (task->isUserDefined() && task->id() == id) {
      return task->toJson();
    }

    ++i;
  }

  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief unregister and delete a user task by id
////////////////////////////////////////////////////////////////////////////////

int Scheduler::unregisterUserTask (string const& id) {
  if (id.empty()) {
    return TRI_ERROR_TASK_INVALID_ID;
  }

  Task* task = nullptr;

  {
    MUTEX_LOCKER(schedulerLock);

    map<Task*, SchedulerThread*>::iterator i = task2thread.begin();
    while (i != task2thread.end()) {
      Task const* t = (*i).first;

      if (t->id() == id) {
        // found the sought task
        if (! t->isUserDefined()) {
          return TRI_ERROR_TASK_NOT_FOUND;
        }

        task = const_cast<Task*>(t);
        break;
      }

      ++i;
    }
  }

  if (task == nullptr) {
    // not found
    return TRI_ERROR_TASK_NOT_FOUND;
  }

  return destroyTask(task);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief unregister all user tasks
////////////////////////////////////////////////////////////////////////////////

int Scheduler::unregisterUserTasks () {
  while (true) {
    Task* task = nullptr;

    {
      MUTEX_LOCKER(schedulerLock);

      map<Task*, SchedulerThread*>::iterator i = task2thread.begin();
      while (i != task2thread.end()) {
        Task const* t = (*i).first;

        if (t->isUserDefined()) {
          task = const_cast<Task*>(t);
          break;
        }

        ++i;
      }
    }

    if (task == nullptr) {
      return TRI_ERROR_NO_ERROR;
    }

    destroyTask(task);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief registers a new task
////////////////////////////////////////////////////////////////////////////////

int Scheduler::registerTask (Task* task) {
  SchedulerThread* thread = nullptr;

  if (task->isUserDefined() && task->id().empty()) {
    // user-defined task without id is invalid
    return TRI_ERROR_TASK_INVALID_ID;
  }

  string const name = task->name();
  LOG_TRACE("registerTask for task %p (%s)", (void*) task, name.c_str());

  {
    size_t n = 0;
    MUTEX_LOCKER(schedulerLock);

    int res = checkInsertTask(task);

    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }

    if (multiThreading && ! task->needsMainEventLoop()) {
      n = (++nextLoop) % nrThreads;
    }

    thread = threads[n];

    task2thread[task] = thread;
    taskRegistered.insert(task);
  }

  if (! thread->registerTask(this, task)) {
    return TRI_ERROR_INTERNAL;
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief unregisters a task
////////////////////////////////////////////////////////////////////////////////

int Scheduler::unregisterTask (Task* task) {
  SchedulerThread* thread = nullptr;

  {
    MUTEX_LOCKER(schedulerLock);

    map<Task*, SchedulerThread*>::iterator i = task2thread.find(task);

    if (i == task2thread.end()) {
      LOG_WARNING("unregisterTask called for an unknown task %p (%s)", (void*) task, task->name().c_str());

      return TRI_ERROR_TASK_NOT_FOUND;
    }
    else {
      LOG_TRACE("unregisterTask for task %p (%s)", (void*) task, task->name().c_str());

      thread = i->second;

      if (taskRegistered.count(task) > 0) {
        taskRegistered.erase(task);
      }

      task2thread.erase(i);
    }
  }

  thread->unregisterTask(task);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys task
////////////////////////////////////////////////////////////////////////////////

int Scheduler::destroyTask (Task* task) {
  SchedulerThread* thread = nullptr;

  {
    MUTEX_LOCKER(schedulerLock);

    map<Task*, SchedulerThread*>::iterator i = task2thread.find(task);

    if (i == task2thread.end()) {
      LOG_WARNING("destroyTask called for an unknown task %p (%s)", (void*) task, task->name().c_str());

      return TRI_ERROR_TASK_NOT_FOUND;
    }
    else {
      LOG_TRACE("destroyTask for task %p (%s)", (void*) task, task->name().c_str());

      thread = i->second;

      if (taskRegistered.count(task) > 0) {
        taskRegistered.erase(task);
      }

      task2thread.erase(i);
    }
  }

  thread->destroyTask(task);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief called to display current status
////////////////////////////////////////////////////////////////////////////////

void Scheduler::reportStatus () {
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief check whether a task can be inserted
/// the caller must ensure the schedulerLock is held
////////////////////////////////////////////////////////////////////////////////

int Scheduler::checkInsertTask (Task const* task) {
  if (task->isUserDefined()) {
    // this is a user-defined task

    // now check if there is an id conflict
    string const id = task->id();

    map<Task*, SchedulerThread*>::const_iterator i = task2thread.begin();
    while (i != task2thread.end()) {
      Task const* t = (*i).first;

      if (t->isUserDefined() && t->id() == id) {
        return TRI_ERROR_TASK_DUPLICATE_ID;
      }

      ++i;
    }
  }

  return TRI_ERROR_NO_ERROR;
}

// -----------------------------------------------------------------------------
// --SECTION--                                            static private methods
// -----------------------------------------------------------------------------

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
    LOG_ERROR("cannot initialise signal handlers for pipe");
  }
#endif

}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:

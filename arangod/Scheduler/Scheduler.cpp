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
/// @author Achim Brandt
////////////////////////////////////////////////////////////////////////////////

#ifdef _WIN32
#include "Basics/win-utils.h"
#endif

#include "Scheduler.h"

#include "Basics/MutexLocker.h"
#include "Basics/StringUtils.h"
#include "Basics/Thread.h"
#include "Logger/Logger.h"
#include "Scheduler/SchedulerThread.h"
#include "Scheduler/Task.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb::basics;
using namespace arangodb::rest;

////////////////////////////////////////////////////////////////////////////////
/// @brief scheduler singleton
////////////////////////////////////////////////////////////////////////////////

Scheduler::Scheduler(size_t nrThreads)
    : nrThreads(nrThreads),
      threads(0),
      stopping(0),
      nextLoop(0),
      _active(true) {
  // check for multi-threading scheduler
  multiThreading = (nrThreads > 1);

  if (!multiThreading) {
    nrThreads = 1;
  }

  // report status
  if (multiThreading) {
    LOG(TRACE) << "scheduler is multi-threaded, number of threads: "
               << nrThreads;
  } else {
    LOG(TRACE) << "scheduler is single-threaded";
  }

  // setup signal handlers
  initializeSignalHandlers();
}

Scheduler::~Scheduler() {}

////////////////////////////////////////////////////////////////////////////////
/// @brief starts scheduler, keeps running
////////////////////////////////////////////////////////////////////////////////

bool Scheduler::start(ConditionVariable* cv) {
  MUTEX_LOCKER(mutexLocker, schedulerLock);

  // start the schedulers threads
  for (size_t i = 0; i < nrThreads; ++i) {
    bool ok = threads[i]->start(cv);

    if (!ok) {
      LOG(FATAL) << "cannot start threads";
      FATAL_ERROR_EXIT();
    }

    if (!_affinityCores.empty()) {
      size_t c = _affinityCores[_affinityPos];

      LOG_TOPIC(DEBUG, Logger::THREADS) << "using core " << c
                                        << " for scheduler thread " << i;
      threads[i]->setProcessorAffinity(c);

      ++_affinityPos;

      if (_affinityPos >= _affinityCores.size()) {
        _affinityPos = 0;
      }
    }
  }

  // and wait until the threads are started
  bool waiting = true;

  while (waiting) {
    waiting = false;

    usleep(1000);

    for (size_t i = 0; i < nrThreads; ++i) {
      if (!threads[i]->isRunning()) {
        waiting = true;
        LOG(TRACE) << "waiting for thread #" << i << " to start";
      }
    }
  }

  LOG(TRACE) << "all scheduler threads are up and running";
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if the scheduler threads are up and running
////////////////////////////////////////////////////////////////////////////////

bool Scheduler::isStarted() { return true; }

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if scheduler is still running
////////////////////////////////////////////////////////////////////////////////

bool Scheduler::isRunning() {
  MUTEX_LOCKER(mutexLocker, schedulerLock);

  for (size_t i = 0; i < nrThreads; ++i) {
    if (threads[i]->isRunning()) {
      return true;
    }
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief starts shutdown sequence
////////////////////////////////////////////////////////////////////////////////

void Scheduler::beginShutdown() {
  if (stopping != 0) {
    return;
  }

  MUTEX_LOCKER(mutexLocker, schedulerLock);

  LOG(DEBUG) << "beginning shutdown sequence of scheduler";

  for (size_t i = 0; i < nrThreads; ++i) {
    threads[i]->beginShutdown();
  }

  // set the flag AFTER stopping the threads
  stopping = 1;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if scheduler is shuting down
////////////////////////////////////////////////////////////////////////////////

bool Scheduler::isShutdownInProgress() { return stopping != 0; }

////////////////////////////////////////////////////////////////////////////////
/// @brief shuts down the scheduler
////////////////////////////////////////////////////////////////////////////////

void Scheduler::shutdown() {
  for (auto& it : taskRegistered) {
    std::string const name = it.second->name();
    LOG(DEBUG) << "forcefully removing task '" << name << "'";

    deleteTask(it.second);
  }

  taskRegistered.clear();
  task2thread.clear();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief list user tasks
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<VPackBuilder> Scheduler::getUserTasks() {
  auto builder = std::make_shared<VPackBuilder>();
  try {
    VPackArrayBuilder b(builder.get());
    MUTEX_LOCKER(mutexLocker, schedulerLock);
    for (auto& it : task2thread) {
      auto const* task = it.first;

      if (task->isUserDefined()) {
        VPackObjectBuilder b2(builder.get());
        task->toVelocyPack(*builder);
      }
    }
  } catch (...) {
    return std::make_shared<VPackBuilder>();
  }
  return builder;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get a single user task
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<VPackBuilder> Scheduler::getUserTask(std::string const& id) {
  MUTEX_LOCKER(mutexLocker, schedulerLock);

  for (auto& it : task2thread) {
    auto const* task = it.first;

    if (task->isUserDefined() && task->id() == id) {
      return task->toVelocyPack();
    }
  }

  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief unregister and delete a user task by id
////////////////////////////////////////////////////////////////////////////////

int Scheduler::unregisterUserTask(std::string const& id) {
  if (stopping) {
    return TRI_ERROR_SHUTTING_DOWN;
  }
  if (id.empty()) {
    return TRI_ERROR_TASK_INVALID_ID;
  }

  Task* task = nullptr;

  {
    MUTEX_LOCKER(mutexLocker, schedulerLock);

    for (auto& it : task2thread) {
      auto const* t = it.first;

      if (t->id() == id) {
        // found the sought task
        if (!t->isUserDefined()) {
          return TRI_ERROR_TASK_NOT_FOUND;
        }

        task = const_cast<Task*>(t);
        break;
      }
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

int Scheduler::unregisterUserTasks() {
  if (stopping) {
    return TRI_ERROR_SHUTTING_DOWN;
  }
  while (true) {
    Task* task = nullptr;

    {
      MUTEX_LOCKER(mutexLocker, schedulerLock);

      for (auto& it : task2thread) {
        auto const* t = it.first;

        if (t->isUserDefined()) {
          task = const_cast<Task*>(t);
          break;
        }
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

int Scheduler::registerTask(Task* task) {
  return registerTask(task, nullptr, -1);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief registers a new task and returns the chosen threads number
////////////////////////////////////////////////////////////////////////////////

int Scheduler::registerTask(Task* task, ssize_t* tn) {
  *tn = -1;
  return registerTask(task, tn, -1);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief unregisters a task
////////////////////////////////////////////////////////////////////////////////

int Scheduler::unregisterTask(Task* task) {
  if (stopping) {
    return TRI_ERROR_SHUTTING_DOWN;
  }

  SchedulerThread* thread = nullptr;
  std::string const taskName(task->name());

  {
    MUTEX_LOCKER(mutexLocker, schedulerLock);

    auto it = task2thread.find(
        task);  // TODO(fc) XXX remove this! This should be in the Task

    if (it == task2thread.end()) {
      LOG(WARN) << "unregisterTask called for an unknown task " << (void*)task
                << " (" << taskName << ")";

      return TRI_ERROR_TASK_NOT_FOUND;
    }

    LOG(TRACE) << "unregisterTask for task " << (void*)task << " (" << taskName
               << ")";

    thread = (*it).second;

    taskRegistered.erase(task->taskId());
    task2thread.erase(it);
  }

  thread->unregisterTask(task);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys task
////////////////////////////////////////////////////////////////////////////////

int Scheduler::destroyTask(Task* task) {
  if (stopping) {
    return TRI_ERROR_SHUTTING_DOWN;
  }

  SchedulerThread* thread = nullptr;
  std::string const taskName(task->name());

  {
    MUTEX_LOCKER(mutexLocker, schedulerLock);

    auto it = task2thread.find(task);

    if (it == task2thread.end()) {
      LOG(WARN) << "destroyTask called for an unknown task " << (void*)task
                << " (" << taskName << ")";

      return TRI_ERROR_TASK_NOT_FOUND;
    }

    LOG(TRACE) << "destroyTask for task " << (void*)task << " (" << taskName
               << ")";

    thread = (*it).second;

    taskRegistered.erase(task->taskId());
    task2thread.erase(it);
  }

  thread->destroyTask(task);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief called to display current status
////////////////////////////////////////////////////////////////////////////////

void Scheduler::reportStatus() {}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the process affinity
////////////////////////////////////////////////////////////////////////////////

void Scheduler::setProcessorAffinity(std::vector<size_t> const& cores) {
  LOG_TOPIC(DEBUG, Logger::THREADS) << "scheduler cores: " << cores;
  _affinityCores = cores;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the task for a task id
////////////////////////////////////////////////////////////////////////////////

Task* Scheduler::lookupTaskById(uint64_t taskId) {
  MUTEX_LOCKER(mutexLocker, schedulerLock);

  auto task = taskRegistered.find(taskId);

  if (task == taskRegistered.end()) {
    return nullptr;
  }

  return task->second;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the loop for a task id
////////////////////////////////////////////////////////////////////////////////

EventLoop Scheduler::lookupLoopById(uint64_t taskId) {
  MUTEX_LOCKER(mutexLocker, schedulerLock);

  auto task = taskRegistered.find(taskId);

  if (task == taskRegistered.end()) {
    return static_cast<EventLoop>(nrThreads);
  }

  return task->second->eventLoop();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief registers a new task
/// the caller must not use the task object afterwards, as it may be deleted
/// by this function or a SchedulerThread
////////////////////////////////////////////////////////////////////////////////

int Scheduler::registerTask(Task* task, ssize_t* got, ssize_t want) {
  if (stopping) {
    return TRI_ERROR_SHUTTING_DOWN;
  }
  TRI_ASSERT(task != nullptr);

  if (task->isUserDefined() && task->id().empty()) {
    // user-defined task without id is invalid
    deleteTask(task);
    return TRI_ERROR_TASK_INVALID_ID;
  }

  std::string const& name = task->name();
  LOG(TRACE) << "registerTask for task " << (void*)task << " (" << name << ")";

  // determine thread
  SchedulerThread* thread = nullptr;
  size_t n = 0;
  if (0 <= want) {
    n = want;

    if (nrThreads <= n) {
      deleteTask(task);
      return TRI_ERROR_INTERNAL;
    }
  }

  try {
    MUTEX_LOCKER(mutexLocker, schedulerLock);

    int res = checkInsertTask(task);

    if (res != TRI_ERROR_NO_ERROR) {
      deleteTask(task);
      return res;
    }

    if (0 > want) {
      if (multiThreading && !task->needsMainEventLoop()) {
        n = (++nextLoop) % nrThreads;
      }
    }

    thread = threads[n];

    task2thread[task] = thread;
    taskRegistered[task->taskId()] = task;
  } catch (...) {
    destroyTask(task);
    throw;
  }

  if (nullptr != got) {
    *got = static_cast<ssize_t>(n);
  }

  if (!thread->registerTask(this, task)) {
    // no need to delete the task here, as SchedulerThread::registerTask
    // takes over the ownership
    return TRI_ERROR_INTERNAL;
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check whether a task can be inserted
/// the caller must ensure the schedulerLock is held
////////////////////////////////////////////////////////////////////////////////

int Scheduler::checkInsertTask(Task const* task) {
  if (task->isUserDefined()) {
    // this is a user-defined task

    // now check if there is an id conflict
    std::string const& id = task->id();

    for (auto& it : task2thread) {
      auto const* t = it.first;

      if (t->isUserDefined() && t->id() == id) {
        return TRI_ERROR_TASK_DUPLICATE_ID;
      }
    }
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initializes the signal handlers for the scheduler
////////////////////////////////////////////////////////////////////////////////

void Scheduler::initializeSignalHandlers() {
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
    LOG(ERR) << "cannot initialize signal handlers for pipe";
  }
#endif
}

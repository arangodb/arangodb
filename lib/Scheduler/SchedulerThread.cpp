////////////////////////////////////////////////////////////////////////////////
/// @brief scheduler thread
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
/// @author Martin Schoenert
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2009-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifdef _WIN32
#include "Basics/win-utils.h"
#endif

#include "Basics/logging.h"
#include "SchedulerThread.h"

#include "Scheduler/Scheduler.h"
#include "Scheduler/Task.h"

using namespace triagens::basics;
using namespace triagens::rest;

#ifdef TRI_USE_SPIN_LOCK_SCHEDULER_THREAD
#define SCHEDULER_INIT TRI_InitSpin
#define SCHEDULER_DESTROY TRI_DestroySpin
#define SCHEDULER_LOCK TRI_LockSpin
#define SCHEDULER_UNLOCK TRI_UnlockSpin
#else
#define SCHEDULER_INIT TRI_InitMutex
#define SCHEDULER_DESTROY TRI_DestroyMutex
#define SCHEDULER_LOCK TRI_LockMutex
#define SCHEDULER_UNLOCK TRI_UnlockMutex
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

SchedulerThread::SchedulerThread (Scheduler* scheduler, EventLoop loop, bool defaultLoop)
  : Thread("scheduler"),
    _scheduler(scheduler),
    _defaultLoop(defaultLoop),
    _loop(loop),
    _stopping(0),
    _stopped(0),
    _open(0),
    _hasWork(false) {

  // init lock
  SCHEDULER_INIT(&_queueLock);

  // allow cancelation
  allowAsynchronousCancelation();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

SchedulerThread::~SchedulerThread () {
  SCHEDULER_DESTROY(&_queueLock);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if the scheduler thread is up and running
////////////////////////////////////////////////////////////////////////////////

bool SchedulerThread::isStarted () {
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief opens the scheduler thread for business
////////////////////////////////////////////////////////////////////////////////

bool SchedulerThread::open () {
  _open = 1;
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief begin shutdown sequence
////////////////////////////////////////////////////////////////////////////////

void SchedulerThread::beginShutdown () {
  LOG_TRACE("beginning shutdown sequence of scheduler thread (%d)", (int) threadId());

  _stopping = 1;
  _scheduler->wakeupLoop(_loop);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief registers a task
////////////////////////////////////////////////////////////////////////////////

bool SchedulerThread::registerTask (Scheduler* scheduler, Task* task) {
  // thread has already been stopped
  if (_stopped) {
    // do nothing
    return false;
  }

  // same thread, in this case it does not matter if we are inside the loop
  if (threadId() == currentThreadId()) {
    bool ok = setupTask(task, scheduler, _loop);

    if (! ok) {
      LOG_WARNING("In SchedulerThread::registerTask setupTask has failed");
      cleanupTask(task);
      deleteTask(task);
    }

    return ok;
  }

  // different thread, be careful - we have to stop the event loop
  // put the register request onto the queue
  SCHEDULER_LOCK(&_queueLock);

  Work w(SETUP, scheduler, task);
  _queue.push_back(w);
  _hasWork = true;

  scheduler->wakeupLoop(_loop);

  SCHEDULER_UNLOCK(&_queueLock);
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief unregisters a task
////////////////////////////////////////////////////////////////////////////////

void SchedulerThread::unregisterTask (Task* task) {
  deactivateTask(task);

  // thread has already been stopped
  if (_stopped) {
    // do nothing
  }

  // same thread, in this case it does not matter if we are inside the loop
  else if (threadId() == currentThreadId()) {
    cleanupTask(task);
  }

  // different thread, be careful - we have to stop the event loop
  else {

    // put the unregister request unto the queue
    SCHEDULER_LOCK(&_queueLock);

    Work w(CLEANUP, 0, task);
    _queue.push_back(w);
    _hasWork = true;

    _scheduler->wakeupLoop(_loop);

    SCHEDULER_UNLOCK(&_queueLock);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief unregisters a task
////////////////////////////////////////////////////////////////////////////////

void SchedulerThread::destroyTask (Task* task) {
  deactivateTask(task);

  // thread has already been stopped
  if (_stopped) {
    deleteTask(task);
  }

  // same thread, in this case it does not matter if we are inside the loop
  else if (threadId() == currentThreadId()) {
    cleanupTask(task);
    deleteTask(task);
  }

  // different thread, be careful - we have to stop the event loop
  else {
    // put the unregister request unto the queue
    SCHEDULER_LOCK(&_queueLock);

    Work w(DESTROY, 0, task);
    _queue.push_back(w);
    _hasWork = true;

    _scheduler->wakeupLoop(_loop);

    SCHEDULER_UNLOCK(&_queueLock);
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    Thread methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void SchedulerThread::run () {
  LOG_TRACE("scheduler thread started (%d)", (int) threadId());

  if (_defaultLoop) {
#ifdef TRI_HAVE_POSIX_THREADS
    sigset_t all;
    sigemptyset(&all);
    pthread_sigmask(SIG_SETMASK, &all, 0);
#endif
  }

  while (_stopping == 0 && _open == 0) {
    usleep(1000);
  }

  while (_stopping == 0) {
    try {
      _scheduler->eventLoop(_loop);
    }
    catch (...) {
#ifdef TRI_HAVE_POSIX_THREADS
      if (_stopping != 0) {
        LOG_WARNING("caught cancellation exception during work");
        throw;
      }
#endif

      LOG_WARNING("caught exception from ev_loop");
    }

#if defined(DEBUG_SCHEDULER_THREAD)
    LOG_TRACE("left scheduler loop %d", (int) threadId());
#endif

    SCHEDULER_LOCK(&_queueLock);

    if (_hasWork) {
      while (! _queue.empty()) {
        Work w = _queue.front();
        _queue.pop_front();

        SCHEDULER_UNLOCK(&_queueLock);

        switch (w.work) {

          case CLEANUP: {
            cleanupTask(w.task);
            break;
          }

          case SETUP: {
            bool ok = setupTask(w.task, w.scheduler, _loop);
            if (! ok) {
              cleanupTask(w.task);
              deleteTask(w.task);
            }
            break;
          }

          case DESTROY: {
            cleanupTask(w.task);
            deleteTask(w.task);
            break;
          }
        }

        SCHEDULER_LOCK(&_queueLock);
      }

      _hasWork = false;
    }

    SCHEDULER_UNLOCK(&_queueLock);
  }

  LOG_TRACE("scheduler thread stopped (%d)", (int) threadId());

  _stopped = 1;

  SCHEDULER_LOCK(&_queueLock);

  while (! _queue.empty()) {
    Work w = _queue.front();
    _queue.pop_front();

    SCHEDULER_UNLOCK(&_queueLock);

    switch (w.work) {
      case CLEANUP:
        break;

      case SETUP:
        break;

      case DESTROY:
        deleteTask(w.task);
        break;
    }

    SCHEDULER_LOCK(&_queueLock);
  }

  SCHEDULER_UNLOCK(&_queueLock);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:

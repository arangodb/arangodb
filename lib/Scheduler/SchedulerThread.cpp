////////////////////////////////////////////////////////////////////////////////
/// @brief scheduler thread
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
/// @author Martin Schoenert
/// @author Copyright 2009-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifdef _WIN32
#include "BasicsC/win-utils.h"
#endif

#include "SchedulerThread.h"

#include "Logger/Logger.h"
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
/// @addtogroup Scheduler
/// @{
////////////////////////////////////////////////////////////////////////////////

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
    _hasWork(0),
    _open(0) {

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

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Scheduler
/// @{
////////////////////////////////////////////////////////////////////////////////

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
  LOGGER_TRACE("beginning shutdown sequence of scheduler thread (" << threadId() << ")");

  _stopping = 1;
  _scheduler->wakeupLoop(_loop);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief registers a task
////////////////////////////////////////////////////////////////////////////////

void SchedulerThread::registerTask (Scheduler* scheduler, Task* task) {

  // thread has already been stopped
  if (_stopped) {
    // do nothing
  }

  // same thread, in this case it does not matter if we are inside the loop
  else if (threadId() == currentThreadId()) {
    bool ok = setupTask(task, scheduler, _loop);
    if (!ok) {
      LOGGER_WARNING("In SchedulerThread::registerTask setupTask has failed");
      cleanupTask(task);
      deleteTask(task);
    }
    scheduler->wakeupLoop(_loop);
  }

  // different thread, be careful - we have to stop the event loop
  else {

    // put the register request onto the queue
    SCHEDULER_LOCK(&_queueLock);

    Work w(SETUP, scheduler, task);
    _queue.push_back(w);
    _hasWork = 1;

    scheduler->wakeupLoop(_loop);

    SCHEDULER_UNLOCK(&_queueLock);
  }
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
    _scheduler->wakeupLoop(_loop);
  }

  // different thread, be careful - we have to stop the event loop
  else {

    // put the unregister request unto the queue
    SCHEDULER_LOCK(&_queueLock);

    Work w(CLEANUP, 0, task);
    _queue.push_back(w);
    _hasWork = 1;

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
    _scheduler->wakeupLoop(_loop);
  }

  // different thread, be careful - we have to stop the event loop
  else {

    // put the unregister request unto the queue
    SCHEDULER_LOCK(&_queueLock);

    Work w(DESTROY, 0, task);
    _queue.push_back(w);
    _hasWork = 1;

    _scheduler->wakeupLoop(_loop);

    SCHEDULER_UNLOCK(&_queueLock);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    Thread methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Scheduler
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void SchedulerThread::run () {
  LOGGER_TRACE("scheduler thread started (" << threadId() << ")");

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
        LOGGER_WARNING("caught cancellation exception during work");
        throw;
      }
#endif

      LOGGER_WARNING("caught exception from ev_loop");
    }

#if defined(DEBUG_SCHEDULER_THREAD)
    LOGGER_TRACE("left scheduler loop " << threadId());
#endif

    if (_hasWork != 0) {
      SCHEDULER_LOCK(&_queueLock);

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
            if (!ok) {
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

      _hasWork = 0;

      SCHEDULER_UNLOCK(&_queueLock);
    }
  }

  LOGGER_TRACE("scheduler thread stopped (" << threadId() << ")");

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

////////////////////////////////////////////////////////////////////////////////
/// @brief scheduler thread
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
/// @author Martin Schoenert
/// @author Copyright 2009-2011, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "SchedulerThread.h"

#include <Basics/Logger.h>
#include <Rest/Task.h>

using namespace triagens::basics;

namespace triagens {
  namespace rest {

    // -----------------------------------------------------------------------------
    // constructors and destructors
    // -----------------------------------------------------------------------------

    SchedulerThread::SchedulerThread (Scheduler* scheduler, EventLoop loop, bool defaultLoop)
      : Thread("SchedulerThread"),
        scheduler(scheduler),
        defaultLoop(defaultLoop),
        loop(loop),
        stopping(0),
        hasWork(0) {

      // allow cancelation
      allowAsynchronousCancelation();
    }

    // -----------------------------------------------------------------------------
    // public methods
    // -----------------------------------------------------------------------------

    void SchedulerThread::beginShutdown () {

      // same thread, in this case it does not matter if we are inside the loop
      if (threadId() == currentThreadId()) {

        if (stopping == 0) {
          LOGGER_DEBUG << "beginning shutdown sequence (self, " << threadId() << ")";

          stopping = 1;
          scheduler->wakeupLoop(loop);
        }
      }

      // different thread, be careful and send a signal
      else {
        if (stopping == 0) {
          LOGGER_DEBUG << "beginning shutdown sequence (" << threadId() << ")";

          stopping = 1;
          scheduler->wakeupLoop(loop);
        }
      }
    }



    void SchedulerThread::registerTask (Scheduler* scheduler, Task* task) {

      // same thread, in this case it does not matter if we are inside the loop
      if (threadId() == currentThreadId()) {
        setupTask(task, scheduler, loop);
        scheduler->wakeupLoop(loop);
      }

      // different thread, be careful - we have to stop the event loop
      else {

        // put the register request unto the queue
        queueLock.lock();

        Work w(SETUP, scheduler, task);
        queue.push_back(w);
        hasWork = 1;

        scheduler->wakeupLoop(loop);

        queueLock.unlock();
      }
    }



    void SchedulerThread::unregisterTask (Task* task) {
      deactivateTask(task);

      // same thread, in this case it does not matter if we are inside the loop
      if (threadId() == currentThreadId()) {
        cleanupTask(task);
        scheduler->wakeupLoop(loop);
      }

      // different thread, be careful - we have to stop the event loop
      else {

        // put the unregister request unto the queue
        queueLock.lock();

        Work w(CLEANUP, 0, task);
        queue.push_back(w);
        hasWork = 1;

        scheduler->wakeupLoop(loop);

        queueLock.unlock();
      }
    }



    void SchedulerThread::destroyTask (Task* task) {
      deactivateTask(task);

      // same thread, in this case it does not matter if we are inside the loop
      if (threadId() == currentThreadId()) {
        cleanupTask(task);
        deleteTask(task);
        scheduler->wakeupLoop(loop);
      }

      // different thread, be careful - we have to stop the event loop
      else {

        // put the unregister request unto the queue
        queueLock.lock();

        Work w(DESTROY, 0, task);
        queue.push_back(w);
        hasWork = 1;

        scheduler->wakeupLoop(loop);

        queueLock.unlock();
      }
    }

    // -----------------------------------------------------------------------------
    // Thread methods
    // -----------------------------------------------------------------------------

    void SchedulerThread::run () {
      LOGGER_TRACE << "scheduler thread started (" << threadId() << ")";

      if (defaultLoop) {
        sigset_t all;
        sigemptyset(&all);

        pthread_sigmask(SIG_SETMASK, &all, 0);
      }

      while (stopping == 0) {
        try {
          scheduler->eventLoop(loop);
        }
        catch (...) {
#ifdef TRI_HAVE_POSIX_THREADS
          if (stopping != 0) {
            LOGGER_WARNING << "caught cancellation exception during work";
            throw;
          }
#endif

          LOGGER_WARNING << "caught exception from ev_loop";
        }

#if defined(DEBUG_SCHEDULER_THREAD)
        LOGGER_TRACE << "left scheduler loop " << threadId();
#endif

        if (hasWork != 0) {
          queueLock.lock();

          while (! queue.empty()) {
            Work w = queue.front();
            queue.pop_front();

            queueLock.unlock();

            switch (w.work) {
              case CLEANUP:
                cleanupTask(w.task);
                break;

              case SETUP:
                setupTask(w.task, w.scheduler, loop);
                break;

              case DESTROY:
                cleanupTask(w.task);
                deleteTask(w.task);
                break;
            }

            queueLock.lock();
          }

          hasWork = 0;

          queueLock.unlock();
        }
      }

      LOGGER_TRACE << "scheduler thread stopped (" << threadId() << ")";
    }
  }
}

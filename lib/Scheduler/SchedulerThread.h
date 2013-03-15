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

#ifndef TRIAGENS_SCHEDULER_SCHEDULER_THREAD_H
#define TRIAGENS_SCHEDULER_SCHEDULER_THREAD_H 1

#include "Basics/Thread.h"

#include "BasicsC/locks.h"
#include "Scheduler/Task.h"
#include "Scheduler/TaskManager.h"

// #define TRI_USE_SPIN_LOCK_SCHEDULER_THREAD 1

// -----------------------------------------------------------------------------
// --SECTION--                                              forward declarations
// -----------------------------------------------------------------------------

namespace triagens {
  namespace rest {
    class Scheduler;

// -----------------------------------------------------------------------------
// --SECTION--                                             class SchedulerThread
// -----------------------------------------------------------------------------

/////////////////////////////////////////////////////////////////////////////
/// @brief job scheduler thread
/////////////////////////////////////////////////////////////////////////////

    class SchedulerThread : public basics::Thread, private TaskManager {
      private:
        SchedulerThread (SchedulerThread const&);
        SchedulerThread& operator= (SchedulerThread const&);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Scheduler
/// @{
////////////////////////////////////////////////////////////////////////////////

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

        SchedulerThread (Scheduler*, EventLoop, bool defaultLoop);

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

        ~SchedulerThread ();

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

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if the scheduler thread is up and running
////////////////////////////////////////////////////////////////////////////////

        bool isStarted ();

////////////////////////////////////////////////////////////////////////////////
/// @brief opens the scheduler thread for business
////////////////////////////////////////////////////////////////////////////////

        bool open ();

////////////////////////////////////////////////////////////////////////////////
/// @brief begin shutdown sequence
////////////////////////////////////////////////////////////////////////////////

        void beginShutdown ();

////////////////////////////////////////////////////////////////////////////////
/// @brief registers a task
////////////////////////////////////////////////////////////////////////////////

        void registerTask (Scheduler*, Task*);

////////////////////////////////////////////////////////////////////////////////
/// @brief unregisters a task
////////////////////////////////////////////////////////////////////////////////

        void unregisterTask (Task*);

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a task
////////////////////////////////////////////////////////////////////////////////

        void destroyTask (Task*);

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

      protected:

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

        void run ();

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                     private types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Scheduler
/// @{
////////////////////////////////////////////////////////////////////////////////

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief what to do with the task
////////////////////////////////////////////////////////////////////////////////

        enum work_e {
          CLEANUP,
          DESTROY,
          SETUP
        };

////////////////////////////////////////////////////////////////////////////////
/// @brief what to do with the thread
////////////////////////////////////////////////////////////////////////////////

        class Work {
          public:
            Work (work_e w, Scheduler* scheduler, Task* task)
              : work(w), scheduler(scheduler), task(task) {
            }

            work_e work;
            Scheduler* scheduler;
            Task* task;
        };

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Scheduler
/// @{
////////////////////////////////////////////////////////////////////////////////

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief underlying scheduler
////////////////////////////////////////////////////////////////////////////////

        Scheduler* _scheduler;

////////////////////////////////////////////////////////////////////////////////
/// @brief if true, this is the default loop
////////////////////////////////////////////////////////////////////////////////

        bool _defaultLoop;

////////////////////////////////////////////////////////////////////////////////
/// @brief event loop
////////////////////////////////////////////////////////////////////////////////

        EventLoop _loop;

////////////////////////////////////////////////////////////////////////////////
/// @brief true if scheduler threads is shutting down
////////////////////////////////////////////////////////////////////////////////

        volatile sig_atomic_t _stopping;

////////////////////////////////////////////////////////////////////////////////
/// @brief true if scheduler threads has stopped
////////////////////////////////////////////////////////////////////////////////

        volatile sig_atomic_t _stopped;

////////////////////////////////////////////////////////////////////////////////
/// @brief queue lock
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_USE_SPIN_LOCK_SCHEDULER_THREAD
        TRI_spin_t _queueLock;
#else
        TRI_mutex_t _queueLock;
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief work queue
////////////////////////////////////////////////////////////////////////////////

        deque<Work> _queue;

////////////////////////////////////////////////////////////////////////////////
/// @brief work indicator
////////////////////////////////////////////////////////////////////////////////

        volatile sig_atomic_t _hasWork;

////////////////////////////////////////////////////////////////////////////////
/// @brief open for business
////////////////////////////////////////////////////////////////////////////////

        volatile sig_atomic_t _open;
    };
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:

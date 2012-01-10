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

#ifndef TRIAGENS_FYN_SCHEDULER_SCHEDULER_THREAD_H
#define TRIAGENS_FYN_SCHEDULER_SCHEDULER_THREAD_H 1

#include <Basics/Thread.h>

#include <Basics/Mutex.h>

#include "Scheduler/Task.h"

namespace triagens {
  namespace rest {
    class Scheduler;

    /////////////////////////////////////////////////////////////////////////////
    /// @brief job scheduler thread
    /////////////////////////////////////////////////////////////////////////////

    class SchedulerThread : public basics::Thread, private TaskManager {
      public:

        /////////////////////////////////////////////////////////////////////////
        /// @brief constructs a scheduler thread
        /////////////////////////////////////////////////////////////////////////

        SchedulerThread (Scheduler*, EventLoop, bool defaultLoop);

      public:

        /////////////////////////////////////////////////////////////////////////
        /// @brief begin shutdown sequence
        /////////////////////////////////////////////////////////////////////////

        void beginShutdown ();

        /////////////////////////////////////////////////////////////////////////
        /// @brief registers a task
        /////////////////////////////////////////////////////////////////////////

        void registerTask (Scheduler*, Task*);

        /////////////////////////////////////////////////////////////////////////
        /// @brief unregisters a task
        /////////////////////////////////////////////////////////////////////////

        void unregisterTask (Task*);

        /////////////////////////////////////////////////////////////////////////
        /// @brief destroys a task
        /////////////////////////////////////////////////////////////////////////

        void destroyTask (Task*);

      protected:

        /////////////////////////////////////////////////////////////////////////
        /// {@inheritDoc}
        /////////////////////////////////////////////////////////////////////////

        void run ();

      private:
        enum work_e {
          CLEANUP,
          DESTROY,
          SETUP
        };

        class Work {
          public:
            Work (work_e w, Scheduler* scheduler, Task* task)
              : work(w), scheduler(scheduler), task(task) {
            }

            work_e work;
            Scheduler* scheduler;
            Task* task;
        };

      private:
        Scheduler* scheduler;

        bool defaultLoop;
        EventLoop loop;

        volatile sig_atomic_t stopping;

        basics::Mutex queueLock;
        volatile sig_atomic_t hasWork;
        deque<Work> queue;
    };
  }
}

#endif

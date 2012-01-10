////////////////////////////////////////////////////////////////////////////////
/// @brief abstract base class for tasks
///
/// @file
/// Tasks are handled by the scheduler. The scheduler calls the task callback
/// as soon as a specific event occurs.
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

#ifndef TRIAGENS_FYN_REST_TASK_H
#define TRIAGENS_FYN_REST_TASK_H 1

#include <Basics/Common.h>

#include "Scheduler/Scheduler.h"

namespace triagens {
  namespace rest {
    class Task;

    ////////////////////////////////////////////////////////////////////////////////
    /// @ingroup Scheduler
    /// @brief abstract base class for tasks
    ////////////////////////////////////////////////////////////////////////////////

    class TaskManager {
      private:
        TaskManager (TaskManager const&);
        TaskManager& operator= (TaskManager const&);

      public:

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief constructor
        ////////////////////////////////////////////////////////////////////////////////

        TaskManager () {
        }

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief destructor
        ////////////////////////////////////////////////////////////////////////////////

        virtual ~TaskManager () {
        }

      public:

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief deactivates a task
        ///
        /// Sets the task flag active to 0. This happens when unregisterTask or
        /// destroyTask is called.
        ////////////////////////////////////////////////////////////////////////////////

        void deactivateTask (Task* task);

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief deletes a task
        ////////////////////////////////////////////////////////////////////////////////

        void deleteTask (Task* task);

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief called to setup the callback information
        ///
        /// Called to setup all the necessary information within the event loop. In case
        /// of a multi-threaded scheduler the event loop identifier is supplied.
        ////////////////////////////////////////////////////////////////////////////////

        void setupTask (Task*, Scheduler*, EventLoop loop);

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief called to clear the callback information
        ////////////////////////////////////////////////////////////////////////////////

        void cleanupTask (Task*);
    };

    ////////////////////////////////////////////////////////////////////////////////
    /// @ingroup Scheduler
    /// @brief abstract base class for tasks
    ////////////////////////////////////////////////////////////////////////////////

    class  Task {
      friend class TaskManager;

      private:
        Task (Task const&);
        Task& operator= (Task const&);

      public:

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief constructs a new task
        ///
        /// Note that the constructor has no access to the event loop. The connection
        /// is provided in the method setup and any setup with regards to the event
        /// loop must be done there. It is not possible to simply delete a tasks. You
        /// must use the method destroy to cleanup the task, remove it from the event
        /// loop and delete it. The method cleanup itself will not delete task but
        /// remove it from the event loop. It is possible to use setup again to reuse
        /// the task.
        ////////////////////////////////////////////////////////////////////////////////

        explicit
        Task (string const& name);

      public:

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief returns the task name for debugging
        ////////////////////////////////////////////////////////////////////////////////

        string const& getName () const {
          return name;
        }

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief returns true if task is still active
        ////////////////////////////////////////////////////////////////////////////////

        bool isActive () const {
          return active != 0;
        }

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief allow thread to run on slave event loop
        ////////////////////////////////////////////////////////////////////////////////

        virtual bool needsMainEventLoop () const;

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief called by scheduler to indicate an event
        ///
        /// The method will only be called from within the scheduler thread, which
        /// belongs to the loop parameter.
        ////////////////////////////////////////////////////////////////////////////////

        virtual bool handleEvent (EventToken token, EventType event) = 0;

      protected:

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief deletes a task
        ///
        /// The method will only be called from after the task has been cleaned by
        /// the method cleanup.
        ////////////////////////////////////////////////////////////////////////////////

        virtual ~Task ();

      protected:

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief called to set up the callback information
        ///
        /// The method will only be called from within the scheduler thread, which
        /// belongs to the loop parameter.
        ////////////////////////////////////////////////////////////////////////////////

        virtual void setup (Scheduler*, EventLoop) = 0;

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief called to clear the callback information
        ///
        /// The method will only be called from within the scheduler thread, which
        /// belongs to the loop parameter.
        ////////////////////////////////////////////////////////////////////////////////

        virtual void cleanup () = 0;

      protected:

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief scheduler
        ////////////////////////////////////////////////////////////////////////////////

        Scheduler* scheduler;

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief event loop identifier
        ////////////////////////////////////////////////////////////////////////////////

        EventLoop loop;

      private:
        string const name;
        sig_atomic_t active;
    };
  }
}

#endif

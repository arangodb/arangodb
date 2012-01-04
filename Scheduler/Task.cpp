////////////////////////////////////////////////////////////////////////////////
/// @brief abstract base class for tasks
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
/// @author Copyright 2009-2011, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Task.h"

#include "Scheduler/Scheduler.h"

namespace triagens {
  namespace rest {

    // -----------------------------------------------------------------------------
    // TaskManager
    // -----------------------------------------------------------------------------

    void TaskManager::deactivateTask (Task* task) {
      task->active = 0;
    }



    void TaskManager::deleteTask (Task* task) {
      delete task;
    }



    void TaskManager::setupTask (Task* task, Scheduler* scheduler, EventLoop loop) {
      task->setup(scheduler, loop);
    }



    void TaskManager::cleanupTask (Task* task) {
      task->cleanup();
    }

    // -----------------------------------------------------------------------------
    // constructors and destructors
    // -----------------------------------------------------------------------------

    Task::Task (string const& name)
      : scheduler(0), loop(0), name(name), active(1) {
    }



    Task::~Task () {
    }

    // -----------------------------------------------------------------------------
    // protected methods
    // -----------------------------------------------------------------------------

    bool Task::needsMainEventLoop () const {
      return false;
    }
  }
}

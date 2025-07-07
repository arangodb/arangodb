////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Julia Volmer
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "Containers/Concurrent/ListOfNonOwnedLists.h"
#include "Containers/Concurrent/ThreadOwnedList.h"
#include "TaskMonitoring/task.h"

namespace arangodb::task_monitoring {

using ThreadRegistry = containers::ThreadOwnedList<TaskInRegistry>;
struct Registry : public containers::ListOfNonOwnedLists<ThreadRegistry> {};

/**
   Global variable that holds all active tasks.

   Includes a list of thread owned lists, one for each initialized
   thread.
 */
extern Registry registry;

/**
   Get thread registry of all active tasks on current thread.

   Creates the thread registry when called for the first time and adds it to the
   global registry.
 */
auto get_thread_registry() noexcept -> ThreadRegistry&;

}  // namespace arangodb::task_monitoring

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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

#include <atomic>

#include "Task.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

#include "Basics/debugging.h"
#include "Basics/system-functions.h"

using namespace arangodb::rest;

namespace {
std::atomic_uint_fast64_t NEXT_TASK_ID(static_cast<uint64_t>(TRI_microtime() * 100000.0));
}

Task::Task(Scheduler* scheduler, std::string const& name)
    : _scheduler(scheduler),
      _taskId(NEXT_TASK_ID.fetch_add(1, std::memory_order_seq_cst)),
      _name(name) {
  TRI_ASSERT(_scheduler != nullptr);
}

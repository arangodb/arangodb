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

#include "Task.h"
#include "Scheduler/Scheduler.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb::rest;

namespace {
std::atomic_uint_fast64_t NEXT_TASK_ID(static_cast<uint64_t>(TRI_microtime() *
                                                             100000.0));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new task
////////////////////////////////////////////////////////////////////////////////

Task::Task(std::string const& id, std::string const& name)
    : _scheduler(nullptr),
      _taskId(NEXT_TASK_ID.fetch_add(1, std::memory_order_seq_cst)),
      _loop(0),  // TODO(fc) XXX this should be an "invalid" marker!
      _id(id),
      _name(name) {}

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new task
////////////////////////////////////////////////////////////////////////////////

Task::Task(std::string const& name) : Task("", name) {}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a task
////////////////////////////////////////////////////////////////////////////////

Task::~Task() {}

////////////////////////////////////////////////////////////////////////////////
/// @brief get a VelocyPack representation of the task
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<VPackBuilder> Task::toVelocyPack() const {
  try {
    auto builder = std::make_shared<VPackBuilder>();
    {
      VPackObjectBuilder b(builder.get());
      toVelocyPack(*builder);
    }
    return builder;
  } catch (...) {
    return std::make_shared<VPackBuilder>();
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get a VelocyPack representation of the task
////////////////////////////////////////////////////////////////////////////////

void Task::toVelocyPack(VPackBuilder& builder) const {
  TRI_ASSERT(builder.isOpenObject());
  builder.add("id", VPackValue(id()));
  builder.add("name", VPackValue(name()));
  getDescription(builder);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the task is user-defined
/// note: this function may be overridden
////////////////////////////////////////////////////////////////////////////////

bool Task::isUserDefined() const { return false; }

////////////////////////////////////////////////////////////////////////////////
/// @brief allow thread to run on slave event loop
////////////////////////////////////////////////////////////////////////////////

bool Task::needsMainEventLoop() const { return false; }

////////////////////////////////////////////////////////////////////////////////
/// @brief get a task specific description in VelocyPack format
/// this does nothing for basic tasks, but derived classes may override it
////////////////////////////////////////////////////////////////////////////////

void Task::getDescription(VPackBuilder&) const {}

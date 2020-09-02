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

#ifndef ARANGOD_SCHEDULER_TASK_H
#define ARANGOD_SCHEDULER_TASK_H 1

#include <memory>
#include <string>

#include "Basics/Common.h"

namespace arangodb {
namespace velocypack {
class Builder;
}

class TaskData;

namespace rest {
class Scheduler;

class Task : public std::enable_shared_from_this<Task> {
  Task(Task const&) = delete;
  Task& operator=(Task const&) = delete;

 public:
  Task(Scheduler*, std::string const& name);
  virtual ~Task() = default;

 public:
  uint64_t taskId() const { return _taskId; }
  std::string const& name() const { return _name; }

  // get a VelocyPack representation of the task for reporting
  std::shared_ptr<arangodb::velocypack::Builder> toVelocyPack() const;
  void toVelocyPack(arangodb::velocypack::Builder&) const;

 protected:
  Scheduler* const _scheduler;
  uint64_t const _taskId;

 private:
  std::string const _name;
};
}  // namespace rest
}  // namespace arangodb

#endif

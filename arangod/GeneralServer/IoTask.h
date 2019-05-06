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

#ifndef ARANGOD_SCHEDULER_IO_TASK_H
#define ARANGOD_SCHEDULER_IO_TASK_H 1

#include "Basics/Common.h"
#include "GeneralServer/GeneralServer.h"

namespace arangodb {
namespace rest {

class IoTask : public std::enable_shared_from_this<IoTask> {
  IoTask(IoTask const&) = delete;
  IoTask& operator=(IoTask const&) = delete;

 public:
  IoTask(GeneralServer& server, 
         GeneralServer::IoContext&, 
         char const* name);
  virtual ~IoTask() = default;

 public:
  // doesn't seem to be called right now, but can be used for debugging
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  char const* name() const { return _name; }
#endif

  uint64_t id() const { return _taskId; }

 protected:
  GeneralServer::IoContext& _context;
  GeneralServer& _server;
  uint64_t const _taskId;

 private:
  char const* _name;
};
}  // namespace rest
}  // namespace arangodb

#endif

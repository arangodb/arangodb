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
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_HTTP_SERVER_REST_HANDLER_H
#define ARANGOD_HTTP_SERVER_REST_HANDLER_H 1

#include "Basics/Common.h"

#include "Basics/Exceptions.h"
#include "Basics/WorkMonitor.h"
#include "Dispatcher/Dispatcher.h"
#include "Dispatcher/Job.h"
#include "Rest/GeneralResponse.h"
#include "Scheduler/events.h"
#include "Statistics/StatisticsAgent.h"

namespace arangodb {
class GeneralRequest;
class WorkMonitor;

namespace rest {
class RestHandlerFactory;

class RestHandler : public RequestStatisticsAgent, public arangodb::WorkItem {
  RestHandler(RestHandler const&) = delete;
  RestHandler& operator=(RestHandler const&) = delete;

 public:
  RestHandler(GeneralRequest*, GeneralResponse*);

 protected:
  ~RestHandler() = default;

 public:
  enum class status { DONE, FAILED, ASYNC };

 public:
  // returns true if a handler is executed directly
  virtual bool isDirect() const = 0;

  // returns true if a handler desires to start a new dispatcher thread
  virtual bool needsOwnThread() const { return _needsOwnThread; }

  // returns the queue name
  virtual size_t queue() const { return Dispatcher::STANDARD_QUEUE; }

  // prepares execution of a handler, has to be called before execute
  virtual void prepareExecute() {}

  // executes a handler
  virtual status execute() = 0;

  // finalizes execution of a handler, has to be called after execute
  virtual void finalizeExecute() {}

  // tries to cancel an execution
  virtual bool cancel() { return false; }

  // handles error
  virtual void handleError(basics::Exception const&) = 0;

  // adds a response
  virtual void addResponse(RestHandler*) {}

 public:
  // returns the id of the underlying task
  uint64_t taskId() const { return _taskId; }

  // returns the event loop of the underlying task
  EventLoop eventLoop() const { return _loop; }

  // sets the id of the underlying task or 0 if dettach
  void setTaskId(uint64_t id, EventLoop);

  // execution cycle including error handling and prepare
  status executeFull();

  // return a pointer to the request
  GeneralRequest const* request() const { return _request.get(); }

  // steal the pointer to the request
  std::unique_ptr<GeneralRequest> stealRequest() { return std::move(_request); }

  // returns the response
  GeneralResponse* response() const { return _response.get(); }

  // steal the response
  std::unique_ptr<GeneralResponse> stealResponse() {
    return std::move(_response);
  }

 protected:
  // resets the request
  void resetResponse(rest::ResponseCode);

 protected:
  // handler id
  uint64_t const _handlerId;

  // task id or (initially) 0
  uint64_t _taskId = 0;

  // event loop
  EventLoop _loop;

  std::unique_ptr<GeneralRequest> _request;
  std::unique_ptr<GeneralResponse> _response;

 private:
  bool _needsOwnThread = false;
};
}
}

#endif

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
#include "Rest/GeneralResponse.h"
#include "Scheduler/EventLoop.h"
#include "Scheduler/JobQueue.h"
#include "Statistics/RequestStatistics.h"

namespace arangodb {
class GeneralRequest;
class RequestStatistics;
  
enum class RestStatus { DONE, FAIL};

namespace rest {
class RestHandler : public std::enable_shared_from_this<RestHandler> {
  RestHandler(RestHandler const&) = delete;
  RestHandler& operator=(RestHandler const&) = delete;

 public:
  static thread_local RestHandler const* CURRENT_HANDLER;

 public:
  RestHandler(GeneralRequest*, GeneralResponse*);
  virtual ~RestHandler();

 public:
  uint64_t handlerId() const { return _handlerId; }
  bool needsOwnThread() const { return _needsOwnThread; }
  uint64_t messageId() const;

  GeneralRequest const* request() const { return _request.get(); }
  std::unique_ptr<GeneralRequest> stealRequest() { return std::move(_request); }

  GeneralResponse* response() const { return _response.get(); }
  std::unique_ptr<GeneralResponse> stealResponse() {
    return std::move(_response);
  }

  RequestStatistics* statistics() const { return _statistics.load(); }

  RequestStatistics* stealStatistics() {
    return _statistics.exchange(nullptr);
  }
  
  void setStatistics(RequestStatistics* stat) {
    RequestStatistics* old = _statistics.exchange(stat);

    if (old != nullptr) {
      old->release();
    }
  }

  /// Execute the rest handler state machine
  int runHandler(std::function<void(rest::RestHandler*)> cb);
  
 public:
  virtual char const* name() const = 0;
  virtual bool isDirect() const = 0;
  virtual size_t queue() const { return JobQueue::STANDARD_QUEUE; }

  virtual void prepareExecute() {}
  virtual RestStatus execute() = 0;
  virtual void finalizeExecute() {}

  // you might need to implment this in you handler
  // if it will be executed in an async job
  virtual bool cancel() {
    _canceled.store(true);
    return false;
  }

  virtual void handleError(basics::Exception const&) = 0;

 protected:
  
  void resetResponse(rest::ResponseCode);
  
 private:
  
  enum class State { PREPARE, EXECUTE, FINALIZE, DONE, FAILED };
  
  int prepareEngine(std::function<void(rest::RestHandler*)> const&);
  int executeEngine(std::function<void(rest::RestHandler*)> const&);
  int finalizeEngine(std::function<void(rest::RestHandler*)> const&);

 protected:
  uint64_t const _handlerId;

  std::atomic<bool> _canceled;

  std::unique_ptr<GeneralRequest> _request;
  std::unique_ptr<GeneralResponse> _response;

  std::atomic<RequestStatistics*> _statistics;

 private:
  bool _needsOwnThread = false;
  State _state = State::PREPARE;
};

}
}

#endif

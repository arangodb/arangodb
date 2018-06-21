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
#include "Rest/GeneralResponse.h"
#include "Scheduler/JobQueue.h"

namespace arangodb {
class GeneralRequest;
class RequestStatistics;
  
enum class RestStatus { DONE, WAITING };

namespace rest {
class RestHandler : public std::enable_shared_from_this<RestHandler> {
  friend class GeneralCommTask;
  
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
  
  void setStatistics(RequestStatistics* stat);

  /// Execute the rest handler state machine
  void continueHandlerExecution() {
    TRI_ASSERT(_state == HandlerState::PAUSED);
    runHandlerStateMachine();
  }
  
  /// Execute the rest handler state machine
  void runHandler(std::function<void(rest::RestHandler*)> cb) {
    TRI_ASSERT(_state == HandlerState::PREPARE);
    _callback = std::move(cb);
    runHandlerStateMachine();
  }
  
 public:
  /// @brief rest handler name
  virtual char const* name() const = 0;
  /// @brief allow execution on the network thread
  virtual bool isDirect() const = 0;
  /// @brief priority of this request
  virtual size_t queue() const { return JobQueue::STANDARD_QUEUE; }

  virtual void prepareExecute(bool isContinue) {}
  virtual RestStatus execute() = 0;
  virtual RestStatus continueExecute() { return RestStatus::DONE; }
  virtual void shutdownExecute(bool isFinalized) noexcept {}

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
  
  enum class HandlerState { PREPARE, EXECUTE, PAUSED, CONTINUED, FINALIZE, DONE, FAILED };
  
  void runHandlerStateMachine();
  
  void prepareEngine();
  /// @brief Executes the RestHandler
  ///        May set the state to PAUSED, FINALIZE or FAILED
  ///        If isContinue == true it will call continueExecute()
  ///        otherwise execute() will be called
  void executeEngine(bool isContinue);
  void shutdownEngine();

 protected:
  uint64_t const _handlerId;

  std::atomic<bool> _canceled;

  std::unique_ptr<GeneralRequest> _request;
  std::unique_ptr<GeneralResponse> _response;

  std::atomic<RequestStatistics*> _statistics;

 private:
  bool _needsOwnThread = false;
  HandlerState _state;
  std::function<void(rest::RestHandler*)> _callback;

  mutable Mutex _executionMutex;
};

}
}

#endif

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
#include "Scheduler/Scheduler.h"
#include "GeneralServer/RequestLane.h"

namespace arangodb {
class GeneralRequest;
class RequestStatistics;

enum class RestStatus { DONE, WAITING, FAIL };

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
  void assignHandlerId();
  uint64_t handlerId() const { return _handlerId; }
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
  void runHandler(std::function<void(rest::RestHandler*)> cb) {
    TRI_ASSERT(_state == HandlerState::PREPARE);
    _callback = std::move(cb);
    runHandlerStateMachine();
  }

  /// Execute the rest handler state machine
  void continueHandlerExecution();

  /// @brief forwards the request to the appropriate server
  bool forwardRequest();

  // The priority is derived from the lane.
  // Header fields might influence the priority.
  // In order to change the priority of a handler
  // adjust the lane, do not overwrite the priority
  // function!
  RequestPriority priority(RequestLane) const;
  RequestPriority priority() const {return priority(lane());}

 public:
  // rest handler name for debugging and logging
  virtual char const* name() const = 0;

  // what lane to use for this request
  virtual RequestLane lane() const = 0;

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

  /// @brief determines the possible forwarding target for this request
  ///
  /// This method will be called to determine if the request should be
  /// forwarded to another server, and if so, which server. If it should be
  /// handled by this server, the method should return 0. Otherwise, this
  /// method should return a valid (non-zero) short ID (TransactionID) for the
  /// target server.
  virtual uint32_t forwardingTarget() { return 0; }

  void resetResponse(rest::ResponseCode);

  void generateError(rest::ResponseCode, int, std::string const&);

  // generates an error
  void generateError(rest::ResponseCode, int);

  // generates an error
  void generateError(arangodb::Result const&);

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
  std::atomic<bool> _canceled;

  std::unique_ptr<GeneralRequest> _request;
  std::unique_ptr<GeneralResponse> _response;

  std::atomic<RequestStatistics*> _statistics;

 private:
  uint64_t _handlerId;

  HandlerState _state;
  std::function<void(rest::RestHandler*)> _callback;

  mutable Mutex _executionMutex;
};

}
}

#endif

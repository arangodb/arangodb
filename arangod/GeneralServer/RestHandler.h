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
#include "GeneralServer/RestEngine.h"
#include "Rest/GeneralResponse.h"
#include "Scheduler/EventLoop.h"
#include "Scheduler/JobQueue.h"
#include "Statistics/RequestStatistics.h"

namespace arangodb {
class GeneralRequest;
class RequestStatistics;
class WorkMonitor;

namespace rest {
class GeneralCommTask;
class RestHandlerFactory;

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
  uint64_t messageId() const;
  bool needsOwnThread() const { return _needsOwnThread; }

  GeneralRequest const* request() const { return _request.get(); }
  std::unique_ptr<GeneralRequest> stealRequest() { return std::move(_request); }

  GeneralResponse* response() const { return _response.get(); }
  std::unique_ptr<GeneralResponse> stealResponse() {
    return std::move(_response);
  }

  std::shared_ptr<WorkContext> context() { return _context; }

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

 protected:
  uint64_t const _handlerId;

  std::atomic<bool> _canceled;

  std::unique_ptr<GeneralRequest> _request;
  std::unique_ptr<GeneralResponse> _response;

  std::shared_ptr<WorkContext> _context;

  std::atomic<RequestStatistics*> _statistics;

 private:
  bool _needsOwnThread = false;

 public:
  void initEngine(EventLoop loop,
                  std::function<void(RestHandler*)> storeResult) {
    _storeResult = storeResult;
    _engine.init(loop);
  }

  int asyncRunEngine() { return _engine.asyncRun(shared_from_this()); }
  int syncRunEngine() {
    _storeResult = [](RestHandler*) {};
    return _engine.syncRun(shared_from_this());
  }

  int prepareEngine();
  int executeEngine();
  int runEngine(bool synchron);
  int finalizeEngine();

 private:
  RestEngine _engine;
  std::function<void(rest::RestHandler*)> _storeResult;
};

inline uint64_t RestHandler::messageId() const {
  uint64_t messageId = 0UL;
  auto req = _request.get();
  auto res = _response.get();
  if (req) {
    messageId = req->messageId();
  } else if (res) {
    messageId = res->messageId();
  } else {
    LOG_TOPIC(WARN, Logger::COMMUNICATION)
        << "could not find corresponding request/response";
  }

  return messageId;
}
}
}

#endif

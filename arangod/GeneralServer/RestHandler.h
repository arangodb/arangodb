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
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_HTTP_SERVER_REST_HANDLER_H
#define ARANGOD_HTTP_SERVER_REST_HANDLER_H 1

#include "Basics/Common.h"
#include "Basics/ResultT.h"
#include "GeneralServer/RequestLane.h"
#include "Rest/GeneralResponse.h"
#include "Statistics/RequestStatistics.h"

#include <atomic>
#include <string_view>
#include <thread>

namespace arangodb {
namespace application_features {
class ApplicationServer;
}
namespace basics {
class Exception;
}

namespace futures {
template <typename T>
class Future;
template <typename T>
class Try;
}  // namespace futures

class GeneralRequest;
class RequestStatistics;
class Result;

enum class RestStatus { DONE, WAITING, FAIL };

namespace rest {
class RestHandler : public std::enable_shared_from_this<RestHandler> {
  friend class CommTask;

  RestHandler(RestHandler const&) = delete;
  RestHandler& operator=(RestHandler const&) = delete;

 public:
  RestHandler(application_features::ApplicationServer&, GeneralRequest*, GeneralResponse*);
  virtual ~RestHandler();

 public:
  void assignHandlerId();
  uint64_t handlerId() const { return _handlerId; }
  uint64_t messageId() const;

  GeneralRequest const* request() const { return _request.get(); }
  GeneralResponse* response() const { return _response.get(); }
  std::unique_ptr<GeneralResponse> stealResponse() {
    return std::move(_response);
  }

  application_features::ApplicationServer& server() { return _server; };

  RequestStatistics::Item const& statistics() { return _statistics; }
  RequestStatistics::Item&& stealStatistics();
  void setStatistics(RequestStatistics::Item&& stat);

  /// Execute the rest handler state machine
  void runHandler(std::function<void(rest::RestHandler*)> cb) {
    TRI_ASSERT(_state == HandlerState::PREPARE);
    _callback = std::move(cb);
    runHandlerStateMachine();
  }

  /// Execute the rest handler state machine. Retry the wakeup,
  /// returns true if _state == PAUSED, false otherwise
  bool wakeupHandler();

  /// @brief forwards the request to the appropriate server
  futures::Future<Result> forwardRequest(bool& forwarded);

  void handleExceptionPtr(std::exception_ptr) noexcept;

 public:
  // rest handler name for debugging and logging
  virtual char const* name() const = 0;

  // what lane to use for this request
  virtual RequestLane lane() const = 0;

  RequestLane getRequestLane() {
    bool found;
    _request->header(StaticStrings::XArangoFrontend, found);

    if (found) {
      return RequestLane::CLIENT_UI;
    }

    return lane();
  }

  virtual void prepareExecute(bool isContinue) {}
  virtual RestStatus execute() = 0;
  virtual RestStatus continueExecute() { return RestStatus::DONE; }
  virtual void shutdownExecute(bool isFinalized) noexcept {}

  // you might need to implment this in your handler
  // if it will be executed in an async job
  virtual void cancel() { _canceled.store(true); }

  virtual void handleError(basics::Exception const&) = 0;

 protected:
  /// @brief determines the possible forwarding target for this request
  ///
  /// This method will be called to determine if the request should be
  /// forwarded to another server, and if so, which server. If it should be
  /// handled by this server, the method should return an empty string.
  /// Otherwise, this method should return a valid short name for the
  /// target server.
  /// std::string -> empty string or valid short name
  /// boolean -> should auth header and user be removed in that request
  virtual ResultT<std::pair<std::string, bool>> forwardingTarget() {
    return {std::make_pair(StaticStrings::Empty, false)};
  }

  void resetResponse(rest::ResponseCode);

  void generateError(rest::ResponseCode, int errorNumber, std::string_view errorMessage);

  // generates an error
  void generateError(rest::ResponseCode, int errorCode);

  // generates an error
  void generateError(arangodb::Result const&);

  template <typename T>
  RestStatus waitForFuture(futures::Future<T>&& f) {
    if (f.isReady()) {             // fast-path out
      f.result().throwIfFailed();  // just throw the error upwards
      return RestStatus::DONE;
    }
    bool done = false;
    std::move(f).thenFinal([self = shared_from_this(), &done](futures::Try<T>&& t) -> void {
      auto thisPtr = self.get();
      if (t.hasException()) {
        thisPtr->handleExceptionPtr(std::move(t).exception());
      }
      if (std::this_thread::get_id() == thisPtr->_executionMutexOwner.load()) {
        done = true;
      } else {
        thisPtr->wakeupHandler();
      }
    });
    return done ? RestStatus::DONE : RestStatus::WAITING;
  }

  enum class HandlerState : uint8_t {
    PREPARE = 0,
    EXECUTE,
    PAUSED,
    CONTINUED,
    FINALIZE,
    DONE,
    FAILED
  };

  /// handler state machine
  HandlerState state() const { return _state; }

 private:
  void runHandlerStateMachine();

  void prepareEngine();
  /// @brief Executes the RestHandler
  ///        May set the state to PAUSED, FINALIZE or FAILED
  ///        If isContinue == true it will call continueExecute()
  ///        otherwise execute() will be called
  void executeEngine(bool isContinue);
  void compressResponse();

 protected:
  std::unique_ptr<GeneralRequest> _request;
  std::unique_ptr<GeneralResponse> _response;
  application_features::ApplicationServer& _server;
  RequestStatistics::Item _statistics;

 private:
  mutable Mutex _executionMutex;

  std::function<void(rest::RestHandler*)> _callback;

  uint64_t _handlerId;

  std::atomic<std::thread::id> _executionMutexOwner;

  HandlerState _state;

 protected:
  std::atomic<bool> _canceled;
};

}  // namespace rest
}  // namespace arangodb

#endif

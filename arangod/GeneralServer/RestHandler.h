////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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

#pragma once

#include "Async/SuspensionSemaphore.h"
#include "Async/async.h"
#include "Basics/ResultT.h"
#include "Futures/Unit.h"
#include "GeneralServer/RequestLane.h"
#include "Logger/LogContext.h"
#include "Metrics/GaugeCounterGuard.h"
#include "Rest/GeneralResponse.h"
#include "Statistics/RequestStatistics.h"

#include <atomic>
#include <concepts>
#include <memory>
#include <mutex>
#include <string_view>

namespace arangodb {
namespace application_features {
class ApplicationServer;
}
namespace basics {
class Exception;
}

namespace futures {
template<typename T>
class Future;
}  // namespace futures

template<typename>
struct async;

class GeneralRequest;
class RequestStatistics;
class Result;

enum class RestStatus { DONE, WAITING };

namespace rest {
class RestHandler : public std::enable_shared_from_this<RestHandler> {
  friend class CommTask;

  RestHandler(RestHandler const&) = delete;
  RestHandler& operator=(RestHandler const&) = delete;

 public:
  RestHandler(ArangodServer&, GeneralRequest*, GeneralResponse*);
  virtual ~RestHandler();

  void assignHandlerId();
  uint64_t handlerId() const noexcept { return _handlerId; }
  uint64_t messageId() const;

  /// @brief called when the handler is queued for execution in the scheduler
  void trackQueueStart() noexcept;

  /// @brief called when the handler is dequeued in the scheduler
  void trackQueueEnd() noexcept;

  /// @brief called when the handler execution is started
  void trackTaskStart() noexcept;

  /// @brief called when the handler execution is finalized
  void trackTaskEnd() noexcept;

  GeneralRequest const* request() const { return _request.get(); }
  GeneralResponse* response() const { return _response.get(); }
  std::unique_ptr<GeneralResponse> stealResponse() {
    return std::move(_response);
  }

  ArangodServer& server() noexcept { return _server; }
  ArangodServer const& server() const noexcept { return _server; }

  [[nodiscard]] RequestStatistics::Item const& requestStatistics()
      const noexcept {
    return _statistics;
  }
  [[nodiscard]] RequestStatistics::Item&& stealRequestStatistics();
  void setRequestStatistics(RequestStatistics::Item&& stat);

  void setIsAsyncRequest() noexcept { _isAsyncRequest = true; }

  /// Execute the rest handler state machine
  virtual void runHandler(
      std::function<void(rest::RestHandler*)> responseCallback);

  /// Continue execution of a suspended (via WAITING) rest handler state machine
  bool wakeupHandler();

  /// @brief forwards the request to the appropriate server
  futures::Future<Result> forwardRequest(bool& forwarded);

  void handleExceptionPtr(std::exception_ptr) noexcept;

 public:
  // rest handler name for debugging and logging
  virtual char const* name() const = 0;

  // what lane to use for this request
  virtual RequestLane lane() const = 0;

  RequestLane determineRequestLane();

  [[nodiscard]] virtual auto prepareExecute(bool isContinue)
      -> std::vector<std::shared_ptr<LogContext::Values>>;
  virtual RestStatus execute();
  virtual futures::Future<futures::Unit> executeAsync();
  virtual RestStatus continueExecute() { return RestStatus::DONE; }
  virtual void shutdownExecute(bool isFinalized) noexcept;

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

  void generateError(rest::ResponseCode code, ErrorCode errorNumber,
                     std::string_view errorMessage);

  // generates an error
  void generateError(rest::ResponseCode code, ErrorCode errorNumber);

  // generates an error
  void generateError(arangodb::Result const&);

  [[nodiscard]] RestStatus waitForFuture(futures::Future<futures::Unit>&& f);
  [[nodiscard]] RestStatus waitForFuture(futures::Future<RestStatus>&& f);

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

  auto runHandlerStateMachine() -> futures::Future<futures::Unit>;

  [[nodiscard]] auto prepareEngine()
      -> std::vector<std::shared_ptr<LogContext::Values>>;
  /// @brief Executes the RestHandler
  auto executeEngine() -> async<void>;
  void compressResponse();

 protected:
  // This alias allows the RestHandler and derived classes to add values to the
  // LogContext. The intention behind RestHandler being a friend of
  // LogContext::Accessor and defining this alias as protected is to restrict
  // usage of ScopedValues to RestHandlers only in order to prevent ScopedValues
  // to be created in some inner function where they might cause significant
  // performance overhead.
  using ScopedValue = LogContext::Accessor::ScopedValue;

  std::unique_ptr<GeneralRequest> _request;
  std::unique_ptr<GeneralResponse> _response;
  ArangodServer& _server;
  RequestStatistics::Item _statistics;

 private:
  mutable std::mutex _executionMutex;
  mutable std::atomic_uint8_t _executionCounter{0};
  mutable RestStatus _followupRestStatus;

 protected:
  // TODO Move this in a separate header, side-by-side with SuspensionSemaphore?
  // Note: _suspensionSemaphore.notify() must be called for this to resume.
  // RestHandler::wakeupHandler() does that, and can be called e.g. by the
  // SharedQueryState's wakeup handler (for AQL-related code).
  template<typename F>
  requires requires(F f) { { f() } -> std::same_as<RestStatus>; }
  [[nodiscard]] auto waitingFunToCoro(F&& funArg) -> async<void> {
    auto fun = std::forward<F>(funArg);
    auto state = fun();

    while (state == RestStatus::WAITING) {
      // Get the number of wakeups. We call fun() up to that many
      // times before suspending again.
      auto n = co_await _suspensionSemaphore.await();
      for (auto i = 0; i < n && state == RestStatus::WAITING; ++i) {
        state = fun();
      }
    }
    co_return;
  }

 private:
  SuspensionSemaphore _suspensionSemaphore;

  std::function<void(rest::RestHandler*)> _sendResponseCallback;

  uint64_t _handlerId;

  HandlerState _state;
  // whether or not we have tracked this task as ongoing.
  // can only be true during handler execution, and only for
  // low priority tasks
  bool _trackedAsOngoingLowPrio;

  // whether or not the handler handles a request for the async
  // job api (/_api/job) or the batch API (/_api/batch)
  bool _isAsyncRequest = false;

  RequestLane _lane;

  std::shared_ptr<LogContext::Values> _logContextScopeValues;

 protected:
  metrics::GaugeCounterGuard<std::uint64_t> _currentRequestsSizeTracker;

  std::atomic<bool> _canceled;
};

}  // namespace rest
}  // namespace arangodb

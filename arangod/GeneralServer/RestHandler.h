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

#include "Async/SuspensionCounter.h"
#include "Async/async.h"
#include "Basics/ResultT.h"
#include "Futures/Unit.h"
#include "GeneralServer/RequestLane.h"
#include "Logger/LogContext.h"
#include "Logger/LogStructuredParamsAllowList.h"
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
  void runHandler(std::function<void(rest::RestHandler*)> responseCallback);

  /// Continue execution of a suspended (via WAITING) rest handler state machine
  [[deprecated]] bool wakeupHandler();

  /// @brief forwards the request to the appropriate server
  futures::Future<Result> forwardRequest(bool& forwarded);

  void handleExceptionPtr(std::exception_ptr) noexcept;

 public:
  // rest handler name for debugging and logging
  virtual char const* name() const = 0;

  // what lane to use for this request
  virtual RequestLane lane() const = 0;

  RequestLane determineRequestLane();

  // TODO execute() should be changed to return void, as it must not return
  //      WAITING anymore.
  virtual RestStatus execute();
  virtual futures::Future<futures::Unit> executeAsync();

  virtual void shutdownExecute(bool isFinalized) noexcept;

  // you might need to implement this in your handler
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

  void prepareEngine();
  /// @brief Executes the RestHandler
  auto executeEngine() -> async<void>;
  void compressResponse();

  // The ValueBuilder, as it is, is unsuitable for composition. By composition,
  // I mean, for example, creating a builder in the base class with certain
  // values, and reusing that and adding additional values in a subclass.
  // This is because all temporary builders, as returned by .with<>() calls,
  // must still live when .share() is called - the value returned by share is,
  // however, no longer mutable or extensible.
  // So when overriding this method, all values added here must also be added in
  // the overridden method. And when changing this method, all derived methods
  // have to be manually changed, too!
  [[nodiscard]] virtual auto makeSharedLogContextValue() const
      -> std::shared_ptr<LogContext::Values> {
    return LogContext::makeValue()
        .with<structuredParams::UrlName>(_request->fullUrl())
        .with<structuredParams::UserName>(_request->user())
        .share();
  }

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

 protected:
  // TODO Move this in a separate header, side-by-side with SuspensionCounter?
  // Note: _suspensionCounter.notify() must be called for this to resume.
  // RestHandler::wakeupHandler() does that, and can be called e.g. by the
  // SharedQueryState's wakeup handler (for AQL-related code).
  template<typename F>
  requires requires(F f) {
    { f() } -> std::same_as<RestStatus>;
  }
  [[nodiscard]] auto waitingFunToCoro(F&& fun) -> async<void> {
    co_await arangodb::waitingFunToCoro(_suspensionCounter,
                                        [&]() -> std::optional<std::monostate> {
                                          auto state = fun();
                                          if (state == RestStatus::WAITING) {
                                            return std::nullopt;
                                          }
                                          return std::monostate{};
                                        });
    co_return;
  }

  template<typename F, typename T = std::invoke_result_t<F>::value_type>
  requires requires(F f) {
    { f() } -> std::same_as<std::optional<T>>;
  }
  [[nodiscard]] auto waitingFunToCoro(F&& fun) -> async<T> {
    co_return co_await arangodb::waitingFunToCoro(_suspensionCounter,
                                                  std::forward<F>(fun));
  }

 protected:
  SuspensionCounter _suspensionCounter;

 private:
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

 protected:
  metrics::GaugeCounterGuard<std::uint64_t> _currentRequestsSizeTracker;

  std::atomic<bool> _canceled;
};

}  // namespace rest
}  // namespace arangodb

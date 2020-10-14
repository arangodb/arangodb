////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018-2019 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "Methods.h"

#include "Agency/AgencyFeature.h"
#include "Agency/Agent.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/Common.h"
#include "Basics/HybridLogicalClock.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ServerState.h"
#include "Futures/Utilities.h"
#include "Logger/LogMacros.h"
#include "Network/ConnectionPool.h"
#include "Network/NetworkFeature.h"
#include "Network/Utils.h"

#include <fuerte/connection.h>
#include <fuerte/requests.h>
#include <fuerte/types.h>

#include "Scheduler/Scheduler.h"
#include "Scheduler/SchedulerFeature.h"

namespace arangodb {
namespace network {
using namespace arangodb::fuerte;

using PromiseRes = arangodb::futures::Promise<network::Response>;

/// @brief shardId or empty
std::string Response::destinationShard() const {
  if (this->destination.size() > 6 && this->destination.compare(0, 6, "shard:", 6) == 0) {
    return this->destination.substr(6);
  }
  return StaticStrings::Empty;
}

std::string Response::serverId() const {
  if (this->destination.size() > 7 && this->destination.compare(0, 7, "server:", 7) == 0) {
    return this->destination.substr(7);
  }
  return StaticStrings::Empty;
}

Result Response::combinedResult() const {
  if (fail()) {
    // fuerte connection failed
    return Result{fuerteToArangoErrorCode(*this), fuerteToArangoErrorMessage(*this)};
  } else {
    if (!statusIsSuccess(response->statusCode())) {
      // HTTP status error. Try to extract a precise error from the body, and
      // fall back to the HTTP status.
      return resultFromBody(response->slice(), fuerteStatusToArangoErrorCode(*response));
    } else {
      return Result{};
    }
  }
}

auto prepareRequest(RestVerb type, std::string path, VPackBufferUInt8 payload,
                    RequestOptions const& options, Headers headers) {

  auto req = fuerte::createRequest(type, path, options.parameters, std::move(payload));

  req->header.database = options.database;
  req->header.setMeta(std::move(headers));

  if (!options.contentType.empty()) {
    req->header.contentType(options.contentType);
  }
  if (!options.acceptType.empty()) {
    req->header.acceptType(options.acceptType);
  }

  TRI_voc_tick_t timeStamp = TRI_HybridLogicalClock();
  req->header.addMeta(StaticStrings::HLCHeader,
                      arangodb::basics::HybridLogicalClock::encodeTimeStamp(timeStamp));

  auto state = ServerState::instance();
  if (state->isCoordinator() || state->isDBServer()) {
    req->header.addMeta(StaticStrings::ClusterCommSource, state->getId());
  } else if (state->isAgent()) {
    auto agent = AgencyFeature::AGENT;
    if (agent != nullptr) {
      req->header.addMeta(StaticStrings::ClusterCommSource, "AGENT-" + agent->id());
    }
  }

  return req;
}

/// @brief Function to produce a response object from thin air:
static std::unique_ptr<fuerte::Response> buildResponse(fuerte::StatusCode statusCode,
                                                       Result const& res) {
  VPackBuffer<uint8_t> buffer;
  VPackBuilder builder(buffer);
  {
    VPackObjectBuilder guard(&builder);
    auto errorNum = res.errorNumber();
    builder.add(StaticStrings::Error, VPackValue(errorNum != TRI_ERROR_NO_ERROR));
    builder.add(StaticStrings::ErrorNum, VPackValue(errorNum));
    if (errorNum != TRI_ERROR_NO_ERROR) {
      builder.add(StaticStrings::ErrorMessage, VPackValue(res.errorMessage()));
    }
    builder.add(StaticStrings::Code, VPackValue(static_cast<int>(statusCode)));
  }
  fuerte::ResponseHeader responseHeader;
  responseHeader.responseCode = statusCode;
  responseHeader.contentType(ContentType::VPack);
  auto resp = std::make_unique<fuerte::Response>(responseHeader);
  resp->setPayload(std::move(buffer), 0);
  return resp;
}

/// @brief send a request to a given destination
FutureRes sendRequest(ConnectionPool* pool, DestinationId dest, RestVerb type,
                      std::string path, velocypack::Buffer<uint8_t> payload,
                      RequestOptions const& options, Headers headers) {
  LOG_TOPIC("2713a", DEBUG, Logger::COMMUNICATION)
      << "request to '" << dest << "' '" << fuerte::to_string(type) << " "
      << path << "'";

  // FIXME build future.reset(..)
  try {
    auto req = prepareRequest(type, std::move(path), std::move(payload),
                              options, std::move(headers));
    req->timeout(std::chrono::duration_cast<std::chrono::milliseconds>(options.timeout));

    if (!pool || !pool->config().clusterInfo) {
      LOG_TOPIC("59b95", ERR, Logger::COMMUNICATION)
          << "connection pool unavailable";
      return futures::makeFuture(Response{std::move(dest), Error::Canceled,
                                          std::move(req), nullptr});
    }

    arangodb::network::EndpointSpec spec;
    int res = resolveDestination(*pool->config().clusterInfo, dest, spec);
    if (res != TRI_ERROR_NO_ERROR) {
      // We fake a successful request with statusCode 503 and a backend not
      // available error here:
      auto resp = buildResponse(fuerte::StatusServiceUnavailable, Result{res});
      return futures::makeFuture(Response{std::move(dest), Error::NoError,
                                          std::move(req), std::move(resp)});
    }
    TRI_ASSERT(!spec.endpoint.empty());

    struct Pack {
      DestinationId dest;
      futures::Promise<network::Response> promise;
      std::unique_ptr<fuerte::Response> tmp_res;
      std::unique_ptr<fuerte::Request> tmp_req;
      fuerte::Error tmp_err;
      bool skipScheduler;
      Pack(DestinationId&& dest, bool skip)
          : dest(std::move(dest)), skipScheduler(skip) {}
    };

    // fits in SSO of std::function
    static_assert(sizeof(std::shared_ptr<Pack>) <= 2 * sizeof(void*), "");
    auto conn = pool->leaseConnection(spec.endpoint);
    auto p = std::make_shared<Pack>(std::move(dest), options.skipScheduler);

    FutureRes f = p->promise.getFuture();
    conn->sendRequest(std::move(req), [p(std::move(p))](fuerte::Error err,
                                                        std::unique_ptr<fuerte::Request> req,
                                                        std::unique_ptr<fuerte::Response> res) mutable {
      Scheduler* sch = SchedulerFeature::SCHEDULER;
      if (p->skipScheduler || sch == nullptr) {
        p->promise.setValue(network::Response{std::move(p->dest), err,
                                              std::move(req), std::move(res)});
        return;
      }

      p->tmp_err = err;
      p->tmp_res = std::move(res);
      p->tmp_req = std::move(req);

      bool queued = sch->queue(RequestLane::CLUSTER_INTERNAL, [p]() mutable {
        p->promise.setValue(Response{std::move(p->dest), p->tmp_err,
                                     std::move(p->tmp_req), std::move(p->tmp_res)});
      });
      if (ADB_UNLIKELY(!queued)) {
        p->promise.setValue(Response{std::move(p->dest), fuerte::Error::QueueCapacityExceeded,
                                     std::move(p->tmp_req), nullptr});
      }
    });
    return f;

  } catch (std::exception const& e) {
    LOG_TOPIC("236d7", DEBUG, Logger::COMMUNICATION)
        << "failed to send request: " << e.what();
  } catch (...) {
    LOG_TOPIC("36d72", DEBUG, Logger::COMMUNICATION)
        << "failed to send request.";
  }
  return futures::makeFuture(
      Response{std::string(), Error::Canceled, nullptr, nullptr});
}

/// Stateful handler class with enough information to keep retrying
/// a request until an overall timeout is hit (or the request succeeds)
class RequestsState final : public std::enable_shared_from_this<RequestsState> {
 public:
  RequestsState(ConnectionPool* pool, DestinationId&& destination, RestVerb type,
                std::string&& path, velocypack::Buffer<uint8_t>&& payload,
                Headers&& headers, RequestOptions const& options)
      : _destination(std::move(destination)),
        _options(options),
        _pool(pool),
        _startTime(std::chrono::steady_clock::now()),
        _endTime(_startTime + std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                                  options.timeout)) {
    _tmp_req = prepareRequest(type, std::move(path), std::move(payload), _options, std::move(headers));
  }

  ~RequestsState() = default;

 private:
  DestinationId const _destination;
  RequestOptions const _options;
  ConnectionPool* _pool;

  std::shared_ptr<arangodb::Scheduler::WorkItem> _workItem;
  std::unique_ptr<fuerte::Request> _tmp_req;
  std::unique_ptr<fuerte::Response> _tmp_res;  /// temporary response

  futures::Promise<network::Response> _promise;  /// promise called

  std::chrono::steady_clock::time_point const _startTime;
  std::chrono::steady_clock::time_point const _endTime;

  fuerte::Error _tmp_err;

 public:
  FutureRes future() { return _promise.getFuture(); }

  // scheduler requests that are due
  void startRequest() {
    TRI_ASSERT(_tmp_req != nullptr);
    if (ADB_UNLIKELY(!_pool)) {
      LOG_TOPIC("5949f", ERR, Logger::COMMUNICATION)
          << "connection pool unavailable";
      _tmp_err = Error::Canceled;
      _tmp_res = nullptr;
      resolvePromise();
      return;
    }

    auto now = std::chrono::steady_clock::now();
    if (now > _endTime || _pool->config().clusterInfo->server().isStopping()) {
      _tmp_err = Error::Timeout;
      _tmp_res = nullptr;
      resolvePromise();
      return;  // we are done
    }

    arangodb::network::EndpointSpec spec;
    int res = resolveDestination(*_pool->config().clusterInfo, _destination, spec);
    if (res != TRI_ERROR_NO_ERROR) {  // ClusterInfo did not work
      // We fake a successful request with statusCode 503 and a backend not
      // available error here:
      _tmp_err = Error::NoError;
      _tmp_res = buildResponse(fuerte::StatusServiceUnavailable, Result{res});
      resolvePromise();
      return;
    }

    // simon: shorten actual request timeouts to allow time for retry
    //        otherwise resilience_failover tests likely fail
    auto t = _endTime - now;
    if (t >= std::chrono::duration<double>(100)) {
      t -= std::chrono::seconds(30);
    }
    TRI_ASSERT(t.count() > 0);
    _tmp_req->timeout(std::chrono::duration_cast<std::chrono::milliseconds>(t));

    auto conn = _pool->leaseConnection(spec.endpoint);
    conn->sendRequest(std::move(_tmp_req),
                      [self = shared_from_this()](fuerte::Error err,
                                                  std::unique_ptr<fuerte::Request> req,
                                                  std::unique_ptr<fuerte::Response> res) {
                        self->_tmp_err = err;
                        self->_tmp_req = std::move(req);
                        self->_tmp_res = std::move(res);
                        self->handleResponse();
                      });
  }

 private:
  void handleResponse() {
    switch (_tmp_err) {
      case fuerte::Error::NoError: {
        TRI_ASSERT(_tmp_res != nullptr);
        if (checkResponseContent()) {
          break;
        }
        [[fallthrough]]; // retry case
      }

      case fuerte::Error::CouldNotConnect:
      case fuerte::Error::ConnectionClosed:
      case fuerte::Error::Timeout:
      case fuerte::Error::Canceled: {
        // Note that this case includes the refusal of a leader to accept
        // the operation, in which case we have to flush ClusterInfo:

        auto const now = std::chrono::steady_clock::now();
        auto tryAgainAfter = now - _startTime;
        if (tryAgainAfter < std::chrono::milliseconds(200)) {
          tryAgainAfter = std::chrono::milliseconds(200);
        } else if (tryAgainAfter > std::chrono::seconds(3)) {
          tryAgainAfter = std::chrono::seconds(3);
        }

        // Now check if the request was directed to an explicit server and see if that
        // server is failed, if so, we should no longer retry, regardless of the timeout:
        bool found = false;
        if (_destination.size() > 7 && _destination.compare(0, 7, "server:") == 0) {
          auto failedServers = _pool->config().clusterInfo->getFailedServers();
          for (auto const& f : failedServers) {
            if (_destination.compare(7, _destination.size() - 7, f) == 0) {
              found = true;
              LOG_TOPIC("feade", DEBUG, Logger::COMMUNICATION)
                  << "Found destination "
                  << _destination << " to be in failed servers list, will no longer retry, aborting operation";
              break;
            }
          }
        }

        if (found || (now + tryAgainAfter) >= _endTime) {  // cancel out
          resolvePromise();
        } else {
          retryLater(tryAgainAfter);
        }
        break;
      }

      default:  // a "proper error" which has to be returned to the client
        resolvePromise();
        break;
    }
  }

  bool checkResponseContent() {
    TRI_ASSERT(_tmp_res != nullptr);
    switch (_tmp_res->statusCode()) {
      case fuerte::StatusOK:
      case fuerte::StatusCreated:
      case fuerte::StatusAccepted:
      case fuerte::StatusNoContent:
        _tmp_err = Error::NoError;
        resolvePromise();
        return true;  // done

      case fuerte::StatusServiceUnavailable:
        return false;  // goto retry

      case fuerte::StatusNotFound:
        if (_options.retryNotFound && TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND ==
                                          network::errorCodeFromBody(_tmp_res->slice())) {
          return false;  // goto retry
        }
        [[fallthrough]];
      default:  // a "proper error" which has to be returned to the client
        _tmp_err = Error::NoError;
        resolvePromise();
        return true;  // done
    }
  }

  /// @brief schedule calling the response promise
  void resolvePromise() {
    TRI_ASSERT(_tmp_req != nullptr);
    TRI_ASSERT(_tmp_res != nullptr || _tmp_err != Error::NoError);
    LOG_TOPIC_IF("2713e", DEBUG, Logger::COMMUNICATION, _tmp_err != fuerte::Error::NoError)
        << "error on request to '" << _destination << "' '"
        << fuerte::to_string(_tmp_req->type()) << " " << _tmp_req->header.path
        << "' '" << fuerte::to_string(_tmp_err) << "'";

    Scheduler* sch = SchedulerFeature::SCHEDULER;
    if (_options.skipScheduler || sch == nullptr) {
      _promise.setValue(Response{std::move(_destination), _tmp_err,
                                 std::move(_tmp_req), std::move(_tmp_res)});
      return;
    }

    bool queued =
        sch->queue(RequestLane::CLUSTER_INTERNAL, [self = shared_from_this()]() mutable {
          self->_promise.setValue(Response{std::move(self->_destination),
                                           self->_tmp_err, std::move(self->_tmp_req),
                                           std::move(self->_tmp_res)});
        });
    if (ADB_UNLIKELY(!queued)) {
      _promise.setValue(Response{std::move(_destination), fuerte::Error::QueueCapacityExceeded,
                                 std::move(_tmp_req), nullptr});
    }
  }

  void retryLater(std::chrono::steady_clock::duration tryAgainAfter) {
    TRI_ASSERT(_tmp_req != nullptr);
    LOG_TOPIC("2713f", DEBUG, Logger::COMMUNICATION)
        << "retry request to '" << _destination << "' '"
        << fuerte::to_string(_tmp_req->type()) << " " << _tmp_req->header.path << "'";

    auto* sch = SchedulerFeature::SCHEDULER;
    if (ADB_UNLIKELY(sch == nullptr)) {
      _promise.setValue(Response{std::move(_destination),
                                 fuerte::Error::Canceled, nullptr, nullptr});
      return;
    }

    bool queued;
    std::tie(queued, _workItem) =
        sch->queueDelay(RequestLane::CLUSTER_INTERNAL, tryAgainAfter,
                        [self = shared_from_this()](bool canceled) {
                          if (canceled) {
                            self->_promise.setValue(
                                Response{std::move(self->_destination),
                                         Error::Canceled, nullptr, nullptr});
                          } else {
                            self->startRequest();
                          }
                        });
    if (ADB_UNLIKELY(!queued)) {
      // scheduler queue is full, cannot requeue
      _promise.setValue(Response{std::move(_destination),
                                 Error::QueueCapacityExceeded, nullptr, nullptr});
    }
  }
};

/// @brief send a request to a given destination, retry until timeout is exceeded
FutureRes sendRequestRetry(ConnectionPool* pool, DestinationId destination,
                           arangodb::fuerte::RestVerb type, std::string path,
                           velocypack::Buffer<uint8_t> payload,
                           RequestOptions const& options, Headers headers) {
  try {
    if (!pool || !pool->config().clusterInfo) {
      LOG_TOPIC("59b96", ERR, Logger::COMMUNICATION)
          << "connection pool unavailable";
      return futures::makeFuture(
          Response{destination, Error::Canceled, nullptr, nullptr});
    }

    LOG_TOPIC("2713b", DEBUG, Logger::COMMUNICATION)
        << "request to '" << destination << "' '" << fuerte::to_string(type)
        << " " << path << "'";

    auto rs = std::make_shared<RequestsState>(pool, std::move(destination), type,
                                              std::move(path), std::move(payload),
                                              std::move(headers), options);
    rs->startRequest();  // will auto reference itself
    return rs->future();

  } catch (std::exception const& e) {
    LOG_TOPIC("6d723", DEBUG, Logger::COMMUNICATION)
        << "failed to send request: " << e.what();
  } catch (...) {
    LOG_TOPIC("d7236", DEBUG, Logger::COMMUNICATION)
        << "failed to send request.";
  }


  return futures::makeFuture(
      Response{std::string(), Error::Canceled, nullptr, nullptr});
}

}  // namespace network
}  // namespace arangodb

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

  req->timeout(std::chrono::duration_cast<std::chrono::milliseconds>(options.timeout));

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

/// @brief send a request to a given destination
FutureRes sendRequest(ConnectionPool* pool, DestinationId dest, RestVerb type,
                      std::string path, velocypack::Buffer<uint8_t> payload,
                      RequestOptions const& options, Headers headers) {
  // FIXME build future.reset(..)

  if (!pool || !pool->config().clusterInfo) {
    LOG_TOPIC("59b95", ERR, Logger::COMMUNICATION)
        << "connection pool unavailable";
    return futures::makeFuture(Response{std::move(dest), Error::Canceled, nullptr});
  }
  
  LOG_TOPIC("2713a", DEBUG, Logger::COMMUNICATION)
      << "request to '" << dest
      << "' '" << fuerte::to_string(type) << " " << path << "'";

  arangodb::network::EndpointSpec spec;
  int res = resolveDestination(*pool->config().clusterInfo, dest, spec);
  if (res != TRI_ERROR_NO_ERROR) {  // FIXME return an error  ?!
    return futures::makeFuture(Response{std::move(dest), Error::Canceled, nullptr});
  }
  TRI_ASSERT(!spec.endpoint.empty());

  auto req = prepareRequest(type, std::move(path), std::move(payload),
                            options, std::move(headers));
  struct Pack {
    DestinationId dest;
    futures::Promise<network::Response> promise;
    std::unique_ptr<fuerte::Response> tmp;
    bool skipScheduler;
    Pack(DestinationId&& dest, bool skip)
        : dest(std::move(dest)), promise(), skipScheduler(skip) {}
  };
  // fits in SSO of std::function
  static_assert(sizeof(std::shared_ptr<Pack>) <= 2 * sizeof(void*), "");
  auto conn = pool->leaseConnection(spec.endpoint);
  auto p = std::make_shared<Pack>(std::move(dest), options.skipScheduler);

  FutureRes f = p->promise.getFuture();
  conn->sendRequest(std::move(req), [p(std::move(p))](fuerte::Error err,
                                                      std::unique_ptr<fuerte::Request> req,
                                                      std::unique_ptr<fuerte::Response> res) {
    Scheduler* sch = SchedulerFeature::SCHEDULER;
    if (p->skipScheduler || sch == nullptr) {
      p->promise.setValue(network::Response{std::move(p->dest), err, std::move(res)});
      return;
    }

    p->tmp = std::move(res);

    bool queued = sch->queue(RequestLane::CLUSTER_INTERNAL, [p, err]() {
      p->promise.setValue(Response{std::move(p->dest), err, std::move(p->tmp)});
    });
    if (ADB_UNLIKELY(!queued)) {
      p->promise.setValue(Response{std::move(p->dest), fuerte::Error::Canceled, nullptr});
    }
  });
  return f;
}

/// Handler class with enough information to keep retrying
/// a request until an overall timeout is hit (or the request succeeds)
class RequestsState final : public std::enable_shared_from_this<RequestsState> {
 public:
  RequestsState(ConnectionPool* pool, DestinationId&& destination, RestVerb type,
                std::string&& path, velocypack::Buffer<uint8_t>&& payload,
                Headers&& headers, RequestOptions const& options)
      : _payload(std::move(payload)),
        _destination(std::move(destination)),
        _path(std::move(path)),
        _headers(std::move(headers)),
        _options(options),
        _pool(pool),
        _type(type),
        _workItem(nullptr),
        _response(nullptr),
        _promise(),
        _startTime(std::chrono::steady_clock::now()),
        _endTime(_startTime + std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                                  options.timeout)) {}

  ~RequestsState() = default;

 private:
  velocypack::Buffer<uint8_t> _payload;
  DestinationId _destination;
  std::string _path;
  Headers _headers;
  RequestOptions const _options;
  ConnectionPool* _pool;
  RestVerb _type;

  std::shared_ptr<arangodb::Scheduler::WorkItem> _workItem;
  std::unique_ptr<fuerte::Response> _response;   /// temporary response
  futures::Promise<network::Response> _promise;  /// promise called

  std::chrono::steady_clock::time_point const _startTime;
  std::chrono::steady_clock::time_point const _endTime;

 public:
  FutureRes future() { return _promise.getFuture(); }

  // scheduler requests that are due
  void startRequest() {
    auto now = std::chrono::steady_clock::now();
    if (now > _endTime || _pool->config().clusterInfo->server().isStopping()) {
      callResponse(Error::Timeout, nullptr);
      return;  // we are done
    }

    arangodb::network::EndpointSpec spec;
    int res = resolveDestination(*_pool->config().clusterInfo, _destination, spec);
    if (res != TRI_ERROR_NO_ERROR) {  // ClusterInfo did not work
      callResponse(Error::Canceled, nullptr);
      return;
    }

    if (!_pool) {
      LOG_TOPIC("5949f", ERR, Logger::COMMUNICATION)
          << "connection pool unavailable";
      callResponse(Error::Canceled, nullptr);
      return;
    }

    auto localOptions = _options;
    localOptions.timeout =
        std::chrono::duration_cast<std::chrono::milliseconds>(_endTime - now);
    TRI_ASSERT(localOptions.timeout.count() > 0);
    
    auto conn = _pool->leaseConnection(spec.endpoint);
    auto req = prepareRequest(_type, _path, _payload, localOptions, _headers);
    conn->sendRequest(std::move(req),
                      [self = shared_from_this()](fuerte::Error err,
                                                  std::unique_ptr<fuerte::Request> req,
                                                  std::unique_ptr<fuerte::Response> res) {
                        self->handleResponse(err, std::move(req), std::move(res));
                      });
  }

 private:
  void handleResponse(fuerte::Error err, std::unique_ptr<fuerte::Request> req,
                      std::unique_ptr<fuerte::Response> res) {
    switch (err) {
      case fuerte::Error::NoError: {
        TRI_ASSERT(res);
        if (checkResponse(err, req, res)) {
          break;
        }
        [[fallthrough]];
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

        if ((now + tryAgainAfter) >= _endTime) {  // cancel out
          callResponse(err, std::move(res));
        } else {
          retryLater(tryAgainAfter);
        }
        break;
      }

      default:  // a "proper error" which has to be returned to the client
        callResponse(err, std::move(res));
        break;
    }
  }
  
  bool checkResponse(fuerte::Error err,
                     std::unique_ptr<fuerte::Request>& req,
                     std::unique_ptr<fuerte::Response>& res) {
    switch (res->statusCode()) {
      case fuerte::StatusOK:
      case fuerte::StatusCreated:
      case fuerte::StatusAccepted:
      case fuerte::StatusNoContent:
        callResponse(Error::NoError, std::move(res));
        return true; // done
        
      case fuerte::StatusUnavailable:
        return false; // goto retry
      
      case fuerte::StatusNotFound:
        if (_options.retryNotFound &&
            TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND == network::errorCodeFromBody(res->slice())) {
          return false; // goto retry
        }
        [[fallthrough]];
      default:  // a "proper error" which has to be returned to the client
        callResponse(err, std::move(res));
        return true; // done
    }
  }

  /// @broef schedule calling the response promise
  void callResponse(Error err, std::unique_ptr<fuerte::Response> res) {
    
    LOG_TOPIC_IF("2713d", DEBUG, Logger::COMMUNICATION, err != fuerte::Error::NoError)
        << "error on request to '" << _destination
        << "' '" << fuerte::to_string(_type) << " " << _path
        << "' '" << fuerte::to_string(err) << "'";
    
    Scheduler* sch = SchedulerFeature::SCHEDULER;
    if (_options.skipScheduler || sch == nullptr) {
      _promise.setValue(Response{std::move(_destination), err, std::move(res)});
      return;
    }

    _response = std::move(res);
    bool queued =
        sch->queue(RequestLane::CLUSTER_INTERNAL, [self = shared_from_this(), err]() {
          self->_promise.setValue(Response{std::move(self->_destination), err,
                                           std::move(self->_response)});
        });
    if (ADB_UNLIKELY(!queued)) {
      _promise.setValue(Response{std::move(_destination), fuerte::Error::QueueCapacityExceeded, nullptr});
    }
  }

  void retryLater(std::chrono::steady_clock::duration tryAgainAfter) {
    
    LOG_TOPIC("2713e", DEBUG, Logger::COMMUNICATION)
        << "retry request to '" << _destination
        << "' '" << fuerte::to_string(_type) << " " << _path << "'";
    
    auto* sch = SchedulerFeature::SCHEDULER;
    if (ADB_UNLIKELY(sch == nullptr)) {
      _promise.setValue(Response{std::move(_destination), fuerte::Error::Canceled, nullptr});
      return;
    }
    
    bool queued;
    std::tie(queued, _workItem) =
        sch->queueDelay(RequestLane::CLUSTER_INTERNAL, tryAgainAfter,
                        [self = shared_from_this()](bool canceled) {
          if (canceled) {
            self->_promise.setValue(Response{self->_destination, Error::Canceled, nullptr});
          } else {
            self->startRequest();
          }
        });
    if (ADB_UNLIKELY(!queued)) {
      // scheduler queue is full, cannot requeue
      _promise.setValue(Response{std::move(_destination), Error::QueueCapacityExceeded, nullptr});
    }
  }
};

/// @brief send a request to a given destination, retry until timeout is exceeded
FutureRes sendRequestRetry(ConnectionPool* pool, DestinationId destination,
                           arangodb::fuerte::RestVerb type, std::string path,
                           velocypack::Buffer<uint8_t> payload,
                           RequestOptions const& options,
                           Headers headers) {
  if (!pool || !pool->config().clusterInfo) {
    LOG_TOPIC("59b96", ERR, Logger::COMMUNICATION)
        << "connection pool unavailable";
    return futures::makeFuture(Response{destination, Error::Canceled, nullptr});
  }
  
  LOG_TOPIC("2713b", DEBUG, Logger::COMMUNICATION)
      << "request to '" << destination
      << "' '" << fuerte::to_string(type) << " " << path << "'";

  //  auto req = prepareRequest(type, path, std::move(payload), timeout, headers);
  auto rs = std::make_shared<RequestsState>(pool, std::move(destination),
                                            type, std::move(path),
                                            std::move(payload),
                                            std::move(headers), options);
  rs->startRequest();  // will auto reference itself
  return rs->future();
}

}  // namespace network
}  // namespace arangodb

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
#include "Cluster/ServerState.h"
#include "Futures/Utilities.h"
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

template <typename T>
auto prepareRequest(RestVerb type, std::string const& path, T&& payload,
                    Timeout timeout, Headers const& headers) {
  fuerte::StringMap params;  // intentionally empty
  auto req = fuerte::createRequest(type, path, params, std::forward<T>(payload));
  req->header.parseArangoPath(path);  // strips /_db/<name>/
  if (req->header.database.empty()) {
    req->header.database = StaticStrings::SystemDatabase;
  }
  req->header.addMeta(headers);

  TRI_voc_tick_t timeStamp = TRI_HybridLogicalClock();
  req->header.addMeta(StaticStrings::HLCHeader,
                      arangodb::basics::HybridLogicalClock::encodeTimeStamp(timeStamp));

  req->timeout(std::chrono::duration_cast<std::chrono::milliseconds>(timeout));

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
FutureRes sendRequest(DestinationId const& destination, RestVerb type,
                      std::string const& path, velocypack::Buffer<uint8_t> payload,
                      Timeout timeout, Headers const& headers) {
  // FIXME build future.reset(..)

  ConnectionPool* pool = NetworkFeature::pool();
  if (!pool) {
    LOG_TOPIC("59b95", ERR, Logger::COMMUNICATION) << "connection pool unavailable";
    return futures::makeFuture(
        Response{destination, Error::Canceled, nullptr});
  }

  arangodb::network::EndpointSpec endpoint;
  int res = resolveDestination(destination, endpoint);
  if (res != TRI_ERROR_NO_ERROR) {  // FIXME return an error  ?!
    return futures::makeFuture(
        Response{destination, Error::Canceled, nullptr});
  }
  TRI_ASSERT(!endpoint.empty());

  auto req = prepareRequest(type, path, std::move(payload), timeout, headers);

  // fits in SSO of std::function
  struct Pack {
    DestinationId destination;
    ConnectionPool::Ref ref;
    futures::Promise<network::Response> promise;
    Pack(DestinationId const& dest, ConnectionPool::Ref r)
    : destination(dest), ref(std::move(r)), promise() {}
  };
  static_assert(sizeof(std::shared_ptr<Pack>) <= 2*sizeof(void*), "does not fit in sfo");
  auto p = std::make_shared<Pack>(destination, pool->leaseConnection(endpoint));

  auto conn = p->ref.connection();
  auto f = p->promise.getFuture();
  conn->sendRequest(std::move(req), [p = std::move(p)](fuerte::Error err,
                                                       std::unique_ptr<fuerte::Request> req,
                                                       std::unique_ptr<fuerte::Response> res) {
    p->promise.setValue(network::Response{p->destination, err, std::move(res)});
  });
  return f;
}

/// Handler class with enough information to keep retrying
/// a request until an overall timeout is hit (or the request succeeds)
class RequestsState : public std::enable_shared_from_this<RequestsState> {
 public:
  RequestsState(DestinationId const& destination, RestVerb type,
                std::string const& path, velocypack::Buffer<uint8_t>&& payload,
                Timeout timeout, Headers const& headers, bool retryNotFound)
      : _destination(destination),
        _type(type),
        _path(path),
        _payload(std::move(payload)),
        _headers(headers),
        _workItem(nullptr),
        _promise(),
        _startTime(std::chrono::steady_clock::now()),
        _endTime(_startTime +
                 std::chrono::duration_cast<std::chrono::steady_clock::duration>(timeout)),
        _retryOnCollNotFound(retryNotFound) {}

 private:
  DestinationId _destination;
  RestVerb _type;
  std::string _path;
  velocypack::Buffer<uint8_t> _payload;
  Headers _headers;
  std::shared_ptr<arangodb::Scheduler::WorkItem> _workItem;
  futures::Promise<network::Response> _promise;
  
  std::chrono::steady_clock::time_point const _startTime;
  std::chrono::steady_clock::time_point const _endTime;
  const bool _retryOnCollNotFound;

 public:
  
  FutureRes future() {
    return _promise.getFuture();
  }
  
  // scheduler requests that are due
  void startRequest() {
    auto now = std::chrono::steady_clock::now();
    if (now > _endTime || application_features::ApplicationServer::isStopping()) {
      callResponse(Error::Timeout, nullptr);
      return;  // we are done
    }

    arangodb::network::EndpointSpec endpoint;
    int res = resolveDestination(_destination, endpoint);
    if (res != TRI_ERROR_NO_ERROR) {  // ClusterInfo did not work
      callResponse(Error::Canceled, nullptr);
      return;
    }

    ConnectionPool* pool = NetworkFeature::pool();
    if (!pool) {
      LOG_TOPIC("5949f", ERR, Logger::COMMUNICATION) << "connection pool unavailable";
      callResponse(Error::Canceled, nullptr);
      return;
    }
    
    auto localTO = std::chrono::duration_cast<std::chrono::milliseconds>(_endTime - now);
    TRI_ASSERT(localTO.count() > 0);

    auto ref = pool->leaseConnection(endpoint);
    auto req = prepareRequest(_type, _path, _payload, localTO, _headers);
    auto self = RequestsState::shared_from_this();
    auto cb = [self, ref](fuerte::Error err,
                          std::unique_ptr<fuerte::Request> req,
                          std::unique_ptr<fuerte::Response> res) {
      self->handleResponse(err, std::move(req), std::move(res));
    };
    ref.connection()->sendRequest(std::move(req), std::move(cb));
  }

 private:
  void handleResponse(fuerte::Error err, std::unique_ptr<fuerte::Request> req,
                      std::unique_ptr<fuerte::Response> res) {
    switch (err) {
      case fuerte::Error::NoError: {
        TRI_ASSERT(res);
        if (res->statusCode() == fuerte::StatusOK ||
            res->statusCode() == fuerte::StatusCreated ||
            res->statusCode() == fuerte::StatusAccepted ||
            res->statusCode() == fuerte::StatusNoContent) {
          callResponse(Error::NoError, std::move(res));
          break;
        } else if (res->statusCode() == fuerte::StatusNotFound && _retryOnCollNotFound &&
                   TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND ==
                       network::errorCodeFromBody(res->slice())) {
          LOG_TOPIC("5a8e9", DEBUG, Logger::COMMUNICATION) << "retrying request";
        } else {
          LOG_TOPIC("5a8d9", DEBUG, Logger::COMMUNICATION) << "canceling request";
          callResponse(Error::Canceled, std::move(res));
          break;
        }
#ifndef _MSC_VER
        [[fallthrough]];
#endif
      }

      case fuerte::Error::CouldNotConnect:
      case fuerte::Error::Timeout: {
        // Note that this case includes the refusal of a leader to accept
        // the operation, in which case we have to flush ClusterInfo:

        auto const now = std::chrono::steady_clock::now();
        auto tryAgainAfter = now - _startTime;
        if (tryAgainAfter < std::chrono::milliseconds(200)) {
          tryAgainAfter = std::chrono::milliseconds(200);
        } else if (tryAgainAfter > std::chrono::seconds(3)) {
          tryAgainAfter = std::chrono::seconds(3);
        }
        if ((now + tryAgainAfter) >= _endTime) { // cancel out
          callResponse(err, std::move(res));
          break;
        }

        auto* sch = SchedulerFeature::SCHEDULER;
        auto self = RequestsState::shared_from_this();
        auto cb = [self](bool canceled) {
          if (canceled) {
            self->callResponse(Error::Canceled, nullptr);
          } else {
            self->startRequest();
          }
        };
        _workItem = sch->queueDelay(RequestLane::CLUSTER_INTERNAL,
                                    tryAgainAfter, std::move(cb));
        break;
      }

      default:  // a "proper error" which has to be returned to the client
        callResponse(err, std::move(res));
        break;
    }
  }
  
  
  void callResponse(Error err,
                    std::unique_ptr<fuerte::Response> res) {
    _promise.setValue(Response{_destination, err, std::move(res)});
  }
};

/// @brief send a request to a given destination, retry until timeout is exceeded
FutureRes sendRequestRetry(DestinationId const& destination,
                           arangodb::fuerte::RestVerb type, std::string const& path,
                           velocypack::Buffer<uint8_t> payload, Timeout timeout,
                           Headers const& headers, bool retryNotFound) {

  //  auto req = prepareRequest(type, path, std::move(payload), timeout, headers);
  auto rs = std::make_shared<RequestsState>(destination, type, path,
                                            std::move(payload), timeout,
                                            headers, retryNotFound);
  rs->startRequest();  // will auto reference itself
  return rs->future();
}

}  // namespace network
}  // namespace arangodb

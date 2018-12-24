////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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

namespace arangodb {
namespace network {
using namespace arangodb::fuerte;
  
void prepareRequest(fuerte::Request& req) {
  
  TRI_voc_tick_t timeStamp = TRI_HybridLogicalClock();
  req.header.addMeta(StaticStrings::HLCHeader,
                     arangodb::basics::HybridLogicalClock::encodeTimeStamp(timeStamp));

  
  auto state = ServerState::instance();
  if (state->isCoordinator() || state->isDBServer()) {
    req.header.addMeta(StaticStrings::ClusterCommSource, state->getId());
  } else if (state->isAgent()) {
    auto agent = AgencyFeature::AGENT;
    if (agent != nullptr) {
      req.header.addMeta(StaticStrings::ClusterCommSource, "AGENT-" + agent->id());
    }
  }
  
#ifdef DEBUG_CLUSTER_COMM
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
#if ARANGODB_ENABLE_BACKTRACE
  std::string bt;
  TRI_GetBacktrace(bt);
  std::replace(bt.begin(), bt.end(), '\n', ';');  // replace all '\n' to ';'
  headersCopy["X-Arango-BT-SYNC"] = bt;
#endif
#endif
#endif
}

FutureRes sendRequest(DestinationId const& destination, RestVerb type,
                      std::string const& path, velocypack::Buffer<uint8_t> payload,
                      Timeout timeout, Headers const& headers) {
  // FIXME build future.reset(..)
  
  ConnectionPool* pool = NetworkFeature::pool();
  if (!pool) {
    LOG_TOPIC(ERR, Logger::FIXME) << "connection pool unavailble";
    return futures::makeFuture(Response{destination, errorToInt(ErrorCondition::Canceled), nullptr});
  }
  
  arangodb::network::EndpointSpec endpoint;
  Result res = resolveDestination(destination, endpoint);
  if (!res.ok()) { // FIXME return an error  ?!
    return futures::makeFuture(Response{destination, errorToInt(ErrorCondition::Canceled), nullptr});
  }
  TRI_ASSERT(!endpoint.empty());

  fuerte::StringMap params;
  auto req = fuerte::createRequest(type, path, params, std::move(payload));
  req->header.parseArangoPath(path); // strips /_db/<name>/
  if (req->header.database.empty()) {
    req->header.database = StaticStrings::SystemDatabase;
  }
  req->header.addMeta(headers);
  prepareRequest(*req);
  
  // FIXME this is really ugly
  auto promise = std::make_shared<futures::Promise<network::Response>>();
  auto f = promise->getFuture();
  
  ConnectionPool::Ref ref = pool->leaseConnection(endpoint);
  auto conn = ref.connection();
  conn->sendRequest(std::move(req), [destination, ref = std::move(ref),
                                     promise = std::move(promise)](fuerte::Error err,
                                           std::unique_ptr<fuerte::Request> req,
                                           std::unique_ptr<fuerte::Response> res) {
    promise->setValue(network::Response{destination, err, std::move(res)});
  });
  return f;
}
  
FutureRes sendRequest(DestinationId const& destination, arangodb::fuerte::RestVerb type,
                      std::string const& path, arangodb::velocypack::Slice payload,
                      Timeout timeout, Headers const& headers) {
  ConnectionPool* pool = NetworkFeature::pool();
  if (!pool) {
    LOG_TOPIC(ERR, Logger::FIXME) << "connection pool unavailble";
    return futures::makeFuture(Response{destination, errorToInt(ErrorCondition::Canceled), nullptr});
  }
  
  arangodb::network::EndpointSpec endpoint;
  Result res = resolveDestination(destination, endpoint);
  if (!res.ok()) { // FIXME return an error  ?!
    return futures::makeFuture(Response{destination, errorToInt(ErrorCondition::Canceled), nullptr});
  }
  TRI_ASSERT(!endpoint.empty());
  
  fuerte::StringMap params;
  auto req = fuerte::createRequest(type, path, params, std::move(payload));
  req->header.parseArangoPath(path); // strips /_db/<name>/
  if (req->header.database.empty()) {
    req->header.database = StaticStrings::SystemDatabase;
  }
  req->header.addMeta(headers);
  prepareRequest(*req);
  
  // FIXME this is really ugly
  auto promise = std::make_shared<futures::Promise<network::Response>>();
  auto f = promise->getFuture();
  
  ConnectionPool::Ref ref = pool->leaseConnection(endpoint);
  auto conn = ref.connection();
  conn->sendRequest(std::move(req), [destination, ref = std::move(ref),
                                     promise = std::move(promise)](fuerte::Error err,
                                                                   std::unique_ptr<fuerte::Request> req,
                                                                   std::unique_ptr<fuerte::Response> res) {
    promise->setValue(network::Response{destination, err, std::move(res)});
  });
  return f;
}

}} // arangodb::network

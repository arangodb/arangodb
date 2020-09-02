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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "Queries.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Aql/Query.h"
#include "Aql/QueryExecutionState.h"
#include "Aql/QueryList.h"
#include "Auth/TokenCache.h"
#include "Basics/Common.h"
#include "Basics/Exceptions.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ServerState.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "Network/NetworkFeature.h"
#include "Network/Methods.h"
#include "Network/Utils.h"
#include "VocBase/vocbase.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::methods;

namespace {

network::Headers buildHeaders() {
  auto auth = AuthenticationFeature::instance();

  network::Headers headers;
  if (auth != nullptr && auth->isActive()) {
    headers.try_emplace(StaticStrings::Authorization,
                        "bearer " + auth->tokenCache().jwtToken());
  }
  return headers;
}

/// @brief return the list of currently running queries
void getQueries(TRI_vocbase_t& vocbase, std::vector<aql::QueryEntryCopy> const& queries,
                velocypack::Builder& out, char const* action, bool fanout) {
  out.openArray();
  
  // local case
  for (auto const& q : queries) {
    q.toVelocyPack(out);
  }

  if (ServerState::instance()->isCoordinator() && fanout) {
    // coordinator case, fan out to other coordinators!
    NetworkFeature const& nf = vocbase.server().getFeature<NetworkFeature>();
    network::ConnectionPool* pool = nf.pool();
    if (pool == nullptr) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_SHUTTING_DOWN);
    }

    std::vector<network::FutureRes> futures;

    network::RequestOptions options;
    options.timeout = network::Timeout(30.0);
    options.database = vocbase.name();
    options.param("local", "true");
    
    std::string const url = std::string("/_api/query/") + action;

    auto& ci = vocbase.server().getFeature<ClusterFeature>().clusterInfo();
    for (auto const& coordinator : ci.getCurrentCoordinators()) {
      if (coordinator == ServerState::instance()->getId()) {
        // ourselves!
        continue;
      }

      auto f = network::sendRequest(pool, "server:" + coordinator, fuerte::RestVerb::Get,
                                    url, VPackBuffer<uint8_t>{}, options, buildHeaders());
      futures.emplace_back(std::move(f));
    }

    if (!futures.empty()) {
      auto responses = futures::collectAll(futures).get();
      for (auto const& it : responses) {
        if (!it.hasValue()) {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_CLUSTER_BACKEND_UNAVAILABLE);
        }
        auto& resp = it.get();
        if (resp.response && resp.response->statusCode() == fuerte::StatusOK) {
          auto slice = resp.response->slice();
          // copy results from other coordinators
          if (slice.isArray()) {
            for (auto const& entry : VPackArrayIterator(slice)) {
              out.add(entry);
            }
          }
        } else if (resp.response) {
          THROW_ARANGO_EXCEPTION(network::resultFromBody(resp.response->slice(), TRI_ERROR_FAILED));
        }
      }
    }
  } 
  
  out.close();
}

} // namespace
  
/// @brief return the list of slow queries
void Queries::listSlow(TRI_vocbase_t& vocbase, velocypack::Builder& out, bool fanout) {
  getQueries(vocbase, vocbase.queryList()->listSlow(), out, "slow", fanout);
}

/// @brief return the list of current queries
void Queries::listCurrent(TRI_vocbase_t& vocbase, velocypack::Builder& out, bool fanout) {
  getQueries(vocbase, vocbase.queryList()->listCurrent(), out, "current", fanout);
}
  
/// @brief clears the list of slow queries
void Queries::clearSlow(TRI_vocbase_t& vocbase, bool fanout) {
  vocbase.queryList()->clearSlow();

  if (ServerState::instance()->isCoordinator() && fanout) {
    // coordinator case, fan out to other coordinators!
    NetworkFeature const& nf = vocbase.server().getFeature<NetworkFeature>();
    network::ConnectionPool* pool = nf.pool();
    if (pool == nullptr) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_SHUTTING_DOWN);
    }

    std::vector<network::FutureRes> futures;

    network::RequestOptions options;
    options.timeout = network::Timeout(30.0);
    options.database = vocbase.name();
    options.param("local", "true");

    VPackBuffer<uint8_t> body;

    auto& ci = vocbase.server().getFeature<ClusterFeature>().clusterInfo();
    for (auto const& coordinator : ci.getCurrentCoordinators()) {
      if (coordinator == ServerState::instance()->getId()) {
        // ourselves!
        continue;
      }

      auto f = network::sendRequest(pool, "server:" + coordinator, fuerte::RestVerb::Delete,
                                    "/_api/query/slow", body, options, buildHeaders());
      futures.emplace_back(std::move(f));
    }

    if (!futures.empty()) {
      auto responses = futures::collectAll(futures).get();
      for (auto const& it : responses) {
        if (!it.hasValue()) {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_CLUSTER_BACKEND_UNAVAILABLE);
        }
        auto& resp = it.get();
        if (resp.response && resp.response->statusCode() != fuerte::StatusOK) {
          THROW_ARANGO_EXCEPTION(network::resultFromBody(resp.response->slice(), TRI_ERROR_FAILED));
        }
      }
    }
  } 
}

/// @brief kills the given query
Result Queries::kill(TRI_vocbase_t& vocbase, TRI_voc_tick_t id) {
  return vocbase.queryList()->kill(id);
}

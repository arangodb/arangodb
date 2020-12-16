////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
#include "RestServer/DatabaseFeature.h"
#include "Utils/ExecContext.h"
#include "VocBase/vocbase.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/StringRef.h>
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

arangodb::Result checkAuthorization(TRI_vocbase_t& vocbase, bool allDatabases) {
  Result res;

  if (allDatabases) {
    // list of queries requested for _all_ databases
    if (!vocbase.isSystem()) {
      // request must be made in the system database
      res.reset(TRI_ERROR_ARANGO_USE_SYSTEM_DATABASE);
    } else if (ExecContext::isAuthEnabled() &&
               !ExecContext::current().isSuperuser()) {
      // request must be made only by superusers (except authentication is turned off)
      res.reset(TRI_ERROR_FORBIDDEN,
                "only superusers are allowed to perform actions on all queries");
    }
  }

  return res;
}

/// @brief return the list of currently running or slow queries
arangodb::Result getQueries(TRI_vocbase_t& vocbase,
                            velocypack::Builder& out, 
                            arangodb::velocypack::StringRef action, 
                            bool allDatabases, 
                            bool fanout) {
  Result res = checkAuthorization(vocbase, allDatabases);
  
  if (res.fail()) {
    return res;
  }

  arangodb::DatabaseFeature& databaseFeature = vocbase.server().getFeature<DatabaseFeature>();

  std::vector<arangodb::aql::QueryEntryCopy> queries;
  
  // local case
  if (action == "slow") {
    if (allDatabases) {
      databaseFeature.enumerate([&queries](TRI_vocbase_t* vocbase) {
        auto forDatabase = vocbase->queryList()->listSlow();
        queries.reserve(queries.size() + forDatabase.size());
        std::move(forDatabase.begin(), forDatabase.end(), std::back_inserter(queries));
      });
    } else {
      queries = vocbase.queryList()->listSlow();
    }
  } else {
    if (allDatabases) {
      databaseFeature.enumerate([&queries](TRI_vocbase_t* vocbase) {
        auto forDatabase = vocbase->queryList()->listCurrent();
        queries.reserve(queries.size() + forDatabase.size());
        std::move(forDatabase.begin(), forDatabase.end(), std::back_inserter(queries));
      });
    } else {
      queries = vocbase.queryList()->listCurrent();
    }
  }

  // build the result
  out.openArray();
  
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
    options.param("all", allDatabases ? "true" : "false");

    VPackBuffer<uint8_t> body;
    
    std::string const url = std::string("/_api/query/") + action.toString();

    auto& ci = vocbase.server().getFeature<ClusterFeature>().clusterInfo();
    for (auto const& coordinator : ci.getCurrentCoordinators()) {
      if (coordinator == ServerState::instance()->getId()) {
        // ourselves!
        continue;
      }

      auto f = network::sendRequest(pool, "server:" + coordinator, fuerte::RestVerb::Get,
                                    url, body, options, buildHeaders());
      futures.emplace_back(std::move(f));
    }

    if (!futures.empty()) {
      auto responses = futures::collectAll(futures).get();
      for (auto const& it : responses) {
        auto& resp = it.get();
        res.reset(resp.combinedResult());
        if (res.is(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND)) {
          // it is expected in a multi-coordinator setup that a coordinator is not
          // aware of a database that was created very recently.
          res.reset();
        }
        if (res.fail()) {
          break;
        }
        if (resp.statusCode() == fuerte::StatusOK) {
          auto slice = resp.response->slice();
          // copy results from other coordinators
          if (slice.isArray()) {
            for (auto const& entry : VPackArrayIterator(slice)) {
              out.add(entry);
            }
          }
        }
      }
    }
  } 
  
  out.close();

  return res;
}

} // namespace
  
/// @brief return the list of slow queries
Result Queries::listSlow(TRI_vocbase_t& vocbase, velocypack::Builder& out, bool allDatabases, bool fanout) {
  return getQueries(vocbase, out, arangodb::velocypack::StringRef("slow"), allDatabases, fanout);
}

/// @brief return the list of current queries
Result Queries::listCurrent(TRI_vocbase_t& vocbase, velocypack::Builder& out, bool allDatabases, bool fanout) {
  return getQueries(vocbase, out, arangodb::velocypack::StringRef("current"), allDatabases, fanout);
}
  
/// @brief clears the list of slow queries
Result Queries::clearSlow(TRI_vocbase_t& vocbase, bool allDatabases, bool fanout) {
  Result res;

  if (allDatabases) {
    // list of queries requested for _all_ databases
    if (!vocbase.isSystem()) {
      // request must be made in the system database
      return res.reset(TRI_ERROR_ARANGO_USE_SYSTEM_DATABASE);
    }
    if (ExecContext::isAuthEnabled() &&
        !ExecContext::current().isSuperuser()) {
      // request must be made only by superusers (except authentication is turned off)
      return res.reset(TRI_ERROR_FORBIDDEN,
                       "only superusers may retrieve the list of queries for all databases");
    }
      
    arangodb::DatabaseFeature& databaseFeature = vocbase.server().getFeature<DatabaseFeature>();
    databaseFeature.enumerate([](TRI_vocbase_t* vocbase) {
      vocbase->queryList()->clearSlow();
    });
  } else {
    vocbase.queryList()->clearSlow();
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
    options.param("all", allDatabases ? "true" : "false");

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
        auto& resp = it.get();
        res.reset(resp.combinedResult());
        if (res.is(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND)) {
          // it is expected in a multi-coordinator setup that a coordinator is not
          // aware of a database that was created very recently.
          res.reset();
        }
        if (res.fail()) {
          break;
        }
      }
    }
  }

  return res;
}

/// @brief kills the given query
Result Queries::kill(TRI_vocbase_t& vocbase, TRI_voc_tick_t id, bool allDatabases) {
  Result res = checkAuthorization(vocbase, allDatabases);
  
  if (res.ok()) {
    if (allDatabases) {
      arangodb::DatabaseFeature& databaseFeature = vocbase.server().getFeature<DatabaseFeature>();
      bool found = false;
      databaseFeature.enumerate([id, &res, &found](TRI_vocbase_t* vocbase) {
        res = vocbase->queryList()->kill(id);
        if (res.ok()) {
          // unfortunately there is no way to stop the iteration once we found the query.
          found = true;
        }
      });
      if (found) {
        // we found the query in question. so we now clear the errors we potentially
        // got from other databases that did not have the query
        res.reset();
      }
    } else {
      res.reset(vocbase.queryList()->kill(id));
    }
  }

  return res;
}

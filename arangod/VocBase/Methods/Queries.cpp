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
////////////////////////////////////////////////////////////////////////////////

#include "Queries.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Aql/Query.h"
#include "Aql/QueryExecutionState.h"
#include "Aql/QueryList.h"
#include "Auth/TokenCache.h"
#include "Basics/Exceptions.h"
#include "Basics/StaticStrings.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ServerState.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "Network/NetworkFeature.h"
#include "Network/Methods.h"
#include "Network/Utils.h"
#include "RestServer/DatabaseFeature.h"
#include "Ssl/jwt.h"
#include "Utils/ExecContext.h"
#include "VocBase/vocbase.h"

#include <absl/strings/str_cat.h>
#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>

#include <functional>
#include <ranges>
#include <string>
#include <unordered_map>

using namespace arangodb;
using namespace arangodb::methods;

namespace {
enum class QueriesMode { Current, Slow };

/**
 * Remembers per query owner whether the calling identity may access that
 * owner's queries
 *
 * ExecContext::canAccessQuery() is asked at most once per owner, so a list
 * with many queries of the same user asks a single question only.
 */
class QueryAccess {
 public:
  /// @brief whether the calling identity may see, kill or clear this entry
  auto isAccessible(velocypack::Slice const entry) -> bool {
    auto const user = entry.get("user");
    auto const owner = user.isString() ? user.copyString() : std::string{};
    if (auto const known = _decisions.find(owner); known != _decisions.end()) {
      return known->second;
    }
    auto const accessible = ExecContext::current().canAccessQuery(owner).ok();
    _decisions.emplace(owner, accessible);
    return accessible;
  }

 private:
  std::unordered_map<std::string, bool> _decisions;
};

/**
 * Headers for fanning a request out to the other coordinators
 *
 * The caller's identity is forwarded as a user JWT, the same kind of token
 * /_open/auth hands out, so the other coordinators apply the same per-user
 * scoping in every auth mode. Without a user name (superuser JWT,
 * authentication off) no header is set and the connection pool's superuser
 * token is used.
 */
auto fanoutHeaders() -> network::Headers {
  auto const* auth = AuthenticationFeature::instance();
  auto const& context = ExecContext::current();
  if (auth == nullptr || !auth->isActive() || context.user().empty()) {
    return {};
  }
  return {{StaticStrings::Authorization,
           "bearer " + auth::generateUserToken(auth->tokenCache().jwtSecret(),
                                               context.user())}};
}

/**
 * Outcome of a request that was fanned out to another coordinator
 *
 * A coordinator that does not know the database yet (it was created very
 * recently) is not an error: it simply has no queries to contribute.
 */
auto coordinatorResult(network::Response const& response) -> Result {
  auto result = response.combinedResult();
  if (result.is(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND)) {
    return {};
  }
  return result;
}

/**
 * Actions on the queries of all databases require the _system database and
 * superuser privileges
 */
auto checkAllDatabasesAuthorization(TRI_vocbase_t const& vocbase) -> Result {
  if (!vocbase.isSystem()) {
    // request must be made in the system database
    return {TRI_ERROR_ARANGO_USE_SYSTEM_DATABASE};
  }
  if (!ExecContext::current().isSuperuserOrDisabled()) {
    // request must be made only by superusers
    return {TRI_ERROR_FORBIDDEN,
            "only superusers are allowed to perform actions on all queries"};
  }
  return {};
}

/// @brief return the list of currently running or slow queries
arangodb::Result getQueries(TRI_vocbase_t& vocbase, velocypack::Builder& out,
                            QueriesMode mode, bool allDatabases, bool fanout) {
  if (allDatabases) {
    if (auto const res = checkAllDatabasesAuthorization(vocbase); res.fail()) {
      return res;
    }
  }

  TRI_ASSERT(mode == QueriesMode::Slow || mode == QueriesMode::Current);

  arangodb::DatabaseFeature& databaseFeature =
      vocbase.server().getFeature<DatabaseFeature>();

  std::vector<std::shared_ptr<velocypack::String>> queries;

  // local case
  if (mode == QueriesMode::Slow) {
    // slow queries
    if (allDatabases) {
      databaseFeature.enumerate([&queries](TRI_vocbase_t* vocbase) {
        auto forDatabase = vocbase->queryList()->listSlow();
        queries.reserve(queries.size() + forDatabase.size());
        std::move(forDatabase.begin(), forDatabase.end(),
                  std::back_inserter(queries));
      });
    } else {
      queries = vocbase.queryList()->listSlow();
    }
  } else {
    // currently running queries
    TRI_ASSERT(mode == QueriesMode::Current);

    if (allDatabases) {
      databaseFeature.enumerate([&queries](TRI_vocbase_t* vocbase) {
        auto forDatabase = vocbase->queryList()->listCurrent();
        queries.reserve(queries.size() + forDatabase.size());
        std::move(forDatabase.begin(), forDatabase.end(),
                  std::back_inserter(queries));
      });
    } else {
      queries = vocbase.queryList()->listCurrent();
    }
  }

  // build the result, containing only the queries the caller may see
  QueryAccess access;
  auto const isAccessible = [&access](velocypack::Slice const entry) {
    return access.isAccessible(entry);
  };
  auto const toSlice = [](std::shared_ptr<velocypack::String> const& query) {
    return query->slice();
  };

  VPackArrayBuilder resultArray(&out);

  for (auto const entry : queries | std::views::transform(toSlice) |
                              std::views::filter(isAccessible)) {
    out.add(entry);
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

    auto url = absl::StrCat("/_api/query/",
                            (mode == QueriesMode::Slow ? "slow" : "current"));
    auto const headers = fanoutHeaders();

    auto& ci = vocbase.server().getFeature<ClusterFeature>().clusterInfo();
    for (auto const& coordinator : ci.getCurrentCoordinators()) {
      if (coordinator == ServerState::instance()->getId()) {
        // ourselves!
        continue;
      }

      auto f = network::sendRequestRetry(
          pool, "server:" + coordinator, fuerte::RestVerb::Get, url,
          VPackBuffer<uint8_t>{}, options, headers);
      futures.emplace_back(std::move(f));
    }

    if (!futures.empty()) {
      auto responses = futures::collectAll(futures).waitAndGet();
      for (auto const& it : responses) {
        auto& resp = it.get();
        if (auto const result = coordinatorResult(resp); result.fail()) {
          return result;
        }
        auto slice = resp.slice();
        // copy results from other coordinators. they already scope their
        // answer to the calling user (see fanoutHeaders), so filtering again
        // here is only a safety net for coordinators that do not scope, e.g.
        // in a mixed-version cluster during a rolling upgrade
        if (slice.isArray()) {
          for (auto const entry :
               VPackArrayIterator(slice) | std::views::filter(isAccessible)) {
            out.add(entry);
          }
        }
      }
    }
  }

  return {};
}

}  // namespace

/// @brief return the list of slow queries
Result Queries::listSlow(TRI_vocbase_t& vocbase, velocypack::Builder& out,
                         bool allDatabases, bool fanout) {
  return getQueries(vocbase, out, QueriesMode::Slow, allDatabases, fanout);
}

/// @brief return the list of current queries
Result Queries::listCurrent(TRI_vocbase_t& vocbase, velocypack::Builder& out,
                            bool allDatabases, bool fanout) {
  return getQueries(vocbase, out, QueriesMode::Current, allDatabases, fanout);
}

/// @brief clears the slow queries the caller may access
Result Queries::clearSlow(TRI_vocbase_t& vocbase, bool allDatabases,
                          bool fanout) {
  QueryAccess access;
  auto const shouldClear = [&access](velocypack::Slice const entry) {
    return access.isAccessible(entry);
  };

  if (allDatabases) {
    if (auto const res = checkAllDatabasesAuthorization(vocbase); res.fail()) {
      return res;
    }

    arangodb::DatabaseFeature& databaseFeature =
        vocbase.server().getFeature<DatabaseFeature>();
    databaseFeature.enumerate([&shouldClear](TRI_vocbase_t* database) {
      database->queryList()->clearSlow(shouldClear);
    });
  } else {
    vocbase.queryList()->clearSlow(shouldClear);
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
    auto const headers = fanoutHeaders();

    auto& ci = vocbase.server().getFeature<ClusterFeature>().clusterInfo();
    for (auto const& coordinator : ci.getCurrentCoordinators()) {
      if (coordinator == ServerState::instance()->getId()) {
        // ourselves!
        continue;
      }

      auto f = network::sendRequestRetry(
          pool, "server:" + coordinator, fuerte::RestVerb::Delete,
          "/_api/query/slow", body, options, headers);
      futures.emplace_back(std::move(f));
    }

    if (!futures.empty()) {
      auto responses = futures::collectAll(futures).waitAndGet();
      for (auto const& it : responses) {
        if (auto const result = coordinatorResult(it.get()); result.fail()) {
          return result;
        }
      }
    }
  }

  return {};
}

/// @brief kills the given query if the caller is its owner or an admin
Result Queries::kill(TRI_vocbase_t& vocbase, TRI_voc_tick_t id,
                     bool allDatabases) {
  if (allDatabases) {
    if (auto const res = checkAllDatabasesAuthorization(vocbase); res.fail()) {
      return res;
    }
  }

  auto const authorize = [](aql::Query const& query) {
    return ExecContext::current().canAccessQuery(query.user());
  };

  if (!allDatabases) {
    return vocbase.queryList()->kill(id, authorize);
  }

  // unfortunately there is no way to stop the enumeration once we found the
  // query, so only a result from the database that has the query is kept
  Result outcome{TRI_ERROR_QUERY_NOT_FOUND, "query ID not found in query list"};
  vocbase.server().getFeature<DatabaseFeature>().enumerate(
      [id, &authorize, &outcome](TRI_vocbase_t* database) {
        if (auto const result = database->queryList()->kill(id, authorize);
            !result.is(TRI_ERROR_QUERY_NOT_FOUND)) {
          outcome = result;
        }
      });
  return outcome;
}

/// @brief kills the given query on behalf of the server itself
///
/// Internal use only (e.g. when a participating DB server is gone), hence
/// intentionally not scoped to a user.
Result Queries::kill(DatabaseFeature& df, std::string const& databaseName,
                     TRI_voc_tick_t id) {
  auto vocbase = df.useDatabase(databaseName);
  if (!vocbase) {
    return {TRI_ERROR_ARANGO_DATABASE_NOT_FOUND};
  }
  return vocbase->queryList()->kill(id);
}

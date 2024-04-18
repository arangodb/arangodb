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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "QueryList.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Aql/Query.h"
#include "Aql/QueryExecutionState.h"
#include "Aql/QueryInfoLoggerFeature.h"
#include "Aql/QueryProfile.h"
#include "Basics/ErrorCode.h"
#include "Basics/Exceptions.h"
#include "Basics/ReadLocker.h"
#include "Basics/Result.h"
#include "Basics/WriteLocker.h"
#include "Containers/SmallVector.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "RestServer/QueryRegistryFeature.h"
#include "VocBase/vocbase.h"

#include <absl/strings/str_cat.h>
#include <velocypack/Builder.h>
#include <velocypack/Dumper.h>
#include <velocypack/Sink.h>
#include <velocypack/Slice.h>
#include <velocypack/Value.h>

#include <algorithm>
#include <cmath>
#include <memory>

using namespace arangodb;
using namespace arangodb::aql;

/// @brief create a query list
QueryList::QueryList(QueryRegistryFeature& feature)
    : _queryRegistryFeature(feature),
      _enabled(feature.trackingEnabled()),
      _trackSlowQueries(_enabled && feature.trackSlowQueries()),
      _trackQueryString(feature.trackQueryString()),
      _trackBindVars(feature.trackBindVars()),
      _trackDataSources(feature.trackDataSources()),
      _slowQueryThreshold(feature.slowQueryThreshold()),
      _slowStreamingQueryThreshold(feature.slowStreamingQueryThreshold()),
      _maxSlowQueries(kDefaultMaxSlowQueries),
      _maxQueryStringLength(feature.maxQueryStringLength()) {
  _current.reserve(32);
}

/// @brief whether or not queries are tracked
bool QueryList::enabled() const noexcept {
  return _enabled.load(std::memory_order_relaxed);
}

/// @brief toggle query tracking
void QueryList::enabled(bool value) noexcept {
  _enabled.store(value, std::memory_order_relaxed);
}

/// @brief whether or not slow queries are tracked
bool QueryList::trackSlowQueries() const noexcept {
  return _trackSlowQueries.load(std::memory_order_relaxed);
}

/// @brief toggle slow query tracking
void QueryList::trackSlowQueries(bool value) noexcept {
  _trackSlowQueries.store(value, std::memory_order_relaxed);
}

/// @brief whether to track the full query string
bool QueryList::trackQueryString() const noexcept {
  return _trackQueryString.load(std::memory_order_relaxed);
}

/// @brief toggle slow query tracking
void QueryList::trackQueryString(bool value) noexcept {
  _trackQueryString.store(value, std::memory_order_relaxed);
}

/// @brief whether or not bind vars are tracked with queries
bool QueryList::trackBindVars() const noexcept {
  return _trackBindVars.load(std::memory_order_relaxed);
}

/// @brief toggle query bind vars tracking
void QueryList::trackBindVars(bool value) noexcept {
  _trackBindVars.store(value, std::memory_order_relaxed);
}

/// @brief whether or not data sources are tracked with queries
bool QueryList::trackDataSources() const noexcept {
  return _trackDataSources.load(std::memory_order_relaxed);
}

/// @brief threshold for slow queries (in seconds)
double QueryList::slowQueryThreshold() const noexcept {
  return _slowQueryThreshold.load(std::memory_order_relaxed);
}

/// @brief set the slow query threshold
void QueryList::slowQueryThreshold(double value) noexcept {
  if (value < 0.0 || value == HUGE_VAL || value != value) {
    // only let useful values pass
    value = 0.0;
  }
  _slowQueryThreshold.store(value, std::memory_order_relaxed);
}

/// @brief threshold for slow streaming queries (in seconds)
double QueryList::slowStreamingQueryThreshold() const noexcept {
  return _slowStreamingQueryThreshold.load(std::memory_order_relaxed);
}

/// @brief set the slow streaming query threshold
void QueryList::slowStreamingQueryThreshold(double value) noexcept {
  if (value < 0.0 || value == HUGE_VAL || value != value) {
    // basic checks
    value = 0.0;
  }
  _slowStreamingQueryThreshold.store(value, std::memory_order_relaxed);
}

/// @brief return the max number of slow queries to keep
size_t QueryList::maxSlowQueries() const noexcept {
  return _maxSlowQueries.load(std::memory_order_relaxed);
}

/// @brief set the max number of slow queries to keep
void QueryList::maxSlowQueries(size_t value) noexcept {
  _maxSlowQueries.store(std::min(value, size_t(16384)),
                        std::memory_order_relaxed);
}

/// @brief return the max length of query strings that are stored / returned
size_t QueryList::maxQueryStringLength() const noexcept {
  return _maxQueryStringLength.load(std::memory_order_relaxed);
}

/// @brief set the max length of query strings that are stored / returned
void QueryList::maxQueryStringLength(size_t value) noexcept {
  _maxQueryStringLength.store(std::clamp<size_t>(value, 64, 32 * 1024 * 1024),
                              std::memory_order_relaxed);
}

/// @brief insert a query
bool QueryList::insert(Query& query) {
  // not enabled or no query string
  if (!enabled() || query.queryString().empty()) {
    return false;
  }

  try {
    WRITE_LOCKER(writeLocker, _lock);

    TRI_IF_FAILURE("QueryList::insert") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }

    // return whether or not insertion worked
    bool inserted =
        _current.insert({query.id(), query.weak_from_this()}).second;

    writeLocker.unlock();

    _queryRegistryFeature.trackQueryStart();
    return inserted;
  } catch (...) {
    return false;
  }
}

/// @brief remove a query
void QueryList::remove(Query& query) {
  // we're intentionally not checking _enabled here...

  // note: there is the possibility that a query got inserted when the
  // tracking was turned on, but is going to be removed when the tracking
  // is turned off. in this case, removal is forced so the contents of
  // the list are correct

  TRI_ASSERT(!query.queryString().empty());

  {
    // acquire the query list's write lock only for a short amount of
    // time. if we need to insert a slow query later, we will re-acquire
    // the lock. but the hope is that for the majority of queries this is
    // not required
    WRITE_LOCKER(writeLocker, _lock);

    if (_current.erase(query.id()) == 0) {
      // not found
      return;
    }
  }

  // elapsed time since query start
  double elapsed = query.executionTime();

  _queryRegistryFeature.trackQueryEnd(elapsed);

  if (query.queryOptions().skipAudit) {
    // internal queries that are excluded from audit logging will not be
    // logged here as slow queries, and will not be inserted into the
    // _queries system collection
    return;
  }

  // building the query slice is expensive. only do it if we actually need
  // to log the query.
  std::shared_ptr<velocypack::String> querySlice;
  auto buildQuerySlice = [&query, &querySlice, this]() {
    if (querySlice == nullptr) {
      Query::QuerySerializationOptions options{
          .includeUser = true,
          .includeQueryString = trackQueryString(),
          .includeBindParameters = trackBindVars(),
          .includeDataSources = trackDataSources(),
          // always true because the query is already finalized
          .includeResultCode = true,
          .queryStringMaxLength = maxQueryStringLength(),
      };

      velocypack::Builder builder;
      query.toVelocyPack(builder, /*isCurrent*/ false, options);

      querySlice = std::make_shared<velocypack::String>(builder.slice());
    }
    return querySlice;
  };

  double threshold =
      query.queryOptions().stream
          ? _slowStreamingQueryThreshold.load(std::memory_order_relaxed)
          : _slowQueryThreshold.load(std::memory_order_relaxed);

  // check if we need to push the query into the list of slow queries
  bool isSlowQuery =
      (trackSlowQueries() && elapsed >= threshold && threshold >= 0.0);
  if (isSlowQuery) {
    // yes.
    try {
      TRI_IF_FAILURE("QueryList::remove") {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
      }

      _queryRegistryFeature.trackSlowQuery(elapsed);

      Query::QuerySerializationOptions options{
          .includeUser = true,
          .includeQueryString = trackQueryString(),
          .includeBindParameters = trackBindVars(),
          .includeDataSources = trackDataSources(),
          // always true because the query is already finalized
          .includeResultCode = true,
          .queryStringMaxLength = maxQueryStringLength(),
      };

      query.logSlow(options);

      {
        // acquire the query list lock again
        WRITE_LOCKER(writeLocker, _lock);

        _slow.emplace_back(buildQuerySlice());

        if (_slow.size() > _maxSlowQueries) {
          // list is full. free first element
          _slow.pop_front();
        }
      }  // release lock so that Builder can be destroyed without
         // holding on to the lock
    } catch (...) {
    }
  }

  if (query.vocbase().server().hasFeature<QueryInfoLoggerFeature>()) {
    auto& qilf = query.vocbase().server().getFeature<QueryInfoLoggerFeature>();

    // building the query slice is expensive. only do it if we actually need
    // to log the query.
    if (qilf.shouldLog(query.vocbase().isSystem(), isSlowQuery)) {
      qilf.log(buildQuerySlice());
    }
  }
}

/// @brief kills a query
Result QueryList::kill(TRI_voc_tick_t id) {
  // We must not call the std::shared_ptr<Query> dtor under the `_lock`,
  // because this can result in a deadlock since the Query dtor tries to
  // remove itself from this list which acquires the write `_lock`
  std::shared_ptr<Query> queryPtr;
  {
    READ_LOCKER(writeLocker, _lock);
    auto it = _current.find(id);
    if (it == _current.end()) {
      return {TRI_ERROR_QUERY_NOT_FOUND, "query ID not found in query list"};
    }
    queryPtr = it->second.lock();
  }
  if (queryPtr) {
    auto const length = _maxQueryStringLength.load(std::memory_order_relaxed);
    killQuery(*queryPtr, length, false);
  }
  return {};
}

/// @brief kills all currently running queries that match the filter function
/// (i.e. the filter should return true for a queries to be killed)
uint64_t QueryList::kill(std::function<bool(Query&)> const& filter,
                         bool silent) {
  // We must not call the std::shared_ptr<Query> dtor under the `_lock`,
  // because this can result in a deadlock since the Query dtor tries to
  // remove itself from this list which acquires the write `_lock`
  containers::SmallVector<std::shared_ptr<Query>, 16> queries;
  {
    READ_LOCKER(readLocker, _lock);
    queries.reserve(_current.size());
    for (auto& it : _current) {
      if (auto queryPtr = it.second.lock(); queryPtr) {
        queries.push_back(std::move(queryPtr));
      }
    }
  }
  uint64_t killed = 0;
  auto const maxLength = _maxQueryStringLength.load(std::memory_order_relaxed);
  for (auto const& queryPtr : queries) {
    if (auto& query = *queryPtr; filter(query)) {
      killQuery(query, maxLength, silent);
      ++killed;
    }
  }
  return killed;
}

/// @brief get the list of currently running queries
std::vector<std::shared_ptr<velocypack::String>> QueryList::listCurrent()
    const {
  // We must not call the std::shared_ptr<Query> dtor under the `_lock`,
  // because this can result in a deadlock since the Query dtor tries to
  // remove itself from this list which acquires the write `_lock`
  constexpr size_t n = 16;
  containers::SmallVector<std::shared_ptr<Query>, n> queries;
  std::vector<std::shared_ptr<velocypack::String>> result;
  // reserve room for some queries outside of the lock already,
  // so we reduce the possibility of having to reserve more room later
  queries.reserve(n);
  result.reserve(n);

  Query::QuerySerializationOptions options{
      .includeUser = true,
      .includeQueryString = trackQueryString(),
      .includeBindParameters = trackBindVars(),
      .includeDataSources = trackDataSources(),
      // always false because we are returning only currently running queries
      .includeResultCode = false,
      .queryStringMaxLength = maxQueryStringLength(),
  };

  velocypack::Builder builder;

  {
    READ_LOCKER(readLocker, _lock);

    // reserve the actually needed space
    result.reserve(_current.size());
    queries.reserve(_current.size());

    for (auto const& it : _current) {
      auto queryPtr = it.second.lock();
      if (!queryPtr) {
        continue;
      }
      // should not fail as we have reserved enough space above
      TRI_ASSERT(queries.capacity() > queries.size());
      queries.push_back(std::move(queryPtr));

      // stringify current query into Builder
      builder.clear();
      queries.back()->toVelocyPack(builder, /*isCurrent*/ true, options);
      TRI_ASSERT(builder.slice().isObject());

      // copy Builder's contents into result
      result.emplace_back(
          std::make_shared<velocypack::String>(builder.slice()));
    }
  }

  return result;
}

/// @brief get the list of slow queries
std::vector<std::shared_ptr<velocypack::String>> QueryList::listSlow() const {
  std::vector<std::shared_ptr<velocypack::String>> result;

  READ_LOCKER(readLocker, _lock);
  // reserve the actually needed space
  result.reserve(_slow.size());

  for (auto const& it : _slow) {
    result.emplace_back(it);
  }

  return result;
}

/// @brief clear the list of slow queries
void QueryList::clearSlow() {
  WRITE_LOCKER(writeLocker, _lock);
  _slow.clear();
}

size_t QueryList::count() const {
  READ_LOCKER(writeLocker, _lock);
  return _current.size();
}

void QueryList::killQuery(Query& query, size_t maxLength, bool silent) {
  // build message only if we need to
  auto buildMessage = [&]() {
    return absl::StrCat("killing AQL query '",
                        query.extractQueryString(maxLength, trackQueryString()),
                        "', id: ", query.id(), ", token: QRY", query.id());
  };

  if (silent) {
    LOG_TOPIC("f7722", TRACE, arangodb::Logger::QUERIES) << buildMessage();
  } else {
    LOG_TOPIC("90113", WARN, arangodb::Logger::QUERIES) << buildMessage();
  }

  query.kill();
}

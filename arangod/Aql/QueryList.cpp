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
#include "Aql/QueryProfile.h"
#include "Aql/Timing.h"
#include "Basics/Exceptions.h"
#include "Basics/ReadLocker.h"
#include "Basics/ResourceUsage.h"
#include "Basics/Result.h"
#include "Basics/StringUtils.h"
#include "Basics/WriteLocker.h"
#include "Basics/conversions.h"
#include "Basics/system-functions.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "RestServer/QueryRegistryFeature.h"
#include "VocBase/vocbase.h"

#include <velocypack/Builder.h>
#include <velocypack/Dumper.h>
#include <velocypack/Sink.h>
#include <velocypack/Value.h>

#include "Containers/SmallVector.h"

using namespace arangodb;
using namespace arangodb::aql;

QueryEntryCopy::QueryEntryCopy(
    TRI_voc_tick_t id, std::string const& database, std::string const& user,
    std::string&& queryString,
    std::shared_ptr<arangodb::velocypack::Builder> const& bindParameters,
    std::vector<std::string> dataSources, double started, double runTime,
    size_t peakMemoryUsage, QueryExecutionState::ValueType state, bool stream,
    std::optional<ErrorCode> resultCode)
    : id(id),
      database(database),
      user(user),
      queryString(std::move(queryString)),
      bindParameters(bindParameters),
      dataSources(std::move(dataSources)),
      started(started),
      runTime(runTime),
      peakMemoryUsage(peakMemoryUsage),
      state(state),
      resultCode(resultCode),
      stream(stream) {}

void QueryEntryCopy::toVelocyPack(velocypack::Builder& out) const {
  auto timeString = TRI_StringTimeStamp(started, Logger::getUseLocalTime());

  out.add(VPackValue(VPackValueType::Object));
  out.add("id", VPackValue(basics::StringUtils::itoa(id)));
  out.add("database", VPackValue(database));
  out.add("user", VPackValue(user));
  out.add("query", VPackValue(queryString));
  if (bindParameters != nullptr && !bindParameters->slice().isNone()) {
    out.add("bindVars", bindParameters->slice());
  } else {
    out.add("bindVars", arangodb::velocypack::Slice::emptyObjectSlice());
  }
  if (!dataSources.empty()) {
    out.add("dataSources", VPackValue(VPackValueType::Array));
    for (auto const& dn : dataSources) {
      out.add(VPackValue(dn));
    }
    out.close();
  }
  out.add("started", VPackValue(timeString));
  out.add("runTime", VPackValue(runTime));
  out.add("peakMemoryUsage", VPackValue(peakMemoryUsage));
  out.add("state", VPackValue(aql::QueryExecutionState::toString(state)));
  out.add("stream", VPackValue(stream));
  if (resultCode.has_value()) {
    // exit code can only be determined if query is fully finished
    out.add("exitCode", VPackValue(*resultCode));
  }
  out.close();
}

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
      _maxSlowQueries(defaultMaxSlowQueries),
      _maxQueryStringLength(feature.maxQueryStringLength()) {
  _current.reserve(32);
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
  double const elapsed = query.executionTime();

  _queryRegistryFeature.trackQueryEnd(elapsed);

  if (!trackSlowQueries()) {
    return;
  }

  bool const isStreaming = query.queryOptions().stream;
  double threshold =
      isStreaming ? _slowStreamingQueryThreshold.load(std::memory_order_relaxed)
                  : _slowQueryThreshold.load(std::memory_order_relaxed);

  // check if we need to push the query into the list of slow queries
  if (elapsed >= threshold && threshold >= 0.0) {
    // yes.
    try {
      TRI_IF_FAILURE("QueryList::remove") {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
      }

      _queryRegistryFeature.trackSlowQuery(elapsed);

      // we calculate the query start timestamp as the current time minus
      // the elapsed time since query start. this is not 100% accurrate, but
      // best effort, and saves us from bookkeeping the start timestamp of the
      // query inside the Query object.
      double const now = TRI_microtime();
      size_t const maxQueryStringLength =
          _maxQueryStringLength.load(std::memory_order_relaxed);

      std::string q =
          query.extractQueryString(maxQueryStringLength, trackQueryString());
      std::string bindParameters;
      if (_trackBindVars) {
        // also log bind variables
        query.stringifyBindParameters(bindParameters,
                                      ", bind vars: ", maxQueryStringLength);
      }

      std::string dataSources;
      if (_trackDataSources) {
        query.stringifyDataSources(dataSources, ", data sources: ");
      }

      auto resultCode = query.resultCode();

      LOG_TOPIC("8bcee", WARN, Logger::QUERIES)
          << "slow " << (isStreaming ? "streaming " : "") << "query: '" << q
          << "'" << bindParameters << dataSources
          << ", database: " << query.vocbase().name()
          << ", user: " << query.user() << ", id: " << query.id()
          << ", token: QRY" << query.id()
          << ", peak memory usage: " << query.resourceMonitor().peak()
          << ", exit code: " << resultCode
          << ", took: " << Logger::FIXED(elapsed) << " s";

      // acquire the query list lock again
      WRITE_LOCKER(writeLocker, _lock);

      _slow.emplace_back(
          query.id(), query.vocbase().name(), query.user(), std::move(q),
          _trackBindVars ? query.bindParametersAsBuilder() : nullptr,
          _trackDataSources ? query.collectionNames()
                            : std::vector<std::string>(),
          now - elapsed, /* start timestamp */
          elapsed /* run time */, query.resourceMonitor().peak(),
          query.killed() ? QueryExecutionState::ValueType::KILLED
                         : QueryExecutionState::ValueType::FINISHED,
          isStreaming, resultCode);

      // _slow is an std::list, but since c++11 the size() method of all
      // standard containers is O(1), so this is ok
      if (_slow.size() > _maxSlowQueries) {
        // free first element
        _slow.pop_front();
      }
    } catch (...) {
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
std::vector<QueryEntryCopy> QueryList::listCurrent() {
  // We must not call the std::shared_ptr<Query> dtor under the `_lock`,
  // because this can result in a deadlock since the Query dtor tries to
  // remove itself from this list which acquires the write `_lock`
  containers::SmallVector<std::shared_ptr<Query>, 16> queries;
  std::vector<QueryEntryCopy> result;
  // reserve room for some queries outside of the lock already,
  // so we reduce the possibility of having to reserve more room later
  result.reserve(16);

  auto const maxLength = _maxQueryStringLength.load(std::memory_order_relaxed);
  auto showQueryString = trackQueryString();
  double const now = TRI_microtime();

  {
    READ_LOCKER(readLocker, _lock);
    // reserve the actually needed space
    //
    result.reserve(_current.size());
    queries.reserve(_current.size());
    for (auto const& it : _current) {
      auto queryPtr = it.second.lock();
      if (!queryPtr) {
        continue;
      }
      queries.push_back(std::move(queryPtr));
      auto& query = *queries.back();

      // elapsed time since query start
      double const elapsed = elapsedSince(query.startTime());

      // we calculate the query start timestamp as the current time minus
      // the elapsed time since query start. this is not 100% accurrate, but
      // best effort, and saves us from bookkeeping the start timestamp of the
      // query inside the Query object.
      result.emplace_back(
          query.id(), query.vocbase().name(), query.user(),
          query.extractQueryString(maxLength, showQueryString),
          _trackBindVars ? query.bindParametersAsBuilder() : nullptr,
          _trackDataSources ? query.collectionNames()
                            : std::vector<std::string>(),
          now - elapsed /* start timestamp */, elapsed /* run time */,
          query.resourceMonitor().peak(),
          query.killed() ? QueryExecutionState::ValueType::KILLED
                         : query.state(),
          query.queryOptions().stream,
          /*resultCode*/ std::nullopt /*not set yet*/);
    }
  }

  return result;
}

/// @brief get the list of slow queries
std::vector<QueryEntryCopy> QueryList::listSlow() {
  std::vector<QueryEntryCopy> result;
  // reserve room for some queries outside of the lock already,
  // so we reduce the possibility of having to reserve more room
  // later
  result.reserve(16);

  {
    READ_LOCKER(readLocker, _lock);
    // reserve the actually needed space
    result.reserve(_slow.size());

    for (auto const& it : _slow) {
      result.emplace_back(it);
    }
  }

  return result;
}

/// @brief clear the list of slow queries
void QueryList::clearSlow() {
  WRITE_LOCKER(writeLocker, _lock);
  _slow.clear();
}

size_t QueryList::count() {
  READ_LOCKER(writeLocker, _lock);
  return _current.size();
}

void QueryList::killQuery(Query& query, size_t maxLength, bool silent) {
  std::string msg = "killing AQL query '" +
                    query.extractQueryString(maxLength, trackQueryString()) +
                    "', id: " + std::to_string(query.id()) + ", token: QRY" +
                    std::to_string(query.id());

  if (silent) {
    LOG_TOPIC("f7722", TRACE, arangodb::Logger::QUERIES) << msg;
  } else {
    LOG_TOPIC("90113", WARN, arangodb::Logger::QUERIES) << msg;
  }

  query.kill();
}

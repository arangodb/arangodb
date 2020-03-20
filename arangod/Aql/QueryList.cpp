////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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

#include "QueryList.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Aql/Query.h"
#include "Aql/QueryProfile.h"
#include "Basics/Exceptions.h"
#include "Basics/ReadLocker.h"
#include "Basics/Result.h"
#include "Basics/StringUtils.h"
#include "Basics/WriteLocker.h"
#include "Basics/conversions.h"
#include "Basics/system-functions.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "RestServer/MetricsFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "VocBase/vocbase.h"

#include <velocypack/Builder.h>
#include <velocypack/StringRef.h>
#include <velocypack/Value.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::aql;

QueryEntryCopy::QueryEntryCopy(TRI_voc_tick_t id, std::string&& queryString,
                               std::shared_ptr<arangodb::velocypack::Builder> const& bindParameters,
                               double started, double runTime,
                               QueryExecutionState::ValueType state, bool stream)
    : id(id),
      queryString(std::move(queryString)),
      bindParameters(bindParameters),
      started(started),
      runTime(runTime),
      state(state),
      stream(stream) {}

void QueryEntryCopy::toVelocyPack(velocypack::Builder& out) const {
  auto timeString = TRI_StringTimeStamp(started, Logger::getUseLocalTime());

  out.add(VPackValue(VPackValueType::Object));
  out.add("id", VPackValue(basics::StringUtils::itoa(id)));
  out.add("query", VPackValue(queryString));
  if (bindParameters != nullptr) {
    out.add("bindVars", bindParameters->slice());
  } else {
    out.add("bindVars", arangodb::velocypack::Slice::emptyObjectSlice());
  }
  out.add("started", VPackValue(timeString));
  out.add("runTime", VPackValue(runTime));
  out.add("state", VPackValue(aql::QueryExecutionState::toString(state)));
  out.add("stream", VPackValue(stream));
  out.close();
}

/// @brief create a query list
QueryList::QueryList(QueryRegistryFeature& feature, TRI_vocbase_t*)
    : _lock(),
      _current(),
      _slow(),
      _slowCount(0),
      _enabled(feature.trackSlowQueries()),
      _trackSlowQueries(feature.trackSlowQueries()),
      _trackBindVars(feature.trackBindVars()),
      _slowQueryThreshold(feature.slowQueryThreshold()),
      _slowStreamingQueryThreshold(feature.slowStreamingQueryThreshold()),
      _maxSlowQueries(defaultMaxSlowQueries),
      _maxQueryStringLength(defaultMaxQueryStringLength) {
  _current.reserve(64);
}

/// @brief insert a query
bool QueryList::insert(Query* query) {
  // not enable or no query string
  if (!_enabled || query == nullptr || query->queryString().empty()) {
    return false;
  }

  try {
    WRITE_LOCKER(writeLocker, _lock);

    TRI_IF_FAILURE("QueryList::insert") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }

    auto it = _current.insert({query->id(), query});
    if (it.second) {
      return true;
    }
  } catch (...) {
  }

  return false;
}

/// @brief remove a query
void QueryList::remove(Query* query) {
  // we're intentionally not checking _enabled here...

  // note: there is the possibility that a query got inserted when the
  // tracking was turned on, but is going to be removed when the tracking
  // is turned off. in this case, removal is forced so the contents of
  // the list are correct

  // no query string
  if (query == nullptr || query->queryString().empty()) {
    return;
  }

  WRITE_LOCKER(writeLocker, _lock);

  if (_current.erase(query->id()) == 0) {
    // not found
    return;
  }

  bool const isStreaming = query->queryOptions().stream;
  double threshold = (isStreaming ? _slowStreamingQueryThreshold : _slowQueryThreshold);

  if (!_trackSlowQueries || threshold < 0.0) {
    return;
  }

  double const started = query->startTime();
  double const now = TRI_microtime();

  query->vocbase().server().getFeature<arangodb::MetricsFeature>().counter(
      StaticStrings::AqlQueryRuntimeMs) += static_cast<uint64_t>(1000 * (now - started));

  try {
    // check if we need to push the query into the list of slow queries
    if (now - started >= threshold && !query->killed()) {
      // yes.

      TRI_IF_FAILURE("QueryList::remove") {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
      }

      std::string q = extractQueryString(query, _maxQueryStringLength);

      QueryProfile* profile = query->profile();
      double loadTime = 0.0;

      if (profile != nullptr) {
        loadTime = profile->timer(QueryExecutionState::ValueType::LOADING_COLLECTIONS);
      }

      std::string bindParameters;
      if (_trackBindVars) {
        // also log bind variables
        auto bp = query->bindParameters();
        if (bp != nullptr) {
          bindParameters.append(", bind vars: ");
          bindParameters.append(bp->slice().toJson());
          if (bindParameters.size() > _maxQueryStringLength) {
            bindParameters.resize(_maxQueryStringLength - 3);
            bindParameters.append("...");
          }
        }
      }

      if (loadTime >= 0.1) {
        LOG_TOPIC("d728e", WARN, Logger::QUERIES)
            << "slow " << (isStreaming ? "streaming " : "") << "query: '" << q
            << "'" << bindParameters << ", took: " << Logger::FIXED(now - started)
            << " s, loading took: " << Logger::FIXED(loadTime) << " s";
      } else {
        LOG_TOPIC("8bcee", WARN, Logger::QUERIES)
            << "slow " << (isStreaming ? "streaming " : "") << "query: '" << q << "'"
            << bindParameters << ", took: " << Logger::FIXED(now - started) << " s";
      }

      _slow.emplace_back(query->id(), std::move(q),
                         _trackBindVars ? query->bindParameters() : nullptr,
                         started, now - started,
                         query->killed() ? QueryExecutionState::ValueType::KILLED
                                         : QueryExecutionState::ValueType::FINISHED,
                         isStreaming);

      if (++_slowCount > _maxSlowQueries) {
        // free first element
        _slow.pop_front();
        --_slowCount;
      }
    }
  } catch (...) {
  }
}

/// @brief kills a query
Result QueryList::kill(TRI_voc_tick_t id) {
  READ_LOCKER(writeLocker, _lock);

  auto it = _current.find(id);

  if (it == _current.end()) {
    return {TRI_ERROR_QUERY_NOT_FOUND};
  }

  Query* query = (*it).second;
  LOG_TOPIC("25cc4", WARN, arangodb::Logger::FIXME)
      << "killing AQL query " << id << " '" << query->queryString() << "'";

  query->kill();
  return Result();
}

/// @brief kills all currently running queries that match the filter function
/// (i.e. the filter should return true for a queries to be killed)
uint64_t QueryList::kill(std::function<bool(Query&)> const& filter, bool silent) {
  uint64_t killed = 0;

  READ_LOCKER(readLocker, _lock);

  for (auto& it : _current) {
    Query& query = *(it.second);

    if (!filter(query)) {
      continue;
    }

    if (silent) {
      LOG_TOPIC("f7722", TRACE, arangodb::Logger::FIXME)
          << "killing AQL query " << query.id() << " '" << query.queryString() << "'";
    } else {
      LOG_TOPIC("90113", WARN, arangodb::Logger::FIXME)
          << "killing AQL query " << query.id() << " '" << query.queryString() << "'";
    }

    query.kill();
    ++killed;
  }

  return killed;
}

/// @brief get the list of currently running queries
std::vector<QueryEntryCopy> QueryList::listCurrent() {
  double const now = TRI_microtime();
  size_t const maxLength = _maxQueryStringLength;

  std::vector<QueryEntryCopy> result;
  // reserve room for some queries outside of the lock already,
  // so we reduce the possibility of having to reserve more room
  // later
  result.reserve(16);

  {
    READ_LOCKER(readLocker, _lock);
    // reserve the actually needed space
    result.reserve(_current.size());

    for (auto const& it : _current) {
      Query const* query = it.second;

      if (query == nullptr || query->queryString().empty()) {
        continue;
      }

      double const started = query->startTime();

      result.emplace_back(query->id(), extractQueryString(query, maxLength),
                          _trackBindVars ? query->bindParameters() : nullptr,
                          started, now - started,
                          query->killed() ? QueryExecutionState::ValueType::KILLED
                                          : query->state(),
                          query->queryOptions().stream);
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
  _slowCount = 0;
}

size_t QueryList::count() {
  READ_LOCKER(writeLocker, _lock);
  return _current.size();
}

std::string QueryList::extractQueryString(Query const* query, size_t maxLength) const {
  return query->queryString().extract(maxLength);
}

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
#include "Aql/Timing.h"
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
#include <velocypack/Dumper.h>
#include <velocypack/Sink.h>
#include <velocypack/StringRef.h>
#include <velocypack/Value.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::aql;

QueryEntryCopy::QueryEntryCopy(TRI_voc_tick_t id, std::string const& database,
                               std::string const& user, std::string&& queryString,
                               std::shared_ptr<arangodb::velocypack::Builder> const& bindParameters,
                               std::vector<std::string> dataSources,
                               double started, double runTime,
                               QueryExecutionState::ValueType state, bool stream)
    : id(id),
      database(database),
      user(user),
      queryString(std::move(queryString)),
      bindParameters(bindParameters),
      dataSources(std::move(dataSources)),
      started(started),
      runTime(runTime),
      state(state),
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
  out.add("state", VPackValue(aql::QueryExecutionState::toString(state)));
  out.add("stream", VPackValue(stream));
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
      _maxQueryStringLength(defaultMaxQueryStringLength) {
  _current.reserve(64);
}

/// @brief insert a query
bool QueryList::insert(Query* query) {
  // not enable or no query string
  if (!enabled() || query == nullptr || query->queryString().empty()) {
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

  // elapsed time since query start
  double const elapsed = elapsedSince(query->startTime());

  _queryRegistryFeature.trackQuery(elapsed);

  if (!trackSlowQueries() || query->killed()) {
    return;
  }

  bool const isStreaming = query->queryOptions().stream;
  double threshold = (isStreaming ? _slowStreamingQueryThreshold : _slowQueryThreshold);

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

      std::string q = extractQueryString(query, _maxQueryStringLength);
      std::string bindParameters;
      if (_trackBindVars) {
        // also log bind variables
        auto bp = query->bindParameters();
        if (bp != nullptr && !bp->slice().isNone()) {
          bindParameters.append(", bind vars: ");
          bindParameters.append(bp->slice().toJson());
          if (bindParameters.size() > _maxQueryStringLength) {
            bindParameters.resize(_maxQueryStringLength - 3);
            bindParameters.append("...");
          }
        }
      }
      
      std::string dataSources;
      if (_trackDataSources) {
        auto const d = query->collectionNames();
        if (!d.empty()) {
          size_t i = 0;
          dataSources = ", data sources: [";
          arangodb::velocypack::StringSink sink(&dataSources);
          arangodb::velocypack::Dumper dumper(&sink);
          for (auto const& dn : d) {
            if (i > 0) {
              dataSources.push_back(',');
            }
            dumper.appendString(dn.data(), dn.size());
            ++i;
          }
          dataSources.push_back(']');
        }
      }

      LOG_TOPIC("8bcee", WARN, Logger::QUERIES)
          << "slow " << (isStreaming ? "streaming " : "") << "query: '" << q << "'"
          << bindParameters << dataSources << ", database: " << query->vocbase().name()
          << ", user: " << query->user() << ", token: QRY" << query->id() 
          << ", took: " << Logger::FIXED(elapsed) << " s";

      _slow.emplace_back(query->id(), query->vocbase().name(), query->user(), std::move(q),
                         _trackBindVars ? query->bindParameters() : nullptr,
                         _trackDataSources ? query->collectionNames() : std::vector<std::string>(),
                         now - elapsed, /* start timestamp */ 
                         elapsed /* run time */,
                         query->killed() ? QueryExecutionState::ValueType::KILLED
                                         : QueryExecutionState::ValueType::FINISHED,
                         isStreaming);

      // _slow is an std::list, but since c++11 the size() method of all standard
      // containers is O(1), so this is ok
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
  READ_LOCKER(writeLocker, _lock);

  auto it = _current.find(id);

  if (it == _current.end()) {
    return {TRI_ERROR_QUERY_NOT_FOUND, "query ID not found in query list"};
  }

  Query* query = (*it).second;
  LOG_TOPIC("25cc4", WARN, arangodb::Logger::FIXME)
      << "killing AQL query " << id << " '" << extractQueryString(query, _maxQueryStringLength) << "'";

  query->kill();
  return Result();
}

/// @brief kills all currently running queries that match the filter function
/// (i.e. the filter should return true for a queries to be killed)
uint64_t QueryList::kill(std::function<bool(Query&)> const& filter, bool silent) {
  uint64_t killed = 0;
  size_t const maxLength = _maxQueryStringLength;

  READ_LOCKER(readLocker, _lock);

  for (auto& it : _current) {
    Query& query = *(it.second);

    if (!filter(query)) {
      continue;
    }

    if (silent) {
      LOG_TOPIC("f7722", TRACE, arangodb::Logger::FIXME)
          << "killing AQL query " << query.id() << " '" << extractQueryString(&query, maxLength) << "'";
    } else {
      LOG_TOPIC("90113", WARN, arangodb::Logger::FIXME)
          << "killing AQL query " << query.id() << " '" << extractQueryString(&query, maxLength) << "'";
    }

    query.kill();
    ++killed;
  }

  return killed;
}

/// @brief get the list of currently running queries
std::vector<QueryEntryCopy> QueryList::listCurrent() {
  std::vector<QueryEntryCopy> result;
  // reserve room for some queries outside of the lock already,
  // so we reduce the possibility of having to reserve more room
  // later
  result.reserve(16);
      
  size_t const maxLength = _maxQueryStringLength;
  double const now = TRI_microtime();

  {
    READ_LOCKER(readLocker, _lock);
    // reserve the actually needed space
    result.reserve(_current.size());

    for (auto const& it : _current) {
      Query const* query = it.second;

      TRI_ASSERT(query != nullptr);

      // elapsed time since query start
      double const elapsed = elapsedSince(query->startTime());

      // we calculate the query start timestamp as the current time minus
      // the elapsed time since query start. this is not 100% accurrate, but
      // best effort, and saves us from bookkeeping the start timestamp of the 
      // query inside the Query object.
      result.emplace_back(query->id(), query->vocbase().name(), query->user(),
                          extractQueryString(query, maxLength),
                          _trackBindVars ? query->bindParameters() : nullptr,
                          _trackDataSources ? query->collectionNames() : std::vector<std::string>(),
                          now - elapsed /* start timestamp */, 
                          elapsed /* run time */,
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
}

size_t QueryList::count() {
  READ_LOCKER(writeLocker, _lock);
  return _current.size();
}

std::string QueryList::extractQueryString(Query const* query, size_t maxLength) const {
  TRI_ASSERT(query != nullptr);
  if (trackQueryString()) {
    return query->queryString().extract(maxLength);
  }
  return "<hidden>";
}

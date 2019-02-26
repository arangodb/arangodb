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
#include "Basics/WriteLocker.h"
#include "Logger/Logger.h"
#include "RestServer/QueryRegistryFeature.h"
#include "VocBase/vocbase.h"

#include <velocypack/Builder.h>
#include <velocypack/StringRef.h>
#include <velocypack/Value.h>

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

/// @brief create a query list
QueryList::QueryList(TRI_vocbase_t*)
    : _current(64),
      _slowLock(),
      _slow(),
      _slowCount(0),
      _enabled(application_features::ApplicationServer::getFeature<arangodb::QueryRegistryFeature>("QueryRegistry")
                   ->trackSlowQueries()),
      _trackSlowQueries(
          application_features::ApplicationServer::getFeature<arangodb::QueryRegistryFeature>("QueryRegistry")
              ->trackSlowQueries()),
      _trackBindVars(application_features::ApplicationServer::getFeature<arangodb::QueryRegistryFeature>("QueryRegistry")
                         ->trackBindVars()),
      _slowQueryThreshold(
          application_features::ApplicationServer::getFeature<arangodb::QueryRegistryFeature>("QueryRegistry")
              ->slowQueryThreshold()),
      _slowStreamingQueryThreshold(
          application_features::ApplicationServer::getFeature<arangodb::QueryRegistryFeature>("QueryRegistry")
              ->slowStreamingQueryThreshold()),
      _maxSlowQueries(defaultMaxSlowQueries),
      _maxQueryStringLength(defaultMaxQueryStringLength) {
}

/// @brief insert a query
bool QueryList::insert(Query* query) {
  // not enable or no query string
  if (!_enabled || query == nullptr || query->queryString().empty()) {
    return false;
  }

  try {
    TRI_IF_FAILURE("QueryList::insert") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }

    return _current.emplace(query->id(), query);
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

  auto removed = _current.erase(query->id());
  if (!removed) {
    return;
  }

  bool const isStreaming = query->queryOptions().stream;
  double threshold = (isStreaming ? _slowStreamingQueryThreshold : _slowQueryThreshold);

  if (!_trackSlowQueries || threshold < 0.0) {
    return;
  }

  double const started = query->startTime();
  double const now = TRI_microtime();

  try {
    // check if we need to push the query into the list of slow queries
    if (now - started >= threshold && !query->killed()) {
      // yes.

      TRI_IF_FAILURE("QueryList::remove") {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
      }

      WRITE_LOCKER(writeLocker, _slowLock);

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
        LOG_TOPIC(WARN, Logger::QUERIES)
            << "slow " << (isStreaming ? "streaming " : "") << "query: '" << q
            << "'" << bindParameters << ", took: " << Logger::FIXED(now - started)
            << " s, loading took: " << Logger::FIXED(loadTime) << " s";
      } else {
        LOG_TOPIC(WARN, Logger::QUERIES)
            << "slow " << (isStreaming ? "streaming " : "") << "query: '" << q << "'"
            << bindParameters << ", took: " << Logger::FIXED(now - started) << " s";
      }

      _slow.emplace_back(query->id(), std::move(q),
                         _trackBindVars ? query->bindParameters() : nullptr,
                         started, now - started,
                         QueryExecutionState::ValueType::FINISHED, isStreaming);

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
int QueryList::kill(TRI_voc_tick_t id) {
  // Note: this works because for the vykuov_hash_map the iterator returned by
  // find keeps a lock on the bucket. This prevents another thread from removing
  // (and potentially deleting) the query we are trying to kill.
  auto it = _current.find(id);
  if (it != _current.end()) {
    Query* query = (*it).second;
    LOG_TOPIC(WARN, arangodb::Logger::FIXME)
        << "killing AQL query " << id << " '" << query->queryString() << "'";

      query->kill();
      return TRI_ERROR_NO_ERROR;
  }
  return TRI_ERROR_QUERY_NOT_FOUND;
}

/// @brief kills all currently running queries
uint64_t QueryList::killAll(bool silent) {
  uint64_t killed = 0;

  for (auto it : _current) {
    Query* query = it.second;

    if (silent) {
      LOG_TOPIC(TRACE, arangodb::Logger::FIXME)
          << "killing AQL query " << query->id() << " '" << query->queryString() << "'";
    } else {
      LOG_TOPIC(WARN, arangodb::Logger::FIXME)
          << "killing AQL query " << query->id() << " '" << query->queryString() << "'";
    }

    query->kill();
    ++killed;
  }

  return killed;
}

/// @brief get the list of currently running queries
std::vector<QueryEntryCopy> QueryList::listCurrent() {
  double const now = TRI_microtime();
  size_t const maxLength = _maxQueryStringLength;

  std::vector<QueryEntryCopy> result;
  result.reserve(16);

  for (auto const it : _current) {
    Query const* query = it.second;

    if (query == nullptr || query->queryString().empty()) {
      continue;
    }

    double const started = query->startTime();

    result.emplace_back(query->id(), extractQueryString(query, maxLength),
                        _trackBindVars ? query->bindParameters() : nullptr, started,
                        now - started, query->state(), query->queryOptions().stream);
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
    READ_LOCKER(readLocker, _slowLock);
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
  WRITE_LOCKER(writeLocker, _slowLock);
  _slow.clear();
  _slowCount = 0;
}

std::string QueryList::extractQueryString(Query const* query, size_t maxLength) const {
  return query->queryString().extract(maxLength);
}

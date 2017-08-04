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
#include "Basics/ReadLocker.h"
#include "Basics/StringRef.h"
#include "Basics/WriteLocker.h"
#include "Basics/Exceptions.h"
#include "Logger/Logger.h"
#include "RestServer/QueryRegistryFeature.h"
#include "VocBase/vocbase.h"

#include <velocypack/Builder.h>
#include <velocypack/Value.h>

using namespace arangodb;
using namespace arangodb::aql;
  
QueryEntryCopy::QueryEntryCopy(TRI_voc_tick_t id,
                               std::string&& queryString, 
                               std::shared_ptr<arangodb::velocypack::Builder> bindParameters,
                               double started,
                               double runTime, QueryExecutionState::ValueType state)
    : id(id), queryString(std::move(queryString)), bindParameters(bindParameters), 
      started(started), runTime(runTime), state(state) {}

/// @brief create a query list
QueryList::QueryList(TRI_vocbase_t*)
    : _lock(),
      _current(),
      _slow(),
      _slowCount(0),
      _enabled(application_features::ApplicationServer::getFeature<arangodb::QueryRegistryFeature>("QueryRegistry")->trackSlowQueries()),
      _trackSlowQueries(application_features::ApplicationServer::getFeature<arangodb::QueryRegistryFeature>("QueryRegistry")->trackSlowQueries()),
      _trackBindVars(application_features::ApplicationServer::getFeature<arangodb::QueryRegistryFeature>("QueryRegistry")->trackBindVars()),
      _slowQueryThreshold(application_features::ApplicationServer::getFeature<arangodb::QueryRegistryFeature>("QueryRegistry")->slowQueryThreshold()),
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

    auto it = _current.emplace(query->id(), query);
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
  auto it = _current.find(query->id());

  if (it == _current.end()) {
    return;
  }

  _current.erase(it);
    
  if (!_trackSlowQueries || _slowQueryThreshold < 0.0) {
    return;
  }

  double const started = query->startTime();
  double const now = TRI_microtime();

  try {
    // check if we need to push the query into the list of slow queries
    if (now - started >= _slowQueryThreshold) {
      // yes.

      TRI_IF_FAILURE("QueryList::remove") {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
      }

      std::string q = extractQueryString(query, _maxQueryStringLength);

      QueryProfile* profile = query->profile();
      double loadTime = 0.0;

      if (profile != nullptr) {
        loadTime = profile->timers[QueryExecutionState::toNumber(QueryExecutionState::ValueType::LOADING_COLLECTIONS)];
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
        LOG_TOPIC(WARN, Logger::QUERIES) << "slow query: '" << q << bindParameters << ", took: " << Logger::FIXED(now - started) << ", loading took: " << Logger::FIXED(loadTime);
      } else {
        LOG_TOPIC(WARN, Logger::QUERIES) << "slow query: '" << q << bindParameters << ", took: " << Logger::FIXED(now - started);
      }

      _slow.emplace_back(QueryEntryCopy(
          query->id(),
          std::move(q),
          _trackBindVars ? query->bindParameters() : nullptr,
          started, now - started,
          QueryExecutionState::ValueType::FINISHED));

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
  WRITE_LOCKER(writeLocker, _lock);

  auto it = _current.find(id);

  if (it == _current.end()) {
    return TRI_ERROR_QUERY_NOT_FOUND;
  }

  Query* query = (*it).second;
  LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "killing AQL query " << id << " '" << query->queryString() << "'";

  query->killed(true);
  return TRI_ERROR_NO_ERROR;
}
  
/// @brief kills all currently running queries
uint64_t QueryList::killAll(bool silent) {
  uint64_t killed = 0;

  WRITE_LOCKER(writeLocker, _lock);

  for (auto& it : _current) {
    Query* query = it.second;
   
    if (silent) {
      LOG_TOPIC(TRACE, arangodb::Logger::FIXME) << "killing AQL query " << query->id() << " '" << query->queryString() << "'";
    } else {
      LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "killing AQL query " << query->id() << " '" << query->queryString() << "'";
    }
    
    query->killed(true);
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
       
      result.emplace_back(
          QueryEntryCopy(query->id(),
                         extractQueryString(query, maxLength),
                         _trackBindVars ? query->bindParameters() : nullptr,
                         started, now - started,
                         query->state()));
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
      
std::string QueryList::extractQueryString(Query const* query, size_t maxLength) const {
  return query->queryString().extract(maxLength);
}

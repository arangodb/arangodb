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

#include "Aql/QueryList.h"
#include "Aql/Query.h"
#include "Basics/ReadLocker.h"
#include "Basics/StringRef.h"
#include "Basics/WriteLocker.h"
#include "Basics/Exceptions.h"
#include "Logger/Logger.h"
#include "VocBase/vocbase.h"

using namespace arangodb::aql;

QueryEntryCopy::QueryEntryCopy(TRI_voc_tick_t id,
                               std::string&& queryString, 
                               std::shared_ptr<arangodb::velocypack::Builder> bindParameters,
                               double started,
                               double runTime, QueryExecutionState::ValueType state)
    : id(id), queryString(std::move(queryString)), bindParameters(bindParameters), 
      started(started), runTime(runTime), state(state) {}

double const QueryList::DefaultSlowQueryThreshold = 10.0;
size_t const QueryList::DefaultMaxSlowQueries = 64;
size_t const QueryList::DefaultMaxQueryStringLength = 4096;

/// @brief create a query list
QueryList::QueryList(TRI_vocbase_t*)
    : _lock(),
      _current(),
      _slow(),
      _slowCount(0),
      _enabled(!Query::DisableQueryTracking()),
      _trackSlowQueries(!Query::DisableQueryTracking()),
      _slowQueryThreshold(Query::SlowQueryThreshold()),
      _maxSlowQueries(QueryList::DefaultMaxSlowQueries),
      _maxQueryStringLength(QueryList::DefaultMaxQueryStringLength) {
  _current.reserve(64);
}

/// @brief insert a query
bool QueryList::insert(Query const* query) {
  // not enable or no query string
  if (!_enabled || query == nullptr || query->queryString() == nullptr) {
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
void QueryList::remove(Query const* query) {
  // we're intentionally not checking _enabled here...

  // note: there is the possibility that a query got inserted when the
  // tracking was turned on, but is going to be removed when the tracking
  // is turned off. in this case, removal is forced so the contents of
  // the list are correct

  // no query string
  if (query == nullptr || query->queryString() == nullptr) {
    return;
  }

  size_t const maxLength = _maxQueryStringLength;

  WRITE_LOCKER(writeLocker, _lock);
  auto it = _current.find(query->id());

  if (it != _current.end()) {
    Query const* query = (*it).second;
    _current.erase(it);

    TRI_ASSERT(query != nullptr);
    double const started = query->startTime();
    double const now = TRI_microtime();

    try {
      // check if we need to push the query into the list of slow queries
      if (_trackSlowQueries && _slowQueryThreshold >= 0.0 &&
          now - started >= _slowQueryThreshold) {
        // yes.

        TRI_IF_FAILURE("QueryList::remove") {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
        }

        std::string q = extractQueryString(query, maxLength);

        LOG_TOPIC(WARN, Logger::QUERIES) << "slow query: '" << q << "', took: " << Logger::FIXED(now - started);

        _slow.emplace_back(QueryEntryCopy(
            query->id(),
            std::move(q),
            query->bindParameters(),
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
}

/// @brief kills a query
int QueryList::kill(TRI_voc_tick_t id) {
  WRITE_LOCKER(writeLocker, _lock);

  auto it = _current.find(id);

  if (it == _current.end()) {
    return TRI_ERROR_QUERY_NOT_FOUND;
  }

  Query const* query = (*it).second;
  StringRef queryString(query->queryString(), query->queryLength());

  LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "killing AQL query " << id << " '" << queryString << "'";

  const_cast<arangodb::aql::Query*>(query)->killed(true);
  return TRI_ERROR_NO_ERROR;
}
  
/// @brief kills all currently running queries
uint64_t QueryList::killAll(bool silent) {
  uint64_t killed = 0;

  WRITE_LOCKER(writeLocker, _lock);

  for (auto& it : _current) {
    Query const* query = it.second;
   
    StringRef queryString(query->queryString(), query->queryLength());
  
    if (silent) {
      LOG_TOPIC(TRACE, arangodb::Logger::FIXME) << "killing AQL query " << query->id() << " '" << queryString << "'";
    } else {
      LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "killing AQL query " << query->id() << " '" << queryString << "'";
    }
    
    const_cast<arangodb::aql::Query*>(query)->killed(true);
    ++killed;
  }

  return killed;
}

/// @brief get the list of currently running queries
std::vector<QueryEntryCopy> QueryList::listCurrent() {
  double const now = TRI_microtime();
  size_t const maxLength = _maxQueryStringLength;

  std::vector<QueryEntryCopy> result;

  {
    READ_LOCKER(readLocker, _lock);
    result.reserve(_current.size());

    for (auto const& it : _current) {
      Query const* query = it.second;

      if (query == nullptr || query->queryString() == nullptr) {
        continue;
      }

      double const started = query->startTime();
       
      result.emplace_back(
          QueryEntryCopy(query->id(),
                         extractQueryString(query, maxLength),
                         query->bindParameters(),
                         started, now - started,
                         query->state()));
    }
  }

  return result;
}

/// @brief get the list of slow queries
std::vector<QueryEntryCopy> QueryList::listSlow() {
  std::vector<QueryEntryCopy> result;

  {
    READ_LOCKER(readLocker, _lock);
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
  char const* queryString = query->queryString();
  size_t length = query->queryLength();

  if (length > maxLength) {
    std::string q;

    // query string needs truncation
    length = maxLength;

    // do not create invalid UTF-8 sequences
    while (length > 0) {
      uint8_t c = queryString[length - 1];
      if ((c & 128) == 0) {
        // single-byte character
        break;
      }
      --length;

      // start of a multi-byte sequence
      if ((c & 192) == 192) {
        // decrease length by one more, so we the string contains the
        // last part of the previous (multi-byte?) sequence
        break;
      }
    }
    
    q.reserve(length + 3);
    q.append(queryString, length); 
    q.append("...", 3);
    return q;
  } 
    
  // no truncation
  return std::string(queryString, length);
}

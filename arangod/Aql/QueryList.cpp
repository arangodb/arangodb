////////////////////////////////////////////////////////////////////////////////
/// @brief Aql, list of running queries
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Aql/QueryList.h"
#include "Aql/Query.h"
#include "Basics/ReadLocker.h"
#include "Basics/WriteLocker.h"
#include "VocBase/vocbase.h"

using namespace triagens::aql;

// -----------------------------------------------------------------------------
// --SECTION--                                                 struct QueryEntry
// -----------------------------------------------------------------------------

QueryEntry::QueryEntry (TRI_voc_tick_t id,
                        char const* queryString,
                        size_t queryLength,
                        double started) 
  : id(id),
    queryString(queryString),
    queryLength(queryLength),
    started(started) {
}

// -----------------------------------------------------------------------------
// --SECTION--                                             struct QueryEntryCopy
// -----------------------------------------------------------------------------

QueryEntryCopy::QueryEntryCopy (TRI_voc_tick_t id,
                                std::string const& queryString,
                                double started,
                                double runTime) 
  : id(id),
    queryString(queryString),
    started(started),
    runTime(runTime) {

}

// -----------------------------------------------------------------------------
// --SECTION--                                                   class QueryList
// -----------------------------------------------------------------------------

double const QueryList::DefaultSlowQueryThreshold   = 10.0;
size_t const QueryList::DefaultMaxSlowQueries       = 64;
size_t const QueryList::DefaultMaxQueryStringLength = 4096;

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create a query list
////////////////////////////////////////////////////////////////////////////////

QueryList::QueryList (TRI_vocbase_t* vocbase) 
  : _vocbase(vocbase),
    _lock(),
    _current(),
    _slow(),
    _slowCount(0),
    _enabled(false),
    _trackSlowQueries(true),
    _slowQueryThreshold(QueryList::DefaultSlowQueryThreshold),
    _maxSlowQueries(QueryList::DefaultMaxSlowQueries),
    _maxQueryStringLength(QueryList::DefaultMaxQueryStringLength) {

  _current.reserve(64);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy a query list
////////////////////////////////////////////////////////////////////////////////

QueryList::~QueryList () {
  WRITE_LOCKER(_lock);

  for (auto it : _current) {
    delete it.second;
  }
  _current.clear();
  _slow.clear();
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief insert a query
////////////////////////////////////////////////////////////////////////////////

void QueryList::insert (Query const* query,
                        double stamp) {
  // no query string
  if (query->queryString() == nullptr || ! _enabled) {
    return;
  }

  try { 
    std::unique_ptr<QueryEntry> entry(new QueryEntry( 
      query->id(), 
      query->queryString(),
      query->queryLength(),
      stamp
    ));

    WRITE_LOCKER(_lock);
    auto it = _current.emplace(std::make_pair(query->id(), entry.get()));
    if (it.second) {
      entry.release();
    }
  }
  catch (...) {
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief remove a query
////////////////////////////////////////////////////////////////////////////////

void QueryList::remove (Query const* query,
                        double now) {
  // no query string
  if (query->queryString() == nullptr || ! _enabled) {
    return;
  }

  QueryEntry* entry = nullptr;
  
  {
    WRITE_LOCKER(_lock);
    auto it = _current.find(query->id());

    if (it != _current.end()) { 
      entry = (*it).second;
      _current.erase(it);

      TRI_ASSERT(entry != nullptr);

      try {
        // check if we need to push the query into the list of slow queries
        if (_slowQueryThreshold >= 0.0 && 
            now - entry->started >= _slowQueryThreshold) {
          // yes.

          size_t const maxLength = _maxQueryStringLength;
          size_t length = entry->queryLength;
          if (length > maxLength) {
            length = maxLength;
          }

          _slow.emplace_back(QueryEntryCopy(
            entry->id, 
            std::string(entry->queryString, length).append(entry->queryLength > maxLength ? "..." : ""), 
            entry->started, 
            now - entry->started
          ));

          if (++_slowCount > _maxSlowQueries) {
            // free first element
            _slow.pop_front();
            --_slowCount;
          }
        }
      }
      catch (...) {
      }
    }
  }

  if (entry != nullptr) {
    delete entry;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the list of currently running queries
////////////////////////////////////////////////////////////////////////////////

std::vector<QueryEntryCopy> QueryList::listCurrent () {
  double const now = TRI_microtime();

  std::vector<QueryEntryCopy> result;

  {
    size_t const maxLength = _maxQueryStringLength;

    READ_LOCKER(_lock);
    result.reserve(_current.size());

    for (auto const& it : _current) {
      auto entry = it.second;

      if (entry == nullptr || 
          entry->queryString == nullptr) {
        continue;
      }

      size_t length = entry->queryLength;
      if (length > maxLength) {
        length = maxLength;
      }

      result.emplace_back(QueryEntryCopy(
        entry->id, 
        std::string(entry->queryString, length).append(entry->queryLength > maxLength ? "..." : ""), 
        entry->started, 
        now - entry->started
      ));

       
    }
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the list of slow queries
////////////////////////////////////////////////////////////////////////////////

std::vector<QueryEntryCopy> QueryList::listSlow () {
  std::vector<QueryEntryCopy> result;

  {
    READ_LOCKER(_lock);
    result.reserve(_slow.size());
    result.assign(_slow.begin(), _slow.end());
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief clear the list of slow queries
////////////////////////////////////////////////////////////////////////////////

void QueryList::clearSlow () {
  WRITE_LOCKER(_lock);
  _slow.clear();
  _slowCount = 0;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:

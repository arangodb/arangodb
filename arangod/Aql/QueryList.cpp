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
#include "Basics/logging.h"
#include "Basics/ReadLocker.h"
#include "Basics/WriteLocker.h"
#include "Basics/Exceptions.h"
#include "VocBase/vocbase.h"

using namespace triagens::aql;

// -----------------------------------------------------------------------------
// --SECTION--                                                 struct QueryEntry
// -----------------------------------------------------------------------------

QueryEntry::QueryEntry (triagens::aql::Query const* query,
                        double started) 
  : query(query),
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

QueryList::QueryList (TRI_vocbase_t*) 
  : _lock(),
    _current(),
    _slow(),
    _slowCount(0),
    _enabled(! Query::DisableQueryTracking()),
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

bool QueryList::insert (Query const* query,
                        double stamp) {
  // not enable or no query string
  if (! _enabled || 
      query == nullptr || 
      query->queryString() == nullptr) {
    return false;
  }

  try { 
    std::unique_ptr<QueryEntry> entry(new QueryEntry(query, stamp));

    WRITE_LOCKER(_lock);

    TRI_IF_FAILURE("QueryList::insert") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    } 

    auto it = _current.emplace(std::make_pair(query->id(), entry.get()));
    if (it.second) {
      entry.release();
      return true;
    }
  }
  catch (...) {
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief remove a query
////////////////////////////////////////////////////////////////////////////////

void QueryList::remove (Query const* query,
                        double now) {
  // we're intentionally not checking _enabled here...

  // note: there is the possibility that a query got inserted when the
  // tracking was turned on, but is going to be removed when the tracking
  // is turned off. in this case, removal is forced so the contents of 
  // the list are correct

  // no query string
  if (query == nullptr || 
      query->queryString() == nullptr) {
    return;
  }

  size_t const maxLength = _maxQueryStringLength;
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
        if (_trackSlowQueries && 
            _slowQueryThreshold >= 0.0 && 
            now - entry->started >= _slowQueryThreshold) {
          // yes.

          char const* queryString = entry->query->queryString();
          size_t const originalLength = entry->query->queryLength();
          size_t length = originalLength;

          if (length > maxLength) {
            length = maxLength;
            TRI_ASSERT(length <= originalLength);

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
          }

          TRI_IF_FAILURE("QueryList::remove") {
            THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
          } 

          _slow.emplace_back(QueryEntryCopy(
            entry->query->id(), 
            std::string(queryString, length).append(originalLength > maxLength ? "..." : ""), 
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
/// @brief kills a query
////////////////////////////////////////////////////////////////////////////////

int QueryList::kill (TRI_voc_tick_t id) {
  std::string queryString;

  {
    WRITE_LOCKER(_lock);
  
    auto it = _current.find(id);

    if (it == _current.end()) {
      return TRI_ERROR_QUERY_NOT_FOUND;
    }

    auto entry = (*it).second;
    queryString.assign(entry->query->queryString(), entry->query->queryLength());
    const_cast<triagens::aql::Query*>(entry->query)->killed(true);
  }
  
  // log outside the lock  
  LOG_WARNING("killing AQL query %llu '%s'", (unsigned long long) id, queryString.c_str());

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the list of currently running queries
////////////////////////////////////////////////////////////////////////////////

std::vector<QueryEntryCopy> QueryList::listCurrent () {
  double const now = TRI_microtime();
  size_t const maxLength = _maxQueryStringLength;

  std::vector<QueryEntryCopy> result;

  {
    READ_LOCKER(_lock);
    result.reserve(_current.size());

    for (auto const& it : _current) {
      auto entry = it.second;

      if (entry == nullptr || 
          entry->query->queryString() == nullptr) {
        continue;
      }

      char const* queryString = entry->query->queryString();
      size_t const originalLength = entry->query->queryLength();
      size_t length = originalLength;

      if (length > maxLength) {
        length = maxLength;
        TRI_ASSERT(length <= originalLength);

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
      }

      result.emplace_back(QueryEntryCopy(
        entry->query->id(), 
        std::string(queryString, length).append(originalLength > maxLength ? "..." : ""), 
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

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

#ifndef ARANGOD_AQL_QUERY_LIST_H
#define ARANGOD_AQL_QUERY_LIST_H 1

#include "Basics/Common.h"
#include "Basics/ReadWriteLock.h"
#include "VocBase/voc-types.h"

#include <list>

struct TRI_vocbase_t;

namespace arangodb {
namespace aql {

class Query;

// -----------------------------------------------------------------------------
// --SECTION--                                                 struct QueryEntry
// -----------------------------------------------------------------------------
      
struct QueryEntry {
  QueryEntry(arangodb::aql::Query const*, double);

  arangodb::aql::Query const* query;
  double const started;
};

struct QueryEntryCopy {
  QueryEntryCopy (TRI_voc_tick_t,
                  std::string const&,
                  double,
                  double,
                  std::string const&);

  TRI_voc_tick_t  id;
  std::string     queryString;
  double          started;
  double          runTime;
  std::string     queryState;
};

class QueryList {
 public:
  /// @brief create a query list
  explicit QueryList(TRI_vocbase_t*);

  /// @brief destroy a query list
  ~QueryList();

 public:
  /// @brief whether or not queries are tracked
  /// we're not using a lock here for performance reasons - thus concurrent
  /// modifications of this variable are possible but are considered unharmful
  inline bool enabled() const { return _enabled; }

  /// @brief toggle query tracking
  /// we're not using a lock here for performance reasons - thus concurrent
  /// modifications of this variable are possible but are considered unharmful
  inline void enabled(bool value) { _enabled = value; }

  /// @brief whether or not slow queries are tracked
  /// we're not using a lock here for performance reasons - thus concurrent
  /// modifications of this variable are possible but are considered unharmful
  inline bool trackSlowQueries() const { return _trackSlowQueries; }

  /// @brief toggle slow query tracking
  /// we're not using a lock here for performance reasons - thus concurrent
  /// modifications of this variable are possible but are considered unharmful
  inline void trackSlowQueries(bool value) { _trackSlowQueries = value; }

  /// @brief threshold for slow queries (in seconds)
  /// we're not using a lock here for performance reasons - thus concurrent
  /// modifications of this variable are possible but are considered unharmful
  inline double slowQueryThreshold() const { return _slowQueryThreshold; }

  /// @brief set the slow query threshold
  /// we're not using a lock here for performance reasons - thus concurrent
  /// modifications of this variable are possible but are considered unharmful
  inline void slowQueryThreshold(double value) {
    if (value < 0.0 || value == HUGE_VAL || value != value) {
      // sanity checks
      value = 0.0;
    }
    _slowQueryThreshold = value;
  }

  /// @brief return the max number of slow queries to keep
  /// we're not using a lock here for performance reasons - thus concurrent
  /// modifications of this variable are possible but are considered unharmful
  inline size_t maxSlowQueries() const { return _maxSlowQueries; }

  /// @brief set the max number of slow queries to keep
  /// we're not using a lock here for performance reasons - thus concurrent
  /// modifications of this variable are possible but are considered unharmful
  inline void maxSlowQueries(size_t value) {
    if (value > 16384) {
      // sanity checks
      value = 16384;
    }
    _maxSlowQueries = value;
  }

  /// @brief return the max length of query strings that are stored / returned
  /// we're not using a lock here for performance reasons - thus concurrent
  /// modifications of this variable are possible but are considered unharmful
  inline size_t maxQueryStringLength() const { return _maxQueryStringLength; }

  /// @brief set the max length of query strings that are stored / returned
  /// we're not using a lock here for performance reasons - thus concurrent
  /// modifications of this variable are possible but are considered unharmful
  inline void maxQueryStringLength(size_t value) {
    // sanity checks
    if (value < 64) {
      value = 64;
    } else if (value >= 8 * 1024 * 1024) {
      value = 8 * 1024 * 1024;
    }

    _maxQueryStringLength = value;
  }

  /// @brief enter a query
  bool insert(Query const*, double);

  /// @brief remove a query
  void remove(Query const*, double);

  /// @brief kills a query
  int kill(TRI_voc_tick_t);
  
  /// @brief kills all currently running queries
  uint64_t killAll(bool silent);

  /// @brief return the list of running queries
  std::vector<QueryEntryCopy> listCurrent();

  /// @brief return the list of slow queries
  std::vector<QueryEntryCopy> listSlow();

  /// @brief clear the list of slow queries
  void clearSlow();

 private:
  /// @brief r/w lock for the list
  arangodb::basics::ReadWriteLock _lock;

  /// @brief list of current queries
  std::unordered_map<TRI_voc_tick_t, QueryEntry*> _current;

  /// @brief list of slow queries
  std::list<QueryEntryCopy> _slow;

  /// @brief current number of slow queries
  size_t _slowCount;

  /// @brief whether or not queries are tracked
  bool _enabled;

  /// @brief whether or not slow queries are tracked
  bool _trackSlowQueries;

  /// @brief threshold for slow queries (in seconds)
  double _slowQueryThreshold;

  /// @brief maximum number of slow queries to keep
  size_t _maxSlowQueries;

  /// @brief max length of query strings to return
  size_t _maxQueryStringLength;

  /// @brief default threshold for slow queries
  static double const DefaultSlowQueryThreshold;

  /// @brief default maximum number of slow queries to keep in list
  static size_t const DefaultMaxSlowQueries;

  /// @brief default max length of a query when returning it
  static size_t const DefaultMaxQueryStringLength;
};
}
}

#endif

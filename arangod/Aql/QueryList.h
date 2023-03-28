////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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

#pragma once

#include <cmath>
#include <list>
#include <memory>
#include <optional>

#include "Aql/QueryExecutionState.h"
#include "Basics/Common.h"
#include "Basics/ErrorCode.h"
#include "Basics/ReadWriteLock.h"
#include "VocBase/voc-types.h"

namespace arangodb {
namespace velocypack {
class Builder;
}
class QueryRegistryFeature;
class Result;

namespace aql {

class Query;

struct QueryEntryCopy {
  QueryEntryCopy(
      TRI_voc_tick_t id, std::string const& database, std::string const& user,
      std::string&& queryString,
      std::shared_ptr<arangodb::velocypack::Builder> const& bindParameters,
      std::vector<std::string> dataSources, double started, double runTime,
      QueryExecutionState::ValueType state, bool stream,
      std::optional<ErrorCode> resultCode);

  void toVelocyPack(arangodb::velocypack::Builder& out) const;

  TRI_voc_tick_t const id;
  std::string const database;
  std::string const user;
  std::string const queryString;
  std::shared_ptr<arangodb::velocypack::Builder> const bindParameters;
  std::vector<std::string> dataSources;
  double const started;
  double const runTime;
  QueryExecutionState::ValueType const state;
  std::optional<ErrorCode> resultCode;
  bool stream;
};

class QueryList {
 public:
  /// @brief create a query list
  explicit QueryList(QueryRegistryFeature&);

  /// @brief destroy a query list
  ~QueryList() = default;

  /// @brief whether or not queries are tracked
  /// we're not using a lock here for performance reasons - thus concurrent
  /// modifications of this variable are possible but are considered unharmful
  bool enabled() const noexcept {
    return _enabled.load(std::memory_order_relaxed);
  }

  /// @brief toggle query tracking
  /// we're not using a lock here for performance reasons - thus concurrent
  /// modifications of this variable are possible but are considered unharmful
  void enabled(bool value) noexcept { _enabled.store(value); }

  /// @brief whether or not slow queries are tracked
  /// we're not using a lock here for performance reasons - thus concurrent
  /// modifications of this variable are possible but are considered unharmful
  bool trackSlowQueries() const noexcept {
    return _trackSlowQueries.load(std::memory_order_relaxed);
  }

  /// @brief toggle slow query tracking
  /// we're not using a lock here for performance reasons - thus concurrent
  /// modifications of this variable are possible but are considered unharmful
  void trackSlowQueries(bool value) noexcept { _trackSlowQueries.store(value); }

  /// @brief whether to track the full query string
  bool trackQueryString() const noexcept {
    return _trackQueryString.load(std::memory_order_relaxed);
  }

  /// @brief toggle slow query tracking
  /// we're not using a lock here for performance reasons - thus concurrent
  /// modifications of this variable are possible but are considered unharmful
  void trackQueryString(bool value) noexcept { _trackQueryString.store(value); }

  /// @brief whether or not bind vars are tracked with queries
  /// we're not using a lock here for performance reasons - thus concurrent
  /// modifications of this variable are possible but are considered unharmful
  bool trackBindVars() const noexcept {
    return _trackBindVars.load(std::memory_order_relaxed);
  }

  /// @brief toggle query bind vars tracking
  /// we're not using a lock here for performance reasons - thus concurrent
  /// modifications of this variable are possible but are considered unharmful
  void trackBindVars(bool value) noexcept { _trackBindVars.store(value); }

  /// @brief threshold for slow queries (in seconds)
  /// we're not using a lock here for performance reasons - thus concurrent
  /// modifications of this variable are possible but are considered unharmful
  double slowQueryThreshold() const noexcept {
    return _slowQueryThreshold.load(std::memory_order_relaxed);
  }

  /// @brief set the slow query threshold
  /// we're not using a lock here for performance reasons - thus concurrent
  /// modifications of this variable are possible but are considered unharmful
  void slowQueryThreshold(double value) noexcept {
    if (value < 0.0 || value == HUGE_VAL || value != value) {
      // only let useful values pass
      value = 0.0;
    }
    _slowQueryThreshold.store(value);
  }

  /// @brief threshold for slow streaming queries (in seconds)
  /// we're not using a lock here for performance reasons - thus concurrent
  /// modifications of this variable are possible but are considered unharmful
  double slowStreamingQueryThreshold() const noexcept {
    return _slowStreamingQueryThreshold.load(std::memory_order_relaxed);
  }

  /// @brief set the slow streaming query threshold
  /// we're not using a lock here for performance reasons - thus concurrent
  /// modifications of this variable are possible but are considered unharmful
  void slowStreamingQueryThreshold(double value) noexcept {
    if (value < 0.0 || value == HUGE_VAL || value != value) {
      // basic checks
      value = 0.0;
    }
    _slowStreamingQueryThreshold.store(value);
  }

  /// @brief return the max number of slow queries to keep
  /// we're not using a lock here for performance reasons - thus concurrent
  /// modifications of this variable are possible but are considered unharmful
  size_t maxSlowQueries() const noexcept {
    return _maxSlowQueries.load(std::memory_order_relaxed);
  }

  /// @brief set the max number of slow queries to keep
  /// we're not using a lock here for performance reasons - thus concurrent
  /// modifications of this variable are possible but are considered unharmful
  void maxSlowQueries(size_t value) noexcept {
    if (value > 16384) {
      // basic checks
      value = 16384;
    }
    _maxSlowQueries.store(value);
  }

  /// @brief return the max length of query strings that are stored / returned
  /// we're not using a lock here for performance reasons - thus concurrent
  /// modifications of this variable are possible but are considered unharmful
  size_t maxQueryStringLength() const noexcept {
    return _maxQueryStringLength.load(std::memory_order_relaxed);
  }

  /// @brief set the max length of query strings that are stored / returned
  /// we're not using a lock here for performance reasons - thus concurrent
  /// modifications of this variable are possible.
  void maxQueryStringLength(size_t value) noexcept {
    // basic checks
    value = std::clamp<size_t>(value, 64, 32 * 1024 * 1024);
    _maxQueryStringLength.store(value);
  }

  /// @brief enter a query
  bool insert(Query&);

  /// @brief remove a query
  void remove(Query&);

  /// @brief kills a query
  Result kill(TRI_voc_tick_t id);

  /// @brief kills all currently running queries that match the filter function
  /// (i.e. the filter should return true for a queries to be killed)
  uint64_t kill(std::function<bool(Query&)> const& filter, bool silent);

  /// @brief return the list of running queries
  std::vector<QueryEntryCopy> listCurrent();

  /// @brief return the list of slow queries
  std::vector<QueryEntryCopy> listSlow();

  /// @brief clear the list of slow queries
  void clearSlow();

  size_t count();

 private:
  void killQuery(Query& query, size_t maxLength, bool silent);

  /// @brief default maximum number of slow queries to keep in list
  static constexpr size_t defaultMaxSlowQueries = 64;

  /// @brief query registry, for keeping track of slow queries counter
  QueryRegistryFeature& _queryRegistryFeature;

  /// @brief r/w lock for the list
  arangodb::basics::ReadWriteLock _lock;

  /// @brief list of current queries, protected by _lock
  std::unordered_map<TRI_voc_tick_t, std::weak_ptr<Query>> _current;

  /// @brief list of slow queries, protected by _lock
  std::list<QueryEntryCopy> _slow;

  /// @brief whether or not queries are tracked
  std::atomic<bool> _enabled;

  /// @brief whether or not slow queries are tracked
  std::atomic<bool> _trackSlowQueries;

  /// @brief whether or not the query string is tracked
  std::atomic<bool> _trackQueryString;

  /// @brief whether or not bind vars are also tracked with queries
  std::atomic<bool> _trackBindVars;

  /// @brief whether or not data source names are also tracked with queries
  std::atomic<bool> _trackDataSources;

  /// @brief threshold for slow queries (in seconds)
  std::atomic<double> _slowQueryThreshold;

  /// @brief threshold for slow streaming queries (in seconds)
  std::atomic<double> _slowStreamingQueryThreshold;

  /// @brief maximum number of slow queries to keep
  std::atomic<size_t> _maxSlowQueries;

  /// @brief max length of query strings to return
  std::atomic<size_t> _maxQueryStringLength;
};
}  // namespace aql
}  // namespace arangodb

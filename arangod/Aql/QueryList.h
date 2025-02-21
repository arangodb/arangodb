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

#pragma once

#include "Basics/ReadWriteLock.h"
#include "VocBase/voc-types.h"

#include <velocypack/String.h>

#include <atomic>
#include <list>
#include <memory>
#include <unordered_map>

namespace arangodb {
namespace velocypack {
class Builder;
}
class QueryRegistryFeature;
class Result;

namespace aql {
class Query;

class QueryList {
 public:
  /// @brief create a query list
  explicit QueryList(QueryRegistryFeature& feature);

  QueryList(QueryList const& other) = delete;
  QueryList& operator=(QueryList const& other) = delete;

  /// @brief destroy a query list
  ~QueryList() = default;

  /// @brief whether or not queries are tracked
  bool enabled() const noexcept;

  /// @brief toggle query tracking
  void enabled(bool value) noexcept;

  /// @brief whether or not slow queries are tracked
  bool trackSlowQueries() const noexcept;

  /// @brief toggle slow query tracking
  void trackSlowQueries(bool value) noexcept;

  /// @brief whether to track the full query string
  bool trackQueryString() const noexcept;

  /// @brief toggle slow query tracking
  void trackQueryString(bool value) noexcept;

  /// @brief whether or not bind vars are tracked with queries
  bool trackBindVars() const noexcept;

  /// @brief toggle query bind vars tracking
  void trackBindVars(bool value) noexcept;

  /// @brief whether or not data sources are tracked with queries
  bool trackDataSources() const noexcept;

  /// @brief threshold for slow queries (in seconds)
  double slowQueryThreshold() const noexcept;

  /// @brief set the slow query threshold
  void slowQueryThreshold(double value) noexcept;

  /// @brief threshold for slow streaming queries (in seconds)
  double slowStreamingQueryThreshold() const noexcept;

  /// @brief set the slow streaming query threshold
  void slowStreamingQueryThreshold(double value) noexcept;

  /// @brief return the max number of slow queries to keep
  size_t maxSlowQueries() const noexcept;

  /// @brief set the max number of slow queries to keep
  void maxSlowQueries(size_t value) noexcept;

  /// @brief return the max length of query strings that are stored / returned
  size_t maxQueryStringLength() const noexcept;

  /// @brief set the max length of query strings that are stored / returned
  void maxQueryStringLength(size_t value) noexcept;

  /// @brief insert a query into the list of currently running queries
  bool insert(Query& query);

  /// @brief remove a query from the list of currently running queries.
  /// note: if the query is a slow query, it may be added to the list of
  /// slow queries by the remove call!
  void remove(Query& query);

  /// @brief insert query into slow query list
  void trackSlow(std::shared_ptr<velocypack::String> query);

  /// @brief kills a query
  Result kill(TRI_voc_tick_t id);

  /// @brief kills all currently running queries that match the filter function
  /// (i.e. the filter should return true for a queries to be killed)
  uint64_t kill(std::function<bool(Query&)> const& filter, bool silent);

  /// @brief return the list of running queries.
  /// each entry is a velocypack::String containing a velocypack::Object with
  /// the query information.
  std::vector<std::shared_ptr<velocypack::String>> listCurrent() const;

  /// @brief return the list of currently running queries.
  /// each entry is a velocypack::String containing a velocypack::Object with
  /// the query information.
  std::vector<std::shared_ptr<velocypack::String>> listSlow() const;

  /// @brief clear the list of slow queries
  void clearSlow();

  /// @brief return the number of currently executing queries
  size_t count() const;

 private:
  /// @brief default maximum number of slow queries to keep in list
  static constexpr size_t kDefaultMaxSlowQueries = 64;

  void killQuery(Query& query, size_t maxLength, bool silent);

  /// @brief r/w lock for the list
  basics::ReadWriteLock mutable _lock;

  /// @brief list of current queries, protected by _lock
  std::unordered_map<TRI_voc_tick_t, std::weak_ptr<Query>> _current;

  /// @brief list of slow queries, protected by _lock.
  /// these are self-contained velocypack slices that manage memory
  /// on their own
  std::list<std::shared_ptr<velocypack::String>> _slow;

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

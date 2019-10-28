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

#ifndef ARANGOD_AQL_PLAN_CACHE_H
#define ARANGOD_AQL_PLAN_CACHE_H 1

#include <unordered_map>
#include <string>

#include "Basics/Common.h"
#include "Basics/ReadWriteLock.h"

struct TRI_vocbase_t;

namespace arangodb {
namespace velocypack {
class Builder;
}

namespace aql {
class ExecutionPlan;
class QueryString;
class VariableGenerator;

struct PlanCacheEntry {
  PlanCacheEntry(std::string&& queryString,
                 std::shared_ptr<arangodb::velocypack::Builder>&& builder)
      : queryString(std::move(queryString)), builder(std::move(builder)) {}

  std::string queryString;
  std::shared_ptr<arangodb::velocypack::Builder> builder;
};

class PlanCache {
 public:
  PlanCache(PlanCache const&) = delete;
  PlanCache& operator=(PlanCache const&) = delete;

  /// @brief create cache
  PlanCache();

  /// @brief destroy the cache
  ~PlanCache();

 public:
  /// @brief lookup a plan in the cache
  std::shared_ptr<PlanCacheEntry> lookup(TRI_vocbase_t*, uint64_t, QueryString const&);

  /// @brief store a plan in the cache
  void store(TRI_vocbase_t*, uint64_t, QueryString const&, ExecutionPlan const*);

  /// @brief invalidate all plans for a particular database
  void invalidate(TRI_vocbase_t*);

  /// @brief get the pointer to the global plan cache
  static PlanCache* instance();

 private:
  /// @brief read-write lock for the cache
  arangodb::basics::ReadWriteLock _lock;

  /// @brief cached query plans, organized per database
  std::unordered_map<TRI_vocbase_t*, std::unordered_map<uint64_t, std::shared_ptr<PlanCacheEntry>>> _plans;
};
}  // namespace aql
}  // namespace arangodb

#endif

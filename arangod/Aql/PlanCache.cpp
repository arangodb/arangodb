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

#include "PlanCache.h"
#include "Aql/Ast.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Query.h"
#include "Aql/QueryString.h"
#include "Basics/ReadLocker.h"
#include "Basics/WriteLocker.h"
#include "VocBase/vocbase.h"

#include <velocypack/Builder.h>

using namespace arangodb::aql;

/// @brief singleton instance of the plan cache
static arangodb::aql::PlanCache Instance;

/// @brief create the plan cache
PlanCache::PlanCache() : _lock(), _plans() {}

/// @brief destroy the plan cache
PlanCache::~PlanCache() = default;

/// @brief lookup a plan in the cache
std::shared_ptr<PlanCacheEntry> PlanCache::lookup(TRI_vocbase_t* vocbase, uint64_t queryHash,
                                                  QueryString const& queryString) {
  READ_LOCKER(readLocker, _lock);

  auto it = _plans.find(vocbase);

  if (it == _plans.end()) {
    // no entry found for the requested database
    return std::shared_ptr<PlanCacheEntry>();
  }

  auto it2 = (*it).second.find(queryHash);

  if (it2 == (*it).second.end()) {
    // plan not found in cache
    return std::shared_ptr<PlanCacheEntry>();
  }

  // plan found in cache
  return (*it2).second;
}

/// @brief store a plan in the cache
void PlanCache::store(TRI_vocbase_t* vocbase, uint64_t hash,
                      QueryString const& queryString, ExecutionPlan const* plan) {
  auto entry = std::make_unique<PlanCacheEntry>(queryString.extract(SIZE_MAX),
                                                plan->toVelocyPack(plan->getAst(), true));

  WRITE_LOCKER(writeLocker, _lock);

  // store cache entry
  _plans[vocbase].insert({hash, std::move(entry)});
}

/// @brief invalidate all queries for a particular database
void PlanCache::invalidate(TRI_vocbase_t* vocbase) {
  WRITE_LOCKER(writeLocker, _lock);

  _plans.erase(vocbase);
}

/// @brief get the plan cache instance
PlanCache* PlanCache::instance() { return &Instance; }

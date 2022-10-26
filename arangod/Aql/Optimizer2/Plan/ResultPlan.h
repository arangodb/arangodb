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
/// @author Heiko  Kernbach
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Inspection/Types.h"
#include "Aql/Optimizer2/Types/Types.h"
#include "Aql/Optimizer2/Inspection/VPackInspection.h"

#include <velocypack/Builder.h>

namespace arangodb::aql::optimizer2::plan {

struct ExecutePlanStats {
  size_t writesExecuted;
  size_t writesIgnored;
  size_t scannedFull;
  size_t scannedIndex;
  size_t cursorsCreated;
  size_t cursorsRearmed;
  size_t cacheHits;
  size_t cacheMisses;
  size_t filtered;
  size_t httpRequests;
  double executionTime;
  size_t peakMemoryUsage;
  size_t intermediateCommits;
};

template<class Inspector>
auto inspect(Inspector& f, ExecutePlanStats& v) {
  return f.object(v).fields(
      f.field("writesExecuted", v.writesExecuted),
      f.field("writesIgnored", v.writesIgnored),
      f.field("scannedFull", v.scannedFull),
      f.field("scannedIndex", v.scannedIndex),
      f.field("cursorsCreated", v.cursorsCreated),
      f.field("cursorsRearmed", v.cursorsRearmed),
      f.field("cacheHits", v.cacheHits), f.field("cacheMisses", v.cacheMisses),
      f.field("filtered", v.filtered), f.field("httpRequests", v.httpRequests),
      f.field("executionTime", v.executionTime),
      f.field("peakMemoryUsage", v.peakMemoryUsage),
      f.field("intermediateCommits", v.intermediateCommits));
}

struct ResultPlanExtra {
  std::vector<std::string> warnings;
  ExecutePlanStats stats;
};

template<class Inspector>
auto inspect(Inspector& f, ResultPlanExtra& v) {
  return f.object(v).fields(f.field("warnings", v.warnings),
                            f.field("stats", v.stats));
}

struct ResultPlan {
  VPackBuilder result;
  bool hasMore;
  bool cached;
  ResultPlanExtra extra;
  bool error;
  AttributeTypes::Numeric code;
};

template<class Inspector>
auto inspect(Inspector& f, ResultPlan& v) {
  return f.object(v).fields(
      f.field("result", v.result), f.field("hasMore", v.hasMore),
      f.field("cached", v.cached), f.field("extra", v.extra),
      f.field("error", v.error), f.field("code", v.code));
}

}  // namespace arangodb::aql::optimizer2::plan
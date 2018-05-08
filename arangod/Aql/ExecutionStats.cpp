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

#include "ExecutionStats.h"
#include "Basics/Exceptions.h"
#include "Basics/StringUtils.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Value.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb::aql;

/// @brief convert the statistics to VelocyPack
void ExecutionStats::toVelocyPack(VPackBuilder& builder, bool reportFullCount) const {
  builder.openObject();
  builder.add("writesExecuted", VPackValue(writesExecuted));
  builder.add("writesIgnored", VPackValue(writesIgnored));
  builder.add("scannedFull", VPackValue(scannedFull));
  builder.add("scannedIndex", VPackValue(scannedIndex));
  builder.add("filtered", VPackValue(filtered));
  builder.add("httpRequests", VPackValue(httpRequests));
  if (reportFullCount) {
    // fullCount is exceptional, as it may be hidden
    builder.add("fullCount", VPackValue(fullCount));
  }
  builder.add("executionTime", VPackValue(executionTime));
  
  if (!nodes.empty()) {
    builder.add("nodes", VPackValue(VPackValueType::Array));
    for (std::pair<size_t, ExecutionStats::Node> const& pair : nodes) {
      builder.openObject();
      builder.add("id", VPackValue(pair.first));
      builder.add("calls", VPackValue(pair.second.calls));
      builder.add("items", VPackValue(pair.second.items));
      builder.add("runtime", VPackValue(pair.second.runtime));
      builder.close();
    }
    builder.close();
  }
  builder.close();
}

void ExecutionStats::toVelocyPackStatic(VPackBuilder& builder) {
  builder.openObject();
  builder.add("writesExecuted", VPackValue(0));
  builder.add("writesIgnored", VPackValue(0));
  builder.add("scannedFull", VPackValue(0));
  builder.add("scannedIndex", VPackValue(0));
  builder.add("filtered", VPackValue(0));
  builder.add("httpRequests", VPackValue(0));
  builder.add("fullCount", VPackValue(0));
  builder.add("executionTime", VPackValue(0.0));
  builder.close();
}

/// @brief sumarize two sets of ExecutionStats
void ExecutionStats::add(ExecutionStats const& summand) {
  writesExecuted += summand.writesExecuted;
  writesIgnored += summand.writesIgnored;
  scannedFull += summand.scannedFull;
  scannedIndex += summand.scannedIndex;
  filtered += summand.filtered;
  httpRequests += summand.httpRequests;
  if (summand.fullCount > 0) {
    fullCount += summand.fullCount;
  }
  // intentionally no modification of executionTime
  
  for(auto const& pair : summand.nodes) {
    auto it = nodes.find(pair.first);
    if (it != nodes.end()) {
      it->second += pair.second;
    } else {
      nodes.emplace(pair);
    }
  }
}

ExecutionStats::ExecutionStats()
    : writesExecuted(0),
      writesIgnored(0),
      scannedFull(0),
      scannedIndex(0),
      filtered(0),
      httpRequests(0),
      fullCount(0),
      executionTime(0.0) {}

ExecutionStats::ExecutionStats(VPackSlice const& slice) 
    : ExecutionStats() {
  if (!slice.isObject()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "stats is not an object");
  }

  writesExecuted = slice.get("writesExecuted").getNumber<int64_t>();
  writesIgnored = slice.get("writesIgnored").getNumber<int64_t>();
  scannedFull = slice.get("scannedFull").getNumber<int64_t>();
  scannedIndex = slice.get("scannedIndex").getNumber<int64_t>();
  filtered = slice.get("filtered").getNumber<int64_t>();
  
  if (slice.hasKey("httpRequests")) {
    httpRequests = slice.get("httpRequests").getNumber<int64_t>();
  }

  // note: fullCount is an optional attribute!
  if (slice.hasKey("fullCount")) {
    fullCount = slice.get("fullCount").getNumber<int64_t>();
  } else {
    fullCount = 0;
  }
      
  // note: node stats are optional
  if (slice.hasKey("nodes")) {
    ExecutionStats::Node node;
    for (VPackSlice val : VPackArrayIterator(slice.get("nodes"))) {
      size_t nid = val.get("id").getNumber<size_t>();
      node.calls = val.get("calls").getNumber<size_t>();
      node.items = val.get("items").getNumber<size_t>();
      node.runtime = val.get("runtime").getNumber<double>();
      nodes.emplace(nid, node);
    }
  }
}

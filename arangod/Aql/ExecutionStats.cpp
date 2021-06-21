////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
  builder.add("httpRequests", VPackValue(requests));
  if (reportFullCount) {
    // fullCount is optional
    builder.add("fullCount", VPackValue(fullCount > count ? fullCount : count));
  }
  builder.add("executionTime", VPackValue(executionTime));

  builder.add("peakMemoryUsage", VPackValue(peakMemoryUsage));

  if (!_nodes.empty()) {
    builder.add("nodes", VPackValue(VPackValueType::Array));
    for (auto const& pair : _nodes) {
      builder.openObject();
      builder.add("id", VPackValue(pair.first.id()));
      builder.add("calls", VPackValue(pair.second.calls));
      builder.add("items", VPackValue(pair.second.items));
      builder.add("runtime", VPackValue(pair.second.runtime));
      builder.close();
    }
    builder.close();
  }
  builder.close();
}

/// @brief sumarize two sets of ExecutionStats
void ExecutionStats::add(ExecutionStats const& summand) {
  writesExecuted += summand.writesExecuted;
  writesIgnored += summand.writesIgnored;
  scannedFull += summand.scannedFull;
  scannedIndex += summand.scannedIndex;
  filtered += summand.filtered;
  requests += summand.requests;
  if (summand.fullCount > 0) {
    fullCount += summand.fullCount;
  }
  count += summand.count;
  peakMemoryUsage = std::max(summand.peakMemoryUsage, peakMemoryUsage);
  // intentionally no modification of executionTime, as the overall
  // time is calculated in the end

  for (auto const& pair : summand._nodes) {
    aql::ExecutionNodeId nid = pair.first;
    auto const& alias = _nodeAliases.find(nid);
    if (alias != _nodeAliases.end()) {
      nid = alias->second;
      if (nid.id() == ExecutionNodeId::InternalNode) {
        // ignore this value, it is an internal node that we do not want to expose
        continue;
      }
    }
    auto result = _nodes.insert({nid, pair.second});
    if (!result.second) {
      result.first->second += pair.second;
    }
  }
  // simon: TODO optimize away
  for (auto const& pair : summand._nodeAliases) {
    _nodeAliases.try_emplace(pair.first, pair.second);
  }
}

void ExecutionStats::addNode(arangodb::aql::ExecutionNodeId nid, ExecutionNodeStats const& stats) {
  auto const alias = _nodeAliases.find(nid);
  if (alias != _nodeAliases.end()) {
    nid = alias->second;
    if (nid.id() == ExecutionNodeId::InternalNode) {
      // ignore this value, it is an internal node that we do not want to expose
      return;
    }
  }
  
  auto it = _nodes.find(nid);
  if (it != _nodes.end()) {
    it->second += stats;
  } else {
    _nodes.emplace(nid, stats);
  }
}

ExecutionStats::ExecutionStats()
    : writesExecuted(0),
      writesIgnored(0),
      scannedFull(0),
      scannedIndex(0),
      filtered(0),
      requests(0),
      fullCount(0),
      count(0),
      executionTime(0.0),
      peakMemoryUsage(0) {}

ExecutionStats::ExecutionStats(VPackSlice const& slice) : ExecutionStats() {
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
    requests = slice.get("httpRequests").getNumber<int64_t>();
  }

  if (slice.hasKey("peakMemoryUsage")) {
    peakMemoryUsage = std::max<size_t>(peakMemoryUsage, slice.get("peakMemoryUsage").getNumber<int64_t>());
  }

  // note: fullCount is an optional attribute!
  if (slice.hasKey("fullCount")) {
    fullCount = slice.get("fullCount").getNumber<int64_t>();
  } else {
    fullCount = count;
  }

  // note: node stats are optional
  if (slice.hasKey("nodes")) {
    ExecutionNodeStats node;
    for (VPackSlice val : VPackArrayIterator(slice.get("nodes"))) {
      auto nid = ExecutionNodeId{val.get("id").getNumber<ExecutionNodeId::BaseType>()};
      node.calls = val.get("calls").getNumber<size_t>();
      node.items = val.get("items").getNumber<size_t>();
      node.runtime = val.get("runtime").getNumber<double>();
      auto const& alias = _nodeAliases.find(nid);
      if (alias != _nodeAliases.end()) {
        nid = alias->second;
      }
      _nodes.try_emplace(nid, node);
    }
  }
}

void ExecutionStats::setExecutionTime(double value) { executionTime = value; }

void ExecutionStats::setPeakMemoryUsage(size_t value) {
  peakMemoryUsage = value;
}

void ExecutionStats::clear() {
  writesExecuted = 0;
  writesIgnored = 0;
  scannedFull = 0;
  scannedIndex = 0;
  filtered = 0;
  requests = 0;
  fullCount = 0;
  count = 0;
  executionTime = 0.0;
  peakMemoryUsage = 0;
  _nodes.clear();
}

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

#include "ExecutionStats.h"
#include "Basics/Exceptions.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Logger/LogMacros.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Value.h>

using namespace arangodb::aql;

/// @brief convert the statistics to VelocyPack
void ExecutionStats::toVelocyPack(VPackBuilder& builder,
                                  bool reportFullCount) const {
  builder.openObject();
  builder.add("writesExecuted", VPackValue(writesExecuted));
  builder.add("writesIgnored", VPackValue(writesIgnored));
  builder.add("scannedFull", VPackValue(scannedFull));
  builder.add("scannedIndex", VPackValue(scannedIndex));
  builder.add("cursorsCreated", VPackValue(cursorsCreated));
  builder.add("cursorsRearmed", VPackValue(cursorsRearmed));
  builder.add("cacheHits", VPackValue(cacheHits));
  builder.add("cacheMisses", VPackValue(cacheMisses));

  builder.add("filtered", VPackValue(filtered));
  builder.add("httpRequests", VPackValue(requests));
  if (reportFullCount) {
    // fullCount is optional
    builder.add("fullCount", VPackValue(fullCount > count ? fullCount : count));
  }
  builder.add("executionTime", VPackValue(executionTime));

  builder.add("peakMemoryUsage", VPackValue(peakMemoryUsage));
  builder.add("intermediateCommits", VPackValue(intermediateCommits));

  if (!_nodes.empty()) {
    builder.add("nodes", VPackValue(VPackValueType::Array));
    for (auto const& pair : _nodes) {
      builder.openObject();
      builder.add("id", VPackValue(pair.first.id()));
      builder.add("calls", VPackValue(pair.second.calls));
      builder.add("items", VPackValue(pair.second.items));
      builder.add("filtered", VPackValue(pair.second.filtered));
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
  cursorsCreated += summand.cursorsCreated;
  cursorsRearmed += summand.cursorsRearmed;
  cacheHits += summand.cacheHits;
  cacheMisses += summand.cacheMisses;
  filtered += summand.filtered;
  requests += summand.requests;
  if (summand.fullCount > 0) {
    fullCount += summand.fullCount;
  }
  count += summand.count;
  peakMemoryUsage = std::max(summand.peakMemoryUsage, peakMemoryUsage);
  intermediateCommits += summand.intermediateCommits;
  // intentionally no modification of executionTime, as the overall
  // time is calculated in the end

  for (auto const& pair : summand._nodes) {
    aql::ExecutionNodeId nid = pair.first;
    auto const& alias = _nodeAliases.find(nid);
    if (alias != _nodeAliases.end()) {
      nid = alias->second;
      if (nid.id() == ExecutionNodeId::InternalNode) {
        // ignore this value, it is an internal node that we do not want to
        // expose
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

void ExecutionStats::addNode(arangodb::aql::ExecutionNodeId nid,
                             ExecutionNodeStats const& stats) {
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

ExecutionStats::ExecutionStats() noexcept
    : writesExecuted(0),
      writesIgnored(0),
      scannedFull(0),
      scannedIndex(0),
      cursorsCreated(0),
      cursorsRearmed(0),
      cacheHits(0),
      cacheMisses(0),
      filtered(0),
      requests(0),
      fullCount(0),
      count(0),
      executionTime(0.0),
      peakMemoryUsage(0),
      intermediateCommits(0) {}

ExecutionStats::ExecutionStats(VPackSlice slice) : ExecutionStats() {
  if (!slice.isObject()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "stats is not an object");
  }

  TRI_ASSERT(cursorsCreated == 0);
  TRI_ASSERT(cursorsRearmed == 0);
  TRI_ASSERT(cacheHits == 0);
  TRI_ASSERT(cacheMisses == 0);

  writesExecuted = basics::VelocyPackHelper::getNumericValue<uint64_t>(
      slice, "writesExecuted", 0);
  writesIgnored = basics::VelocyPackHelper::getNumericValue<uint64_t>(
      slice, "writesIgnored", 0);
  scannedFull = basics::VelocyPackHelper::getNumericValue<uint64_t>(
      slice, "scannedFull", 0);
  scannedIndex = basics::VelocyPackHelper::getNumericValue<uint64_t>(
      slice, "scannedIndex", 0);
  filtered =
      basics::VelocyPackHelper::getNumericValue<uint64_t>(slice, "filtered", 0);
  requests = basics::VelocyPackHelper::getNumericValue<uint64_t>(
      slice, "httpRequests", 0);
  peakMemoryUsage = basics::VelocyPackHelper::getNumericValue<uint64_t>(
      slice, "peakMemoryUsage", 0);
  intermediateCommits = basics::VelocyPackHelper::getNumericValue<uint64_t>(
      slice, "intermediateCommits", 0);

  // cursorsCreated and cursorsRearmed are optional attributes.
  // the attributes are currently not shown in profile outputs,
  // but are rather used for testing purposes.
  cursorsCreated = basics::VelocyPackHelper::getNumericValue<uint64_t>(
      slice, "cursorsCreated", 0);
  cursorsRearmed = basics::VelocyPackHelper::getNumericValue<uint64_t>(
      slice, "cursorsRearmed", 0);

  // cacheHits and cacheMisses are also optional attributes.
  cacheHits = basics::VelocyPackHelper::getNumericValue<uint64_t>(
      slice, "cacheHits", 0);
  cacheMisses = basics::VelocyPackHelper::getNumericValue<uint64_t>(
      slice, "cacheMisses", 0);

  // note: fullCount is an optional attribute!
  if (VPackSlice s = slice.get("fullCount"); s.isNumber()) {
    fullCount = s.getNumber<uint64_t>();
  } else {
    fullCount = count;
  }

  // note: node stats are optional
  if (VPackSlice s = slice.get("nodes"); s.isArray()) {
    ExecutionNodeStats node;
    for (VPackSlice val : VPackArrayIterator(s)) {
      auto nid =
          ExecutionNodeId{val.get("id").getNumber<ExecutionNodeId::BaseType>()};
      node.calls = val.get("calls").getNumber<uint64_t>();
      node.items = val.get("items").getNumber<uint64_t>();
      if (VPackSlice s = val.get("filtered"); !s.isNone()) {
        node.filtered = s.getNumber<uint64_t>();
      }
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
  if (value > peakMemoryUsage) {
    // Peak can never go down, it has to be the maximum
    // value seen
    peakMemoryUsage = value;
  }
}

void ExecutionStats::setIntermediateCommits(uint64_t value) {
  intermediateCommits = value;
}

void ExecutionStats::clear() noexcept {
  writesExecuted = 0;
  writesIgnored = 0;
  scannedFull = 0;
  scannedIndex = 0;
  cursorsCreated = 0;
  cursorsRearmed = 0;
  cacheHits = 0;
  cacheMisses = 0;
  filtered = 0;
  requests = 0;
  fullCount = 0;
  count = 0;
  executionTime = 0.0;
  peakMemoryUsage = 0;
  intermediateCommits = 0;
  _nodes.clear();
}

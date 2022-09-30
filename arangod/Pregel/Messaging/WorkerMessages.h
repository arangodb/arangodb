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
/// @author Julia Volmer
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "Cluster/ClusterTypes.h"
#include "Pregel/Graph.h"
#include "Pregel/Statistics.h"
#include "Pregel/Status/Status.h"
#include "velocypack/Iterator.h"

namespace arangodb::pregel {

struct WorkerCreated {
  ServerID senderId;
};
template<typename Inspector>
auto inspect(Inspector& f, WorkerCreated& x) {
  return f.object(x).fields(f.field("onServer", x.senderId));
}

struct GraphLoaded {
  uint64_t vertexCount;
  uint64_t edgeCount;
  GraphLoaded() noexcept = default;
  GraphLoaded(uint64_t vertexCount, uint64_t edgeCount)
      : vertexCount{vertexCount}, edgeCount{edgeCount} {}
  auto add(GraphLoaded const& other) -> void {
    vertexCount = other.vertexCount;
    edgeCount = other.edgeCount;
  }
};

template<typename Inspector>
auto inspect(Inspector& f, GraphLoaded& x) {
  return f.object(x).fields(f.field("vertexCount", x.vertexCount),
                            f.field("edgeCount", x.edgeCount));
}

struct GlobalSuperStepPrepared {
  uint64_t activeCount;
  uint64_t vertexCount;
  uint64_t edgeCount;
  VPackBuilder aggregators;
  GlobalSuperStepPrepared() noexcept = default;
  GlobalSuperStepPrepared(uint64_t activeCount, uint64_t vertexCount,
                          uint64_t edgeCount, VPackBuilder aggregators)
      : activeCount{activeCount},
        vertexCount{vertexCount},
        edgeCount{edgeCount},
        aggregators{std::move(aggregators)} {}
  auto add(GlobalSuperStepPrepared const& other) -> void {
    activeCount += other.activeCount;
    vertexCount += other.vertexCount;
    edgeCount += other.edgeCount;
    // TODO directly aggregate in here when aggregators have an inspector
    VPackBuilder newAggregators;
    {
      VPackArrayBuilder ab(&newAggregators);
      if (!aggregators.isEmpty()) {
        newAggregators.add(VPackArrayIterator(aggregators.slice()));
      }
      newAggregators.add(other.aggregators.slice());
    }
    aggregators = newAggregators;
  }
};
template<typename Inspector>
auto inspect(Inspector& f, GlobalSuperStepPrepared& x) {
  return f.object(x).fields(f.field("activeCount", x.activeCount),
                            f.field("vertexCount", x.vertexCount),
                            f.field("edgeCount", x.edgeCount),
                            f.field("aggregators", x.aggregators));
}

struct GlobalSuperStepFinished {
  MessageStats messageStats;
  GlobalSuperStepFinished() noexcept = default;
  GlobalSuperStepFinished(MessageStats messageStats)
      : messageStats{std::move(messageStats)} {}
  auto add(GlobalSuperStepFinished const& other) -> void {
    messageStats.accumulate(other.messageStats);
  }
};

template<typename Inspector>
auto inspect(Inspector& f, GlobalSuperStepFinished& x) {
  return f.object(x).fields(f.field("messageStats", x.messageStats));
}

struct Stored {
  Stored() noexcept = default;
  bool operator==(const Stored&) const = default;
  auto add(Stored const& other) -> void {}
};
template<typename Inspector>
auto inspect(Inspector& f, Stored& x) {
  return f.object(x).fields();
}

struct CleanupFinished {
  CleanupFinished() noexcept = default;
  auto add(CleanupFinished const& other) -> void {}
};
template<typename Inspector>
auto inspect(Inspector& f, CleanupFinished& x) {
  return f.object(x).fields();
}

struct StatusUpdated {
  std::string senderId;
  Status status;
};

template<typename Inspector>
auto inspect(Inspector& f, StatusUpdated& x) {
  return f.object(x).fields(f.field(Utils::senderKey, x.senderId),
                            f.field("status", x.status));
}

struct PregelResults {
  VPackBuilder results;
  PregelResults() noexcept = default;
  PregelResults(VPackBuilder results) : results{std::move(results)} {}
  auto add(PregelResults const& other) -> void {
    VPackBuilder newAggregators;
    {
      VPackArrayBuilder ab(&newAggregators);
      if (!results.isEmpty()) {
        newAggregators.add(VPackArrayIterator(results.slice()));
      }
      if (other.results.slice().isArray()) {
        newAggregators.add(VPackArrayIterator(other.results.slice()));
      }
    }
    results = newAggregators;
  }
};
template<typename Inspector>
auto inspect(Inspector& f, PregelResults& x) {
  return f.object(x).fields(f.field("results", x.results));
}

struct PregelMessage {
  std::string senderId;
  uint64_t gss;
  PregelShard shard;
  VPackBuilder messages;
};

template<typename Inspector>
auto inspect(Inspector& f, PregelMessage& x) {
  return f.object(x).fields(f.field(Utils::senderKey, x.senderId),
                            f.field(Utils::globalSuperstepKey, x.gss),
                            f.field("shard", x.shard),
                            f.field("messages", x.messages));
}

}  // namespace arangodb::pregel

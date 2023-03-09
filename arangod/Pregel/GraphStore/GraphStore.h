////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Basics/ResourceUsage.h"
#include "Basics/Guarded.h"
#include "Cluster/ClusterInfo.h"
#include "Utils/DatabaseGuard.h"

#include "Pregel/ExecutionNumber.h"
#include "Pregel/GraphStore/Graph.h"
#include "Pregel/GraphFormat.h"
#include "Pregel/GraphStore/Quiver.h"
#include "Pregel/Iterators.h"
#include "Pregel/Status/Status.h"
#include "Pregel/TypedBuffer.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <set>

struct TRI_vocbase_t;

namespace arangodb {

class LogicalCollection;
struct ResourceMonitor;

namespace transaction {
class Methods;
}

namespace traverser {
class EdgeCollectionInfo;
}

namespace pregel {

template<typename T>
struct TypedBuffer;
class WorkerConfig;
template<typename V, typename E>
struct GraphFormat;

class PregelFeature;

////////////////////////////////////////////////////////////////////////////////
/// @brief carry graph data for a worker job. NOT THREAD SAFE ON DOCUMENT LOADS
////////////////////////////////////////////////////////////////////////////////
template<typename V, typename E>
class GraphStore final {
 public:
  GraphStore(PregelFeature& feature, TRI_vocbase_t& vocbase,
             ExecutionNumber executionNumber, GraphFormat<V, E>* graphFormat);

  uint64_t localVertexCount() const { return _localVertexCount; }
  uint64_t localEdgeCount() const { return _localEdgeCount; }
  GraphStoreStatus status() const { return _observables.observe(); }

  GraphFormat<V, E> const* graphFormat() { return _graphFormat.get(); }

  // ====================== NOT THREAD SAFE ===========================
  void loadShards(WorkerConfig* config,
                  std::function<void()> const& statusUpdateCallback,
                  std::function<void()> const& finishedLoadingCallback);
  // ======================================================================

  /// Write results to database
  void storeResults(WorkerConfig* config, std::function<void()>,
                    std::function<void()> const& statusUpdateCallback);

  Quiver<V, E>& quiver() { return _quiver; }

 private:
  auto loadVertices(ShardID const& vertexShard,
                    std::vector<ShardID> const& edgeShards,
                    std::function<void()> const& statusUpdateCallback)
      -> std::vector<Vertex<V, E>>;
  void loadEdges(transaction::Methods& trx, Vertex<V, E>& vertex,
                 ShardID const& edgeShard, std::string_view documentID,
                 uint64_t numVertices, traverser::EdgeCollectionInfo& info);

  void storeVertices(std::vector<ShardID> const& globalShards,
                     std::function<void()> const& statusUpdateCallback);
  uint64_t determineVertexIdRangeStart(uint64_t numVertices);

 private:
  PregelFeature& _feature;
  DatabaseGuard _vocbaseGuard;
  ResourceMonitor _resourceMonitor;
  ExecutionNumber const _executionNumber;
  const std::unique_ptr<GraphFormat<V, E>> _graphFormat;
  WorkerConfig* _config = nullptr;

  std::atomic<uint64_t> _vertexIdRangeStart;

  /// Holds vertex keys, data and pointers to edges
  Quiver<V, E> _quiver;
  GraphStoreObservables _observables;

  // cache the amount of vertices
  std::set<ShardID> _loadedShards;

  // actual count of loaded vertices / edges
  std::atomic<size_t> _localVertexCount;
  std::atomic<size_t> _localEdgeCount;
};

}  // namespace pregel
}  // namespace arangodb

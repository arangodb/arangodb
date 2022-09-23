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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Basics/Result.h"
#include "Basics/voc-errors.h"
#include "Cluster/ClusterInfo.h"
#include "Pregel/Graph.h"
#include "Pregel/GraphFormat.h"
#include "Pregel/Iterators.h"
#include "Pregel/Status/Status.h"
#include "Pregel/TypedBuffer.h"
#include "Pregel/Worker/WorkerConfig.h"
#include "Pregel/WorkerConductorMessages.h"
#include "Utils/DatabaseGuard.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <set>

struct TRI_vocbase_t;

namespace arangodb {

class LogicalCollection;

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

struct DocumentId {
  std::string _collectionName;
  std::string _key;
  static auto create(std::string_view documentId) -> ResultT<DocumentId>;
};

struct ShardResolver {
  virtual auto getShard(DocumentId const& collectionName, WorkerConfig* config)
      -> ResultT<PregelShard> = 0;
  static auto create(bool isCluster, ClusterInfo& clusterInfo)
      -> std::unique_ptr<ShardResolver>;
  virtual ~ShardResolver(){};
};

struct ClusterShardResolver : ShardResolver {
  ClusterInfo& _clusterInfo;
  ClusterShardResolver(ClusterInfo& info) : _clusterInfo{info} {};
  auto getShard(DocumentId const& documentId, WorkerConfig* config)
      -> ResultT<PregelShard> override;
};

struct SingleServerShardResolver : ShardResolver {
  auto getShard(DocumentId const& documentId, WorkerConfig* config)
      -> ResultT<PregelShard> override;
};

// auto createShardResolver(bool isCluster, ClusterInfo& clusterInfo)
//     -> std::unique_ptr<ShardResolver>;

////////////////////////////////////////////////////////////////////////////////
/// @brief carry graph data for a worker job. NOT THREAD SAFE ON DOCUMENT LOADS
////////////////////////////////////////////////////////////////////////////////
template<typename V, typename E>
class GraphStore final {
 public:
  GraphStore(PregelFeature& feature, TRI_vocbase_t& vocbase,
             ExecutionNumber executionNumber, GraphFormat<V, E>* graphFormat,
             std::unique_ptr<ShardResolver> shardResolver);

  std::unique_ptr<ShardResolver> _shardResolver;
  uint64_t numberVertexSegments() const { return _vertices.size(); }
  uint64_t localVertexCount() const { return _localVertexCount; }
  uint64_t localEdgeCount() const { return _localEdgeCount; }
  auto allocatedSize() -> size_t {
    auto total = size_t{0};

    for (auto&& vb : _vertices) {
      total += vb->capacity();
    }
    for (auto&& vb : _vertexKeys) {
      total += vb->capacity();
    }
    for (auto&& vb : _edges) {
      total += vb->capacity();
    }
    for (auto&& vb : _edgeKeys) {
      total += vb->capacity();
    }
    return total;
  }

  GraphStoreStatus status() const { return _observables.observe(); }

  GraphFormat<V, E> const* graphFormat() { return _graphFormat.get(); }

  // ====================== NOT THREAD SAFE ===========================
  auto loadShards(WorkerConfig* config,
                  std::function<void()> const& statusUpdateCallback)
      -> ResultT<GraphLoaded>;
  void loadDocument(WorkerConfig* config, std::string const& documentID);
  void loadDocument(WorkerConfig* config, PregelShard sourceShard,
                    std::string_view key);
  // ======================================================================

  // only thread safe if your threads coordinate access to memory locations
  RangeIterator<Vertex<V, E>> vertexIterator();
  /// j and j are the first and last index of vertex segments
  RangeIterator<Vertex<V, E>> vertexIterator(size_t i, size_t j);
  RangeIterator<Edge<E>> edgeIterator(Vertex<V, E> const* entry);

  /// Write results to database
  auto storeResults(WorkerConfig* config,
                    std::function<void()> const& statusUpdateCallback)
      -> futures::Future<Result>;

 private:
  void loadVertices(ShardID const& vertexShard,
                    std::vector<ShardID> const& edgeShards,
                    std::function<void()> const& statusUpdateCallback);
  void loadEdges(transaction::Methods& trx, Vertex<V, E>& vertex,
                 ShardID const& edgeShard, std::string const& documentID,
                 std::vector<std::unique_ptr<TypedBuffer<Edge<E>>>>& edges,
                 uint64_t numVertices, traverser::EdgeCollectionInfo& info);

  auto storeVertices(std::vector<ShardID> const& globalShards,
                     RangeIterator<Vertex<V, E>>& it, size_t threadNumber,
                     std::function<void()> const& statusUpdateCallback)
      -> Result;
  uint64_t determineVertexIdRangeStart(uint64_t numVertices);

  constexpr size_t vertexSegmentSize() const {
    return 64 * 1024 * 1024 / sizeof(Vertex<V, E>);
  }

  constexpr size_t edgeSegmentSize() const {
    return 64 * 1024 * 1024 / sizeof(Edge<E>);
  }

 private:
  PregelFeature& _feature;
  DatabaseGuard _vocbaseGuard;
  const ExecutionNumber _executionNumber;
  const std::unique_ptr<GraphFormat<V, E>> _graphFormat;
  WorkerConfig* _config = nullptr;

  std::atomic<uint64_t> _vertexIdRangeStart;

  /// Holds vertex keys, data and pointers to edges
  std::mutex _bufferMutex;
  std::vector<std::unique_ptr<TypedBuffer<Vertex<V, E>>>> _vertices;
  std::vector<std::unique_ptr<TypedBuffer<char>>> _vertexKeys;
  std::vector<std::unique_ptr<TypedBuffer<Edge<E>>>> _edges;
  std::vector<TypedBuffer<Edge<E>>*> _nextEdgeBuffer;
  std::vector<std::unique_ptr<TypedBuffer<char>>> _edgeKeys;

  GraphStoreObservables _observables;

  // cache the amount of vertices
  std::set<ShardID> _loadedShards;

  // actual count of loaded vertices / edges
  std::atomic<size_t> _localVertexCount;
  std::atomic<size_t> _localEdgeCount;
  std::atomic<uint32_t> _runningThreads;
};

}  // namespace pregel
}  // namespace arangodb

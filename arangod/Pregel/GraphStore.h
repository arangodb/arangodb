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

#include "Cluster/ClusterInfo.h"
#include "Pregel/Graph.h"
#include "Pregel/GraphFormat.h"
#include "Pregel/Iterators.h"
#include "Pregel/Reports.h"
#include "Pregel/TypedBuffer.h"
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

struct MemRange {
  uint8_t* from;
  uint8_t* to;
};
template<typename V, typename E>
struct ExpandingBuffer {
  std::vector<std::unique_ptr<TypedBuffer<char>>> _storage;
  size_t segmentSize;
  GraphStore<V, E>& parent;

  std::mutex allocLock;

  auto ensureSpace(size_t size) -> void;
  auto allocate(size_t size) -> MemRange;

  ExpandingBuffer(GraphStore<V, E>& parent, size_t segmentSize);
  ExpandingBuffer(ExpandingBuffer const&) = delete;
  ExpandingBuffer(ExpandingBuffer&&) = delete;
  auto operator=(ExpandingBuffer const&) = delete;
  auto operator=(ExpandingBuffer&&) = delete;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief carry graph data for a worker job. NOT THREAD SAFE ON DOCUMENT LOADS
////////////////////////////////////////////////////////////////////////////////
template<typename V, typename E>
class GraphStore final {
 public:
  GraphStore(PregelFeature& feature, TRI_vocbase_t& vocbase,
             uint64_t executionNumber, GraphFormat<V, E>* graphFormat);

  uint64_t numberVertexSegments() const { return _vertices.size(); }
  uint64_t localVertexCount() const { return _localVertexCount; }
  uint64_t localEdgeCount() const { return _localEdgeCount; }
  GraphFormat<V, E> const* graphFormat() { return _graphFormat.get(); }

  // ====================== NOT THREAD SAFE ===========================
  void loadShards(WorkerConfig* state, std::function<void()> const&);
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
  void storeResults(WorkerConfig* config, std::function<void()>);

  auto getConfig() const -> WorkerConfig* {
    ADB_PROD_ASSERT(_config != nullptr);
    return _config;
  }
  auto getFeature() const -> PregelFeature& { return _feature; }

  ReportManager* _reports;

  auto allocateAuxiliaryStorage(size_t bytes) -> MemRange {
    return _auxiliaryStorage.allocate(bytes);
  }

 private:
  void loadVertices(ShardID const& vertexShard,
                    std::vector<ShardID> const& edgeShards);
  void loadEdges(transaction::Methods& trx, Vertex<V, E>& vertex,
                 ShardID const& edgeShard, std::string const& documentID,
                 std::vector<std::unique_ptr<TypedBuffer<Edge<E>>>>& edges,
                 std::vector<std::unique_ptr<TypedBuffer<char>>>& edgeKeys,
                 uint64_t numVertices, traverser::EdgeCollectionInfo& info);

  void storeVertices(std::vector<ShardID> const& globalShards,
                     RangeIterator<Vertex<V, E>>& it, size_t threadNumber);

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
  uint64_t const _executionNumber;
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

  //
  // FIXME: This is a hack;
  //
  // For some algorithms, for example WCC, we need to store dynamic
  // data; if we just allow heap allocations, we lose control over
  // state being spilled to memory mapped files.
  //
  // This buffer is currently here to address this problem; the solution
  // is not ideal, and will hopefully be improved in future iterations
  // on the code.
  //
  // note that ExpandingBuffer does locking, because it might be used
  // concurrently by multiple threads.
  //
  ExpandingBuffer<V, E> _auxiliaryStorage;

  // cache the amount of vertices
  std::set<ShardID> _loadedShards;

  // actual count of loaded vertices / edges
  std::atomic<size_t> _localVertexCount;
  std::atomic<size_t> _localEdgeCount;
  std::atomic<uint32_t> _runningThreads;
};

}  // namespace pregel
}  // namespace arangodb

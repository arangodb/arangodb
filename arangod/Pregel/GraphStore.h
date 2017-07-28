////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGODB_PREGEL_GRAPH_STORE_H
#define ARANGODB_PREGEL_GRAPH_STORE_H 1

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <set>
#include "Basics/Mutex.h"
#include "Cluster/ClusterInfo.h"
#include "Pregel/Graph.h"
#include "Pregel/GraphFormat.h"
#include "Pregel/Iterators.h"
#include "Pregel/TypedBuffer.h"

struct TRI_vocbase_t;

namespace arangodb {
class LogicalCollection;
namespace transaction {
class Methods;
}
namespace pregel {

template <typename T>
struct TypedBuffer;
class WorkerConfig;
template <typename V, typename E>
struct GraphFormat;

////////////////////////////////////////////////////////////////////////////////
/// @brief carry graph data for a worker job. NOT THREAD SAFE ON DOCUMENT LOADS
////////////////////////////////////////////////////////////////////////////////
template <typename V, typename E>
class GraphStore {

 public:
  GraphStore(TRI_vocbase_t* vocbase, GraphFormat<V, E>* graphFormat);
  ~GraphStore();

  uint64_t localVertexCount() const { return _localVerticeCount; }
  uint64_t localEdgeCount() const { return _localEdgeCount; }
  GraphFormat<V, E> const* graphFormat() { return _graphFormat.get(); }

  // ====================== NOT THREAD SAFE ===========================
  void loadShards(WorkerConfig* state, std::function<void()> callback);
  void loadDocument(WorkerConfig* config, std::string const& documentID);
  void loadDocument(WorkerConfig* config, PregelShard sourceShard,
                    PregelKey const& _key);
  // ======================================================================

  // only thread safe if your threads coordinate access to memory locations
  RangeIterator<VertexEntry> vertexIterator();
  RangeIterator<VertexEntry> vertexIterator(size_t start, size_t count);
  RangeIterator<Edge<E>> edgeIterator(VertexEntry const* entry);

  /// get the pointer to the vertex
  V* mutableVertexData(VertexEntry const* entry);
  /// does nothing currently
  void replaceVertexData(VertexEntry const* entry, void* data, size_t size);

  /// Write results to database
  void storeResults(WorkerConfig* config, std::function<void()> callback);
  
private:
  std::unordered_map<ShardID, uint64_t> _preallocateMemory();
  void _loadVertices(size_t i, ShardID const& vertexShard,
                     std::vector<ShardID> const& edgeShards,
                     uint64_t vertexOffset);
  void _loadEdges(transaction::Methods* trx, ShardID const& shard,
                  VertexEntry& vertexEntry, std::string const& documentID);
  void _storeVertices(std::vector<ShardID> const& globalShards,
                      RangeIterator<VertexEntry>& it);
  std::unique_ptr<transaction::Methods> _createTransaction();
  
private:
  VocbaseGuard _vocbaseGuard;
  const std::unique_ptr<GraphFormat<V, E>> _graphFormat;
  WorkerConfig* _config = nullptr;
  
  /// Holds vertex keys and pointers to vertex data and edges
  std::vector<VertexEntry> _index;
  /// Vertex data
  TypedBuffer<V>* _vertexData = nullptr;
  /// Edges (and data)
  TypedBuffer<Edge<E>>* _edges = nullptr;
  
  // cache the amount of vertices
  std::set<ShardID> _loadedShards;
  // hold the current position where the ith vertex shard can
  // start to write its data. At the end the offset should equal the
  // sum of the counts of all ith edge shards
  std::vector<uint64_t> _edgeShardsOffset;
  
  // actual count of loaded vertices / edges
  std::atomic<uint64_t> _localVerticeCount;
  std::atomic<uint64_t> _localEdgeCount;
  std::atomic<uint32_t> _runningThreads;
  bool _destroyed = false;
};
}
}
#endif

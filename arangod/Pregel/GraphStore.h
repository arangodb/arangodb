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

struct TRI_vocbase_t;

namespace arangodb {
class Transaction;
class LogicalCollection;
namespace pregel {

class WorkerConfig;
template <typename V, typename E>
struct GraphFormat;

////////////////////////////////////////////////////////////////////////////////
/// @brief carry graph data for a worker job. NOT THREAD SAFE ON DOCUMENT LOADS
////////////////////////////////////////////////////////////////////////////////
template <typename V, typename E>
class GraphStore {
  VocbaseGuard _vocbaseGuard;
  const std::unique_ptr<GraphFormat<V, E>> _graphFormat;
  WorkerConfig *_config;
  
  // int _indexFd, _vertexFd, _edgeFd;
  // void *_indexMapping, *_vertexMapping, *_edgeMapping;
  // size_t _indexSize, _vertexSize, _edgeSize;
  // std::map<std::string, std::string> _shardsPlanIdMap;

  // only for demo, move to memory
  std::vector<VertexEntry> _index;
  std::vector<V> _vertexData;
  std::vector<Edge<E>> _edges;

  std::set<ShardID> _loadedShards;
  std::atomic<uint64_t> _localVerticeCount;
  std::atomic<uint64_t> _localEdgeCount;
  std::atomic<uint32_t> _runningThreads;
  mutable Mutex _threadMutex;

  void _loadVertices(ShardID const& vertexShard,
                     std::vector<ShardID> const& edgeShards,
                     uint64_t vertexOffset, uint64_t edgeOffset);
  void _loadEdges(Transaction* trx, ShardID const& shard,
                  VertexEntry& vertexEntry, std::string const& documentID);
  std::unique_ptr<Transaction> _createTransaction();
  bool _destroyed = false;

 public:
  GraphStore(TRI_vocbase_t* vocbase, GraphFormat<V, E>* graphFormat);
  ~GraphStore();

  uint64_t localVertexCount() const { return _localVerticeCount; }
  uint64_t localEdgeCount() const { return _localEdgeCount; }

  // ====================== NOT THREAD SAFE ===========================
  void loadShards(WorkerConfig* state, std::function<void()> callback);
  void loadDocument(WorkerConfig* config, std::string const& documentID);
  void loadDocument(WorkerConfig* config, prgl_shard_t sourceShard,
                    std::string const& _key);
  // ======================================================================

  // only thread safe if your threads coordinate access to memory locations
  RangeIterator<VertexEntry> vertexIterator();
  RangeIterator<VertexEntry> vertexIterator(size_t start, size_t count);
  RangeIterator<Edge<E>> edgeIterator(VertexEntry const* entry);

  /// get the pointer to the vertex
  void* mutableVertexData(VertexEntry const* entry);
  /// does nothing currently
  void replaceVertexData(VertexEntry const* entry, void* data, size_t size);

  /// Write results to database
  void storeResults(WorkerConfig const& state);
};
}
}
#endif

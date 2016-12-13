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

#include "GraphStore.h"

#include <algorithm>
#include "Basics/Exceptions.h"
#include "Cluster/ClusterInfo.h"
#include "Indexes/EdgeIndex.h"
#include "Indexes/Index.h"
#include "Pregel/WorkerState.h"
#include "Utils.h"
#include "Utils/CollectionNameResolver.h"
#include "Utils/ExplicitTransaction.h"
#include "Utils/OperationCursor.h"
#include "Utils/StandaloneTransactionContext.h"
#include "Utils/Transaction.h"
#include "VocBase/EdgeCollectionInfo.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ticks.h"
#include "VocBase/vocbase.h"

using namespace arangodb;
using namespace arangodb::pregel;

template <typename V, typename E>
GraphStore<V, E>::GraphStore(TRI_vocbase_t* vb, GraphFormat<V, E>* graphFormat)
    : _vocbaseGuard(vb), _graphFormat(graphFormat) {}

template <typename V, typename E>
GraphStore<V, E>::~GraphStore() {
  _cleanupTransactions();
}

template <typename V, typename E>
void GraphStore<V, E>::loadShards(WorkerState const& state) {
  _createReadTransaction(state);

  std::map<CollectionID, std::vector<ShardID>> const& vertexMap =
      state.vertexCollectionShards();
  std::map<CollectionID, std::vector<ShardID>> const& edgeMap =
      state.edgeCollectionShards();
  for (auto const& pair : vertexMap) {
    std::vector<ShardID> const& vertexShards = pair.second;
    for (size_t i = 0; i < vertexShards.size(); i++) {
      // we might have already loaded these shards
      if (_loadedShards.find(vertexShards[i]) != _loadedShards.end()) {
        continue;
      }

      // distributeshardslike should cause the edges for a vertex to be
      // in the same shard index. x in vertexShard2 => E(x) in edgeShard2
      for (auto const& pair2 : edgeMap) {
        std::vector<ShardID> const& edgeShards = pair2.second;

        if (vertexShards.size() != edgeShards.size()) {
          THROW_ARANGO_EXCEPTION_MESSAGE(
              TRI_ERROR_BAD_PARAMETER,
              "Collections need to have the same number of shards");
        }
        _loadVertices(state, vertexShards[i], edgeShards[i]);
        _loadedShards.insert(vertexShards[i]);
      }
    }
  }
  _cleanupTransactions();
}

template <typename V, typename E>
void GraphStore<V, E>::loadDocument(WorkerState const& state,
                                    ShardID const& shard,
                                    std::string const& _key) {
  /*if (_readTrx == nullptr) {
   _createReadTransaction(state);
  }

  prgl_shard_t sourceShard = (prgl_shard_t)state.shardId(shard);
  bool storeData = _graphFormat->storesVertexData();

  VPackBuilder builder;
  builder.openObject();
  builder.add(StaticStrings::KeyString, VPackValue(_key));
  builder.close();

  OperationOptions options;
  options.ignoreRevs = false;

  TRI_voc_cid_t cid = _readTrx->addCollectionAtRuntime(shard);
  _readTrx->orderDitch(cid);  // will throw when it fails
  OperationResult opResult = _readTrx->document(shard, builder.slice(),
  options);
  if (!opResult.successful()) {
    _cleanupTransactions();
    THROW_ARANGO_EXCEPTION(opResult.code);
  }


  VertexEntry entry(sourceShard, _key);
  if (storeData) {
    V vertexData;
    size_t size =
    _graphFormat->copyVertexData(opResult.slice(), &vertexData, sizeof(V));
    if (size > 0) {
      entry._vertexDataOffset = _vertexData.size();
      _vertexData.push_back(vertexData);
    }
  }
  std::string documentId = _readTrx->extractIdString(opResult.slice());
  _loadEdges(state, edgeShard, entry, documentId);
  _index.push_back(entry);*/
}

template <typename V, typename E>
RangeIterator<VertexEntry> GraphStore<V, E>::vertexIterator() {
  return vertexIterator(0, _index.size());
}

template <typename V, typename E>
RangeIterator<VertexEntry> GraphStore<V, E>::vertexIterator(size_t start,
                                                            size_t end) {
  return RangeIterator<VertexEntry>(_index, start, end);
}

template <typename V, typename E>
void* GraphStore<V, E>::mutableVertexData(VertexEntry const* entry) {
  return _vertexData.data() + entry->_vertexDataOffset;
}

template <typename V, typename E>
V GraphStore<V, E>::copyVertexData(VertexEntry const* entry) {
  return _graphFormat->readVertexData(
      mutableVertexData(entry));  //  _vertexData[entry->_vertexDataOffset];
}

template <typename V, typename E>
void GraphStore<V, E>::replaceVertexData(VertexEntry const* entry, void* data,
                                         size_t size) {
  void* ptr = _vertexData.data() + entry->_vertexDataOffset;
  memcpy(ptr, data, size);
}

template <typename V, typename E>
RangeIterator<Edge<E>> GraphStore<V, E>::edgeIterator(
    VertexEntry const* entry) {
  size_t end = entry->_edgeDataOffset + entry->_edgeCount;
  return RangeIterator<Edge<E>>(_edges, entry->_edgeDataOffset, end);
}

template <typename V, typename E>
void GraphStore<V, E>::_createReadTransaction(WorkerState const& state) {
  std::vector<std::string> readColls, writeColls;
  for (auto shard : state.localVertexShardIDs()) {
    readColls.push_back(shard);
  }
  for (auto shard : state.localEdgeShardIDs()) {
    readColls.push_back(shard);
  }
  double lockTimeout =
      (double)(TRI_TRANSACTION_DEFAULT_LOCK_TIMEOUT / 1000000ULL);
  _readTrx = new ExplicitTransaction(
      StandaloneTransactionContext::Create(_vocbaseGuard.vocbase()), readColls,
      writeColls, lockTimeout, false, false);
  int res = _readTrx->begin();
  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION(res);
  }
}

template <typename V, typename E>
void GraphStore<V, E>::_cleanupTransactions() {
  if (_readTrx) {
    if (_readTrx->getStatus() == TRI_TRANSACTION_RUNNING) {
      if (_readTrx->commit() != TRI_ERROR_NO_ERROR) {
        LOG(WARN) << "Pregel worker: Failed to commit on a read transaction";
      }
    }
    delete _readTrx;
    _readTrx = nullptr;
  }
}

template <typename V, typename E>
void GraphStore<V, E>::_loadVertices(WorkerState const& state,
                                     ShardID const& vertexShard,
                                     ShardID const& edgeShard) {
  //_graphFormat->willUseCollection(vocbase, vertexShard, false);
  bool storeData = _graphFormat->storesVertexData();

  TRI_voc_cid_t cid = _readTrx->addCollectionAtRuntime(vertexShard);
  _readTrx->orderDitch(cid);  // will throw when it fails
  prgl_shard_t sourceShard = (prgl_shard_t)state.shardId(vertexShard);

  /*int res = _readTrx->lockRead();
  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION_FORMAT(res, "while looking up vertices '%s'",
                                  vertexShard.c_str());
  }*/

  ManagedDocumentResult mmdr(_readTrx);
  std::unique_ptr<OperationCursor> cursor = _readTrx->indexScan(
      vertexShard, Transaction::CursorType::ALL, Transaction::IndexHandle(), {},
      &mmdr, 0, UINT64_MAX, 1000, false);

  if (cursor->failed()) {
    THROW_ARANGO_EXCEPTION_FORMAT(cursor->code, "while looking up shard '%s'",
                                  vertexShard.c_str());
  }

  LogicalCollection* collection = cursor->collection();
  std::vector<IndexLookupResult> result;
  result.reserve(1000);

  while (cursor->hasMore()) {
    cursor->getMoreMptr(result, 1000);
    for (auto const& element : result) {
      TRI_voc_rid_t revisionId = element.revisionId();
      if (collection->readRevision(_readTrx, mmdr, revisionId)) {
        VPackSlice document(mmdr.vpack());
        if (document.isExternal()) {
          document = document.resolveExternal();
        }

        // LOG(INFO) << "Loaded Vertex: " << document.toJson();
        std::string key = document.get(StaticStrings::KeyString).copyString();

        VertexEntry entry(sourceShard, key);
        if (storeData) {
          V vertexData;
          size_t size =
              _graphFormat->copyVertexData(document, &vertexData, sizeof(V));
          if (size > 0) {
            entry._vertexDataOffset = _vertexData.size();
            _vertexData.push_back(vertexData);
          } else {
            LOG(ERR) << "Could not load vertex " << document.toJson();
          }
        }

        std::string documentId = _readTrx->extractIdString(document);
        _loadEdges(state, edgeShard, entry, documentId);
        _index.push_back(entry);
      }
    }
  }
}

template <typename V, typename E>
void GraphStore<V, E>::_loadEdges(WorkerState const& state,
                                  ShardID const& edgeShard,
                                  VertexEntry& vertexEntry,
                                  std::string const& documentID) {
  const bool storeData = _graphFormat->storesEdgeData();

  // Transaction* trx = readTransaction(shard);
  traverser::EdgeCollectionInfo info(_readTrx, edgeShard, TRI_EDGE_OUT,
                                     StaticStrings::FromString, 0);

  ManagedDocumentResult mmdr(_readTrx);
  auto cursor = info.getEdges(documentID, &mmdr);
  if (cursor->failed()) {
    THROW_ARANGO_EXCEPTION_FORMAT(cursor->code,
                                  "while looking up edges '%s' from %s",
                                  documentID.c_str(), edgeShard.c_str());
  }

  LogicalCollection* collection = cursor->collection();
  std::vector<IndexLookupResult> result;
  result.reserve(1000);

  while (cursor->hasMore()) {
    if (vertexEntry._edgeCount == 0) {
      vertexEntry._edgeDataOffset = _edges.size();
    }

    cursor->getMoreMptr(result, 1000);
    for (auto const& element : result) {
      TRI_voc_rid_t revisionId = element.revisionId();
      if (collection->readRevision(_readTrx, mmdr, revisionId)) {
        VPackSlice document(mmdr.vpack());
        if (document.isExternal()) {
          document = document.resolveExternal();
        }

        // ====== actual loading ======
        vertexEntry._edgeCount += 1;

        // LOG(INFO) << "Loaded Edge: " << document.toJson();
        std::string toValue =
            document.get(StaticStrings::ToString).copyString();

        std::size_t pos = toValue.find('/');
        std::string collectionName = toValue.substr(0, pos);
        std::string _key = toValue.substr(pos + 1, toValue.length() - pos - 1);

        auto collInfo = Utils::resolveCollection(
            state.database(), collectionName, state.collectionPlanIdMap());

        ShardID responsibleShard;
        Utils::resolveShard(collInfo.get(), StaticStrings::KeyString, _key,
                            responsibleShard);
        prgl_shard_t source = (prgl_shard_t)state.shardId(edgeShard);
        prgl_shard_t target = (prgl_shard_t)state.shardId(responsibleShard);
        if (source == (prgl_shard_t)-1 || target == (prgl_shard_t)-1) {
          LOG(ERR) << "Unknown shard";
          continue;
        }

        Edge<E> edge(source, target, _key);
        if (storeData) {
          // size_t size =
          _graphFormat->copyEdgeData(document, edge.data(), sizeof(E));
          // TODO store size value at some point
        }
        _edges.push_back(edge);
      }
    }
  }

  /*res = trx.finish(res);
  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION_FORMAT(res, "while looking up edges from '%s'",
                                  edgeShard.c_str());
  }*/
}

template <typename V, typename E>
void GraphStore<V, E>::storeResults(WorkerState const& state) {
  std::vector<std::string> readColls, writeColls;
  for (auto shard : state.localVertexShardIDs()) {
    writeColls.push_back(shard);
  }
  for (auto shard : state.localEdgeShardIDs()) {
    writeColls.push_back(shard);
  }
  // for (auto shard : state.localEdgeShardIDs() ) {
  //  writeColls.push_back(shard);
  //}
  double lockTimeout =
      (double)(TRI_TRANSACTION_DEFAULT_LOCK_TIMEOUT / 1000000ULL);
  ExplicitTransaction writeTrx(
      StandaloneTransactionContext::Create(_vocbaseGuard.vocbase()), readColls,
      writeColls, lockTimeout, false, false);
  int res = writeTrx.begin();
  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION(res);
  }

  OperationOptions options;
  for (VertexEntry const& vertexEntry : _index) {
    ShardID const& shard = state.globalShardIDs()[vertexEntry.shard()];
    void* data = mutableVertexData(&vertexEntry);

    VPackBuilder b;
    b.openObject();
    b.add(StaticStrings::KeyString, VPackValue(vertexEntry.key()));
    _graphFormat->buildVertexDocument(b, data, sizeof(V));
    b.close();

    OperationResult result = writeTrx.update(shard, b.slice(), options);
    if (result.code != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION(result.code);
    }

    // TODO loop over edges
  }
  res = writeTrx.finish(res);
  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION(res);
  }
}

template class arangodb::pregel::GraphStore<int64_t, int64_t>;
template class arangodb::pregel::GraphStore<float, float>;

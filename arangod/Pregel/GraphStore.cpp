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
#include "Basics/Common.h"
#include "Basics/MutexLocker.h"
#include "Basics/ThreadPool.h"
#include "Cluster/ClusterInfo.h"
#include "Indexes/EdgeIndex.h"
#include "Indexes/Index.h"
#include "Pregel/CommonFormats.h"
#include "Pregel/PregelFeature.h"
#include "Pregel/Utils.h"
#include "Pregel/WorkerConfig.h"
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
    : _vocbaseGuard(vb),
      _graphFormat(graphFormat),
      _localVerticeCount(0),
      _localEdgeCount(0),
      _runningThreads(0) {}

template <typename V, typename E>
GraphStore<V, E>::~GraphStore() {
  _destroyed = true;
}

template <typename V, typename E>
void GraphStore<V, E>::loadShards(WorkerConfig* config,
                                  std::function<void()> callback) {
  _config = config;
  
  double t = TRI_microtime();
  std::unique_ptr<Transaction> countTrx(_createTransaction());
  std::map<ShardID, uint64_t> shardSizes;
  // Allocating some memory
  uint64_t count = 0;
  for (auto const& shard : _config->localVertexShardIDs()) {
    OperationResult opResult = countTrx->count(shard, true);
    if (opResult.failed()) {
      return;
    }
    shardSizes[shard] = opResult.slice().getUInt();
    count += opResult.slice().getUInt();
  }
  _index.resize(count);
  if (_graphFormat->estimatedVertexSize() > 0) {
    _vertexData.resize(count);
  }
  count = 0;
  for (auto const& shard : _config->localEdgeShardIDs()) {
    OperationResult opResult = countTrx->count(shard, true);
    if (opResult.failed()) {
      return;
    }
    shardSizes[shard] = opResult.slice().getUInt();
    count += opResult.slice().getUInt();
  }
  _edges.resize(count);
  if (countTrx->commit() != TRI_ERROR_NO_ERROR) {
    LOG(WARN) << "Pregel worker: Failed to commit on a read transaction";
  }
  LOG(INFO) << "Allocating memory took " << TRI_microtime() - t << "s";
  

  basics::ThreadPool* pool = PregelFeature::instance()->threadPool();
  uint64_t vertexOffset = 0, edgeOffset = 0;
  // Contains the shards located on this db server in the right order
  std::map<CollectionID, std::vector<ShardID>> const& vertexCollMap =
      _config->vertexCollectionShards();
  std::map<CollectionID, std::vector<ShardID>> const& edgeCollMap =
      _config->edgeCollectionShards();

  for (auto const& pair : vertexCollMap) {
    std::vector<ShardID> const& vertexShards = pair.second;
    for (size_t i = 0; i < vertexShards.size(); i++) {
      // we might have already loaded these shards
      if (_loadedShards.find(vertexShards[i]) != _loadedShards.end()) {
        continue;
      }
      ShardID const& vertexShard(vertexShards[i]);

      uint64_t nextEdgeOffset = edgeOffset;
      std::vector<ShardID> edgeLookups;
      // distributeshardslike should cause the edges for a vertex to be
      // in the same shard index. x in vertexShard2 => E(x) in edgeShard2
      for (auto const& pair2 : edgeCollMap) {
        std::vector<ShardID> const& edgeShards = pair2.second;
        if (vertexShards.size() != edgeShards.size()) {
          THROW_ARANGO_EXCEPTION_MESSAGE(
              TRI_ERROR_BAD_PARAMETER,
              "Collections need to have the same number of shards");
        }
        edgeLookups.push_back(edgeShards[i]);
        nextEdgeOffset += shardSizes[edgeShards[i]];
      }

      _loadedShards.insert(vertexShard);
      _runningThreads++;
      pool->enqueue([this, &vertexShard, edgeLookups, vertexOffset,
                     edgeOffset, callback] {
        
        _loadVertices(vertexShard, edgeLookups, vertexOffset,
                      edgeOffset);
        
        MUTEX_LOCKER(guard, _threadMutex);
        if (--_runningThreads == 0) {
          if (_localEdgeCount < _edges.size()) {
            _edges.resize(_localEdgeCount);
          }
          callback();
        }
      });
      // update to next offset
      vertexOffset += shardSizes[vertexShard];
      edgeOffset = nextEdgeOffset;
    }
  }
}

template <typename V, typename E>
void GraphStore<V, E>::loadDocument(WorkerConfig* config,
                                    std::string const& documentID) {
  PregelID _id = config->documentIdToPregel(documentID);
  if (config->isLocalVertexShard(_id.shard)) {
    loadDocument(config, _id.shard, _id.key);
  }
}

template <typename V, typename E>
void GraphStore<V, E>::loadDocument(WorkerConfig* config,
                                    prgl_shard_t sourceShard,
                                    std::string const& _key) {
  _config = config;
  std::unique_ptr<Transaction> trx(_createTransaction());

  ShardID const& vertexShard = _config->globalShardIDs()[sourceShard];
  VPackBuilder builder;
  builder.openObject();
  builder.add(StaticStrings::KeyString, VPackValue(_key));
  builder.close();

  OperationOptions options;
  options.ignoreRevs = false;

  TRI_voc_cid_t cid = trx->addCollectionAtRuntime(vertexShard);
  trx->orderDitch(cid);  // will throw when it fails
  OperationResult opResult = trx->document(vertexShard, builder.slice(), options);
  if (!opResult.successful()) {
    THROW_ARANGO_EXCEPTION(opResult.code);
  }

  std::string documentId = trx->extractIdString(opResult.slice());
  _index.emplace_back(sourceShard, _key);
  _localVerticeCount++;

  VertexEntry& entry = _index.back();
  if (_graphFormat->estimatedVertexSize() > 0) {
    entry._vertexDataOffset = _vertexData.size();
    entry._edgeDataOffset = _edges.size();

    // allocate space
    _vertexData.push_back(V());
    V& data = _vertexData.back();
    _graphFormat->copyVertexData(documentId, opResult.slice(), &data,
                                 sizeof(V));
  }

  // load edges
  std::map<CollectionID, std::vector<ShardID>> const& vertexMap =
      _config->vertexCollectionShards();
  std::map<CollectionID, std::vector<ShardID>> const& edgeMap =
      _config->edgeCollectionShards();
  for (auto const& pair : vertexMap) {
    std::vector<ShardID> const& vertexShards = pair.second;
    auto it = std::find(vertexShards.begin(), vertexShards.end(), vertexShard);
    if (it != vertexShards.end()) {
      size_t pos = (size_t)(it - vertexShards.begin());
      for (auto const& pair2 : edgeMap) {
        std::vector<ShardID> const& edgeShards = pair2.second;
        _loadEdges(trx.get(), edgeShards[pos], entry, documentId);
      }
      break;
    }
  }
  if (trx->commit() != TRI_ERROR_NO_ERROR) {
    LOG(WARN) << "Pregel worker: Failed to commit on a read transaction";
  }
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
void GraphStore<V, E>::replaceVertexData(VertexEntry const* entry, void* data,
                                         size_t size) {
  // if (size <= entry->_vertexDataOffset)
  void* ptr = _vertexData.data() + entry->_vertexDataOffset;
  memcpy(ptr, data, size);
  LOG(WARN) << "Don't use this function with varying sizes";
}

template <typename V, typename E>
RangeIterator<Edge<E>> GraphStore<V, E>::edgeIterator(
    VertexEntry const* entry) {
  size_t end = entry->_edgeDataOffset + entry->_edgeCount;
  return RangeIterator<Edge<E>>(_edges, entry->_edgeDataOffset, end);
}

template <typename V, typename E>
std::unique_ptr<Transaction> GraphStore<V, E>::_createTransaction() {
  double lockTimeout =
      (double)(TRI_TRANSACTION_DEFAULT_LOCK_TIMEOUT / 1000000ULL);
  std::unique_ptr<Transaction> trx (new ExplicitTransaction(StandaloneTransactionContext::Create(_vocbaseGuard.vocbase()),
  {}, {}, lockTimeout, false, true));
  int res = trx->begin();
  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION(res);
  }
  return trx;
}

template <typename V, typename E>
void GraphStore<V, E>::_loadVertices(ShardID const& vertexShard,
                                     std::vector<ShardID> const& edgeShards,
                                     uint64_t vertexOffset,
                                     uint64_t edgeOffset) {
  
  std::unique_ptr<Transaction> trx(_createTransaction());
  TRI_voc_cid_t cid = trx->addCollectionAtRuntime(vertexShard);
  trx->orderDitch(cid);  // will throw when it fails
  prgl_shard_t sourceShard = (prgl_shard_t)_config->shardId(vertexShard);

  ManagedDocumentResult mmdr(trx.get());
  std::unique_ptr<OperationCursor> cursor = trx->indexScan(
      vertexShard, Transaction::CursorType::ALL, Transaction::IndexHandle(), {},
      &mmdr, 0, UINT64_MAX, 1000, false);

  if (cursor->failed()) {
    THROW_ARANGO_EXCEPTION_FORMAT(cursor->code, "while looking up shard '%s'",
                                  vertexShard.c_str());
  }
  LogicalCollection* collection = cursor->collection();
  uint64_t number = collection->numberDocuments();
  _graphFormat->willLoadVertices(number);

  std::vector<IndexLookupResult> result;
  result.reserve(1000);
  while (cursor->hasMore()) {
    cursor->getMoreMptr(result, 1000);
    for (auto const& element : result) {
      TRI_voc_rid_t revisionId = element.revisionId();
      if (collection->readRevision(trx.get(), mmdr, revisionId)) {
        VPackSlice document(mmdr.vpack());
        if (document.isExternal()) {
          document = document.resolveExternal();
        }

        VertexEntry& ventry = _index[vertexOffset];
        ventry._shard = sourceShard;
        ventry._key = document.get(StaticStrings::KeyString).copyString();
        _localVerticeCount++;

        // load vertex data
        std::string documentId = trx->extractIdString(document);
        if (_graphFormat->estimatedVertexSize() > 0) {
          ventry._vertexDataOffset = vertexOffset;
          V& data = _vertexData.back();
          _graphFormat->copyVertexData(documentId, document, &data, sizeof(V));
        }
        // load edges
        for (ShardID const& it : edgeShards) {
          ventry._edgeDataOffset = edgeOffset;
          _loadEdges(trx.get(), it, ventry, documentId);
        }
        vertexOffset++;
        edgeOffset += ventry._edgeCount;
      }
    }
  }

  if (trx->commit() != TRI_ERROR_NO_ERROR) {
    LOG(WARN) << "Pregel worker: Failed to commit on a read transaction";
  }
}

template <typename V, typename E>
void GraphStore<V, E>::_loadEdges(Transaction* trx,
                                  ShardID const& edgeShard,
                                  VertexEntry& vertexEntry,
                                  std::string const& documentID) {
  size_t offset = vertexEntry._edgeDataOffset;

  traverser::EdgeCollectionInfo info(trx, edgeShard, TRI_EDGE_OUT,
                                     StaticStrings::FromString, 0);

  ManagedDocumentResult mmdr(trx);
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
    cursor->getMoreMptr(result, 1000);
    for (auto const& element : result) {
      TRI_voc_rid_t revisionId = element.revisionId();
      if (collection->readRevision(trx, mmdr, revisionId)) {
        VPackSlice document(mmdr.vpack());
        if (document.isExternal()) {
          document = document.resolveExternal();
        }

        // LOG(INFO) << "Loaded Edge: " << document.toJson();
        std::string toValue =
            document.get(StaticStrings::ToString).copyString();
        std::size_t pos = toValue.find('/');
        std::string collectionName = toValue.substr(0, pos);

        // If this is called from loadDocument we didn't preallocate the vector
        if (_edges.size() == offset) {
          _edges.push_back(Edge<E>());
        }
        Edge<E>& edge = _edges[offset];
        edge._toKey = toValue.substr(pos + 1, toValue.length() - pos - 1);

        auto collInfo = Utils::resolveCollection(
            _config->database(), collectionName, _config->collectionPlanIdMap());
        // resolve the shard of the target vertex.
        ShardID responsibleShard;
        Utils::resolveShard(collInfo.get(), StaticStrings::KeyString,
                            edge._toKey, responsibleShard);
        edge._sourceShard = (prgl_shard_t)_config->shardId(edgeShard);
        edge._targetShard = (prgl_shard_t)_config->shardId(responsibleShard);
        if (edge._sourceShard == (prgl_shard_t)-1 ||
            edge._targetShard == (prgl_shard_t)-1) {
          LOG(ERR) << "Unknown shard";
          continue;
        }

        //  store into the edge store, edgeCount is 0 initally
        offset++;
        _graphFormat->copyEdgeData(document, _edges.back().data(), sizeof(E));
      }
    }
    vertexEntry._edgeCount = offset - vertexEntry._edgeDataOffset;
    _localEdgeCount += vertexEntry._edgeCount;
  }
}

template <typename V, typename E>
void GraphStore<V, E>::storeResults(WorkerConfig const& state) {
  double start = TRI_microtime();

  std::vector<std::string> readColls, writeColls;
  for (auto const& shard : state.localVertexShardIDs()) {
    writeColls.push_back(shard);
  }
  for (auto const& shard : state.localEdgeShardIDs()) {
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

  auto it = _index.begin();
  while (it != _index.end()) {
    TransactionBuilderLeaser b(&writeTrx);

    prgl_shard_t shardID = it->shard();
    b->openArray();
    size_t buffer = 1000;
    while (it != _index.end() && buffer > 0) {
      if (it->shard() != shardID) {
        break;
      }

      V* data = _vertexData.data() + it->_vertexDataOffset;
      b->openObject();
      b->add(StaticStrings::KeyString, VPackValue(it->key()));
      // bool store =
      _graphFormat->buildVertexDocument(*(b.get()), data, sizeof(V));
      b->close();

      ++it;
      buffer--;
    }
    b->close();

    ShardID const& shard = state.globalShardIDs()[shardID];
    OperationResult result = writeTrx.update(shard, b->slice(), options);
    if (result.code != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION(result.code);
    }

    // TODO loop over edges
  }
  res = writeTrx.finish(res);
  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION(res);
  }

  LOG(INFO) << "Storing data took " << (TRI_microtime() - start) << "s";
}

template class arangodb::pregel::GraphStore<int64_t, int64_t>;
template class arangodb::pregel::GraphStore<float, float>;
template class arangodb::pregel::GraphStore<double, float>;
template class arangodb::pregel::GraphStore<double, double>;

// specific algo combos
template class arangodb::pregel::GraphStore<SCCValue, int32_t>;

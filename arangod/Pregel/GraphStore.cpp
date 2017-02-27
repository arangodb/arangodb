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
#include "Cluster/ClusterInfo.h"
#include "Pregel/CommonFormats.h"
#include "Pregel/PregelFeature.h"
#include "Pregel/ThreadPool.h"
#include "Pregel/Utils.h"
#include "Pregel/WorkerConfig.h"
#include "Utils/CollectionNameResolver.h"
#include "Utils/ExplicitTransaction.h"
#include "Utils/OperationCursor.h"
#include "Utils/OperationOptions.h"
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
  usleep(25 * 1000);
}

template <typename V, typename E>
std::map<ShardID, uint64_t> GraphStore<V, E>::_allocateMemory() {
  double t = TRI_microtime();
  std::unique_ptr<Transaction> countTrx(_createTransaction());
  std::map<ShardID, uint64_t> shardSizes;

  // Allocating some memory
  uint64_t count = 0;
  for (auto const& shard : _config->localVertexShardIDs()) {
    OperationResult opResult = countTrx->count(shard, true);
    if (opResult.failed()) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_BAD_PARAMETER);
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
      THROW_ARANGO_EXCEPTION(TRI_ERROR_BAD_PARAMETER);
    }
    shardSizes[shard] = opResult.slice().getUInt();
    count += opResult.slice().getUInt();
  }
  _edges.resize(count);
  if (countTrx->commit() != TRI_ERROR_NO_ERROR) {
    LOG_TOPIC(WARN, Logger::PREGEL)
        << "Pregel worker: Failed to commit on a read transaction";
  }
  LOG_TOPIC(INFO, Logger::PREGEL) << "Allocating memory took "
                                  << TRI_microtime() - t << "s";

  return shardSizes;
}

template <typename V, typename E>
void GraphStore<V, E>::loadShards(WorkerConfig* config,
                                  std::function<void()> callback) {
  _config = config;
  std::map<ShardID, uint64_t> shardSizes(_allocateMemory());

  ThreadPool* pool = PregelFeature::instance()->threadPool();
  uint64_t vertexOffset = 0, edgeOffset = 0;
  // Contains the shards located on this db server in the right order
  std::map<CollectionID, std::vector<ShardID>> const& vertexCollMap =
      _config->vertexCollectionShards();
  std::map<CollectionID, std::vector<ShardID>> const& edgeCollMap =
      _config->edgeCollectionShards();

  _runningThreads = config->localVertexShardIDs().size();
  LOG_TOPIC(INFO, Logger::PREGEL) << "Using " << _runningThreads
                                  << " threads to load data";
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
      pool->enqueue([this, &vertexShard, edgeLookups, vertexOffset, edgeOffset,
                     callback] {

        _loadVertices(vertexShard, edgeLookups, vertexOffset, edgeOffset);
        {
          MUTEX_LOCKER(guard, _threadMutex);
          _runningThreads--;
          if (_runningThreads == 0) {
            if (_localEdgeCount < _edges.size()) {
              _edges.resize(_localEdgeCount);
            }
            callback();
          }
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
  OperationResult opResult =
      trx->document(vertexShard, builder.slice(), options);
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
    LOG_TOPIC(WARN, Logger::PREGEL)
        << "Pregel worker: Failed to commit on a read transaction";
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
V* GraphStore<V, E>::mutableVertexData(VertexEntry const* entry) {
  return _vertexData.data() + entry->_vertexDataOffset;
}

template <typename V, typename E>
void GraphStore<V, E>::replaceVertexData(VertexEntry const* entry, void* data,
                                         size_t size) {
  // if (size <= entry->_vertexDataOffset)
  void* ptr = _vertexData.data() + entry->_vertexDataOffset;
  memcpy(ptr, data, size);
  LOG_TOPIC(WARN, Logger::PREGEL)
      << "Don't use this function with varying sizes";
}

template <typename V, typename E>
RangeIterator<Edge<E>> GraphStore<V, E>::edgeIterator(
    VertexEntry const* entry) {
  size_t end = entry->_edgeDataOffset + entry->_edgeCount;
  return RangeIterator<Edge<E>>(_edges, entry->_edgeDataOffset, end);
}

template <typename V, typename E>
std::unique_ptr<Transaction> GraphStore<V, E>::_createTransaction() {
  double lockTimeout = Transaction::DefaultLockTimeout;
  auto ctx = StandaloneTransactionContext::Create(_vocbaseGuard.vocbase());
  std::unique_ptr<Transaction> trx(
      new ExplicitTransaction(ctx, {}, {}, {}, lockTimeout, false, true));
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
  uint64_t originalVertexOffset = vertexOffset;

  std::unique_ptr<Transaction> trx(_createTransaction());
  TRI_voc_cid_t cid = trx->addCollectionAtRuntime(vertexShard);
  trx->orderDitch(cid);  // will throw when it fails
  prgl_shard_t sourceShard = (prgl_shard_t)_config->shardId(vertexShard);

  ManagedDocumentResult mmdr;
  std::unique_ptr<OperationCursor> cursor = trx->indexScan(
      vertexShard, Transaction::CursorType::ALL, Transaction::IndexHandle(), {},
      &mmdr, 0, UINT64_MAX, 1000, false);
  if (cursor->failed()) {
    THROW_ARANGO_EXCEPTION_FORMAT(cursor->code, "while looking up shard '%s'",
                                  vertexShard.c_str());
  }

  // tell the formatter the number of docs we are about to load
  LogicalCollection* collection = cursor->collection();
  uint64_t number = collection->numberDocuments();
  _graphFormat->willLoadVertices(number);

  auto cb = [&](DocumentIdentifierToken const& token) {
    if (collection->readDocument(trx.get(), mmdr, token)) {
      VPackSlice document(mmdr.vpack());
      if (document.isExternal()) {
        document = document.resolveExternal();
      }

      VertexEntry& ventry = _index[vertexOffset];
      ventry._shard = sourceShard;
      ventry._key = document.get(StaticStrings::KeyString).copyString();
      ventry._edgeDataOffset = edgeOffset;

      // load vertex data
      std::string documentId = trx->extractIdString(document);
      if (_graphFormat->estimatedVertexSize() > 0) {
        ventry._vertexDataOffset = vertexOffset;
        V* ptr = _vertexData.data() + vertexOffset;
        _graphFormat->copyVertexData(documentId, document, ptr, sizeof(V));
      }
      // load edges
      for (ShardID const& it : edgeShards) {
        _loadEdges(trx.get(), it, ventry, documentId);
      }
      vertexOffset++;
      edgeOffset += ventry._edgeCount;
    }
  };
  while (cursor->getMore(cb, 1000)) {
  }

  // Add all new vertices
  _localVerticeCount += (vertexOffset - originalVertexOffset);

  if (trx->commit() != TRI_ERROR_NO_ERROR) {
    LOG_TOPIC(WARN, Logger::PREGEL)
        << "Pregel worker: Failed to commit on a read transaction";
  }
}

template <typename V, typename E>
void GraphStore<V, E>::_loadEdges(Transaction* trx, ShardID const& edgeShard,
                                  VertexEntry& vertexEntry,
                                  std::string const& documentID) {
  //  offset into the edge store, edgeCount is 0 initally
  size_t offset = vertexEntry._edgeDataOffset + vertexEntry._edgeCount;
  size_t originalOffset = offset;

  traverser::EdgeCollectionInfo info(trx, edgeShard, TRI_EDGE_OUT,
                                     StaticStrings::FromString, 0);
  ManagedDocumentResult mmdr;
  std::unique_ptr<OperationCursor> cursor = info.getEdges(documentID, &mmdr);
  if (cursor->failed()) {
    THROW_ARANGO_EXCEPTION_FORMAT(cursor->code,
                                  "while looking up edges '%s' from %s",
                                  documentID.c_str(), edgeShard.c_str());
  }

  LogicalCollection* collection = cursor->collection();
  auto cb = [&](DocumentIdentifierToken const& token) {
    if (collection->readDocument(trx, mmdr, token)) {
      VPackSlice document(mmdr.vpack());
      if (document.isExternal()) {
        document = document.resolveExternal();
      }

      // LOG_TOPIC(INFO, Logger::PREGEL) << "Loaded Edge: " <<
      // document.toJson();
      std::string toValue = document.get(StaticStrings::ToString).copyString();
      std::size_t pos = toValue.find('/');
      std::string collectionName = toValue.substr(0, pos);

      // If this is called from loadDocument we didn't preallocate the vector
      if (_edges.size() <= offset) {
        if (!_config->lazyLoading()) {
          LOG_TOPIC(ERR, Logger::PREGEL) << "WTF";
        }
        _edges.push_back(Edge<E>());
      }
      Edge<E>& edge(_edges[offset]);
      edge._toKey = toValue.substr(pos + 1, toValue.length() - pos - 1);

      auto collInfo = Utils::resolveCollection(
          _config->database(), collectionName, _config->collectionPlanIdMap());
      if (collInfo) {
        // resolve the shard of the target vertex.
        ShardID responsibleShard;
        Utils::resolveShard(collInfo.get(), StaticStrings::KeyString,
                            edge._toKey, responsibleShard);
        prgl_shard_t sourceShard = (prgl_shard_t)_config->shardId(edgeShard);
        edge._targetShard = (prgl_shard_t)_config->shardId(responsibleShard);
        _graphFormat->copyEdgeData(document, edge.data(), sizeof(E));
        if (sourceShard == (prgl_shard_t)-1 ||
            edge._targetShard == (prgl_shard_t)-1) {
          return;
        }
        offset++;
      }
    }
  };
  while (cursor->getMore(cb, 1000)) {
  }

  // Add up all added elements
  size_t added = offset - originalOffset;
  vertexEntry._edgeCount += added;
  _localEdgeCount += added;
}

/// Loops over the array starting a new transaction for different shards
/// Should not dead-lock unless we have to wait really long for other threads
template <typename V, typename E>
void GraphStore<V, E>::_storeVertices(std::vector<ShardID> const& globalShards,
                                      RangeIterator<VertexEntry>& it) {
  // transaction on one shard
  std::unique_ptr<ExplicitTransaction> trx;
  prgl_shard_t currentShard = (prgl_shard_t)-1;
  int res = TRI_ERROR_NO_ERROR;

  // loop over vertices
  while (it != it.end()) {
    if (it->shard() != currentShard) {
      if (trx) {
        res = trx->finish(res);
        if (res != TRI_ERROR_NO_ERROR) {
          THROW_ARANGO_EXCEPTION(res);
        }
      }
      currentShard = it->shard();
      ShardID const& shard = globalShards[currentShard];
      trx.reset(new ExplicitTransaction(
          StandaloneTransactionContext::Create(_vocbaseGuard.vocbase()), {},
          {shard}, {}, Transaction::DefaultLockTimeout, false, false));
      res = trx->begin();
      if (res != TRI_ERROR_NO_ERROR) {
        THROW_ARANGO_EXCEPTION(res);
      }
    }

    TransactionBuilderLeaser b(trx.get());
    b->openArray();
    size_t buffer = 0;
    while (it != it.end() && buffer < 1000) {
      if (it->shard() != currentShard) {
        break;
      }

      V* data = _vertexData.data() + it->_vertexDataOffset;
      b->openObject();
      b->add(StaticStrings::KeyString, VPackValue(it->key()));
      // bool store =
      _graphFormat->buildVertexDocument(*(b.get()), data, sizeof(V));
      b->close();

      ++it;
      ++buffer;
    }
    b->close();

    ShardID const& shard = globalShards[currentShard];
    OperationOptions options;
    OperationResult result = trx->update(shard, b->slice(), options);
    if (result.code != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION(result.code);
    }
  }

  if (trx) {
    res = trx->finish(res);
    if (res != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION(res);
    }
  }
}

template <typename V, typename E>
void GraphStore<V, E>::storeResults(WorkerConfig const& state) {
  double now = TRI_microtime();

  // for (auto const& shard : state.localEdgeShardIDs()) {
  //  writeColls.push_back(shard);
  //}
  std::atomic<size_t> tCount(0);
  size_t total = _index.size();
  size_t delta =
      std::max(10UL, _index.size() / state.localVertexShardIDs().size());
  size_t start = 0, end = delta;

  do {
    tCount++;
    ThreadPool* pool = PregelFeature::instance()->threadPool();
    pool->enqueue([this, start, end, &state, &tCount] {
      try {
        RangeIterator<VertexEntry> it = vertexIterator(start, end);
        _storeVertices(state.globalShardIDs(), it);
      } catch (...) {
        LOG_TOPIC(ERR, Logger::PREGEL) << "Storing vertex data failed";
      }
      tCount--;
    });
    start = end;
    end = end + delta;
    if (total < end + delta) {  // swallow the rest
      end = total;
    }
  } while (start != end);

  while (tCount > 0 && !_destroyed) {
    usleep(25 * 1000);  // 25ms
  }
  LOG_TOPIC(INFO, Logger::PREGEL) << "Storing data took "
                                  << (TRI_microtime() - now) << "s";
}

template class arangodb::pregel::GraphStore<int64_t, int64_t>;
template class arangodb::pregel::GraphStore<float, float>;
template class arangodb::pregel::GraphStore<double, float>;
template class arangodb::pregel::GraphStore<double, double>;

// specific algo combos
template class arangodb::pregel::GraphStore<SCCValue, int8_t>;
template class arangodb::pregel::GraphStore<ECValue, int8_t>;
template class arangodb::pregel::GraphStore<HITSValue, int8_t>;
template class arangodb::pregel::GraphStore<DMIDValue, float>;

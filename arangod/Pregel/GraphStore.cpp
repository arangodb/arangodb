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

#include "Basics/Exceptions.h"
#include "Cluster/ClusterInfo.h"
#include "Indexes/EdgeIndex.h"
#include "Indexes/Index.h"
#include "Utils.h"
#include "Utils/OperationCursor.h"
#include "Utils/ExplicitTransaction.h"
#include "Utils/StandaloneTransactionContext.h"
#include "Utils/Transaction.h"
#include "VocBase/EdgeCollectionInfo.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ticks.h"
#include "VocBase/vocbase.h"
#include "WorkerState.h"

using namespace arangodb;
using namespace arangodb::pregel;

template <typename V, typename E>
GraphStore<V, E>::GraphStore(TRI_vocbase_t* vb, WorkerState const& state,
                             GraphFormat<V, E>* graphFormat)
    : _vocbaseGuard(vb), _graphFormat(graphFormat) {
//  _edgeCollection = ClusterInfo::instance()->getCollection(
//      vb->name(), state->edgeCollectionPlanId());
  
  loadShards(state);
  LOG(INFO) << "Loaded " << _index.size() << "vertices and " << _edges.size() << " edges";
}

template <typename V, typename E>
GraphStore<V, E>::~GraphStore() {
  cleanupTransactions();
}

template <typename V, typename E>
void GraphStore<V, E>::loadShards(WorkerState const& state) {
  
  std::vector<std::string> readColls, writeColls;
  for (auto const& pair : state.vertexCollectionShards()) {
    for (auto const& shard : pair.second) {
      readColls.push_back(shard);
    }
  }
  for (auto const& pair : state.edgeCollectionShards()) {
    for (auto const& shard : pair.second) {
      readColls.push_back(shard);
    }
  }
  double lockTimeout =
  (double)(TRI_TRANSACTION_DEFAULT_LOCK_TIMEOUT / 1000000ULL);
  _transaction = new ExplicitTransaction(StandaloneTransactionContext::Create(_vocbaseGuard.vocbase()),
                                         readColls, writeColls,
                                         lockTimeout, false, false);
  int res = _transaction->begin();
  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION(res);
  }
  
  std::map<CollectionID, std::vector<ShardID>> const& vertexMap = state.vertexCollectionShards();
  std::map<CollectionID, std::vector<ShardID>> const& edgeMap = state.edgeCollectionShards();
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
        TRI_ASSERT(vertexShards.size() == edgeShards.size());
        loadVertices(vertexShards[i], edgeShards[i]);
        _loadedShards.insert(vertexShards[i]);
      }
    }
  }
  cleanupTransactions();
}

template <typename V, typename E>
RangeIterator<VertexEntry> GraphStore<V, E>::vertexIterator(size_t start, size_t end) {
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
RangeIterator<EdgeEntry<E>> GraphStore<V, E>::edgeIterator(VertexEntry const* entry) {
  size_t end = entry->_edgeDataOffset + entry->_edgeCount;
  return RangeIterator<EdgeEntry<E>>(_edges, entry->_edgeDataOffset, end);
}

template <typename V, typename E>
void GraphStore<V, E>::cleanupTransactions() {
  if (_transaction) {
    if (_transaction->getStatus() == TRI_TRANSACTION_RUNNING) {
      if (_transaction->commit() != TRI_ERROR_NO_ERROR) {
        LOG(WARN) << "Pregel worker: Failed to commit on a read transaction";
      }
    }
    delete _transaction;
    _transaction = nullptr;
  }
}

template <typename V, typename E>
void GraphStore<V, E>::loadVertices(ShardID const& vertexShard, ShardID const& edgeShard) {
  //_graphFormat->willUseCollection(vocbase, vertexShard, false);
  bool storeData = _graphFormat->storesVertexData();

  TRI_voc_cid_t cid = _transaction->addCollectionAtRuntime(vertexShard);
  _transaction->orderDitch(cid);  // will throw when it fails

  /*int res = _transaction->lockRead();
  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION_FORMAT(res, "while looking up vertices '%s'",
                                  vertexShard.c_str());
  }*/

  ManagedDocumentResult mmdr(_transaction);
  std::unique_ptr<OperationCursor> cursor = _transaction->indexScan(
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
      if (collection->readRevision(_transaction, mmdr, revisionId)) {
        VPackSlice document(mmdr.vpack());
        if (document.isExternal()) {
          document = document.resolveExternal();
        }

        LOG(INFO) << "Loaded Vertex: " << document.toJson();
        VertexID vertexId = _transaction->extractIdString(document);
        VertexEntry entry(vertexId);
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
        loadEdges(edgeShard, entry);
        _index.push_back(entry);
      }
    }
  }
}

template <typename V, typename E>
void GraphStore<V, E>::loadEdges(ShardID const& shard, VertexEntry& vertexEntry) {
  //_graphFormat->willUseCollection(vocbase, edgeShard, true);
  const bool storeData = _graphFormat->storesEdgeData();
  std::string const& _from = vertexEntry.vertexID();
  const std::string _key = Utils::vertexKeyFromToValue(_from);
  

  /*ShardID shard;
  Utils::resolveShard(_edgeCollection.get(), Utils::edgeShardingKey,
                      _key, shard);*/

  //Transaction* trx = readTransaction(shard);
  traverser::EdgeCollectionInfo info(_transaction, shard, TRI_EDGE_OUT,
                                     StaticStrings::FromString, 0);

  ManagedDocumentResult mmdr(_transaction);
  auto cursor = info.getEdges(_from, &mmdr);
  if (cursor->failed()) {
    THROW_ARANGO_EXCEPTION_FORMAT(cursor->code,
                                  "while looking up edges '%s' from %s",
                                  _from.c_str(), shard.c_str());
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
      if (collection->readRevision(_transaction, mmdr, revisionId)) {
        VPackSlice document(mmdr.vpack());
        if (document.isExternal()) {
          document = document.resolveExternal();
        }

        // ====== actual loading ======
        LOG(INFO) << "Loaded Edge: " << document.toJson();
        std::string toVertexID =
            document.get(StaticStrings::ToString).copyString();
        vertexEntry._edgeCount += 1;

        if (storeData) {
          E edgeData;
          size_t size =
              _graphFormat->copyEdgeData(document, &edgeData, sizeof(E));
          if (size > 0) {
            _edges.emplace_back(toVertexID, edgeData);
          } else {
            LOG(ERR) << "Trouble when reading data for edge "
                     << document.toJson();
          }
        } else {
          _edges.emplace_back(toVertexID);
        }
      }
    }
  }

  /*res = trx.finish(res);
  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION_FORMAT(res, "while looking up edges from '%s'",
                                  edgeShard.c_str());
  }*/
}

/*template <typename V, typename E>
SingleCollectionTransaction* GraphStore<V, E>::writeTransaction(ShardID const& shard) {
  
  auto it = _transactions.find(shard);
  if (it != _transactions.end()) {
    return it->second;
  } else {
    auto trx = std::make_unique<SingleCollectionTransaction>(
                                                             StandaloneTransactionContext::Create(_vocbaseGuard.vocbase()),
                                                             shard,
                                                             TRI_TRANSACTION_WRITE);
    int res = trx->begin();
    if (res != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION_FORMAT(res, "during transaction of shard '%s'",
                                    shard.c_str());
    }
    _transactions[shard] = trx.get();
    return trx.release();
  }
}*/

template <typename V, typename E>
void GraphStore<V,E>::storeResults(WorkerState const& state) {
  
  std::vector<std::string> readColls, writeColls;
  for (auto shard : state.localVertexShardIDs() ) {
    writeColls.push_back(shard);
  }
  //for (auto shard : _workerState->localEdgeShardIDs() ) {
  //  writeColls(shard);
  //}
  double lockTimeout =
  (double)(TRI_TRANSACTION_DEFAULT_LOCK_TIMEOUT / 1000000ULL);
  _transaction = new ExplicitTransaction(StandaloneTransactionContext::Create(_vocbaseGuard.vocbase()),
                                         readColls, writeColls,
                                         lockTimeout, false, false);
  int res = _transaction->begin();
  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION(res);
  }
  
  OperationOptions options;
  for (auto& vertexEntry : _index) {
    
    std::string _from = vertexEntry.vertexID();
    std::string _key = Utils::vertexKeyFromToValue(_from);
    std::string collection = Utils::collectionFromToValue(_from);
    auto collInfo = Utils::resolveCollection(state.database(),
                                             collection,
                                             state.collectionPlanIdMap());
    if (!collInfo) {
      LOG(ERR) << "Could not resolve collection";
      break;
    }
    
    std::string shard;
    Utils::resolveShard(collInfo.get(), StaticStrings::KeyString, _key, shard);
    
    void* data = mutableVertexData(&vertexEntry);
    VPackBuilder b;
    b.openObject();
    b.add(StaticStrings::KeyString, VPackValue(_key));
    _graphFormat->buildVertexDocument(b, data, sizeof(V));
    b.close();
    
    OperationResult result = _transaction->update(shard, b.slice(), options);
    if (result.code != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION(result.code);
    }
  }
  cleanupTransactions();
}


template class arangodb::pregel::GraphStore<int64_t, int64_t>;
template class arangodb::pregel::GraphStore<float, float>;

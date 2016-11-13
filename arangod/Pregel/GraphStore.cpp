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
#include "Utils/SingleCollectionTransaction.h"
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
GraphStore<V, E>::GraphStore(TRI_vocbase_t* vocbase, const WorkerState* state,
                             GraphFormat<V, E>* graphFormat)
    : _vocbaseGuard(vocbase), _workerState(state), _graphFormat(graphFormat) {
  _edgeCollection = ClusterInfo::instance()->getCollection(
      vocbase->name(), state->edgeCollectionPlanId());

  for (auto& shard : state->localVertexShardIDs()) {
    loadVertices(shard);
  }
  cleanupTransactions();
}

template <typename V, typename E>
GraphStore<V, E>::~GraphStore() {
  cleanupTransactions();
}

template <typename V, typename E>
std::vector<VertexEntry>& GraphStore<V, E>::vertexIterator() {
  return _index;
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
  ;
  memcpy(ptr, data, size);
}

template <typename V, typename E>
EdgeIterator<E> GraphStore<V, E>::edgeIterator(VertexEntry const* entry) {
  size_t end = entry->_edgeDataOffset + entry->_edgeCount;
  return EdgeIterator<E>(_edges, entry->_edgeDataOffset, end);
}




template <typename V, typename E>
SingleCollectionTransaction* GraphStore<V, E>::edgeTransaction(
                                                               ShardID const& shard) {
  auto it = _transactions.find(shard);
  if (it != _transactions.end()) {
    return it->second;
  } else {
    auto trx = std::make_unique<SingleCollectionTransaction>(
                                                             StandaloneTransactionContext::Create(_vocbaseGuard.vocbase()), shard,
                                                             TRI_TRANSACTION_READ);
    int res = trx->begin();
    if (res != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION_FORMAT(res, "during transaction of shard '%s'",
                                    shard.c_str());
    }
    _transactions[shard] = trx.get();
    return trx.release();
  }
}

template <typename V, typename E>
void GraphStore<V, E>::cleanupTransactions() {
  for (auto const& it : _transactions) {  // clean transactions
    if (it.second->getStatus() == TRI_TRANSACTION_RUNNING) {
      if (it.second->commit() != TRI_ERROR_NO_ERROR) {
        LOG(WARN) << "Pregel worker: Failed to commit on a read transaction";
      }
    }
    delete (it.second);
  }
  _transactions.clear();
}


template <typename V, typename E>
void GraphStore<V, E>::loadVertices(ShardID const& vertexShard) {
  //_graphFormat->willUseCollection(vocbase, vertexShard, false);
  bool storeData = _graphFormat->storesVertexData();

  SingleCollectionTransaction trx(
      StandaloneTransactionContext::Create(_vocbaseGuard.vocbase()),
      vertexShard, TRI_TRANSACTION_READ);

  int res = trx.begin();
  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION_FORMAT(res, "while looking up vertices '%s'",
                                  vertexShard.c_str());
  }

  TRI_voc_cid_t cid = trx.addCollectionAtRuntime(vertexShard);
  trx.orderDitch(cid);  // will throw when it fails

  res = trx.lockRead();
  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION_FORMAT(res, "while looking up vertices '%s'",
                                  vertexShard.c_str());
  }

  ManagedDocumentResult mmdr(&trx);
  std::unique_ptr<OperationCursor> cursor = trx.indexScan(
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
      if (collection->readRevision(&trx, mmdr, revisionId)) {
        VPackSlice document(mmdr.vpack());
        if (document.isExternal()) {
          document = document.resolveExternal();
        }

        LOG(INFO) << "Loaded Vertex: " << document.toJson();

        std::string vertexId = trx.extractIdString(document);
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
        loadEdges(entry);
        _index.push_back(entry);
      }
    }
  }

  res = trx.unlockRead();
  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION_FORMAT(res, "while looking up shard '%s'",
                                  vertexShard.c_str());
  }

  //_shardsPlanIdMap[vertexShard] =
  // trx.documentCollection()->planId_as_string();
  res = trx.finish(res);
  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION_FORMAT(res, "while looking up vertices '%s'",
                                  vertexShard.c_str());
  }
  /*TRI_ASSERT( ==
             _ctx->_vertexCollectionPlanId);*/
}

template <typename V, typename E>
void GraphStore<V, E>::loadEdges(VertexEntry& vertexEntry) {
  //_graphFormat->willUseCollection(vocbase, edgeShard, true);
  const bool storeData = _graphFormat->storesEdgeData();
  std::string const& _from = vertexEntry.vertexID();
  const std::string _key = Utils::vertexKeyFromToValue(_from);
  ShardID shard;
  Utils::resolveShard(_edgeCollection.get(), Utils::edgeShardingKey, _key,
                      shard);

  SingleCollectionTransaction* trx = edgeTransaction(shard);
  traverser::EdgeCollectionInfo info(trx, shard, TRI_EDGE_OUT,
                                     StaticStrings::FromString, 0);

  ManagedDocumentResult mmdr(trx);
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
      if (collection->readRevision(trx, mmdr, revisionId)) {
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

void storeResults() {
  
}

void storeVertices(ShardID const& vertexShard) {
  
}

void storeEdges(VertexEntry& entry) {
  
}

template class arangodb::pregel::GraphStore<int64_t, int64_t>;
template class arangodb::pregel::GraphStore<float, float>;

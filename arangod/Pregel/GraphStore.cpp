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
#include "Cluster/ClusterInfo.h"
#include "Indexes/EdgeIndex.h"
#include "Indexes/Index.h"
#include "Utils/OperationCursor.h"
#include "Utils/SingleCollectionTransaction.h"
#include "Utils/StandaloneTransactionContext.h"
#include "Utils/Transaction.h"
#include "VocBase/EdgeCollectionInfo.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ticks.h"
#include "VocBase/vocbase.h"

using namespace arangodb;
using namespace arangodb::pregel;

template <typename V, typename E>
GraphStore<V, E>::GraphStore(
    std::string const& vertexCollectionName,
    std::vector<ShardID> const& vertexShards,
    std::vector<ShardID> const& edgeShards, TRI_vocbase_t* vocbase,
    std::shared_ptr<GraphFormat<V, E>> const& graphFormat) {
  for (auto& shard : vertexShards) {
    lookupVertices(shard, vocbase, graphFormat);
  }
  for (auto& shard : edgeShards) {
    lookupEdges(vertexCollectionName, shard, vocbase, graphFormat);
  }
}

template <typename V, typename E>
GraphStore<V, E>::~GraphStore() {}

template <typename V, typename E>
std::vector<VertexEntry>& GraphStore<V, E>::vertexIterator() {
  return _index;
}

template <typename V, typename E>
V* GraphStore<V, E>::vertexData(VertexEntry const* entry) {
  return _vertexData.data() + entry->_vertexDataOffset;
}

template <typename V, typename E>
V GraphStore<V, E>::vertexDataCopy(VertexEntry const* entry) {
  return _vertexData[entry->_vertexDataOffset];
}

template <typename V, typename E>
EdgeIterator<E> GraphStore<V, E>::edgeIterator(VertexEntry const* entry) {
  size_t end = entry->_edgeDataOffset + entry->_edgeCount;
  return EdgeIterator<E>(_edges, entry->_edgeDataOffset, end);
}

template <typename V, typename E>
void GraphStore<V, E>::replaceVertexData(VertexEntry const* entry, V* data) {
  _vertexData[entry->_vertexDataOffset] = *data;
}

template <typename V, typename E>
void GraphStore<V, E>::cleanupReadTransactions() {
  for (auto const& it : _readTrxList) {  // clean transactions
    if (it->getStatus() == TRI_TRANSACTION_RUNNING) {
      if (it->commit() != TRI_ERROR_NO_ERROR) {
        LOG(WARN) << "Pregel worker: Failed to commit on a read transaction";
      }
    }
    delete (it);
  }
  _readTrxList.clear();
}

/*static int64_t _shardCount(ShardID const& shard, TRI_vocbase_t* vocbase) {
    SingleCollectionTransaction
trx(StandaloneTransactionContext::Create(vocbase),
                                    shard,
                                    TRI_TRANSACTION_READ);

    int res = trx.begin();
    if (res != TRI_ERROR_NO_ERROR) {
        THROW_ARANGO_EXCEPTION(res);
    }
    OperationResult opResult = trx.count(shard);
    res = trx.finish(opResult.code);
    if (res != TRI_ERROR_NO_ERROR) {
        THROW_ARANGO_EXCEPTION (res);
    }
    VPackSlice s = opResult.slice();
    TRI_ASSERT(s.isNumber());
    return s.getInt();
}*/

template <typename V, typename E>
void GraphStore<V, E>::lookupVertices(
    ShardID const& vertexShard, TRI_vocbase_t* vocbase,
    std::shared_ptr<GraphFormat<V, E>> const& graphFormat) {
  //    int64_t docCount = _shardCount(vertexShard, vocbase);
  //    if (docCount < 1) {
  //        return;// don't bother
  //   }
  //    uint64_t pregelId = ClusterInfo::instance()->uniqid(docCount);

  SingleCollectionTransaction trx(StandaloneTransactionContext::Create(vocbase),
                                  vertexShard, TRI_TRANSACTION_READ);

  int res = trx.begin();
  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION_FORMAT(res, "while looking up vertices '%s'",
                                  vertexShard.c_str());
  }
  // TODO don't fetch all like this, use curso to load progressively
  OperationResult result =
      trx.all(vertexShard, 0, UINT64_MAX, OperationOptions());
  // Commit or abort.
  res = trx.finish(result.code);
  if (!result.successful()) {
    THROW_ARANGO_EXCEPTION_FORMAT(result.code, "while looking up shard '%s'",
                                  vertexShard.c_str());
  }
  /*if (res != TRI_ERROR_NO_ERROR) {
   THROW_ARANGO_EXCEPTION_FORMAT(res, "while looking up shard '%s'",
   vertexShard.c_str());
   }*/
  VPackSlice vertices = result.slice();
  if (vertices.isExternal()) {
    vertices = vertices.resolveExternal();
  }
  //_readTrxList.push_back(trx);// store transactions, otherwise VPackSlices
  // become invalid

  VPackArrayIterator arr = VPackArrayIterator(vertices);
  LOG(INFO) << "Found vertices: " << arr.size();
  for (auto it : arr) {
    if (it.isExternal()) {
      it = it.resolveExternal();
    }

    V vertexData;
    size_t size = graphFormat->copyVertexData(it, &vertexData, sizeof(V));
    if (size > 0) {
      VertexEntry entry(StaticStrings::KeyString);
      entry._vertexDataOffset = _index.size();
      _index.push_back(entry);
      _vertexData.push_back(vertexData);
      LOG(INFO) << "Loaded vertex " << it.toJson();
    } else {
      LOG(ERR) << "Could not load vertex " << it.toJson();
    }
  }

  /*TRI_ASSERT(trx.>documentCollection()->planId_as_string() ==
             _ctx->_vertexCollectionPlanId);*/
}

template <typename V, typename E>
void GraphStore<V, E>::lookupEdges(
    std::string vertexCollectionName, ShardID const& edgeShardID,
    TRI_vocbase_t* vocbase,
    std::shared_ptr<GraphFormat<V, E>> const& graphFormat) {
  std::unique_ptr<SingleCollectionTransaction> trx(
      new SingleCollectionTransaction(
          StandaloneTransactionContext::Create(vocbase), edgeShardID,
          TRI_TRANSACTION_READ));
  int res = trx->begin();
  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION_FORMAT(res, "while looking up edges '%s'",
                                  edgeShardID.c_str());
  }

  auto info = std::make_unique<arangodb::traverser::EdgeCollectionInfo>(
      trx.get(), edgeShardID, TRI_EDGE_OUT, StaticStrings::FromString, 0);

  for (auto& vertexEntry : _index) {
    // kann man strings besser verketten?
    std::string _from = vertexCollectionName + "/" + vertexEntry.vertexID();

    auto cursor = info->getEdges(_from);
    if (cursor->failed()) {
      THROW_ARANGO_EXCEPTION_FORMAT(cursor->code,
                                    "while looking up edges '%s' from %s",
                                    _from.c_str(), edgeShardID.c_str());
    }

    vertexEntry._edgeDataOffset = _edges.size();
    vertexEntry._edgeCount = 0;

    std::vector<TRI_doc_mptr_t*> result;
    result.reserve(1000);
    while (cursor->hasMore()) {
      cursor->getMoreMptr(result, 1000);
      for (auto const& mptr : result) {
        VPackSlice document(mptr->vpack());
        if (document.isExternal()) {
          document = document.resolveExternal();
        }
        LOG(INFO) << "Loaded Edge: " << document.toJson();

        E edgeData;
        size_t size = graphFormat->copyEdgeData(document, &edgeData, sizeof(E));
        std::string toVertexID =
            document.get(StaticStrings::ToString).copyString();
        if (size > 0) {
          _edges.emplace_back(toVertexID, edgeData);
          vertexEntry._edgeCount += 1;
        } else {
          LOG(ERR) << "Could not load edge " << document.toJson();
        }
      }
    }
  }

  //_readTrxList.push_back(trx.get());
  // trx.release();
}

template class arangodb::pregel::GraphStore<int64_t, int64_t>;

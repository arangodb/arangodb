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

#include "Basics/Common.h"
#include "Basics/MutexLocker.h"
#include "Pregel/CommonFormats.h"
#include "Pregel/IndexHelpers.h"
#include "Pregel/PregelFeature.h"
#include "Pregel/TypedBuffer.h"
#include "Pregel/Utils.h"
#include "Pregel/WorkerConfig.h"
#include "Scheduler/Scheduler.h"
#include "Scheduler/SchedulerFeature.h"
#include "Transaction/Context.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/CollectionNameResolver.h"
#include "Utils/SingleCollectionTransaction.h"
#include "Utils/OperationCursor.h"
#include "Utils/OperationOptions.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ManagedDocumentResult.h"
#include "VocBase/ticks.h"
#include "VocBase/vocbase.h"

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

#include <algorithm>

using namespace arangodb;
using namespace arangodb::pregel;

template <typename V, typename E>
GraphStore<V, E>::GraphStore(TRI_vocbase_t& vb, GraphFormat<V, E>* graphFormat)
    : _vocbaseGuard(vb),
      _graphFormat(graphFormat),
      _localVertexCount(0),
      _localEdgeCount(0),
      _runningThreads(0) {}

template <typename V, typename E>
GraphStore<V, E>::~GraphStore() {
  _destroyed = true;
}

static const char* shardError =
    "Collections need to have the same number of shards"
    " use distributeShardsLike";

template <typename V, typename E>
void GraphStore<V, E>::loadShards(WorkerConfig* config,
                                  std::function<void(bool)> const& cb) {
  _config = config;
  TRI_ASSERT(_runningThreads == 0);
  LOG_TOPIC(DEBUG, Logger::PREGEL)
      << "Using " << config->localVertexShardIDs().size() << " threads to load data. memory-mapping is turned " << (config->useMemoryMaps() ? "on" : "off");
  
  // hold the current position where the ith vertex shard can
  // start to write its data. At the end the offset should equal the
  // sum of the counts of all ith edge shards
  
  
  // Contains the shards located on this db server in the right order
  // assuming edges are sharded after _from, vertices after _key
  // then every ith vertex shard has the corresponding edges in
  // the ith edge shard
  std::map<CollectionID, std::vector<ShardID>> const& vertexCollMap =
  _config->vertexCollectionShards();
  std::map<CollectionID, std::vector<ShardID>> const& edgeCollMap =
  _config->edgeCollectionShards();
  size_t numShards = SIZE_MAX;
  
  for (auto const& pair : vertexCollMap) {
    std::vector<ShardID> const& vertexShards = pair.second;
    if (numShards == SIZE_MAX) {
      numShards = vertexShards.size();
    } else if (numShards != vertexShards.size()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER, shardError);
    }
    
    for (size_t i = 0; i < vertexShards.size(); i++) {
      ShardID const& vertexShard = vertexShards[i];
      
      // distributeshardslike should cause the edges for a vertex to be
      // in the same shard index. x in vertexShard2 => E(x) in edgeShard2
      std::vector<ShardID> edges;
      for (auto const& pair2 : edgeCollMap) {
        std::vector<ShardID> const& edgeShards = pair2.second;
        if (vertexShards.size() != edgeShards.size()) {
          THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER, shardError);
        }
        edges.emplace_back(edgeShards[i]);
      }

      try {
        // we might have already loaded these shards
        if (_loadedShards.find(vertexShard) != _loadedShards.end()) {
          continue;
        }
        _loadedShards.insert(vertexShard);
        _runningThreads++;
        rest::Scheduler* scheduler = SchedulerFeature::SCHEDULER;
        TRI_ASSERT(scheduler);
        scheduler->queue(RequestPriority::LOW, [this, vertexShard,
                                                edges](bool isDirect) {
          TRI_DEFER(_runningThreads--);  // exception safe
          try {
            _loadVertices(vertexShard, edges);
          } catch (std::exception const& ex) {
            LOG_TOPIC(WARN, Logger::PREGEL) << "caught exception while "
                                            << "loading pregel graph: " << ex.what();
          }
        });
      } catch (...) {
        LOG_TOPIC(WARN, Logger::PREGEL) << "unhandled exception while "
                                        << "loading pregel graph";
      }
    }

    // we can only load one vertex collection at a time
    while (_runningThreads > 0) {
      std::this_thread::sleep_for(std::chrono::microseconds(5000));
    }
  }

  rest::Scheduler* scheduler = SchedulerFeature::SCHEDULER;
  scheduler->queue(RequestPriority::LOW, cb);
}

template <typename V, typename E>
void GraphStore<V, E>::loadDocument(WorkerConfig* config, std::string const& documentID) {
  // figure out if we got this vertex locally
  PregelID _id = config->documentIdToPregel(documentID);
  if (config->isLocalVertexShard(_id.shard)) {
    loadDocument(config, _id.shard, StringRef(_id.key));
  }
}

template <typename V, typename E>
void GraphStore<V, E>::loadDocument(WorkerConfig* config, PregelShard sourceShard,
                                    StringRef const& _key) {
  TRI_ASSERT(false);
}

template <typename V, typename E>
RangeIterator<Vertex<V, E>> GraphStore<V, E>::vertexIterator() {
  if (_vertices.empty()) {
    return RangeIterator<Vertex<V, E>>(_vertices, 0, nullptr, 0);
  }
  
  TypedBuffer<Vertex<V, E>>* front = _vertices.front().get();
  return RangeIterator<Vertex<V, E>>(_vertices, 0, front->begin(),
                                     _localVertexCount);
}

template <typename V, typename E>
RangeIterator<Vertex<V, E>> GraphStore<V, E>::vertexIterator(size_t i, size_t j) {
  if (_vertices.size() <= i) {
    return RangeIterator<Vertex<V, E>>(_vertices, 0, nullptr, 0);
  }
  
  size_t numVertices = 0;
  for (size_t x = i; x < j && x < _vertices.size(); x++) {
    numVertices += _vertices[x]->size();
  }
  
  return RangeIterator<Vertex<V, E>>(_vertices, i,
                                    _vertices[i]->begin(),
                                     numVertices);
}

template <typename V, typename E>
RangeIterator<Edge<E>> GraphStore<V, E>::edgeIterator(Vertex<V, E> const* entry) {
  if (entry->getEdgeCount() == 0) {
    return RangeIterator<Edge<E>>(_edges, 0, nullptr, 0);
  }
  
  size_t x = _edges.size();
  for (size_t i = 0; i < _edges.size(); i++) {
    if (_edges[i]->begin() <= entry->getEdges() &&
        entry->getEdges() <= _edges[i]->end()) {
      x = i;
      break;
    }
  }
  
  TRI_ASSERT(x < _edges.size());
  TRI_ASSERT(x != _edges.size() - 1 ||
             _edges[x]->size() >= entry->getEdgeCount());
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  size_t x2 = _edges.size();
  for (size_t i = 0; i < _edges.size(); i++) {
    if (_edges[i]->begin() <= entry->_edgesEnd &&
        entry->_edgesEnd <= _edges[i]->end()) {
      x2 = i;
      break;
    }
  }
  TRI_ASSERT(x2 < _edges.size() && x2 >= x);
  size_t size = 0;
  if (x == x2) {
    size += entry->_edgesEnd - entry->_edgesBegin;
  }
  if (x < x2) {
    size += _edges[x]->end() - entry->_edgesBegin;
  }
  if (x + 1 < x2) {
    for (size_t i = x+1; i <= x2-1; i++) {
      size += _edges[x]->size();
    }
  }
  if (x < x2) {
    size += (entry->_edgesEnd - _edges[x2]->begin());
  }
  if (entry->_edgeCount != size) {
    std::this_thread::sleep_for(std::chrono::seconds(10));
  }
  TRI_ASSERT(entry->_edgeCount == size);
#endif
  return RangeIterator<Edge<E>>(_edges, x,
                                static_cast<Edge<E>*>(entry->getEdges()),
                                entry->getEdgeCount());
}

namespace {
template <typename X>
void moveAppend(std::vector<X>& src, std::vector<X>& dst) {
  if (dst.empty()) {
    dst = std::move(src);
  } else {
    dst.reserve(dst.size() + src.size());
    std::move(std::begin(src), std::end(src), std::back_inserter(dst));
    src.clear();
  }
}
  
template<typename M>
std::unique_ptr<TypedBuffer<M>> createBuffer(WorkerConfig const& config, size_t cap) {
  if (config.useMemoryMaps()) {
    auto ptr = std::make_unique<MappedFileBuffer<M>>(cap);
    ptr->sequentialAccess();
    return ptr;
  } else {
    return std::make_unique<VectorTypedBuffer<M>>(cap);
  }
}
}

static constexpr size_t stringChunkSize = 32 * 1024 * 1024 * sizeof(char);

template <typename V, typename E>
void GraphStore<V, E>::_loadVertices(ShardID const& vertexShard,
                                     std::vector<ShardID> const& edgeShards) {
  
  LOG_TOPIC(DEBUG, Logger::PREGEL)
    << "Pregel worker: loading from vertex shard '" << vertexShard << "'";
  
  transaction::Options trxOpts;
  trxOpts.waitForSync = false;
  trxOpts.allowImplicitCollections = true;
  auto ctx = transaction::StandaloneContext::Create(_vocbaseGuard.database());
  transaction::Methods trx(ctx, {}, {}, {}, trxOpts);
  Result res = trx.begin();
  
  if (!res.ok()) {
    THROW_ARANGO_EXCEPTION(res);
  }

  PregelShard sourceShard = (PregelShard)_config->shardId(vertexShard);
  auto cursor = trx.indexScan(vertexShard, transaction::Methods::CursorType::ALL);
  if (cursor->fail()) {
    THROW_ARANGO_EXCEPTION_FORMAT(cursor->code, "while looking up shard '%s'",
                                  vertexShard.c_str());
  }

  // tell the formatter the number of docs we are about to load
  LogicalCollection* coll = cursor->collection();
  uint64_t numVertices = coll->numberDocuments(&trx, transaction::CountType::Normal);
  _graphFormat->willLoadVertices(numVertices);
  
  LOG_TOPIC(DEBUG, Logger::PREGEL) << "Shard '" << vertexShard << "' has "
  << numVertices << " vertices";
  
  std::vector<std::unique_ptr<TypedBuffer<Vertex<V, E>>>> vertices;
  std::vector<std::unique_ptr<TypedBuffer<char>>> vKeys;
  std::vector<std::unique_ptr<TypedBuffer<Edge<E>>>> edges;
  std::vector<std::unique_ptr<TypedBuffer<char>>> eKeys;

  TypedBuffer<Vertex<V, E>>* vertexBuff = nullptr;
  TypedBuffer<char>* keyBuff = nullptr;
  size_t segmentSize = std::min<size_t>(numVertices, vertexSegmentSize());
  
  std::string documentId; // temp buffer for _id of vertex
  auto cb = [&](LocalDocumentId const& token, VPackSlice slice) {
    if (slice.isExternal()) {
      slice = slice.resolveExternal();
    }
    
    if (vertexBuff == nullptr || vertexBuff->remainingCapacity() == 0) {
      vertices.push_back(createBuffer<Vertex<V, E>>(*_config, segmentSize));
      vertexBuff = vertices.back().get();
    }
    
    Vertex<V, E>* ventry = vertexBuff->appendElement();
    VPackValueLength keyLen;
    VPackSlice keySlice = transaction::helpers::extractKeyFromDocument(slice);
    char const* key = keySlice.getString(keyLen);
    if (keyBuff == nullptr || keyLen > keyBuff->remainingCapacity()) {
      TRI_ASSERT(keyLen < stringChunkSize);
      vKeys.push_back(createBuffer<char>(*_config, stringChunkSize));
      keyBuff = vKeys.back().get();
    }
    
    ventry->_shard = sourceShard;
    ventry->_key = keyBuff->end();
    ventry->_keyLength = keyLen;
    // actually copy in the key
    memcpy(/*dst*/keyBuff->end(), /*src*/key, /*n*/keyLen);
    keyBuff->advance(keyLen);
    
    // load vertex data
    documentId = trx.extractIdString(slice);
    if (_graphFormat->estimatedVertexSize() > 0) {
      _graphFormat->copyVertexData(documentId, slice, ventry->_data);
    }
    
    ventry->_edgesBegin = nullptr;
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    ventry->_edgesEnd = nullptr;
#endif
    ventry->_edgeCount = 0;
    // load edges
    for (ShardID const& edgeShard : edgeShards) {
      _loadEdges(trx, *ventry, edgeShard, documentId, edges, eKeys);
    }
  };
  
  _localVertexCount += numVertices;
  bool hasMore = true;
  while(hasMore && numVertices > 0) {
    TRI_ASSERT(segmentSize > 0);
    hasMore = cursor->nextDocument(cb, segmentSize);
    if (_destroyed) {
      LOG_TOPIC(WARN, Logger::PREGEL) << "Aborted loading graph";
      break;
    }
    
    TRI_ASSERT(numVertices >= segmentSize);
    numVertices -= segmentSize;
    LOG_TOPIC(DEBUG, Logger::PREGEL) << "Shard '" << vertexShard << "', "
      << numVertices << " remaining vertices";
    segmentSize = std::min<size_t>(numVertices, vertexSegmentSize());
  }
  
  std::lock_guard<std::mutex> guard(_bufferMutex);
  ::moveAppend(vertices, _vertices);
  ::moveAppend(vKeys, _vertexKeys);
  ::moveAppend(edges, _edges);
  ::moveAppend(eKeys, _edgeKeys);

  LOG_TOPIC(DEBUG, Logger::PREGEL)
  << "Pregel worker: done loading from vertex shard '" << vertexShard << "'";
}

template <typename V, typename E>
void GraphStore<V, E>::_loadEdges(transaction::Methods& trx, Vertex<V, E>& vertex,
                                  ShardID const& edgeShard, std::string const& documentID,
                                  std::vector<std::unique_ptr<TypedBuffer<Edge<E>>>>& edges,
                                  std::vector<std::unique_ptr<TypedBuffer<char>>>& edgeKeys) {

  traverser::EdgeCollectionInfo info(&trx, edgeShard);
  ManagedDocumentResult mmdr;
  std::unique_ptr<OperationCursor> cursor = info.getEdges(documentID, &mmdr);
  if (cursor->fail()) {
    THROW_ARANGO_EXCEPTION_FORMAT(cursor->code,
                                  "while looking up edges '%s' from %s",
                                  documentID.c_str(), edgeShard.c_str());
  }
  
  TypedBuffer<Edge<E>>* edgeBuff = edges.empty() ? nullptr : edges.back().get();
  TypedBuffer<char>* keyBuff = edgeKeys.empty() ? nullptr : edgeKeys.back().get();

  auto allocateSpace = [&](size_t keyLen) {
    if (edgeBuff == nullptr || edgeBuff->remainingCapacity() == 0) {
      edges.push_back(createBuffer<Edge<E>>(*_config, edgeSegmentSize()));
      edgeBuff = edges.back().get();
    }
    if (keyBuff == nullptr || keyLen > keyBuff->remainingCapacity()) {
      TRI_ASSERT(keyLen < stringChunkSize);
      edgeKeys.push_back(createBuffer<char>(*_config, stringChunkSize));
      keyBuff = edgeKeys.back().get();
    }
    return edgeBuff->appendElement();
  };
  
  size_t addedEdges = 0;
  auto buildEdge = [&](Edge<E>* edge, StringRef toValue) {
    ++addedEdges;
    ++vertex._edgeCount;
    if (vertex._edgesBegin == nullptr) {
      vertex._edgesBegin = edge;
      TRI_ASSERT(vertex._edgeCount == 1);
    }
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    ventry._edgesEnd = edge + 1;
#endif
    
    std::size_t pos = toValue.find('/');
    StringRef collectionName = toValue.substr(0, pos);
    StringRef key = toValue.substr(pos + 1);
    edge->_toKey = keyBuff->end();
    edge->_toKeyLength = key.size();
    keyBuff->advance(key.size());

    // actually copy in the key
    memcpy(edge->_toKey, key.data(), key.size());
    
    // resolve the shard of the target vertex.
    ShardID responsibleShard;
    int res = Utils::resolveShard(_config, collectionName.toString(),
                                  StaticStrings::KeyString,
                                  key, responsibleShard);
    if (res != TRI_ERROR_NO_ERROR) {
      LOG_TOPIC(ERR, Logger::PREGEL)
      << "Could not resolve target shard of edge";
      return res;
    }
    
    // PregelShard sourceShard = (PregelShard)_config->shardId(edgeShard);
    edge->_targetShard = (PregelShard)_config->shardId(responsibleShard);
    if (edge->_targetShard == (PregelShard)-1) {
      LOG_TOPIC(ERR, Logger::PREGEL)
      << "Could not resolve target shard of edge";
      return TRI_ERROR_CLUSTER_BACKEND_UNAVAILABLE;
    }
    return TRI_ERROR_NO_ERROR;
  };
  
  // allow for rocksdb edge index optimization
  if (cursor->hasExtra() &&
      _graphFormat->estimatedEdgeSize() == 0) {
    
    auto cb = [&](LocalDocumentId const& token, VPackSlice edgeSlice) {
      TRI_ASSERT(edgeSlice.isString());
      
      StringRef toValue(edgeSlice);
      Edge<E>* edge = allocateSpace(toValue.size());
      buildEdge(edge, toValue);
    };
    while (cursor->nextWithExtra(cb, 1000)) {
      if (_destroyed) {
        LOG_TOPIC(WARN, Logger::PREGEL) << "Aborted loading graph";
        break;
      }
    }
    
  } else {
    auto cb = [&](LocalDocumentId const& token, VPackSlice slice) {
      if (slice.isExternal()) {
        slice = slice.resolveExternal();
      }
      
      StringRef toValue(transaction::helpers::extractToFromDocument(slice));
      Edge<E>* edge = allocateSpace(toValue.size());
      int res = buildEdge(edge, toValue);
      if (res == TRI_ERROR_NO_ERROR) {
        _graphFormat->copyEdgeData(slice, edge->data());
      }
    };
    while (cursor->nextDocument(cb, 1000)) {
      if (_destroyed) {
        LOG_TOPIC(WARN, Logger::PREGEL) << "Aborted loading graph";
        break;
      }
    }
  }
  
  // Add up all added elements
  _localEdgeCount += addedEdges;
}

/// Loops over the array starting a new transaction for different shards
/// Should not dead-lock unless we have to wait really long for other threads
template <typename V, typename E>
void GraphStore<V, E>::_storeVertices(std::vector<ShardID> const& globalShards,
                                      RangeIterator<Vertex<V, E>>& it) {
  // transaction on one shard
  std::unique_ptr<arangodb::SingleCollectionTransaction> trx;
  PregelShard currentShard = (PregelShard)-1;
  Result res = TRI_ERROR_NO_ERROR;
  
  VPackBuilder builder;
  size_t numDocs = 0;
  
  // loop over vertices
  for (; it.hasMore(); ++it) {
    if (it->shard() != currentShard || numDocs >= 1000) {
      if (trx) {
        res = trx->finish(res);
        if (!res.ok()) {
          THROW_ARANGO_EXCEPTION(res);
        }
      }
      
      currentShard = it->shard();
      
      auto ctx = transaction::StandaloneContext::Create(_vocbaseGuard.database());
      ShardID const& shard = globalShards[currentShard];
      transaction::Options to;
      trx.reset(new SingleCollectionTransaction(ctx, shard, AccessMode::Type::WRITE));
      trx->addHint(transaction::Hints::Hint::INTERMEDIATE_COMMITS);
      res = trx->begin();
      if (!res.ok()) {
        THROW_ARANGO_EXCEPTION(res);
      }
      numDocs = 0;
    }
    
    StringRef const key = it->key();
    V const& data = it->data();
    
    builder.clear();
    // This loop will fill a buffer of vertices until we run into a new
    // collection
    // or there are no more vertices for to store (or the buffer is full)
    builder.openObject();
    builder.add(StaticStrings::KeyString, VPackValuePair(key.data(), key.size(),
                                                         VPackValueType::String));
    /// bool store =
    _graphFormat->buildVertexDocument(builder, &data, sizeof(V));
    builder.close();
    
    ++numDocs;
    if (_destroyed) {
      LOG_TOPIC(WARN, Logger::PREGEL)
      << "Storing data was canceled prematurely";
      trx->abort();
      trx.reset();
      break;
    }
        
    ShardID const& shard = globalShards[currentShard];
    OperationOptions options;
    OperationResult result = trx->update(shard, builder.slice(), options);
    if (result.fail() &&
        result.isNot(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND) &&
        result.isNot(TRI_ERROR_ARANGO_CONFLICT)) {
      THROW_ARANGO_EXCEPTION(result.result);
    }
    if (result.is(TRI_ERROR_ARANGO_CONFLICT)) {
       LOG_TOPIC(WARN, Logger::PREGEL) << "conflict while storing " << builder.toJson();
    }
  }
  
  if (trx) {
    res = trx->finish(res);
    if (!res.ok()) {
      THROW_ARANGO_EXCEPTION(res);
    }
  }
}

template <typename V, typename E>
void GraphStore<V, E>::storeResults(WorkerConfig* config,
                                    std::function<void()> cb) {

  _config = config;
  double now = TRI_microtime();
  TRI_ASSERT(SchedulerFeature::SCHEDULER != nullptr);
  
  size_t numSegments = _vertices.size();
  if (_localVertexCount > 100000) {
    _runningThreads = std::min<size_t>(_config->parallelism(), numSegments);
  } else {
    _runningThreads = 1;
  }
  size_t numT = _runningThreads;
  LOG_TOPIC(DEBUG, Logger::PREGEL) << "Storing vertex data using " <<
    numT << " threads";
  
  for (size_t i = 0; i < numT; i++) {
    SchedulerFeature::SCHEDULER->queue(RequestPriority::LOW, [=](bool isDirect) {
      size_t startI = i * (numSegments / numT);
      size_t endI = (i + 1) * (numSegments / numT);
      TRI_ASSERT(endI <= numSegments);
      
      try {
        RangeIterator<Vertex<V, E>> it = vertexIterator(startI, endI);
        _storeVertices(_config->globalShardIDs(), it);
        // TODO can't just write edges with smart graphs
      } catch (std::exception const& e) {
        LOG_TOPIC(ERR, Logger::PREGEL) << "Storing vertex data failed: '" << e.what() << "'";
      } catch (...) {
        LOG_TOPIC(ERR, Logger::PREGEL) << "Storing vertex data failed";
      }
      _runningThreads--;
      if (_runningThreads == 0) {
        LOG_TOPIC(DEBUG, Logger::PREGEL)
        << "Storing data took " << (TRI_microtime() - now) << "s";
        cb();
      }
    });
  }
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
template class arangodb::pregel::GraphStore<LPValue, int8_t>;
template class arangodb::pregel::GraphStore<SLPAValue, int8_t>;

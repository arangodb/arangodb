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
#include "Basics/LocalTaskQueue.h"
#include "Basics/MutexLocker.h"
#include "Indexes/IndexIterator.h"
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
#include "Utils/OperationOptions.h"
#include "Utils/SingleCollectionTransaction.h"
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
#include <memory>

using namespace arangodb;
using namespace arangodb::pregel;

namespace {
static constexpr size_t minStringChunkSize = 16 * 1024 * sizeof(char);
static constexpr size_t maxStringChunkSize = 32 * 1024 * 1024 * sizeof(char);
static constexpr size_t chunkUnit = 4 * 1024 * sizeof(char);

static_assert(minStringChunkSize % chunkUnit == 0, "invalid chunkUnit value");
static_assert(maxStringChunkSize % chunkUnit == 0, "invalid chunkUnit value");

size_t stringChunkSize(size_t /*numberOfChunks*/, uint64_t numVerticesLeft, bool isVertex) {
  // we assume a conservative 64 bytes per document key
  size_t numBytes = numVerticesLeft * 64;
  if (!isVertex) {
    // assume 16 edges per vertex
    numBytes *= 16;
  }
  numBytes = (numBytes + chunkUnit - 1) & (-chunkUnit);
  numBytes = std::max<size_t>(minStringChunkSize, numBytes);
  numBytes = std::min<size_t>(maxStringChunkSize, numBytes);

  TRI_ASSERT(numBytes % chunkUnit == 0);
  return numBytes;
}
} // namespace

template <typename V, typename E>
GraphStore<V, E>::GraphStore(TRI_vocbase_t& vb, GraphFormat<V, E>* graphFormat)
    : _vocbaseGuard(vb),
      _graphFormat(graphFormat),
      _localVertexCount(0),
      _localEdgeCount(0),
      _runningThreads(0) {}

static const char* shardError =
    "Collections need to have the same number of shards,"
    " use distributeShardsLike";

template <typename V, typename E>
void GraphStore<V, E>::loadShards(WorkerConfig* config,
                                  std::function<void()> const& cb) {
  _config = config;
  TRI_ASSERT(_runningThreads == 0);

  LOG_TOPIC("27f1e", DEBUG, Logger::PREGEL)
      << "Using up to " << _config->parallelism()
      << " threads to load data. memory-mapping is turned "
      << (config->useMemoryMaps() ? "on" : "off");

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

  auto poster = [](std::function<void()> fn) -> bool {
    return SchedulerFeature::SCHEDULER->queue(RequestLane::INTERNAL_LOW, fn);
  };
  auto queue =
      std::make_shared<basics::LocalTaskQueue>(_vocbaseGuard.database().server(), poster);
  queue->setConcurrency(_config->parallelism());

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
        Scheduler* scheduler = SchedulerFeature::SCHEDULER;
        TRI_ASSERT(scheduler);
        auto task =
            std::make_shared<basics::LambdaTask>(queue, [this, vertexShard, edges]() -> Result {
              if (_vocbaseGuard.database().server().isStopping()) {
                LOG_TOPIC("4355b", WARN, Logger::PREGEL)
                    << "Aborted loading graph";
                return {TRI_ERROR_SHUTTING_DOWN};
              }
              try {
                _loadVertices(vertexShard, edges);
              } catch (std::exception const& ex) {
                LOG_TOPIC("c87c9", WARN, Logger::PREGEL)
                    << "caught exception while "
                    << "loading pregel graph: " << ex.what();
              }
              return Result();
            });
        queue->enqueue(task);
      } catch (basics::Exception const& ex) {
        LOG_TOPIC("3f283", WARN, Logger::PREGEL)
            << "unhandled exception while "
            << "loading pregel graph: " << ex.what();
      } catch (...) {
        LOG_TOPIC("3f282", WARN, Logger::PREGEL) << "unhandled exception while "
        << "loading pregel graph";
      }
    }
  }
  queue->dispatchAndWait();
  if (queue->status().fail() && !queue->status().is(TRI_ERROR_SHUTTING_DOWN)) {
    THROW_ARANGO_EXCEPTION(queue->status());
  }

  Scheduler* scheduler = SchedulerFeature::SCHEDULER;
  bool queued = scheduler->queue(RequestLane::INTERNAL_LOW, cb);
  if (!queued) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUEUE_FULL,
                                   "No thread available to queue callback, "
                                   "canceling execution");
  }
}

template <typename V, typename E>
void GraphStore<V, E>::loadDocument(WorkerConfig* config, std::string const& documentID) {
  // figure out if we got this vertex locally
  PregelID _id = config->documentIdToPregel(documentID);
  if (config->isLocalVertexShard(_id.shard)) {
    loadDocument(config, _id.shard, VPackStringRef(_id.key));
  }
}

template <typename V, typename E>
void GraphStore<V, E>::loadDocument(WorkerConfig* config, PregelShard sourceShard,
                                    VPackStringRef const& _key) {
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
  
  size_t i = 0;
  for (; i < _edges.size(); i++) {
    if (_edges[i]->begin() <= entry->getEdges() &&
        entry->getEdges() <= _edges[i]->end()) {
      break;
    }
  }
  
  TRI_ASSERT(i < _edges.size());
  TRI_ASSERT(i != _edges.size() - 1 ||
             _edges[i]->size() >= entry->getEdgeCount());
  return RangeIterator<Edge<E>>(_edges, i,
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

template <typename V, typename E>
void GraphStore<V, E>::_loadVertices(ShardID const& vertexShard,
                                     std::vector<ShardID> const& edgeShards) {
  
  LOG_TOPIC("24837", DEBUG, Logger::PREGEL)
    << "Pregel worker: loading from vertex shard " << vertexShard;
  
  transaction::Options trxOpts;
  trxOpts.waitForSync = false;
  trxOpts.allowImplicitCollectionsForRead = true;
  auto ctx = transaction::StandaloneContext::Create(_vocbaseGuard.database());
  transaction::Methods trx(ctx, {}, {}, {}, trxOpts);
  Result res = trx.begin();
  
  if (!res.ok()) {
    THROW_ARANGO_EXCEPTION(res);
  }

  PregelShard sourceShard = (PregelShard)_config->shardId(vertexShard);
  auto cursor = trx.indexScan(vertexShard, transaction::Methods::CursorType::ALL);

  // tell the formatter the number of docs we are about to load
  LogicalCollection* coll = cursor->collection();
  uint64_t numVertices = coll->numberDocuments(&trx, transaction::CountType::Normal);
  _graphFormat->willLoadVertices(numVertices);
  
  LOG_TOPIC("7c31f", DEBUG, Logger::PREGEL) << "Shard '" << vertexShard << "' has "
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
    slice = slice.resolveExternal();
    
    if (vertexBuff == nullptr || vertexBuff->remainingCapacity() == 0) {
      vertices.push_back(createBuffer<Vertex<V, E>>(*_config, segmentSize));
      vertexBuff = vertices.back().get();
    }
    
    Vertex<V, E>* ventry = vertexBuff->appendElement();
    VPackValueLength keyLen;
    VPackSlice keySlice = transaction::helpers::extractKeyFromDocument(slice);
    char const* key = keySlice.getString(keyLen);
    if (keyBuff == nullptr || keyLen > keyBuff->remainingCapacity()) {
      TRI_ASSERT(keyLen < ::maxStringChunkSize);
      vKeys.push_back(createBuffer<char>(*_config, ::stringChunkSize(vKeys.size(), numVertices, true)));
      keyBuff = vKeys.back().get();
    }
   
    ventry->setShard(sourceShard);
    ventry->setKey(keyBuff->end(), static_cast<uint16_t>(keyLen));
    ventry->setActive(true);
    TRI_ASSERT(keyLen <= std::numeric_limits<uint16_t>::max());
    
    // actually copy in the key
    memcpy(keyBuff->end(), key, keyLen);
    keyBuff->advance(keyLen);
    
    // load vertex data
    documentId = trx.extractIdString(slice);
    if (_graphFormat->estimatedVertexSize() > 0) {
      _graphFormat->copyVertexData(documentId, slice, ventry->_data);
    }
    
    ventry->_edges = nullptr;
    ventry->_edgeCount = 0;
    // load edges
    for (ShardID const& edgeShard : edgeShards) {
      _loadEdges(trx, *ventry, edgeShard, documentId, edges, eKeys, numVertices);
    }
    return true;
  };
  
  _localVertexCount += numVertices;
  bool hasMore = true;
  while (hasMore && numVertices > 0) {
    TRI_ASSERT(segmentSize > 0);
    hasMore = cursor->nextDocument(cb, segmentSize);
    if (_vocbaseGuard.database().server().isStopping()) {
      LOG_TOPIC("4355a", WARN, Logger::PREGEL) << "Aborted loading graph";
      break;
    }
    
    TRI_ASSERT(numVertices >= segmentSize);
    numVertices -= segmentSize;
    LOG_TOPIC("b9ed9", DEBUG, Logger::PREGEL) << "Shard '" << vertexShard << "', "
      << numVertices << " remaining vertices";
    segmentSize = std::min<size_t>(numVertices, vertexSegmentSize());
  }
  
  std::lock_guard<std::mutex> guard(_bufferMutex);
  ::moveAppend(vertices, _vertices);
  ::moveAppend(vKeys, _vertexKeys);
  ::moveAppend(edges, _edges);
  ::moveAppend(eKeys, _edgeKeys);

  LOG_TOPIC("6d389", DEBUG, Logger::PREGEL)
    << "Pregel worker: done loading from vertex shard " << vertexShard;
}

template <typename V, typename E>
void GraphStore<V, E>::_loadEdges(transaction::Methods& trx, Vertex<V, E>& vertex,
                                  ShardID const& edgeShard, std::string const& documentID,
                                  std::vector<std::unique_ptr<TypedBuffer<Edge<E>>>>& edges,
                                  std::vector<std::unique_ptr<TypedBuffer<char>>>& edgeKeys,
                                  size_t numVertices) {

  traverser::EdgeCollectionInfo info(&trx, edgeShard);
  ManagedDocumentResult mmdr;
  auto cursor = info.getEdges(documentID);
  
  TypedBuffer<Edge<E>>* edgeBuff = edges.empty() ? nullptr : edges.back().get();
  TypedBuffer<char>* keyBuff = edgeKeys.empty() ? nullptr : edgeKeys.back().get();

  auto allocateSpace = [&](size_t keyLen) {
    if (edgeBuff == nullptr || edgeBuff->remainingCapacity() == 0) {
      edges.push_back(createBuffer<Edge<E>>(*_config, edgeSegmentSize()));
      edgeBuff = edges.back().get();
    }
    if (keyBuff == nullptr || keyLen > keyBuff->remainingCapacity()) {
      TRI_ASSERT(keyLen < ::maxStringChunkSize);
      edgeKeys.push_back(createBuffer<char>(*_config, ::stringChunkSize(edgeKeys.size(), numVertices, false)));
      keyBuff = edgeKeys.back().get();
    }
  };
  
  size_t addedEdges = 0;
  auto buildEdge = [&](Edge<E>* edge, VPackStringRef toValue) {
    ++addedEdges;
    if (++(vertex._edgeCount) == 1) {
      vertex._edges = edge;
    }
    
    std::size_t pos = toValue.find('/');
    VPackStringRef collectionName = toValue.substr(0, pos);
    VPackStringRef key = toValue.substr(pos + 1);
    edge->_toKey = keyBuff->end();
    edge->_toKeyLength = static_cast<uint16_t>(key.size());
    TRI_ASSERT(key.size() <= std::numeric_limits<uint16_t>::max());
    keyBuff->advance(key.size());

    // actually copy in the key
    memcpy(edge->_toKey, key.data(), key.size());
    
    // resolve the shard of the target vertex.
    ShardID responsibleShard;
    auto& ci = trx.vocbase().server().getFeature<ClusterFeature>().clusterInfo();
    int res = Utils::resolveShard(ci, _config, collectionName.toString(),
                                  StaticStrings::KeyString, key, responsibleShard);
    if (res != TRI_ERROR_NO_ERROR) {
      LOG_TOPIC("b80ba", ERR, Logger::PREGEL)
      << "Could not resolve target shard of edge";
      return res;
    }
    
    // PregelShard sourceShard = (PregelShard)_config->shardId(edgeShard);
    edge->_targetShard = (PregelShard)_config->shardId(responsibleShard);
    if (edge->_targetShard == InvalidPregelShard) {
      LOG_TOPIC("1f413", ERR, Logger::PREGEL)
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
      
      VPackStringRef toValue(edgeSlice);
      size_t space = toValue.size();
      allocateSpace(space);
      Edge<E>* edge = edgeBuff->appendElement();
      buildEdge(edge, toValue);
      return true;
    };
    while (cursor->nextExtra(cb, 1000)) {
      if (_vocbaseGuard.database().server().isStopping()) {
        LOG_TOPIC("29018", WARN, Logger::PREGEL) << "Aborted loading graph";
        break;
      }
    }
    
  } else {
    auto cb = [&](LocalDocumentId const& token, VPackSlice slice) {
      slice = slice.resolveExternal();
      
      VPackStringRef toValue(transaction::helpers::extractToFromDocument(slice));
      allocateSpace(toValue.size());
      Edge<E>* edge = edgeBuff->appendElement();
      int res = buildEdge(edge, toValue);
      if (res == TRI_ERROR_NO_ERROR) {
        _graphFormat->copyEdgeData(slice, edge->data());
      }
      return true;
    };
    while (cursor->nextDocument(cb, 1000)) {
      if (_vocbaseGuard.database().server().isStopping()) {
        LOG_TOPIC("191f5", WARN, Logger::PREGEL) << "Aborted loading graph";
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
  OperationOptions options;
  options.silent = true;
  options.waitForSync = false;

  std::unique_ptr<arangodb::SingleCollectionTransaction> trx;

  ShardID shard;
  PregelShard currentShard = InvalidPregelShard;
  Result res;
  
  VPackBuilder builder;
  size_t numDocs = 0;

  auto commitTransaction = [&]() {
    if (trx) {
      builder.close();

      OperationResult opRes = trx->update(shard, builder.slice(), options);
      if (!opRes.countErrorCodes.empty()) {
        int code = (*(opRes.countErrorCodes.begin())).first;
        if (opRes.countErrorCodes.size() > 1) {
          // more than a single error code. let's just fail this
          THROW_ARANGO_EXCEPTION(code);
        }
        // got only a single error code. now let's use it, whatever it is.
        opRes.result.reset(code);
      }

      if (opRes.fail() &&
          opRes.isNot(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND) &&
          opRes.isNot(TRI_ERROR_ARANGO_CONFLICT)) {
        THROW_ARANGO_EXCEPTION(opRes.result);
      }
      if (opRes.is(TRI_ERROR_ARANGO_CONFLICT)) {
        LOG_TOPIC("4e632", WARN, Logger::PREGEL) << "conflict while storing " << builder.toJson();
      }

      res = trx->finish(res);
      if (!res.ok()) {
        THROW_ARANGO_EXCEPTION(res);
      }
    }

    builder.clear();
    builder.openArray(true);
  };
  
  // loop over vertices
  for (; it.hasMore(); ++it) {
    if (it->shard() != currentShard || numDocs >= 1000) {
      commitTransaction();
      
      currentShard = it->shard();
      shard = globalShards[currentShard];
      
      auto ctx = transaction::StandaloneContext::Create(_vocbaseGuard.database());
      trx = std::make_unique<SingleCollectionTransaction>(ctx, shard, AccessMode::Type::WRITE);
      trx->addHint(transaction::Hints::Hint::INTERMEDIATE_COMMITS);
      
      res = trx->begin();
      if (!res.ok()) {
        THROW_ARANGO_EXCEPTION(res);
      }
      numDocs = 0;
    }
    
    VPackStringRef const key = it->key();
    V const& data = it->data();
    
    //builder.clear();
    // This loop will fill a buffer of vertices until we run into a new
    // collection
    // or there are no more vertices for to store (or the buffer is full)
    builder.openObject(true);
    builder.add(StaticStrings::KeyString, VPackValuePair(key.data(), key.size(),
                                                         VPackValueType::String));
    _graphFormat->buildVertexDocument(builder, &data, sizeof(V));
    builder.close();
    
    ++numDocs;
    if (_vocbaseGuard.database().server().isStopping()) {
      LOG_TOPIC("73ec2", WARN, Logger::PREGEL)
          << "Storing data was canceled prematurely";
      trx->abort();
      trx.reset();
      break;
    }
  }
 
  // commit the remainders in our buffer
  // will throw if it fails
  commitTransaction();
}

template <typename V, typename E>
void GraphStore<V, E>::storeResults(WorkerConfig* config,
                                    std::function<void()> cb) {

  _config = config;
  double now = TRI_microtime();
  TRI_ASSERT(SchedulerFeature::SCHEDULER != nullptr);
  
  size_t const numSegments = _vertices.size();
  
  uint32_t numThreads = 1;
  if (_localVertexCount > 100000) {
    // We expect at least parallelism to fit in a uint32_t.
    numThreads = static_cast<uint32_t>(std::min<size_t>(_config->parallelism(), numSegments));
  }

  _runningThreads.store(numThreads, std::memory_order_relaxed);
  size_t const numT = numThreads;
  LOG_TOPIC("f3fd9", DEBUG, Logger::PREGEL) << "Storing vertex data using " <<
    numT << " threads";
  
  for (size_t i = 0; i < numT; ++i) {
    bool queued = SchedulerFeature::SCHEDULER->queue(RequestLane::INTERNAL_LOW, [=] {
      size_t startI = i * (numSegments / numT);
      size_t endI = (i + 1) * (numSegments / numT);
      TRI_ASSERT(endI <= numSegments);
      
      try {
        RangeIterator<Vertex<V, E>> it = vertexIterator(startI, endI);
        _storeVertices(_config->globalShardIDs(), it);
        // TODO can't just write edges with SmartGraphs
      } catch (std::exception const& e) {
        LOG_TOPIC("e22c8", ERR, Logger::PREGEL) << "Storing vertex data failed: " << e.what();
      } catch (...) {
        LOG_TOPIC("51b87", ERR, Logger::PREGEL) << "Storing vertex data failed";
      }

      uint32_t numRunning = _runningThreads.fetch_sub(1, std::memory_order_relaxed);
      TRI_ASSERT(numRunning > 0);
      if (numRunning - 1 == 0) {
        LOG_TOPIC("b5a21", DEBUG, Logger::PREGEL)
            << "Storing data took " << (TRI_microtime() - now) << "s";
        cb();
      }
    });

    if (!queued) {
      // couldn't queue this storage task. so now at least count down
      _runningThreads.fetch_sub(1, std::memory_order_relaxed);

      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUEUE_FULL,
                                     "No thread available to queue vertex "
                                     "storage, canceling execution");
    }
  }
}

template class arangodb::pregel::GraphStore<int64_t, int64_t>;
template class arangodb::pregel::GraphStore<uint64_t, uint64_t>;
template class arangodb::pregel::GraphStore<uint64_t, uint8_t>;
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

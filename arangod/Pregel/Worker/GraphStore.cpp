////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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

#include "ApplicationFeatures/ApplicationServer.h"
#include "Aql/AttributeNamePath.h"
#include "Aql/Projections.h"
#include "Basics/Common.h"
#include "Basics/GlobalResourceMonitor.h"
#include "Basics/LocalTaskQueue.h"
#include "Basics/MutexLocker.h"
#include "Basics/ResourceUsage.h"
#include "Basics/ScopeGuard.h"
#include "Cluster/ClusterFeature.h"
#include "Indexes/IndexIterator.h"
#include "Metrics/Gauge.h"
#include "Pregel/CommonFormats.h"
#include "Pregel/IndexHelpers.h"
#include "Pregel/PregelFeature.h"
#include "Pregel/Status/Status.h"
#include "Pregel/TypedBuffer.h"
#include "Pregel/Utils.h"
#include "Pregel/Worker/WorkerConfig.h"
#include "Scheduler/Scheduler.h"
#include "Scheduler/SchedulerFeature.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/CollectionNameResolver.h"
#include "Utils/OperationOptions.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ticks.h"
#include "VocBase/vocbase.h"

#include "Pregel/Algos/ColorPropagation/ColorPropagationValue.h"
#include "Pregel/Algos/DMID/DMIDValue.h"
#include "Pregel/Algos/EffectiveCloseness/ECValue.h"
#include "Pregel/Algos/HITS/HITSValue.h"
#include "Pregel/Algos/HITSKleinberg/HITSKleinbergValue.h"
#include "Pregel/Algos/LabelPropagation/LPValue.h"
#include "Pregel/Algos/SCC/SCCValue.h"
#include "Pregel/Algos/SLPA/SLPAValue.h"
#include "Pregel/Algos/WCC/WCCValue.h"

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

#include <algorithm>
#include <memory>

using namespace arangodb;
using namespace arangodb::pregel;

#define LOG_PREGEL(logId, level) \
  LOG_TOPIC(logId, level, Logger::PREGEL) << "[job " << _executionNumber << "] "

namespace {
static constexpr size_t minStringChunkSize = 16 * 1024 * sizeof(char);
static constexpr size_t maxStringChunkSize = 32 * 1024 * 1024 * sizeof(char);
static constexpr size_t chunkUnit = 4 * 1024 * sizeof(char);

static_assert(minStringChunkSize % chunkUnit == 0, "invalid chunkUnit value");
static_assert(maxStringChunkSize % chunkUnit == 0, "invalid chunkUnit value");

size_t stringChunkSize(size_t /*numberOfChunks*/, uint64_t numVerticesLeft,
                       bool isVertex) {
  // we assume a conservative 64 bytes per document key
  size_t numBytes = numVerticesLeft * 64;
  if (!isVertex) {
    // assume 16 edges per vertex. this is an arbitrary estimate.
    numBytes *= 16;
  }
  // round up to nearest multiple of chunkUnit (4096)
  numBytes = ((numBytes - 1u) & ~(chunkUnit - 1u)) + chunkUnit;
  numBytes = std::max<size_t>(minStringChunkSize, numBytes);
  numBytes = std::min<size_t>(maxStringChunkSize, numBytes);

  TRI_ASSERT(numBytes % chunkUnit == 0);
  return numBytes;
}
}  // namespace

template<typename V, typename E>
GraphStore<V, E>::GraphStore(PregelFeature& feature, TRI_vocbase_t& vocbase,
                             ExecutionNumber executionNumber,
                             GraphFormat<V, E>* graphFormat)
    : _feature(feature),
      _vocbaseGuard(vocbase),
      _resourceMonitor(GlobalResourceMonitor::instance()),
      _executionNumber(executionNumber),
      _graphFormat(graphFormat),
      _config(nullptr),
      _vertexIdRangeStart(0),
      _localVertexCount(0),
      _localEdgeCount(0),
      _runningThreads(0) {}

static const char* shardError =
    "Collections need to have the same number of shards,"
    " use distributeShardsLike";

template<typename V, typename E>
void GraphStore<V, E>::loadShards(
    WorkerConfig* config, std::function<void()> const& statusUpdateCallback,
    std::function<void()> const& finishedLoadingCallback) {
  _config = config;
  TRI_ASSERT(_runningThreads == 0);

  LOG_PREGEL("27f1e", DEBUG)
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

  auto poster = [](std::function<void()> fn) -> void {
    return SchedulerFeature::SCHEDULER->queue(RequestLane::INTERNAL_LOW,
                                              std::move(fn));
  };
  auto queue = std::make_shared<basics::LocalTaskQueue>(
      _vocbaseGuard.database().server(), poster);
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

      auto const& edgeCollectionRestrictions =
          _config->edgeCollectionRestrictions(vertexShard);

      // distributeshardslike should cause the edges for a vertex to be
      // in the same shard index. x in vertexShard2 => E(x) in edgeShard2
      std::vector<ShardID> edges;
      for (auto const& pair2 : edgeCollMap) {
        std::vector<ShardID> const& edgeShards = pair2.second;
        if (vertexShards.size() != edgeShards.size()) {
          THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER, shardError);
        }

        // optionally restrict edge collections to a positive list
        if (edgeCollectionRestrictions.empty() ||
            std::find(edgeCollectionRestrictions.begin(),
                      edgeCollectionRestrictions.end(),
                      edgeShards[i]) != edgeCollectionRestrictions.end()) {
          edges.emplace_back(edgeShards[i]);
        }
      }

      try {
        // we might have already loaded these shards
        if (!_loadedShards.emplace(vertexShard).second) {
          continue;
        }
        auto task = std::make_shared<basics::LambdaTask>(
            queue,
            [this, vertexShard, edges, statusUpdateCallback]() -> Result {
              if (_vocbaseGuard.database().server().isStopping()) {
                LOG_PREGEL("4355b", WARN) << "Aborting graph loading";
                return {TRI_ERROR_SHUTTING_DOWN};
              }
              try {
                loadVertices(vertexShard, edges, statusUpdateCallback);
                return Result();
              } catch (basics::Exception const& ex) {
                LOG_PREGEL("8682a", WARN)
                    << "caught exception while loading pregel graph: "
                    << ex.what();
                return Result(ex.code(), ex.what());
              } catch (std::exception const& ex) {
                LOG_PREGEL("c87c9", WARN)
                    << "caught exception while loading pregel graph: "
                    << ex.what();
                return Result(TRI_ERROR_INTERNAL, ex.what());
              } catch (...) {
                LOG_PREGEL("c7240", WARN)
                    << "caught unknown exception while loading pregel graph";
                return Result(TRI_ERROR_INTERNAL,
                              "unknown exception while loading pregel graph");
              }
            });
        queue->enqueue(task);
      } catch (basics::Exception const& ex) {
        LOG_PREGEL("3f283", WARN) << "unhandled exception while "
                                  << "loading pregel graph: " << ex.what();
      } catch (...) {
        LOG_PREGEL("3f282", WARN) << "unhandled exception while "
                                  << "loading pregel graph";
      }
    }
  }
  queue->dispatchAndWait();
  if (queue->status().fail() && !queue->status().is(TRI_ERROR_SHUTTING_DOWN)) {
    THROW_ARANGO_EXCEPTION(queue->status());
  }

  SchedulerFeature::SCHEDULER->queue(RequestLane::INTERNAL_LOW,
                                     statusUpdateCallback);

  SchedulerFeature::SCHEDULER->queue(RequestLane::INTERNAL_LOW,
                                     finishedLoadingCallback);
}

template<typename V, typename E>
void GraphStore<V, E>::loadDocument(WorkerConfig* config,
                                    std::string const& documentID) {
  // figure out if we got this vertex locally
  VertexID _id = config->documentIdToPregel(documentID);
  if (config->isLocalVertexShard(_id.shard)) {
    loadDocument(config, _id.shard, std::string_view(_id.key));
  }
}

template<typename V, typename E>
void GraphStore<V, E>::loadDocument(WorkerConfig* config,
                                    PregelShard sourceShard,
                                    std::string_view key) {
  // Apparently this code has not been implemented yet; find out whether it's
  // needed at all or remove
  TRI_ASSERT(false);
}

template<typename V, typename E>
RangeIterator<Vertex<V, E>> GraphStore<V, E>::vertexIterator() {
  if (_vertices.empty()) {
    return RangeIterator<Vertex<V, E>>(_vertices, 0, nullptr, 0);
  }

  TypedBuffer<Vertex<V, E>>* front = _vertices.front().get();
  return RangeIterator<Vertex<V, E>>(_vertices, 0, front->begin(),
                                     _localVertexCount);
}

template<typename V, typename E>
RangeIterator<Vertex<V, E>> GraphStore<V, E>::vertexIterator(size_t i,
                                                             size_t j) {
  if (_vertices.size() <= i) {
    return RangeIterator<Vertex<V, E>>(_vertices, 0, nullptr, 0);
  }

  size_t numVertices = 0;
  for (size_t x = i; x < j && x < _vertices.size(); x++) {
    numVertices += _vertices[x]->size();
  }

  return RangeIterator<Vertex<V, E>>(_vertices, i, _vertices[i]->begin(),
                                     numVertices);
}

template<typename V, typename E>
RangeIterator<Edge<E>> GraphStore<V, E>::edgeIterator(
    Vertex<V, E> const* entry) {
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
template<typename X>
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
std::unique_ptr<TypedBuffer<M>> createBuffer(PregelFeature& feature,
                                             WorkerConfig const& config,
                                             size_t cap) {
  if (config.useMemoryMaps()) {
    // prefix used for logging in TypedBuffer.h
    std::string logPrefix = fmt::format("[job {}] ", config.executionNumber());

    auto ptr = std::make_unique<MappedFileBuffer<M>>(feature.tempPath(), cap,
                                                     logPrefix);
    ptr->sequentialAccess();
    return ptr;
  }

  return std::make_unique<VectorTypedBuffer<M>>(cap);
}
}  // namespace

template<typename V, typename E>
void GraphStore<V, E>::loadVertices(
    ShardID const& vertexShard, std::vector<ShardID> const& edgeShards,
    std::function<void()> const& statusUpdateCallback) {
  LOG_PREGEL("24838", DEBUG) << "Loading from vertex shard " << vertexShard
                             << ", edge shards: " << edgeShards;

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
  auto cursor =
      trx.indexScan(_resourceMonitor, vertexShard,
                    transaction::Methods::CursorType::ALL, ReadOwnWrites::no);

  // tell the formatter the number of docs we are about to load
  LogicalCollection* coll = cursor->collection();
  uint64_t numVertices =
      coll->numberDocuments(&trx, transaction::CountType::Normal);

  uint64_t const vertexIdRangeStart = determineVertexIdRangeStart(numVertices);
  uint64_t vertexIdRange = vertexIdRangeStart;

  LOG_PREGEL("7c31f", DEBUG)
      << "Shard '" << vertexShard << "' has " << numVertices
      << " vertices. id range: [" << vertexIdRangeStart << ", "
      << (vertexIdRangeStart + numVertices) << ")";

  std::vector<std::unique_ptr<TypedBuffer<Vertex<V, E>>>> vertices;
  std::vector<std::unique_ptr<TypedBuffer<char>>> vKeys;
  std::vector<std::unique_ptr<TypedBuffer<Edge<E>>>> edges;
  std::vector<std::unique_ptr<TypedBuffer<char>>> eKeys;

  std::vector<std::unique_ptr<traverser::EdgeCollectionInfo>>
      edgeCollectionInfos;
  edgeCollectionInfos.reserve(edgeShards.size());

  for (ShardID const& edgeShard : edgeShards) {
    edgeCollectionInfos.emplace_back(
        std::make_unique<traverser::EdgeCollectionInfo>(_resourceMonitor, &trx,
                                                        edgeShard));
  }

  TypedBuffer<Vertex<V, E>>* vertexBuff = nullptr;
  TypedBuffer<char>* keyBuff = nullptr;
  size_t segmentSize = std::min<size_t>(numVertices, vertexSegmentSize());

  // make a copy, as we are going to decrease the original value as we load
  // documents
  uint64_t numVerticesOriginal = numVertices;

  std::string documentId;  // temp buffer for _id of vertex
  auto cb = [&](LocalDocumentId const& token, VPackSlice slice) {
    if (vertexBuff == nullptr || vertexBuff->remainingCapacity() == 0) {
      vertices.push_back(
          createBuffer<Vertex<V, E>>(_feature, *_config, segmentSize));
      vertexBuff = vertices.back().get();
      _feature.metrics()->pregelMemoryUsedForGraph->fetch_add(segmentSize);
    }
    Vertex<V, E>* ventry = vertexBuff->appendElement();
    _observables.memoryBytesUsed += sizeof(Vertex<V, E>);

    VPackValueLength keyLen;
    VPackSlice keySlice = transaction::helpers::extractKeyFromDocument(slice);
    char const* key = keySlice.getString(keyLen);
    if (keyBuff == nullptr || keyLen > keyBuff->remainingCapacity()) {
      TRI_ASSERT(keyLen < ::maxStringChunkSize);
      auto const chunkSize = ::stringChunkSize(vKeys.size(), numVertices, true);
      vKeys.push_back(createBuffer<char>(_feature, *_config, chunkSize));
      _feature.metrics()->pregelMemoryUsedForGraph->fetch_add(chunkSize);
      keyBuff = vKeys.back().get();
    }

    ventry->setShard(sourceShard);
    ventry->setKey(keyBuff->end(), static_cast<uint16_t>(keyLen));
    ventry->setActive(true);
    TRI_ASSERT(keyLen <= std::numeric_limits<uint16_t>::max());

    // actually copy in the key
    memcpy(keyBuff->end(), key, keyLen);
    keyBuff->advance(keyLen);
    _observables.memoryBytesUsed += keyLen;

    // load vertex data
    documentId = trx.extractIdString(slice);
    if (_graphFormat->estimatedVertexSize() > 0) {
      // note: ventry->_data and vertexIdRange may be modified by
      // copyVertexData!
      _graphFormat->copyVertexData(*ctx->getVPackOptions(), documentId, slice,
                                   ventry->data(), vertexIdRange);
    }
    // load edges
    for (std::size_t i = 0; i < edgeShards.size(); ++i) {
      auto const& edgeShard = edgeShards[i];
      auto& info = *edgeCollectionInfos[i];
      loadEdges(trx, *ventry, edgeShard, documentId, edges, eKeys, numVertices,
                info);
    }
    ++_observables.verticesLoaded;
    return true;
  };

  _localVertexCount += numVertices;

  double lastLogStamp = TRI_microtime();

  constexpr uint64_t batchSize = 10000;
  while (cursor->nextDocument(cb, batchSize)) {
    if (_vocbaseGuard.database().server().isStopping()) {
      LOG_PREGEL("4355a", WARN) << "Aborting graph loading";
      break;
    }

    if (batchSize > numVertices) {
      numVertices = 0;
    } else {
      numVertices -= batchSize;
    }

    // log only every 10 seconds
    double now = TRI_microtime();
    if (now - lastLogStamp >= 10.0) {
      lastLogStamp = now;
      LOG_PREGEL("b9ed9", DEBUG) << "Shard '" << vertexShard << "', "
                                 << numVertices << " left to load";
    }
    segmentSize = std::min<size_t>(numVertices, vertexSegmentSize());

    SchedulerFeature::SCHEDULER->queue(RequestLane::INTERNAL_LOW,
                                       statusUpdateCallback);
  }

  // we must not overflow the range we have been assigned to
  TRI_ASSERT(vertexIdRange <= vertexIdRangeStart + numVerticesOriginal);

  std::lock_guard<std::mutex> guard(_bufferMutex);
  ::moveAppend(vertices, _vertices);
  ::moveAppend(vKeys, _vertexKeys);
  ::moveAppend(edges, _edges);
  ::moveAppend(eKeys, _edgeKeys);

  LOG_PREGEL("6d389", DEBUG)
      << "Pregel worker: done loading from vertex shard " << vertexShard;
}

template<typename V, typename E>
void GraphStore<V, E>::loadEdges(
    transaction::Methods& trx, Vertex<V, E>& vertex, ShardID const& edgeShard,
    std::string const& documentID,
    std::vector<std::unique_ptr<TypedBuffer<Edge<E>>>>& edges,
    std::vector<std::unique_ptr<TypedBuffer<char>>>& edgeKeys,
    uint64_t numVertices, traverser::EdgeCollectionInfo& info) {
  auto cursor = info.getEdges(documentID);

  TypedBuffer<Edge<E>>* edgeBuff = edges.empty() ? nullptr : edges.back().get();
  TypedBuffer<char>* keyBuff =
      edgeKeys.empty() ? nullptr : edgeKeys.back().get();

  auto allocateSpace = [&](size_t keyLen) {
    if (edgeBuff == nullptr || edgeBuff->remainingCapacity() == 0) {
      edges.push_back(
          createBuffer<Edge<E>>(_feature, *_config, edgeSegmentSize()));
      _feature.metrics()->pregelMemoryUsedForGraph->fetch_add(
          edgeSegmentSize());
      edgeBuff = edges.back().get();
    }
    if (keyBuff == nullptr || keyLen > keyBuff->remainingCapacity()) {
      TRI_ASSERT(keyLen < ::maxStringChunkSize);
      auto const chunkSize =
          ::stringChunkSize(edgeKeys.size(), numVertices, false);
      edgeKeys.push_back(createBuffer<char>(_feature, *_config, chunkSize));
      _feature.metrics()->pregelMemoryUsedForGraph->fetch_add(chunkSize);
      keyBuff = edgeKeys.back().get();
    }
  };

  bool const isCluster = ServerState::instance()->isRunningInCluster();
  auto& ci = trx.vocbase().server().getFeature<ClusterFeature>().clusterInfo();

  std::string collectionName;  // will be reused
  size_t addedEdges = 0;
  auto buildEdge = [&](Edge<E>* edge, std::string_view toValue) {
    ++addedEdges;
    if (vertex.addEdge(edge) == vertex.maxEdgeCount()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                     "too many edges for vertex");
    }
    ++_observables.edgesLoaded;
    _observables.memoryBytesUsed += sizeof(Edge<E>);

    std::size_t pos = toValue.find('/');
    collectionName = std::string(toValue.substr(0, pos));
    std::string_view key = toValue.substr(pos + 1);
    edge->_toKey = keyBuff->end();
    edge->_toKeyLength = static_cast<uint16_t>(key.size());
    TRI_ASSERT(key.size() <= std::numeric_limits<uint16_t>::max());
    keyBuff->advance(key.size());
    // actually copy in the key
    memcpy(edge->_toKey, key.data(), key.size());
    _observables.memoryBytesUsed += key.size();

    if (isCluster) {
      // resolve the shard of the target vertex.
      ShardID responsibleShard;

      auto res =
          Utils::resolveShard(ci, _config, collectionName,
                              StaticStrings::KeyString, key, responsibleShard);
      if (res != TRI_ERROR_NO_ERROR) {
        LOG_PREGEL("b80ba", ERR) << "Could not resolve target shard of edge '"
                                 << key << "', collection: " << collectionName
                                 << ": " << TRI_errno_string(res);
        return res;
      }

      edge->_targetShard = (PregelShard)_config->shardId(responsibleShard);
    } else {
      // single server is much simpler
      edge->_targetShard = (PregelShard)_config->shardId(collectionName);
    }

    if (edge->_targetShard == InvalidPregelShard) {
      LOG_PREGEL("1f413", ERR) << "Could not resolve target shard of edge";
      return TRI_ERROR_CLUSTER_BACKEND_UNAVAILABLE;
    }
    return TRI_ERROR_NO_ERROR;
  };

  if (_graphFormat->estimatedEdgeSize() == 0) {
    // use covering index optimization
    while (cursor->nextCovering(
        [&](LocalDocumentId const& /*token*/,
            IndexIteratorCoveringData& covering) {
          TRI_ASSERT(covering.isArray());

          std::string_view toValue =
              covering.at(info.coveringPosition()).stringView();
          size_t space = toValue.size();
          allocateSpace(space);
          Edge<E>* edge = edgeBuff->appendElement();
          buildEdge(edge, toValue);
          return true;
        },
        1000)) { /* continue loading */
      // Might overcount a bit;
    }
  } else {
    while (cursor->nextDocument(
        [&](LocalDocumentId const& /*token*/, VPackSlice slice) {
          slice = slice.resolveExternal();

          std::string_view toValue =
              transaction::helpers::extractToFromDocument(slice).stringView();
          allocateSpace(toValue.size());
          Edge<E>* edge = edgeBuff->appendElement();
          auto res = buildEdge(edge, toValue);
          if (res == TRI_ERROR_NO_ERROR) {
            _graphFormat->copyEdgeData(
                *trx.transactionContext()->getVPackOptions(), slice,
                edge->data());
          }
          return true;
        },
        1000)) { /* continue loading */
      // Might overcount a bit;
    }
  }

  // Add up all added elements
  _localEdgeCount += addedEdges;
}

template<typename V, typename E>
uint64_t GraphStore<V, E>::determineVertexIdRangeStart(uint64_t numVertices) {
  if (arangodb::ServerState::instance()->isRunningInCluster()) {
    if (this->_vocbaseGuard.database()
            .server()
            .template hasFeature<ClusterFeature>()) {
      arangodb::ClusterInfo& ci = this->_vocbaseGuard.database()
                                      .server()
                                      .template getFeature<ClusterFeature>()
                                      .clusterInfo();
      return ci.uniqid(numVertices);
    }
  }

  return _vertexIdRangeStart.fetch_add(numVertices, std::memory_order_relaxed);
}

/// Loops over the array starting a new transaction for different shards
/// Should not dead-lock unless we have to wait really long for other threads
template<typename V, typename E>
void GraphStore<V, E>::storeVertices(
    std::vector<ShardID> const& globalShards, RangeIterator<Vertex<V, E>>& it,
    size_t threadNumber, std::function<void()> const& statusUpdateCallback) {
  // transaction on one shard
  OperationOptions options;
  options.silent = true;
  options.waitForSync = false;

  std::unique_ptr<arangodb::SingleCollectionTransaction> trx;

  ShardID shard;
  PregelShard currentShard = InvalidPregelShard;
  Result res;

  VPackBuilder builder;
  uint64_t numDocs = 0;
  double lastLogStamp = TRI_microtime();

  auto commitTransaction = [&]() {
    if (trx) {
      builder.close();

      OperationResult opRes = trx->update(shard, builder.slice(), options);
      if (!opRes.countErrorCodes.empty()) {
        auto code = ErrorCode{opRes.countErrorCodes.begin()->first};
        if (opRes.countErrorCodes.size() > 1) {
          // more than a single error code. let's just fail this
          THROW_ARANGO_EXCEPTION(code);
        }
        // got only a single error code. now let's use it, whatever it is.
        opRes.result.reset(code);
      }

      if (opRes.fail() && opRes.isNot(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND) &&
          opRes.isNot(TRI_ERROR_ARANGO_CONFLICT)) {
        THROW_ARANGO_EXCEPTION(opRes.result);
      }
      if (opRes.is(TRI_ERROR_ARANGO_CONFLICT)) {
        LOG_PREGEL("4e632", WARN)
            << "conflict while storing " << builder.toJson();
      }

      res = trx->finish(res);
      if (!res.ok()) {
        THROW_ARANGO_EXCEPTION(res);
      }

      if (_vocbaseGuard.database().server().isStopping()) {
        LOG_PREGEL("73ec2", WARN) << "Storing data was canceled prematurely";
        THROW_ARANGO_EXCEPTION(TRI_ERROR_SHUTTING_DOWN);
      }

      numDocs = 0;

      // log only every 10 seconds
      double now = TRI_microtime();
      if (now - lastLogStamp >= 10.0) {
        lastLogStamp = now;
        LOG_PREGEL("24837", DEBUG) << "Worker thread " << threadNumber << ", "
                                   << it.size() << " vertices left to store";
      }
    }

    builder.clear();
    builder.openArray(true);
  };

  // loop over vertices
  // This loop will fill a buffer of vertices until we run into a new
  // collection
  // or there are no more vertices for to store (or the buffer is full)
  for (; it.hasMore(); ++it) {
    if (it->shard() != currentShard || numDocs >= 1000) {
      commitTransaction();

      currentShard = it->shard();
      shard = globalShards[currentShard.value];

      auto ctx =
          transaction::StandaloneContext::Create(_vocbaseGuard.database());
      trx = std::make_unique<SingleCollectionTransaction>(
          ctx, shard, AccessMode::Type::WRITE);
      trx->addHint(transaction::Hints::Hint::INTERMEDIATE_COMMITS);

      res = trx->begin();
      if (!res.ok()) {
        THROW_ARANGO_EXCEPTION(res);
      }
    }

    std::string_view const key = it->key();

    builder.openObject(true);
    builder.add(StaticStrings::KeyString,
                VPackValuePair(key.data(), key.size(), VPackValueType::String));
    V const& data = it->data();
    if (auto result = _graphFormat->buildVertexDocument(builder, &data);
        !result) {
      LOG_PREGEL("143af", DEBUG) << "Failed to build vertex document";
    }
    builder.close();
    ++numDocs;
    ++_observables.verticesStored;
    if (numDocs % Utils::batchOfVerticesStoredBeforeUpdatingStatus == 0) {
      SchedulerFeature::SCHEDULER->queue(RequestLane::INTERNAL_LOW,
                                         statusUpdateCallback);
    }
  }

  SchedulerFeature::SCHEDULER->queue(RequestLane::INTERNAL_LOW,
                                     statusUpdateCallback);
  // commit the remainders in our buffer
  // will throw if it fails
  commitTransaction();
}

template<typename V, typename E>
void GraphStore<V, E>::storeResults(
    WorkerConfig* config, std::function<void()> cb,
    std::function<void()> const& statusUpdateCallback) {
  _config = config;
  double now = TRI_microtime();
  TRI_ASSERT(SchedulerFeature::SCHEDULER != nullptr);

  size_t const numSegments = _vertices.size();

  uint32_t numThreads = 1;
  if (_localVertexCount > 100000) {
    // We expect at least parallelism to fit in a uint32_t.
    numThreads = static_cast<uint32_t>(
        std::min<size_t>(_config->parallelism(), numSegments));
  }

  _runningThreads.store(numThreads, std::memory_order_relaxed);
  _feature.metrics()->pregelNumberOfThreads->fetch_add(numThreads);
  size_t const numT = numThreads;
  LOG_PREGEL("f3fd9", DEBUG) << "Storing vertex data (" << numSegments
                             << " vertices) using " << numT << " threads";

  for (size_t i = 0; i < numT; ++i) {
    SchedulerFeature::SCHEDULER->queue(RequestLane::INTERNAL_LOW, [=, this] {
      size_t startI = i * (numSegments / numT);
      size_t endI = (i + 1) * (numSegments / numT);
      TRI_ASSERT(endI <= numSegments);

      try {
        RangeIterator<Vertex<V, E>> it = vertexIterator(startI, endI);
        storeVertices(_config->globalShardIDs(), it, i, statusUpdateCallback);
        // TODO can't just write edges with SmartGraphs
      } catch (std::exception const& e) {
        LOG_PREGEL("e22c8", ERR) << "Storing vertex data failed: " << e.what();
      } catch (...) {
        LOG_PREGEL("51b87", ERR) << "Storing vertex data failed";
      }

      uint32_t numRunning =
          _runningThreads.fetch_sub(1, std::memory_order_relaxed);
      _feature.metrics()->pregelNumberOfThreads->fetch_sub(1);
      TRI_ASSERT(numRunning > 0);
      if (numRunning - 1 == 0) {
        LOG_PREGEL("b5a21", DEBUG)
            << "Storing data took " << (TRI_microtime() - now) << "s";
        cb();
      }
    });
  }
}

template class arangodb::pregel::GraphStore<int64_t, int64_t>;
template class arangodb::pregel::GraphStore<uint64_t, uint64_t>;
template class arangodb::pregel::GraphStore<uint64_t, uint8_t>;
template class arangodb::pregel::GraphStore<float, float>;
template class arangodb::pregel::GraphStore<double, float>;
template class arangodb::pregel::GraphStore<double, double>;
template class arangodb::pregel::GraphStore<float, uint8_t>;

// specific algo combos
template class arangodb::pregel::GraphStore<algos::WCCValue, uint64_t>;
template class arangodb::pregel::GraphStore<algos::SCCValue, int8_t>;
template class arangodb::pregel::GraphStore<ECValue, int8_t>;
template class arangodb::pregel::GraphStore<algos::HITSValue, int8_t>;
template class arangodb::pregel::GraphStore<algos::HITSKleinbergValue, int8_t>;
template class arangodb::pregel::GraphStore<DMIDValue, float>;
template class arangodb::pregel::GraphStore<algos::LPValue, int8_t>;
template class arangodb::pregel::GraphStore<algos::SLPAValue, int8_t>;
template class arangodb::pregel::GraphStore<ColorPropagationValue, int8_t>;

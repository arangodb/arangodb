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
#include "Inspection/JsonPrintInspector.h"
#include "Cluster/ClusterFeature.h"
#include "Indexes/IndexIterator.h"
#include "Metrics/Gauge.h"
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

template<typename V, typename E>
GraphStore<V, E>::GraphStore(
    PregelFeature& feature, TRI_vocbase_t& vocbase,
    ExecutionNumber executionNumber,
    std::shared_ptr<GraphFormat<V, E> const> graphFormat)
    : _feature(feature),
      _vocbaseGuard(vocbase),
      _resourceMonitor(GlobalResourceMonitor::instance()),
      _executionNumber(executionNumber),
      _graphFormat(graphFormat),
      _config(nullptr),
      _vertexIdRangeStart(0),
      _localVertexCount(0),
      _localEdgeCount(0) {}

static const char* shardError =
    "Collections need to have the same number of shards,"
    " use distributeShardsLike";

template<typename V, typename E>
void GraphStore<V, E>::loadShards(
    std::shared_ptr<WorkerConfig const> config,
    std::function<void()> const& statusUpdateCallback,
    std::function<void()> const& finishedLoadingCallback) {
  _config = config;

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
        if (_vocbaseGuard.database().server().isStopping()) {
          LOG_PREGEL("4355b", WARN) << "Aborting graph loading";
        }
        loadVertices(vertexShard, edges, statusUpdateCallback);
      } catch (basics::Exception const& ex) {
        LOG_PREGEL("8682a", WARN)
            << "caught exception while loading pregel graph: " << ex.what();
      } catch (std::exception const& ex) {
        LOG_PREGEL("c87c9", WARN)
            << "caught exception while loading pregel graph: " << ex.what();
      } catch (...) {
        LOG_PREGEL("c7240", WARN)
            << "caught unknown exception while loading pregel graph";
      }
    }
  }
  SchedulerFeature::SCHEDULER->queue(RequestLane::INTERNAL_LOW,
                                     statusUpdateCallback);

  SchedulerFeature::SCHEDULER->queue(RequestLane::INTERNAL_LOW,
                                     finishedLoadingCallback);
}

template<typename V, typename E>
auto GraphStore<V, E>::loadVertices(
    ShardID const& vertexShard, std::vector<ShardID> const& edgeShards,
    std::function<void()> const& statusUpdateCallback)
    -> std::vector<Vertex<V, E>> {
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

  auto sourceShard = _config->shardId(vertexShard);
  auto cursor =
      trx.indexScan(_resourceMonitor, vertexShard,
                    transaction::Methods::CursorType::ALL, ReadOwnWrites::no);

  // tell the formatter the number of docs we are about to load
  LogicalCollection* coll = cursor->collection();
  uint64_t numVertices =
      coll->numberDocuments(&trx, transaction::CountType::Normal);

  auto vertices = std::vector<Vertex<V, E>>(numVertices);

  uint64_t const vertexIdRangeStart = determineVertexIdRangeStart(numVertices);
  uint64_t vertexIdRange = vertexIdRangeStart;

  LOG_PREGEL("7c31f", DEBUG)
      << "Shard '" << vertexShard << "' has " << numVertices
      << " vertices. id range: [" << vertexIdRangeStart << ", "
      << (vertexIdRangeStart + numVertices) << ")";

  std::vector<std::unique_ptr<traverser::EdgeCollectionInfo>>
      edgeCollectionInfos;
  edgeCollectionInfos.reserve(edgeShards.size());

  for (ShardID const& edgeShard : edgeShards) {
    edgeCollectionInfos.emplace_back(
        std::make_unique<traverser::EdgeCollectionInfo>(_resourceMonitor, &trx,
                                                        edgeShard));
  }

  std::string documentId;  // temp buffer for _id of vertex
  auto cb = [&](LocalDocumentId const& token, VPackSlice slice) {
    Vertex<V, E> ventry;
    _observables.memoryBytesUsed += sizeof(Vertex<V, E>);

    auto keySlice = transaction::helpers::extractKeyFromDocument(slice);
    auto key = keySlice.copyString();

    ventry.setShard(sourceShard);
    ventry.setKey(key.c_str(), key.size());
    ventry.setActive(true);

    // load vertex data
    documentId = trx.extractIdString(slice);
    if (_graphFormat->estimatedVertexSize() > 0) {
      // note: ventry->_data and vertexIdRange may be modified by
      // copyVertexData!
      _graphFormat->copyVertexData(*ctx->getVPackOptions(), documentId, slice,
                                   ventry.data(), vertexIdRange);
    }

    // load edges
    for (std::size_t i = 0; i < edgeShards.size(); ++i) {
      auto const& edgeShard = edgeShards[i];
      auto& info = *edgeCollectionInfos[i];
      loadEdges(trx, ventry, edgeShard, documentId, numVertices, info);
    }
    _quiver.emplace(std::move(ventry));
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
    SchedulerFeature::SCHEDULER->queue(RequestLane::INTERNAL_LOW,
                                       statusUpdateCallback);
  }
  LOG_PREGEL("6d389", DEBUG)
      << "Pregel worker: done loading from vertex shard " << vertexShard;
  return vertices;
}

template<typename V, typename E>
void GraphStore<V, E>::loadEdges(transaction::Methods& trx,
                                 Vertex<V, E>& vertex, ShardID const& edgeShard,
                                 std::string_view documentID,
                                 uint64_t numVertices,
                                 traverser::EdgeCollectionInfo& info) {
  auto cursor = info.getEdges(std::string{documentID});

  size_t addedEdges = 0;
  if (_graphFormat->estimatedEdgeSize() == 0) {
    // use covering index optimization
    while (cursor->nextCovering(
        [&](LocalDocumentId const& /*token*/,
            IndexIteratorCoveringData& covering) {
          TRI_ASSERT(covering.isArray());
          ++addedEdges;
          ++_observables.edgesLoaded;

          std::string_view toValue =
              covering.at(info.coveringPosition()).stringView();
          auto toVertexID = _config->documentIdToPregel(toValue);

          vertex.addEdge(Edge<E>(toVertexID, {}));
          return true;
        },
        1000)) { /* continue loading */
    }
  } else {
    while (cursor->nextDocument(
        [&](LocalDocumentId const& /*token*/, VPackSlice slice) {
          slice = slice.resolveExternal();
          ++addedEdges;
          ++_observables.edgesLoaded;

          std::string_view toValue =
              transaction::helpers::extractToFromDocument(slice).stringView();
          auto toVertexID = _config->documentIdToPregel(toValue);

          auto edge = Edge<E>(toVertexID, {});
          _graphFormat->copyEdgeData(
              *trx.transactionContext()->getVPackOptions(), slice, edge.data());
          vertex.addEdge(std::move(edge));
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
    std::vector<ShardID> const& globalShards,
    std::function<void()> const& statusUpdateCallback) {
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
    }

    builder.clear();
    builder.openArray(true);
  };

  // loop over vertices
  // This loop will fill a buffer of vertices until we run into a new
  // collection
  // or there are no more vertices for to store (or the buffer is full)
  for (auto& vertex : _quiver) {
    if (vertex.shard() != currentShard || numDocs >= 1000) {
      commitTransaction();

      currentShard = vertex.shard();
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

    std::string_view const key = vertex.key();

    builder.openObject(true);
    builder.add(StaticStrings::KeyString,
                VPackValuePair(key.data(), key.size(), VPackValueType::String));
    V const& data = vertex.data();
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
    std::shared_ptr<WorkerConfig const> config, std::function<void()> cb,
    std::function<void()> const& statusUpdateCallback) {
  _config = config;
  double now = TRI_microtime();
  TRI_ASSERT(SchedulerFeature::SCHEDULER != nullptr);
  LOG_PREGEL("f3fd9", DEBUG)
      << "Storing vertex data (" << _quiver.numberOfVertices() << " vertices)";

  SchedulerFeature::SCHEDULER->queue(RequestLane::INTERNAL_LOW, [=, this] {
    try {
      storeVertices(_config->globalShardIDs(), statusUpdateCallback);
      // TODO can't just write edges with SmartGraphs
    } catch (std::exception const& e) {
      LOG_PREGEL("e22c8", ERR) << "Storing vertex data failed: " << e.what();
    } catch (...) {
      LOG_PREGEL("51b87", ERR) << "Storing vertex data failed";
    }
    LOG_PREGEL("b5a21", DEBUG)
        << "Storing data took " << (TRI_microtime() - now) << "s";
    cb();
  });
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
template class arangodb::pregel::GraphStore<algos::ECValue, int8_t>;
template class arangodb::pregel::GraphStore<algos::HITSValue, int8_t>;
template class arangodb::pregel::GraphStore<algos::HITSKleinbergValue, int8_t>;
template class arangodb::pregel::GraphStore<algos::DMIDValue, float>;
template class arangodb::pregel::GraphStore<algos::LPValue, int8_t>;
template class arangodb::pregel::GraphStore<algos::SLPAValue, int8_t>;
template class arangodb::pregel::GraphStore<algos::ColorPropagationValue,
                                            int8_t>;

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
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

#include "GraphLoader.h"

#include <memory>
#include <cstdint>
#include <variant>

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/overload.h"
#include "Cluster/ServerState.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "Futures/Future.h"
#include "Logger/LogMacros.h"
#include "Pregel/GraphFormat.h"
#include "Pregel/Algos/ColorPropagation/ColorPropagationValue.h"
#include "Pregel/Algos/DMID/DMIDValue.h"
#include "Pregel/Algos/EffectiveCloseness/ECValue.h"
#include "Pregel/Algos/HITS/HITSValue.h"
#include "Pregel/Algos/HITSKleinberg/HITSKleinbergValue.h"
#include "Pregel/Algos/LabelPropagation/LPValue.h"
#include "Pregel/Algos/SCC/SCCValue.h"
#include "Pregel/Algos/SLPA/SLPAValue.h"
#include "Pregel/Algos/WCC/WCCValue.h"
#include "Pregel/GraphStore/LoadableVertexShard.h"
#include "Pregel/StatusMessages.h"
#include "Pregel/Worker/WorkerConfig.h"
#include "Scheduler/SchedulerFeature.h"
#include "StorageEngine/PhysicalCollection.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"
#include "Transaction/Options.h"
#include "Transaction/StandaloneContext.h"
#include "VocBase/LogicalCollection.h"

#define LOG_PREGEL(logId, level)          \
  LOG_TOPIC(logId, level, Logger::PREGEL) \
      << "[job " << config->executionNumber() << "] "

namespace arangodb::pregel {
template<typename V, typename E>
auto GraphLoader<V, E>::requestVertexIds(uint64_t numVertices)
    -> VertexIdRange {
  if (arangodb::ServerState::instance()->isRunningInCluster()) {
    if (config->vocbase()->server().template hasFeature<ClusterFeature>()) {
      arangodb::ClusterInfo& ci = this->config->vocbase()
                                      ->server()
                                      .template getFeature<ClusterFeature>()
                                      .clusterInfo();
      auto n = ci.uniqid(numVertices);
      return VertexIdRange{.current = n, .maxId = n + numVertices};
    } else {
      ADB_PROD_ASSERT(false) << "ClusterFeature not present in server";
    }
  } else {
    uint64_t base = currentIdBase.load();
    while (!currentIdBase.compare_exchange_strong(base, base + numVertices)) {
    };
    return VertexIdRange{.current = base, .maxId = base + numVertices};
  }
  ADB_PROD_ASSERT(false);
  return VertexIdRange{};
}

template<typename V, typename E>
auto GraphLoader<V, E>::load() -> futures::Future<Magazine<V, E>> {
  LOG_PREGEL("ff00f", DEBUG)
      << "GraphSerdeConfig: " << inspection::json(config->graphSerdeConfig());
  auto const server = ServerState::instance()->getId();
  auto const myLoadableVertexShards =
      config->graphSerdeConfig().loadableVertexShardsForServer(server);

  auto loadableShardIdx = std::make_shared<std::atomic<size_t>>(0);
  auto futures = std::vector<futures::Future<Magazine<V, E>>>{};
  auto self = this->shared_from_this();

  for (auto futureN = size_t{0}; futureN < config->parallelism(); ++futureN) {
    futures.emplace_back(SchedulerFeature::SCHEDULER->queueWithFuture(
        RequestLane::INTERNAL_LOW,
        [this, self, futureN, loadableShardIdx, myLoadableVertexShards,
         server]() -> Magazine<V, E> {
          auto result = Magazine<V, E>{};

          LOG_PREGEL("8633a", WARN)
              << fmt::format("Starting vertex loader number {}", futureN);

          while (true) {
            auto myLoadableVertexShardIdx = loadableShardIdx->fetch_add(1);
            if (myLoadableVertexShardIdx >= myLoadableVertexShards.size()) {
              break;
            }

            try {
              auto const& loadableVertexShard =
                  myLoadableVertexShards.at(myLoadableVertexShardIdx);
              result.emplace(loadVertices(loadableVertexShard));
            } catch (basics::Exception const& ex) {
              LOG_PREGEL("8682a", WARN)
                  << fmt::format("vertex loader number {} caught exception: {}",
                                 futureN, ex.what());

              break;
            } catch (std::exception const& ex) {
              LOG_PREGEL("c87c9", WARN)
                  << fmt::format("vertex loader number {} caught exception: {}",
                                 futureN, ex.what());
              break;
            } catch (...) {
              LOG_PREGEL("c7240", WARN) << fmt::format(
                  "vertex loader number {} caught unknown exception", futureN);
              break;
            }
          }
          return result;
        }));
  }
  return collectAll(futures).thenValue([this, self](auto&& results) {
    auto result = Magazine<V, E>{};
    for (auto&& r : results) {
      // TODO: maybe handle exceptions here?
      auto&& magazine = r.get();

      for (auto&& q : magazine) {
        result.emplace(std::move(q));
      }
    }
    std::visit(overload{[&](ActorLoadingUpdate const& update) {
                          update.fn(message::GraphLoadingUpdate{
                              .verticesLoaded = result.numberOfVertices(),
                              .edgesLoaded = result.numberOfEdges(),
                              .memoryBytesUsed = 0});
                        },
                        [](OldLoadingUpdate const& update) {
                          SchedulerFeature::SCHEDULER->queue(
                              RequestLane::INTERNAL_LOW, update.fn);
                        }},
               updateCallback);
    return result;
  });
}

template<typename V, typename E>
auto GraphLoader<V, E>::loadVertices(LoadableVertexShard loadableVertexShard)
    -> std::shared_ptr<Quiver<V, E>> {
  auto const& vertexShard = loadableVertexShard.vertexShard;
  auto const& edgeShards = loadableVertexShard.edgeShards;
  auto result = std::make_shared<Quiver<V, E>>();

  transaction::Options trxOpts;
  trxOpts.waitForSync = false;
  trxOpts.allowImplicitCollectionsForRead = true;
  auto ctx = transaction::StandaloneContext::Create(*config->vocbase());
  transaction::Methods trx(ctx, {}, {}, {}, trxOpts,
                           transaction::Hints::Hint::INTERNAL);
  Result res = trx.begin();

  if (!res.ok()) {
    THROW_ARANGO_EXCEPTION(res);
  }

  auto sourceShard = config->graphSerdeConfig().pregelShard(vertexShard);
  auto cursor =
      trx.indexScan(resourceMonitor, vertexShard,
                    transaction::Methods::CursorType::ALL, ReadOwnWrites::no);

  // tell the formatter the number of docs we are about to load
  LogicalCollection* coll = cursor->collection();
  uint64_t numVertices = coll->getPhysical()->numberDocuments(&trx);

  auto vertexIdRange = requestVertexIds(numVertices);
  LOG_PREGEL("7c31f", DEBUG)
      << "Shard '" << vertexShard << "' has " << numVertices
      << " vertices. id range: " << inspection::json(vertexIdRange);

  std::vector<std::unique_ptr<traverser::EdgeCollectionInfo>>
      edgeCollectionInfos;
  edgeCollectionInfos.reserve(edgeShards.size());

  for (ShardID const& edgeShard : edgeShards) {
    edgeCollectionInfos.emplace_back(
        std::make_unique<traverser::EdgeCollectionInfo>(resourceMonitor, &trx,
                                                        edgeShard));
  }

  std::string documentId;  // temp buffer for _id of vertex
  auto cb = [&](LocalDocumentId const& token, VPackSlice slice) {
    Vertex<V, E> ventry;
    auto keySlice = transaction::helpers::extractKeyFromDocument(slice);
    auto key = keySlice.copyString();

    ventry.setShard(sourceShard);
    ventry.setKey(key.c_str(), key.size());
    ventry.setActive(true);

    // load vertex data
    documentId = trx.extractIdString(slice);
    if (graphFormat->estimatedVertexSize() > 0) {
      // note: ventry->_data and vertexIdRange may be modified by
      // copyVertexData!

      TRI_ASSERT(vertexIdRange.current < vertexIdRange.maxId)
          << fmt::format("vertexId exceeded maximum: {} < {}",
                         vertexIdRange.current, vertexIdRange.maxId);
      graphFormat->copyVertexData(*ctx->getVPackOptions(), documentId, slice,
                                  ventry.data(), vertexIdRange.current);
      vertexIdRange.current += 1;
    }

    // load edges
    for (std::size_t i = 0; i < edgeShards.size(); ++i) {
      auto& info = *edgeCollectionInfos[i];
      loadEdges(trx, ventry, documentId, info);
    }
    result->emplace(std::move(ventry));
    return true;
  };

  while (cursor->nextDocument(cb, batchSize)) {
    if (config->vocbase()->server().isStopping()) {
      LOG_PREGEL("4355a", WARN) << "Aborting graph loading";
      break;
    }

    std::visit(overload{[&](ActorLoadingUpdate const& update) {
                          update.fn(message::GraphLoadingUpdate{
                              .verticesLoaded = result->numberOfVertices(),
                              .edgesLoaded = result->numberOfEdges(),
                              .memoryBytesUsed = 0  // TODO
                          });
                        },
                        [](OldLoadingUpdate const& update) {
                          SchedulerFeature::SCHEDULER->queue(
                              RequestLane::INTERNAL_LOW, update.fn);
                        }},
               updateCallback);
  }
  return result;
}

template<typename V, typename E>
void GraphLoader<V, E>::loadEdges(transaction::Methods& trx,
                                  Vertex<V, E>& vertex,
                                  std::string_view documentID,
                                  traverser::EdgeCollectionInfo& info) {
  auto cursor = info.getEdges(std::string{documentID});

  if (graphFormat->estimatedEdgeSize() == 0) {
    // use covering index optimization
    while (cursor->nextCovering(
        [&](LocalDocumentId const& /*token*/,
            IndexIteratorCoveringData& covering) {
          TRI_ASSERT(covering.isArray());
          std::string_view toValue =
              covering.at(info.coveringPosition()).stringView();
          auto toVertexID = config->documentIdToPregel(toValue);

          vertex.addEdge(Edge<E>(toVertexID, {}));
          return true;
        },
        1000)) { /* continue loading */
    }
  } else {
    while (cursor->nextDocument(
        [&](LocalDocumentId const& /*token*/, VPackSlice slice) {
          slice = slice.resolveExternal();
          std::string_view toValue =
              transaction::helpers::extractToFromDocument(slice).stringView();
          auto toVertexID = config->documentIdToPregel(toValue);

          auto edge = Edge<E>(toVertexID, {});
          graphFormat->copyEdgeData(
              *trx.transactionContext()->getVPackOptions(), slice, edge.data());
          vertex.addEdge(std::move(edge));
          return true;
        },
        1000)) { /* continue loading */
      // Might overcount a bit;
    }
  }
}

}  // namespace arangodb::pregel

template struct arangodb::pregel::GraphLoader<int64_t, int64_t>;
template struct arangodb::pregel::GraphLoader<uint64_t, uint64_t>;
template struct arangodb::pregel::GraphLoader<uint64_t, uint8_t>;
template struct arangodb::pregel::GraphLoader<float, float>;
template struct arangodb::pregel::GraphLoader<double, float>;
template struct arangodb::pregel::GraphLoader<double, double>;
template struct arangodb::pregel::GraphLoader<float, uint8_t>;

// specific algo combos
template struct arangodb::pregel::GraphLoader<arangodb::pregel::algos::WCCValue,
                                              uint64_t>;
template struct arangodb::pregel::GraphLoader<arangodb::pregel::algos::SCCValue,
                                              int8_t>;
template struct arangodb::pregel::GraphLoader<arangodb::pregel::algos::ECValue,
                                              int8_t>;
template struct arangodb::pregel::GraphLoader<
    arangodb::pregel::algos::HITSValue, int8_t>;
template struct arangodb::pregel::GraphLoader<
    arangodb::pregel::algos::HITSKleinbergValue, int8_t>;
template struct arangodb::pregel::GraphLoader<
    arangodb::pregel::algos::DMIDValue, float>;
template struct arangodb::pregel::GraphLoader<arangodb::pregel::algos::LPValue,
                                              int8_t>;
template struct arangodb::pregel::GraphLoader<
    arangodb::pregel::algos::SLPAValue, int8_t>;
template struct arangodb::pregel::GraphLoader<
    arangodb::pregel::algos::ColorPropagationValue, int8_t>;

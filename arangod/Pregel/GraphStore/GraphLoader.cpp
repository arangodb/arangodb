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

#include "Pregel/Worker/WorkerConfig.h"

#include "Cluster/ServerState.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "Logger/LogMacros.h"
#include "Scheduler/SchedulerFeature.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"
#include "Transaction/Options.h"
#include "Transaction/StandaloneContext.h"
#include "VocBase/LogicalCollection.h"

#include "ApplicationFeatures/ApplicationServer.h"

#define LOG_PREGEL(logId, level)          \
  LOG_TOPIC(logId, level, Logger::PREGEL) \
      << "[job " << config->executionNumber() << "] "

namespace {
constexpr auto shardError = std::string_view{
    "Collections need to have the same number of shards,"
    " use distributeShardsLike"};
}

namespace arangodb::pregel {
template<typename V, typename E>
auto GraphLoader<V, E>::requestVertexIds(uint64_t numVertices) -> void {
  if (arangodb::ServerState::instance()->isRunningInCluster()) {
    if (config->vocbase()->server().template hasFeature<ClusterFeature>()) {
      arangodb::ClusterInfo& ci = this->config->vocbase()
                                      ->server()
                                      .template getFeature<ClusterFeature>()
                                      .clusterInfo();
      currentVertexId = ci.uniqid(numVertices);
      currentVertexIdMax = currentVertexId + numVertices;
    } else {
      ADB_PROD_ASSERT(false) << "ClusterFeature not present in server";
    }
  } else {
    // Just bump the max
    currentVertexIdMax += numVertices;
  }
}

template<typename V, typename E>
auto GraphLoader<V, E>::load() -> std::shared_ptr<Quiver<V, E>> {
  // Contains the shards located on this db server in the right order
  // assuming edges are sharded after _from, vertices after _key
  // then every ith vertex shard has the corresponding edges in
  // the ith edge shard
  std::map<CollectionID, std::vector<ShardID>> const& vertexCollMap =
      config->vertexCollectionShards();
  std::map<CollectionID, std::vector<ShardID>> const& edgeCollMap =
      config->edgeCollectionShards();
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
          config->edgeCollectionRestrictions(vertexShard);

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
        loadVertices(vertexShard, edges);
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
  return result;
}

template<typename V, typename E>
auto GraphLoader<V, E>::loadVertices(ShardID const& vertexShard,
                                     std::vector<ShardID> const& edgeShards)
    -> void {
  transaction::Options trxOpts;
  trxOpts.waitForSync = false;
  trxOpts.allowImplicitCollectionsForRead = true;
  auto ctx = transaction::StandaloneContext::Create(*config->vocbase());
  transaction::Methods trx(ctx, {}, {}, {}, trxOpts);
  Result res = trx.begin();

  if (!res.ok()) {
    THROW_ARANGO_EXCEPTION(res);
  }

  auto sourceShard = config->shardId(vertexShard);
  auto cursor =
      trx.indexScan(resourceMonitor, vertexShard,
                    transaction::Methods::CursorType::ALL, ReadOwnWrites::no);

  // tell the formatter the number of docs we are about to load
  LogicalCollection* coll = cursor->collection();
  uint64_t numVertices =
      coll->numberDocuments(&trx, transaction::CountType::Normal);

  requestVertexIds(numVertices);
  LOG_PREGEL("7c31f", DEBUG)
      << "Shard '" << vertexShard << "' has " << numVertices
      << " vertices. id range: [" << currentVertexId << ", "
      << currentVertexIdMax << ")";

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

      TRI_ASSERT(currentVertexId < currentVertexIdMax)
          << fmt::format("vertexId exceeded maximum: {} < {}", currentVertexId,
                         currentVertexIdMax);
      graphFormat->copyVertexData(*ctx->getVPackOptions(), documentId, slice,
                                  ventry.data(), currentVertexId);
      currentVertexId += 1;
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

    // log only every 10 seconds
    SchedulerFeature::SCHEDULER->queue(RequestLane::INTERNAL_LOW,
                                       statusUpdateCallback);
  }
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

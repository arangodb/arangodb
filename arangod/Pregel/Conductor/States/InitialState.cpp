#include "InitialState.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Cluster/ServerState.h"
#include "Pregel/Algorithm.h"
#include "Pregel/Conductor/Conductor.h"
#include "VocBase/LogicalCollection.h"

using namespace arangodb::pregel::conductor;

Initial::Initial(Conductor& conductor) : conductor{conductor} {
  conductor._timing.total.start();
}

auto Initial::run() -> std::optional<std::unique_ptr<State>> {
  auto [workerInitializations, leadingServerForShard] =
      _workerInitializations();

  conductor._leadingServerForShard = leadingServerForShard;
  auto servers = std::vector<ServerID>{};
  for (auto const& [server, _] : workerInitializations) {
    servers.push_back(server);
  }
  conductor._status = ConductorStatus::forWorkers(servers);

  auto created =
      conductor._workers.createWorkers(workerInitializations)
          .thenValue([&](auto result) -> Result {
            if (result.fail()) {
              return Result{result.errorNumber(), result.errorMessage()};
            }
            return Result{};
          })
          .get();
  if (created.fail()) {
    LOG_PREGEL_CONDUCTOR_STATE("ae855", ERR)
        << fmt::format("Loading state: {}", created.errorMessage());

    return std::make_unique<Canceled>(conductor);
  }
  return std::make_unique<Loading>(conductor);
}

namespace {
using namespace arangodb;
auto resolveInfo(
    TRI_vocbase_t* vocbase, CollectionID const& collectionID,
    std::unordered_map<CollectionID, std::string>& collectionPlanIdMap,
    std::map<ServerID, std::map<CollectionID, std::vector<ShardID>>>& serverMap,
    std::vector<ShardID>& allShards) -> void {
  ServerState* ss = ServerState::instance();
  if (!ss->isRunningInCluster()) {
    auto lc = vocbase->lookupCollection(collectionID);

    if (lc == nullptr || lc->deleted()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
                                     collectionID);
    }

    collectionPlanIdMap.try_emplace(collectionID,
                                    std::to_string(lc->planId().id()));
    allShards.push_back(collectionID);
    serverMap[ss->getId()][collectionID].push_back(collectionID);

  } else if (ss->isCoordinator()) {
    ClusterInfo& ci =
        vocbase->server().getFeature<ClusterFeature>().clusterInfo();
    std::shared_ptr<LogicalCollection> lc =
        ci.getCollection(vocbase->name(), collectionID);
    if (lc->deleted()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
                                     collectionID);
    }
    collectionPlanIdMap.try_emplace(collectionID,
                                    std::to_string(lc->planId().id()));

    std::shared_ptr<std::vector<ShardID>> shardIDs =
        ci.getShardList(std::to_string(lc->id().id()));
    allShards.insert(allShards.end(), shardIDs->begin(), shardIDs->end());

    for (auto const& shard : *shardIDs) {
      std::shared_ptr<std::vector<ServerID> const> servers =
          ci.getResponsibleServer(shard);
      if (servers->size() > 0) {
        serverMap[(*servers)[0]][lc->name()].push_back(shard);
      }
    }
  } else {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_CLUSTER_ONLY_ON_COORDINATOR);
  }
}
}  // namespace

auto Initial::_workerInitializations() const
    -> std::tuple<std::unordered_map<ServerID, CreateWorker>,
                  std::unordered_map<ShardID, ServerID>> {
  std::unordered_map<CollectionID, std::string> collectionPlanIdMap;
  std::map<ServerID, std::map<CollectionID, std::vector<ShardID>>> vertexMap,
      edgeMap;
  std::vector<ShardID> shardList;

  // resolve plan id's and shards on the servers
  for (CollectionID const& collectionID : conductor._vertexCollections) {
    resolveInfo(&(conductor._vocbaseGuard.database()), collectionID,
                collectionPlanIdMap, vertexMap, shardList);
  }
  for (CollectionID const& collectionID : conductor._edgeCollections) {
    resolveInfo(&(conductor._vocbaseGuard.database()), collectionID,
                collectionPlanIdMap, edgeMap, shardList);
  }

  auto createWorkers = std::unordered_map<ServerID, CreateWorker>{};
  auto leadingServerForShard = std::unordered_map<ShardID, ServerID>{};
  for (auto const& [server, vertexShards] : vertexMap) {
    auto const& edgeShards = edgeMap[server];
    createWorkers.emplace(
        server, CreateWorker{.executionNumber = conductor._executionNumber,
                             .algorithm = conductor._algorithm->name(),
                             .userParameters = conductor._userParams,
                             .coordinatorId = ServerState::instance()->getId(),
                             .useMemoryMaps = conductor._useMemoryMaps,
                             .edgeCollectionRestrictions =
                                 conductor._edgeCollectionRestrictions,
                             .vertexShards = vertexShards,
                             .edgeShards = edgeShards,
                             .collectionPlanIds = collectionPlanIdMap,
                             .allShards = shardList});
    for (auto const& [_, shards] : vertexShards) {
      for (auto const& shard : shards) {
        leadingServerForShard[shard] = server;
      }
    }
  }

  return make_tuple(createWorkers, leadingServerForShard);
}

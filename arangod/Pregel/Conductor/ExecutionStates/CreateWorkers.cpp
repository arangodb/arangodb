#include "CreateWorkers.h"

#include "Cluster/ClusterFeature.h"
#include "Cluster/ServerState.h"
#include "Pregel/Conductor/Actor.h"
#include "Pregel/Conductor/ExecutionStates/LoadingState.h"
#include "VocBase/LogicalCollection.h"
#include "ApplicationFeatures/ApplicationServer.h"

using namespace arangodb;
using namespace arangodb::pregel::conductor;

CreateWorkers::CreateWorkers(ConductorState& conductor) : conductor{conductor} {
  conductor.timing.total.start();
}

auto CreateWorkers::messages()
    -> std::unordered_map<ServerID, worker::message::CreateNewWorker> {
  auto workerSpecifications = _workerSpecifications();

  auto servers = std::vector<ServerID>{};
  for (auto const& [server, _] : workerSpecifications) {
    servers.emplace_back(server);
    sentServers.emplace(server);
  }
  conductor.status = ConductorStatus::forWorkers(servers);

  return workerSpecifications;
}

auto CreateWorkers::receive(actor::ActorPID sender,
                            message::ConductorMessages message)
    -> std::optional<std::unique_ptr<ExecutionState>> {
  if (not sentServers.contains(sender.server) or
      not std::holds_alternative<ResultT<message::WorkerCreated>>(message)) {
    // TODO return error state (GORDO-1553)
    return std::nullopt;
  }
  auto workerCreated = std::get<ResultT<message::WorkerCreated>>(message);
  if (not workerCreated.ok()) {
    // TODO return error state (GORDO-1553)
    return std::nullopt;
  }
  conductor.workers.emplace_back(sender);
  respondedServers.emplace(sender.server);
  responseCount++;

  if (responseCount == sentServers.size() and respondedServers == sentServers) {
    return std::make_unique<Loading>(conductor);
  }
  return std::nullopt;
};

static void resolveInfo(
    TRI_vocbase_t* vocbase, CollectionID const& collectionID,
    std::unordered_map<CollectionID, std::string>& collectionPlanIdMap,
    std::map<ServerID, std::map<CollectionID, std::vector<ShardID>>>& serverMap,
    std::vector<ShardID>& allShards) {
  ServerState* ss = ServerState::instance();
  if (!ss->isRunningInCluster()) {  // single server mode
    auto lc = vocbase->lookupCollection(collectionID);

    if (lc == nullptr || lc->deleted()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
                                     collectionID);
    }

    collectionPlanIdMap.try_emplace(collectionID,
                                    std::to_string(lc->planId().id()));
    allShards.push_back(collectionID);
    serverMap[ss->getId()][collectionID].push_back(collectionID);

  } else if (ss->isCoordinator()) {  // we are in the cluster

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

auto CreateWorkers::_workerSpecifications() const
    -> std::unordered_map<ServerID, worker::message::CreateNewWorker> {
  std::unordered_map<CollectionID, std::string> collectionPlanIdMap;
  std::map<ServerID, std::map<CollectionID, std::vector<ShardID>>> vertexMap,
      edgeMap;
  std::vector<ShardID> shardList;

  for (CollectionID const& collectionID :
       conductor.specifications.vertexCollections) {
    resolveInfo(&(conductor.vocbaseGuard.database()), collectionID,
                collectionPlanIdMap, vertexMap, shardList);
  }
  for (CollectionID const& collectionID :
       conductor.specifications.edgeCollections) {
    resolveInfo(&(conductor.vocbaseGuard.database()), collectionID,
                collectionPlanIdMap, edgeMap, shardList);
  }

  auto createWorkers =
      std::unordered_map<ServerID, worker::message::CreateNewWorker>{};
  for (auto const& [server, vertexShards] : vertexMap) {
    auto const& edgeShards = edgeMap[server];
    createWorkers.emplace(
        server, worker::message::CreateNewWorker{
                    .executionSpecifications = conductor.specifications,
                    .collectionSpecifications = CollectionSpecifications{
                        .vertexShards = std::move(vertexShards),
                        .edgeShards = std::move(edgeShards),
                        .collectionPlanIds = std::move(collectionPlanIdMap),
                        .allShards = std::move(shardList)}});
  }
  return createWorkers;
}

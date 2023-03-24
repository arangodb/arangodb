#include "CreateWorkersState.h"

#include "Pregel/Conductor/ExecutionStates/CollectionLookup.h"
#include "Pregel/Conductor/ExecutionStates/LoadingState.h"
#include "Pregel/Conductor/ExecutionStates/FatalErrorState.h"

using namespace arangodb;
using namespace arangodb::pregel;
using namespace arangodb::pregel::conductor;

CreateWorkers::CreateWorkers(ConductorState& conductor) : conductor{conductor} {
  conductor.timing.total.start();
}

auto workerSpecification(
    std::unique_ptr<CollectionLookup> const& collectionLookup,
    ExecutionSpecifications const& specifications)
    -> std::unordered_map<ServerID, worker::message::CreateWorker> {
  auto createWorkers =
      std::unordered_map<ServerID, worker::message::CreateWorker>{};
  for (auto const& [server, vertexShards] :
       collectionLookup->getServerMapVertices()) {
    auto edgeShards = collectionLookup->getServerMapEdges().at(server);
    createWorkers.emplace(
        server,
        worker::message::CreateWorker{
            .executionNumber = specifications.executionNumber,
            .algorithm = std::string{specifications.algorithm},
            .userParameters = specifications.userParameters,
            .coordinatorId = "",
            .useMemoryMaps = specifications.useMemoryMaps,
            .parallelism = specifications.parallelism,
            .edgeCollectionRestrictions =
                specifications.edgeCollectionRestrictions,
            .vertexShards = std::move(vertexShards),
            .edgeShards = std::move(edgeShards),
            .collectionPlanIds = collectionLookup->getCollectionPlanIdMapAll(),
            .allShards = collectionLookup->getAllShards()});
  }
  return createWorkers;
}

auto CreateWorkers::messagesToServers()
    -> std::unordered_map<ServerID, worker::message::CreateWorker> {
  auto workerSpecifications =
      workerSpecification(conductor.lookupInfo, conductor.specifications);

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
    return std::make_unique<FatalError>(conductor);
  }
  auto workerCreated = std::get<ResultT<message::WorkerCreated>>(message);
  if (not workerCreated.ok()) {
    return std::make_unique<FatalError>(conductor);
  }
  conductor.workers.emplace(sender);

  _updateResponsibleActorPerShard(sender);

  respondedServers.emplace(sender.server);
  responseCount++;

  if (responseCount == sentServers.size() and respondedServers == sentServers) {
    return std::make_unique<Loading>(conductor, std::move(actorForShard));
  }
  return std::nullopt;
};

auto CreateWorkers::_updateResponsibleActorPerShard(actor::ActorPID actor)
    -> void {
  auto vertexCollectionsOnSenderServer =
      conductor.lookupInfo->getServerMapVertices()[actor.server];
  auto edgeCollectionsOnSenderServer =
      conductor.lookupInfo->getServerMapEdges()[actor.server];
  for (auto const& [_, shards] : vertexCollectionsOnSenderServer) {
    for (auto const& shard : shards) {
      actorForShard.emplace(shard, actor);
    }
  }
  for (auto const& [_, shards] : edgeCollectionsOnSenderServer) {
    for (auto const& shard : shards) {
      actorForShard.emplace(shard, actor);
    }
  }
}

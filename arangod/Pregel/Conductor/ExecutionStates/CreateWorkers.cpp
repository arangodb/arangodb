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
  conductor._timing.total.start();
}

auto CreateWorkers::messages()
    -> std::unordered_map<ServerID, worker::message::CreateNewWorker> {
  auto workerSpecifications = _workerSpecifications();

  auto servers = std::vector<ServerID>{};
  for (auto const& [server, _] : workerSpecifications) {
    servers.emplace_back(server);
    sentServers.emplace(server);
  }
  conductor._status = ConductorStatus::forWorkers(servers);

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
  conductor._workers.emplace_back(sender);
  respondedServers.emplace(sender.server);
  responseCount++;

  if (responseCount == sentServers.size() and respondedServers == sentServers) {
    return std::make_unique<Loading>(conductor);
  }
  return std::nullopt;
};

auto CreateWorkers::_workerSpecifications() const
    -> std::unordered_map<ServerID, worker::message::CreateNewWorker> {
  auto createWorkers =
      std::unordered_map<ServerID, worker::message::CreateNewWorker>{};
  TRI_ASSERT(!conductor._lookupInfo->getServerMapVertices().empty());
  for (auto const& [server, vertexShards] :
       conductor._lookupInfo->getServerMapVertices()) {
    auto const& edgeShards =
        conductor._lookupInfo->getServerMapEdges().at(server);
    TRI_ASSERT(!edgeShards.empty());
    createWorkers.emplace(
        server, worker::message::CreateNewWorker{
                    .executionSpecifications = conductor._specifications,
                    .collectionSpecifications = CollectionSpecifications{
                        .vertexShards = std::move(vertexShards),
                        .edgeShards = std::move(edgeShards),
                        .collectionPlanIds = std::move(
                            conductor._lookupInfo->getCollectionPlanIdMapAll()),
                        .allShards =
                            std::move(conductor._lookupInfo->getAllShards())}});
  }
  return createWorkers;
}

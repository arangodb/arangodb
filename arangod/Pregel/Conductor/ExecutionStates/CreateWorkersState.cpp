#include "CreateWorkersState.h"

#include "Pregel/Conductor/ExecutionStates/LoadingState.h"
#include "Pregel/Conductor/ExecutionStates/FatalErrorState.h"

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
    return std::make_unique<FatalError>(conductor);
  }
  auto workerCreated = std::get<ResultT<message::WorkerCreated>>(message);
  if (not workerCreated.ok()) {
    return std::make_unique<FatalError>(conductor);
  }
  conductor.workers.emplace(sender);
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
  for (auto const& [server, vertexShards] :
       conductor.lookupInfo->getServerMapVertices()) {
    auto edgeShards = conductor.lookupInfo->getServerMapEdges().at(server);
    createWorkers.emplace(
        server, worker::message::CreateNewWorker{
                    .executionSpecifications = conductor.specifications,
                    .collectionSpecifications = CollectionSpecifications{
                        .vertexShards = std::move(vertexShards),
                        .edgeShards = std::move(edgeShards),
                        .collectionPlanIds =
                            conductor.lookupInfo->getCollectionPlanIdMapAll(),
                        .allShards = conductor.lookupInfo->getAllShards()}});
  }
  return createWorkers;
}

#include "InitialState.h"

#include "Cluster/ServerState.h"
#include "Pregel/Algorithm.h"
#include "Pregel/Conductor/Conductor.h"

using namespace arangodb::pregel::conductor;

Initial::Initial(Conductor& conductor, WorkerApi<WorkerCreated>&& workerApi)
    : conductor{conductor}, _workerApi{std::move(workerApi)} {
  conductor._timing.total.start();
}

auto Initial::run() -> std::optional<std::unique_ptr<State>> {
  auto const& [workerInitializations, leadingServerForShard] =
      _workerInitializations();

  conductor._leadingServerForShard = leadingServerForShard;
  auto servers = std::vector<ServerID>{};
  for (auto const& [server, _] : workerInitializations) {
    servers.push_back(server);
  }
  conductor._status = ConductorStatus::forWorkers(servers);

  _workerApi._servers = servers;
  auto sent = _workerApi.send(workerInitializations);
  if (sent.fail()) {
    LOG_PREGEL_CONDUCTOR_STATE("ae855", ERR) << sent.errorMessage();
    return std::make_unique<Canceled>(conductor, std::move(_workerApi));
  }

  auto aggregate = _workerApi.aggregateAllResponses();
  if (aggregate.fail()) {
    LOG_PREGEL_CONDUCTOR_STATE("1e855", ERR) << aggregate.errorMessage();
    return std::make_unique<Canceled>(conductor, std::move(_workerApi));
  }

  return std::make_unique<Loading>(conductor, std::move(_workerApi));
}

auto Initial::receive(MessagePayload message) -> void {
  auto queued = _workerApi.queue(message);
  if (queued.fail()) {
    LOG_PREGEL_CONDUCTOR_STATE("7698e", ERR) << queued.errorMessage();
  }
}

auto Initial::_workerInitializations() const
    -> std::tuple<std::unordered_map<ServerID, CreateWorker>,
                  std::unordered_map<ShardID, ServerID>> {
  auto graphSource = conductor._graphSource;
  auto createWorkers = std::unordered_map<ServerID, CreateWorker>{};
  auto leadingServerForShard = std::unordered_map<ShardID, ServerID>{};
  for (auto const& [server, vertexShards] : graphSource.vertexShards) {
    auto const& edgeShards = graphSource.edgeShards[server];
    createWorkers.emplace(
        server, CreateWorker{.executionNumber = conductor._executionNumber,
                             .algorithm = conductor._algorithm->name(),
                             .userParameters = conductor._userParams,
                             .coordinatorId = ServerState::instance()->getId(),
                             .useMemoryMaps = conductor._useMemoryMaps,
                             .edgeCollectionRestrictions =
                                 graphSource.edgeCollectionRestrictions,
                             .vertexShards = vertexShards,
                             .edgeShards = edgeShards,
                             .collectionPlanIds = graphSource.planIds,
                             .allShards = graphSource.allShards});
    for (auto const& [_, shards] : vertexShards) {
      for (auto const& shard : shards) {
        leadingServerForShard[shard] = server;
      }
    }
  }

  return make_tuple(createWorkers, leadingServerForShard);
}

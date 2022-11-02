#include "ComputingState.h"
#include <cstdint>

#include "Cluster/ClusterTypes.h"
#include "Pregel/Conductor/Conductor.h"
#include "Metrics/Gauge.h"
#include "Pregel/MasterContext.h"
#include "Pregel/PregelFeature.h"

using namespace arangodb::pregel::conductor;

Computing::Computing(Conductor& conductor, SendCountPerServer count,
                     WorkerApi<GlobalSuperStepFinished>&& workerApi)
    : conductor{conductor},
      _sendCountPerServer{std::move(count)},
      _workerApi{std::move(workerApi)} {}

Computing::Computing(Conductor& conductor,
                     WorkerApi<GlobalSuperStepFinished>&& workerApi)

    : conductor{conductor}, _workerApi{std::move(workerApi)} {
  conductor._timing.computation.start();
  conductor._feature.metrics()->pregelConductorsRunningNumber->fetch_add(1);
}

auto Computing::run() -> std::optional<std::unique_ptr<State>> {
  conductor._timing.gss.emplace_back(Duration{
      ._start = std::chrono::steady_clock::now(), ._finish = std::nullopt});

  conductor._preGlobalSuperStep();

  auto const runGlobalSuperStepCommand = _runGlobalSuperStepCommand();
  VPackBuilder startCommand;
  serialize(startCommand, runGlobalSuperStepCommand);
  LOG_PREGEL_CONDUCTOR_STATE("d98de", DEBUG) << fmt::format(
      "Initiate starting GSS with {}", startCommand.slice().toJson());
  auto sent = _workerApi.send(runGlobalSuperStepCommand);
  if (sent.fail()) {
    LOG_PREGEL_CONDUCTOR_STATE("f34bb", ERR) << sent.errorMessage();
    return std::make_unique<FatalError>(conductor, std::move(_workerApi));
  }

  auto aggregate = _workerApi.aggregateAllResponses();
  if (aggregate.fail()) {
    LOG_PREGEL_CONDUCTOR_STATE("ff1000", ERR) << aggregate.errorMessage();
    return std::make_unique<FatalError>(conductor, std::move(_workerApi));
  }

  auto workerData = aggregate.get();
  conductor._statistics.accumulate(workerData.messageStats);
  conductor._aggregators->resetValues();
  for (auto aggregator : VPackArrayIterator(workerData.aggregators.slice())) {
    conductor._aggregators->aggregateValues(aggregator);
  }
  conductor._statistics.setActiveCounts(workerData.activeCount);
  conductor._totalVerticesCount = workerData.vertexCount;
  conductor._totalEdgesCount = workerData.edgeCount;

  auto post = conductor._postGlobalSuperStep();

  LOG_PREGEL_CONDUCTOR_STATE("39385", DEBUG)
      << fmt::format("Finished gss {} in {}s", conductor._globalSuperstep,
                     conductor._timing.gss.back().elapsedSeconds().count());
  conductor._timing.gss.back().finish();

  if (post.finished) {
    if (conductor._masterContext) {
      conductor._masterContext->postApplication();
    }
    conductor._timing.computation.finish();
    conductor._feature.metrics()->pregelConductorsRunningNumber->fetch_sub(1);

    if (conductor._storeResults) {
      return std::make_unique<Storing>(conductor, std::move(_workerApi));
    }
    return std::make_unique<Done>(conductor, std::move(_workerApi));
  }

  conductor._globalSuperstep++;
  return std::make_unique<Computing>(
      conductor,
      _transformSendCountFromShardToServer(
          std::move(workerData.sendCountPerShard)),
      std::move(_workerApi));
}

auto Computing::receive(MessagePayload message) -> void {
  auto queued = _workerApi.queue(message);
  if (queued.fail()) {
    LOG_PREGEL_CONDUCTOR_STATE("7698e", ERR) << queued.errorMessage();
  }
}

auto Computing::cancel() -> std::optional<std::unique_ptr<State>> {
  return std::make_unique<Canceled>(conductor, std::move(_workerApi));
}

auto Computing::_runGlobalSuperStepCommand() const
    -> std::unordered_map<ServerID, RunGlobalSuperStep> {
  VPackBuilder aggregators;
  {
    VPackObjectBuilder ob(&aggregators);
    conductor._aggregators->serializeValues(aggregators);
  }
  auto out = std::unordered_map<ServerID, RunGlobalSuperStep>();
  for (auto const& server : _workerApi._servers) {
    out.emplace(server, RunGlobalSuperStep{
                            .gss = conductor._globalSuperstep,
                            .vertexCount = conductor._totalVerticesCount,
                            .edgeCount = conductor._totalEdgesCount,
                            .sendCount = _sendCountPerServer.contains(server)
                                             ? _sendCountPerServer.at(server)
                                             : 0,
                            .aggregators = aggregators});
  }
  return out;
}

auto Computing::_transformSendCountFromShardToServer(
    std::unordered_map<ShardID, uint64_t> sendCountPerShard) const
    -> std::unordered_map<ServerID, uint64_t> {
  auto sendCountPerServer = std::unordered_map<ServerID, uint64_t>{};
  for (auto const& [shard, counts] : sendCountPerShard) {
    sendCountPerServer[conductor._leadingServerForShard[shard]] += counts;
  }
  return sendCountPerServer;
}

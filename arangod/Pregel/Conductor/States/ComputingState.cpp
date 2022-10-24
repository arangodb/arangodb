#include "ComputingState.h"
#include <cstdint>
#include <optional>

#include "Pregel/Algorithm.h"
#include "Pregel/Conductor/Conductor.h"
#include "Metrics/Gauge.h"
#include "Pregel/Conductor/States/State.h"
#include "Pregel/MasterContext.h"
#include "Pregel/PregelFeature.h"
#include "velocypack/Builder.h"
#include "velocypack/Iterator.h"

using namespace arangodb::pregel::conductor;

Computing::Computing(Conductor& conductor) : conductor{conductor} {
  if (!conductor._timing.computation.hasStarted()) {
    conductor._timing.computation.start();
  }
  conductor._feature.metrics()->pregelConductorsRunningNumber->fetch_add(1);
}

Computing::~Computing() {
  if (!conductor._timing.computation.hasFinished()) {
    conductor._timing.computation.finish();
  }
  conductor._feature.metrics()->pregelConductorsRunningNumber->fetch_sub(1);
}

auto Computing::run() -> std::optional<std::unique_ptr<State>> {
  LOG_PREGEL_CONDUCTOR_STATE("76631", INFO)
      << fmt::format("Start running Pregel {} with {} vertices, {} edges",
                     conductor._algorithm->name(),
                     conductor._totalVerticesCount, conductor._totalEdgesCount);

  conductor._timing.gss.emplace_back(Duration{
      ._start = std::chrono::steady_clock::now(), ._finish = std::nullopt});

  conductor._preGlobalSuperStep();

  auto const runGlobalSuperStepCommand = _runGlobalSuperStepCommand();
  VPackBuilder startCommand;
  serialize(startCommand, runGlobalSuperStepCommand);
  LOG_PREGEL_CONDUCTOR_STATE("d98de", DEBUG) << fmt::format(
      "Initiate starting GSS with {}", startCommand.slice().toJson());

  return _aggregate.doUnderLock(
      [&](auto& agg) -> std::optional<std::unique_ptr<State>> {
        auto aggregate = conductor._workers.runGlobalSuperStep(
            runGlobalSuperStepCommand, _sendCountPerServer);
        if (aggregate.fail()) {
          LOG_PREGEL_CONDUCTOR_STATE("f34bb", ERR) << aggregate.errorMessage();
          return std::make_unique<FatalError>(conductor);
        }
        agg = aggregate.get();
        return std::nullopt;
      });
}

auto Computing::receive(MessagePayload message)
    -> std::optional<std::unique_ptr<State>> {
  auto explicitMessage = getResultTMessage<GlobalSuperStepFinished>(message);
  if (explicitMessage.fail()) {
    LOG_PREGEL_CONDUCTOR_STATE("7698e", ERR) << explicitMessage.errorMessage();
    return std::make_unique<FatalError>(conductor);
  }
  auto finishedAggregate =
      _aggregate.doUnderLock([message = std::move(explicitMessage).get()](
                                 auto& agg) { return agg.aggregate(message); });
  if (!finishedAggregate.has_value()) {
    return std::nullopt;
  }

  conductor._statistics.accumulate(finishedAggregate.value().messageStats);
  conductor._aggregators->resetValues();
  for (auto aggregator :
       VPackArrayIterator(finishedAggregate.value().aggregators.slice())) {
    conductor._aggregators->aggregateValues(aggregator);
  }
  conductor._statistics.setActiveCounts(finishedAggregate.value().activeCount);
  conductor._totalVerticesCount = finishedAggregate.value().vertexCount;
  conductor._totalEdgesCount = finishedAggregate.value().edgeCount;
  auto sendCountPerServer = std::unordered_map<ServerID, uint64_t>{};
  for (auto const& [shard, counts] :
       finishedAggregate.value().sendCountPerShard) {
    sendCountPerServer[conductor._leadingServerForShard[shard]] += counts;
  }
  _sendCountPerServer = _transformSendCountFromShardToServer(
      std::move(finishedAggregate.value().sendCountPerShard));

  auto post = conductor._postGlobalSuperStep();

  LOG_PREGEL_CONDUCTOR_STATE("39385", DEBUG)
      << fmt::format("Finished gss {} in {}s", conductor._globalSuperstep,
                     conductor._timing.gss.back().elapsedSeconds().count());
  conductor._timing.gss.back().finish();

  if (post.finished) {
    if (conductor._masterContext) {
      conductor._masterContext->postApplication();
    }
    if (conductor._storeResults) {
      return std::make_unique<Storing>(conductor);
    }
    return std::make_unique<Done>(conductor);
  }

  conductor._globalSuperstep++;
  return run();
}

auto Computing::_runGlobalSuperStepCommand() const -> RunGlobalSuperStep {
  VPackBuilder aggregators;
  {
    VPackObjectBuilder ob(&aggregators);
    conductor._aggregators->serializeValues(aggregators);
  }
  return RunGlobalSuperStep{.gss = conductor._globalSuperstep,
                            .vertexCount = conductor._totalVerticesCount,
                            .edgeCount = conductor._totalEdgesCount,
                            .sendCount = 0,
                            .aggregators = std::move(aggregators)};
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

auto Computing::cancel() -> std::optional<std::unique_ptr<State>> {
  return std::make_unique<Canceled>(conductor);
}

#include "ComputingState.h"
#include <cstdint>
#include <optional>

#include "Cluster/ClusterTypes.h"
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
  conductor._timing.gss.emplace_back(Duration{
      ._start = std::chrono::steady_clock::now(), ._finish = std::nullopt});

  conductor._preGlobalSuperStep();

  auto const runGlobalSuperStepCommand = _runGlobalSuperStepCommand();
  VPackBuilder startCommand;
  serialize(startCommand, runGlobalSuperStepCommand);
  LOG_PREGEL_CONDUCTOR("d98de", DEBUG)
      << "Initiate starting GSS: " << startCommand.slice().toJson();

  return _aggregate.doUnderLock(
      [&](auto& agg) -> std::optional<std::unique_ptr<State>> {
        auto aggregate = conductor._workers.runGlobalSuperStep(
            runGlobalSuperStepCommand, _sendCountPerServer);
        if (aggregate.fail()) {
          LOG_PREGEL_CONDUCTOR("f34bb", ERR)
              << fmt::format("Computing state: {}", aggregate.errorMessage());
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
    // TODO The state changes to Canceled if this message fails, but there can
    // still be other GraphLoaded messages that are/will be sent from workers,
    // what happens to them? Currently: in Canceled state any received messages
    // are ignored, this is fine but we have to be careful to change this
    LOG_PREGEL_CONDUCTOR("7698e", ERR)
        << fmt::format("Computing state: {}", explicitMessage.errorMessage());
    return std::make_unique<FatalError>(conductor);
  }
  auto finishedAggregate = _aggregate.doUnderLock(
      [&](auto& agg) { return agg.aggregate(explicitMessage.get()); });

  if (finishedAggregate.has_value()) {
    conductor._statistics.accumulate(finishedAggregate.value().messageStats);
    conductor._aggregators->resetValues();
    for (auto aggregator :
         VPackArrayIterator(finishedAggregate.value().aggregators.slice())) {
      conductor._aggregators->aggregateValues(aggregator);
    }
    conductor._statistics.setActiveCounts(
        finishedAggregate.value().activeCount);
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
    if (post.finished) {
      if (conductor._storeResults) {
        return std::make_unique<Storing>(conductor);
      }
      return std::make_unique<Done>(conductor);
    }

    conductor._timing.gss.back().finish();
    LOG_PREGEL_CONDUCTOR("39385", DEBUG)
        << "Finished gss " << conductor._globalSuperstep << " in "
        << conductor._timing.gss.back().elapsedSeconds().count() << "s";
    conductor._globalSuperstep++;
    return run();
  };
  return std::nullopt;
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

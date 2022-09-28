#include "ComputingState.h"

#include "Pregel/Conductor/Conductor.h"
#include "Metrics/Gauge.h"
#include "Pregel/Conductor/States/State.h"
#include "Pregel/MasterContext.h"
#include "Pregel/PregelFeature.h"
#include "Pregel/WorkerConductorMessages.h"
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
  do {
    auto prepared = _prepareGlobalSuperStep().get();
    if (prepared.fail()) {
      LOG_PREGEL_CONDUCTOR("04189", ERR) << prepared.errorMessage();
      return std::make_unique<FatalError>(conductor);
    }

    auto post = conductor._postGlobalSuperStep();
    if (post.finished) {
      if (conductor._storeResults) {
        return std::make_unique<Storing>(conductor);
      }
      return std::make_unique<Done>(conductor);
    }
    conductor._preGlobalSuperStep();

    auto runGlobalSuperStep = _runGlobalSuperStep().get();
    if (runGlobalSuperStep.fail()) {
      LOG_PREGEL_CONDUCTOR("f34bb", ERR) << runGlobalSuperStep.errorMessage();
      return std::make_unique<FatalError>(conductor);
    }
  } while (true);
}

auto Computing::_prepareGlobalSuperStep() -> futures::Future<Result> {
  if (conductor._feature.isStopping()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_SHUTTING_DOWN);
  }

  conductor._aggregators->resetValues();
  conductor._statistics.resetActiveCount();
  conductor._totalVerticesCount = 0;
  conductor._totalEdgesCount = 0;

  return conductor._workers
      .prepareGlobalSuperStep(
          PrepareGlobalSuperStep{.gss = conductor._globalSuperstep,
                                 .vertexCount = conductor._totalVerticesCount,
                                 .edgeCount = conductor._totalEdgesCount})
      .thenValue([&](auto globalSuperStepPrepared) -> Result {
        if (globalSuperStepPrepared.fail()) {
          return Result{globalSuperStepPrepared.errorNumber(),
                        fmt::format("While preparing global super step {}: {}",
                                    conductor._globalSuperstep,
                                    globalSuperStepPrepared.errorMessage())};
        }
        for (auto aggregator : VPackArrayIterator(
                 globalSuperStepPrepared.get().aggregators.slice())) {
          conductor._aggregators->aggregateValues(aggregator);
        }
        conductor._statistics.accumulateActiveCounts(
            globalSuperStepPrepared.get().activeCount);
        conductor._totalVerticesCount +=
            globalSuperStepPrepared.get().vertexCount;
        conductor._totalEdgesCount += globalSuperStepPrepared.get().edgeCount;
        return Result{};
      });
}

auto Computing::_runGlobalSuperStepCommand() -> RunGlobalSuperStep {
  VPackBuilder aggregators;
  {
    VPackObjectBuilder ob(&aggregators);
    conductor._aggregators->serializeValues(aggregators);
  }
  return RunGlobalSuperStep{.gss = conductor._globalSuperstep,
                            .vertexCount = conductor._totalVerticesCount,
                            .edgeCount = conductor._totalEdgesCount,
                            .aggregators = std::move(aggregators)};
}

auto Computing::_runGlobalSuperStep() -> futures::Future<Result> {
  auto runGlobalSuperStepCommand = _runGlobalSuperStepCommand();
  conductor._timing.gss.emplace_back(Duration{
      ._start = std::chrono::steady_clock::now(), ._finish = std::nullopt});
  VPackBuilder startCommand;
  serialize(startCommand, runGlobalSuperStepCommand);
  LOG_PREGEL_CONDUCTOR("d98de", DEBUG)
      << "Initiate starting GSS: " << startCommand.slice().toJson();
  return conductor._workers.runGlobalSuperStep(runGlobalSuperStepCommand)
      .thenValue([&](auto globalSuperStepFinished) -> Result {
        if (globalSuperStepFinished.fail()) {
          return Result{globalSuperStepFinished.errorNumber(),
                        fmt::format("While running global super step {}: {}",
                                    conductor._globalSuperstep,
                                    globalSuperStepFinished.errorMessage())};
        }
        conductor._statistics.accumulate(
            globalSuperStepFinished.get().messageStats);
        conductor._timing.gss.back().finish();
        LOG_PREGEL_CONDUCTOR("39385", DEBUG)
            << "Finished gss " << conductor._globalSuperstep << " in "
            << conductor._timing.gss.back().elapsedSeconds().count() << "s";
        conductor._globalSuperstep++;
        return Result{};
      });
}

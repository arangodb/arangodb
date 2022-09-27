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
    auto messages = _prepareGlobalSuperStep().get();
    if (messages.fail()) {
      LOG_PREGEL_CONDUCTOR("04189", ERR) << messages.errorMessage();
      return std::make_unique<FatalError>(conductor);
    }

    auto post = conductor._postGlobalSuperStep(messages.get());
    if (post.finished) {
      if (conductor._storeResults) {
        return std::make_unique<Storing>(conductor);
      }
      if (conductor._inErrorAbort) {
        return std::make_unique<FatalError>(conductor);
      }
      return std::make_unique<Done>(conductor);
    }
    bool preGlobalSuperStep = conductor._preGlobalSuperStep();
    if (!preGlobalSuperStep) {
      return std::make_unique<FatalError>(conductor);
    }

    auto runGlobalSuperStep = _runGlobalSuperStep(post.activateAll).get();
    if (runGlobalSuperStep.fail()) {
      LOG_PREGEL_CONDUCTOR("f34bb", ERR) << runGlobalSuperStep.errorMessage();
      return std::make_unique<FatalError>(conductor);
    }
  } while (true);
}

auto Computing::_prepareGlobalSuperStep()
    -> futures::Future<ResultT<VPackBuilder>> {
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
      .thenValue([&](auto globalSuperStepPrepared) -> ResultT<VPackBuilder> {
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
        return globalSuperStepPrepared.get().messages;
      });
}

auto Computing::_runGlobalSuperStepCommand(bool activateAll)
    -> RunGlobalSuperStep {
  VPackBuilder toWorkerMessages;
  {
    VPackObjectBuilder ob(&toWorkerMessages);
    if (conductor._masterContext) {
      conductor._masterContext->preGlobalSuperstepMessage(toWorkerMessages);
    }
  }
  VPackBuilder aggregators;
  {
    VPackObjectBuilder ob(&aggregators);
    conductor._aggregators->serializeValues(aggregators);
  }
  return RunGlobalSuperStep{.gss = conductor._globalSuperstep,
                            .vertexCount = conductor._totalVerticesCount,
                            .edgeCount = conductor._totalEdgesCount,
                            .activateAll = activateAll,
                            .toWorkerMessages = std::move(toWorkerMessages),
                            .aggregators = std::move(aggregators)};
}

auto Computing::_runGlobalSuperStep(bool activateAll)
    -> futures::Future<Result> {
  auto runGlobalSuperStepCommand = _runGlobalSuperStepCommand(activateAll);
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

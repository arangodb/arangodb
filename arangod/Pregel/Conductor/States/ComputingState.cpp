#include "ComputingState.h"

#include "Pregel/Conductor/Conductor.h"
#include "Metrics/Gauge.h"
#include "Pregel/Conductor/States/State.h"
#include "Pregel/MasterContext.h"
#include "Pregel/PregelFeature.h"
#include "Pregel/WorkerConductorMessages.h"
#include "velocypack/Builder.h"

using namespace arangodb::pregel::conductor;

Computing::Computing(Conductor& conductor) : conductor{conductor} {
  conductor.updateState(ExecutionState::RUNNING);
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

auto Computing::_prepareGlobalSuperStep() -> GlobalSuperStepPreparedFuture {
  if (conductor._feature.isStopping()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_SHUTTING_DOWN);
  }

  conductor._aggregators->resetValues();
  conductor._statistics.resetActiveCount();
  conductor._totalVerticesCount = 0;  // might change during execution
  conductor._totalEdgesCount = 0;

  auto prepareGssCommand =
      PrepareGlobalSuperStep{.gss = conductor._globalSuperstep,
                             .vertexCount = conductor._totalVerticesCount,
                             .edgeCount = conductor._totalEdgesCount};
  auto results =
      std::vector<futures::Future<ResultT<GlobalSuperStepPrepared>>>{};
  for (auto const& [_, worker] : conductor.workers) {
    results.emplace_back(worker->prepareGlobalSuperStep(prepareGssCommand));
  }
  return futures::collectAll(results);
}

auto Computing::_runGlobalSuperStep(bool activateAll)
    -> GlobalSuperStepFinishedFuture {
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
  auto startGssCommand =
      RunGlobalSuperStep{.gss = conductor._globalSuperstep,
                         .vertexCount = conductor._totalVerticesCount,
                         .edgeCount = conductor._totalEdgesCount,
                         .activateAll = activateAll,
                         .toWorkerMessages = std::move(toWorkerMessages),
                         .aggregators = std::move(aggregators)};
  VPackBuilder startCommand;
  serialize(startCommand, startGssCommand);
  LOG_PREGEL_CONDUCTOR("d98de", DEBUG)
      << "Initiate starting GSS: " << startCommand.slice().toJson();

  conductor._timing.gss.emplace_back(Duration{
      ._start = std::chrono::steady_clock::now(), ._finish = std::nullopt});

  auto results =
      std::vector<futures::Future<ResultT<GlobalSuperStepFinished>>>{};
  for (auto const& [_, worker] : conductor.workers) {
    results.emplace_back(worker->runGlobalSuperStep(startGssCommand));
  }
  LOG_PREGEL_CONDUCTOR("411a5", DEBUG)
      << "Conductor started new gss " << conductor._globalSuperstep;
  return futures::collectAll(results);
}

auto Computing::run() -> void {
  _prepareGlobalSuperStep()
      .thenValue([&](auto results) {
        VPackBuilder messagesFromWorkers;
        for (auto const& result : results) {
          // TODO check try
          if (result.get().fail()) {
            LOG_PREGEL_CONDUCTOR("04189", ERR) << fmt::format(
                "Got unsuccessful response from worker while "
                "preparing global super step: "
                "{}\n",
                result.get().errorMessage());
            conductor.changeState(StateType::InError);
          }
          auto gssPrepared = result.get().get();
          conductor._aggregators->aggregateValues(
              gssPrepared.aggregators.slice());
          messagesFromWorkers.add(gssPrepared.messages.slice());
          conductor._statistics.accumulateActiveCounts(gssPrepared.senderId,
                                                       gssPrepared.activeCount);
          conductor._totalVerticesCount += gssPrepared.vertexCount;
          conductor._totalEdgesCount += gssPrepared.edgeCount;
        }
        auto post = conductor._postGlobalSuperStep(messagesFromWorkers);
        if (post.finished) {
          if (conductor._storeResults) {
            conductor.changeState(conductor::StateType::Storing);
            return;
          }
          if (conductor._inErrorAbort) {
            conductor.changeState(conductor::StateType::FatalError);
            return;
          }
          conductor.changeState(conductor::StateType::Done);
          return;
        }
        bool preGlobalSuperStep = conductor._preGlobalSuperStep();
        if (!preGlobalSuperStep) {
          conductor.changeState(conductor::StateType::FatalError);
          return;
        }
        _runGlobalSuperStep(post.activateAll).thenValue([&](auto results) {
          for (auto const& result : results) {
            if (result.get().fail()) {
              LOG_PREGEL_CONDUCTOR("f34bb", ERR)
                  << "Conductor could not start GSS "
                  << conductor._globalSuperstep;
              conductor.changeState(conductor::StateType::InError);
              return;
            }
            auto finished = result.get().get();
            conductor._statistics.accumulateMessageStats(
                finished.senderId, finished.messageStats.slice());
          }
          conductor._timing.gss.back().finish();
          LOG_PREGEL_CONDUCTOR("39385", DEBUG)
              << "Finished gss " << conductor._globalSuperstep << " in "
              << conductor._timing.gss.back().elapsedSeconds().count() << "s";
          conductor._globalSuperstep++;
          run();  // run next GSS
        });
      })
      .wait();
}

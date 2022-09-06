#include "ComputingState.h"

#include "Pregel/Conductor/Conductor.h"
#include "Metrics/Gauge.h"
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
      PrepareGlobalSuperStep{.executionNumber = conductor._executionNumber,
                             .gss = conductor._globalSuperstep,
                             .vertexCount = conductor._totalVerticesCount,
                             .edgeCount = conductor._totalEdgesCount};
  auto results =
      std::vector<futures::Future<ResultT<GlobalSuperStepPrepared>>>{};
  for (auto const& [_, worker] : conductor.workers) {
    results.emplace_back(worker->prepareGlobalSuperStep(prepareGssCommand));
  }
  return futures::collectAll(results);
}

auto Computing::run() -> void {
  VPackBuilder messages =
      _prepareGlobalSuperStep()
          .thenValue([&](std::vector<futures::Try<arangodb::ResultT<
                             arangodb::pregel::GlobalSuperStepPrepared>>>
                             results) {
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
              conductor._statistics.accumulateActiveCounts(
                  gssPrepared.senderId, gssPrepared.activeCount);
              conductor._totalVerticesCount += gssPrepared.vertexCount;
              conductor._totalEdgesCount += gssPrepared.edgeCount;
            }
            return messagesFromWorkers;
          })
          .get();
  conductor._startGlobalStep(messages);
}

auto Computing::receive(Message const& message) -> void {
  if (message.type() != MessageType::GssFinished) {
    LOG_PREGEL_CONDUCTOR("42e3b", WARN)
        << "When computing, we expect a GssFinished "
           "message, but we received message type "
        << static_cast<int>(message.type());
    return;
  }
  conductor._timing.gss.back().finish();
  LOG_PREGEL_CONDUCTOR("39385", DEBUG)
      << "Finished gss " << conductor._globalSuperstep << " in "
      << conductor._timing.gss.back().elapsedSeconds().count() << "s";
  conductor._globalSuperstep++;

  TRI_ASSERT(SchedulerFeature::SCHEDULER != nullptr);
  Scheduler* scheduler = SchedulerFeature::SCHEDULER;
  // don't block the response for workers waiting on this callback
  // this should allow workers to go into the IDLE state
  scheduler->queue(RequestLane::INTERNAL_LOW, [this,
                                               self = conductor
                                                          .shared_from_this()] {
    MUTEX_LOCKER(guard, conductor._callbackMutex);

    if (conductor._state == ExecutionState::RUNNING) {
      run();  // trigger next superstep
    } else {  // this prop shouldn't occur unless we are recovering or in error
      LOG_PREGEL_CONDUCTOR("923db", WARN)
          << "No further action taken after receiving all responses";
    }
  });
}

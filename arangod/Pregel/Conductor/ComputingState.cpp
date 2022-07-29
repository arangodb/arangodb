#include "ComputingState.h"

#include "Pregel/Conductor.h"
#include "Metrics/Gauge.h"
#include "Pregel/MasterContext.h"
#include "Pregel/PregelFeature.h"
#include "Pregel/WorkerConductorMessages.h"

using namespace arangodb::pregel::conductor;

Computing::Computing(Conductor& conductor) : conductor{conductor} {
  conductor.updateState(ExecutionState::RUNNING);
  // TODO choose how to handle state after Recovery
  if (!conductor._timing.computation.hasStarted()) {
    conductor._timing.computation.start();
  }
  conductor._feature.metrics()->pregelConductorsRunningNumber->fetch_add(1);
}

Computing::~Computing() {
  // TODO choose how to handle state after Recovery
  if (!conductor._timing.computation.hasFinished()) {
    conductor._timing.computation.finish();
  }
  conductor._feature.metrics()->pregelConductorsRunningNumber->fetch_sub(1);
}

auto Computing::run() -> void { conductor._startGlobalStep(); }

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
      conductor._startGlobalStep();  // trigger next superstep
    } else {  // this prop shouldn't occur unless we are recovering or in error
      LOG_PREGEL_CONDUCTOR("923db", WARN)
          << "No further action taken after receiving all responses";
    }
  });
}

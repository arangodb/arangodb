#include "DoneState.h"

#include "Pregel/AggregatorHandler.h"
#include "Pregel/Conductor.h"
#include "Pregel/WorkerConductorMessages.h"

using namespace arangodb::pregel::conductor;

Done::Done(Conductor& conductor) : conductor{conductor} {
  conductor.updateState(ExecutionState::DONE);
  if (!conductor._timing.total.hasFinished()) {
    conductor._timing.total.finish();
  }
}

Done::~Done() {}

auto Done::run() -> void {
  VPackBuilder debugOut;
  debugOut.openObject();
  debugOut.add("stats", VPackValue(VPackValueType::Object));
  conductor._statistics.serializeValues(debugOut);
  debugOut.close();
  conductor._aggregators->serializeValues(debugOut);
  debugOut.close();

  LOG_PREGEL_CONDUCTOR("063b5", INFO)
      << "Done. We did " << conductor._globalSuperstep << " rounds."
      << (conductor._timing.loading.hasStarted()
              ? fmt::format("Startup time: {}s",
                            conductor._timing.loading.elapsedSeconds().count())
              : "")
      << (conductor._timing.computation.hasStarted()
              ? fmt::format(
                    ", computation time: {}s",
                    conductor._timing.computation.elapsedSeconds().count())
              : "")
      << (conductor._storeResults
              ? fmt::format(", storage time: {}s",
                            conductor._timing.storing.elapsedSeconds().count())
              : "")
      << ", overall: " << conductor._timing.total.elapsedSeconds().count()
      << "s"
      << ", stats: " << debugOut.slice().toJson();
}

auto Done::receive(Message const& message) -> void {
  LOG_PREGEL_CONDUCTOR("88f66", WARN)
      << "When done, we expect no messages, but received message type "
      << static_cast<int>(message.type());
}

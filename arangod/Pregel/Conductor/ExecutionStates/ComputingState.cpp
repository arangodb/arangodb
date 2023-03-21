#include "ComputingState.h"

using namespace arangodb::pregel::conductor;

Computing::Computing(ConductorState& conductor,
                     std::unique_ptr<MasterContext> masterContext)
    : conductor{conductor}, masterContext{std::move(masterContext)} {
  // TODO somewhere call masterContext->preApplication();
  conductor.timing.computation.start();
  // TODO GORDO-1510
  // _feature.metrics()->pregelConductorsRunningNumber->fetch_add(1);
  // TODO start gss timing
  // TODO conductor.mastercontext->preGlobalSuperstep()
}

Computing::~Computing() {}

auto Computing::messages()
    -> std::unordered_map<actor::ActorPID, worker::message::WorkerMessages> {
  VPackBuilder aggregators;
  {
    VPackObjectBuilder ob(&aggregators);
    masterContext->_aggregators->serializeValues(aggregators);
  }
  auto out =
      std::unordered_map<actor::ActorPID, worker::message::WorkerMessages>();
  for (auto const& worker : conductor.workers) {
    out.emplace(worker,
                worker::message::RunGlobalSuperStep{
                    .gss = masterContext->_globalSuperstep,
                    .vertexCount = masterContext->_vertexCount,
                    .edgeCount = masterContext->_edgeCount,
                    .sendCount = sendCountPerServer.contains(worker.server)
                                     ? sendCountPerServer.at(worker.server)
                                     : 0,
                    .aggregators = aggregators});
  }
  return out;
}
auto Computing::receive(actor::ActorPID sender,
                        message::ConductorMessages message)
    -> std::optional<std::unique_ptr<ExecutionState>> {
  return std::nullopt;
};

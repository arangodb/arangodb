#include "ComputingState.h"
#include "Pregel/Conductor/ExecutionStates/FatalErrorState.h"
#include "Pregel/Conductor/ExecutionStates/ProduceAQLResultsState.h"
#include "Pregel/Conductor/ExecutionStates/StoringState.h"
#include "velocypack/Iterator.h"

using namespace arangodb::pregel::conductor;

Computing::Computing(
    ConductorState& conductor, std::unique_ptr<MasterContext> masterContext,
    std::unordered_map<actor::ActorPID, uint64_t> sendCountPerActor)
    : conductor{conductor},
      masterContext{std::move(masterContext)},
      sendCountPerActor{std::move(sendCountPerActor)} {
  // TODO GORDO-1510
  // _feature.metrics()->pregelConductorsRunningNumber->fetch_add(1);
  conductor.timing.gss.emplace_back(Duration{
      ._start = std::chrono::steady_clock::now(), ._finish = std::nullopt});
}

Computing::~Computing() {}

auto Computing::messages()
    -> std::unordered_map<actor::ActorPID, worker::message::WorkerMessages> {
  if (masterContext->_globalSuperstep == 0) {
    conductor.timing.computation.start();
    masterContext->preApplication();
  }
  masterContext->preGlobalSuperstep();

  VPackBuilder aggregators;
  {
    VPackObjectBuilder ob(&aggregators);
    masterContext->_aggregators->serializeValues(aggregators);
  }
  auto out =
      std::unordered_map<actor::ActorPID, worker::message::WorkerMessages>();
  for (auto const& worker : conductor.workers) {
    out.emplace(worker, worker::message::RunGlobalSuperStep{
                            .gss = masterContext->_globalSuperstep,
                            .vertexCount = masterContext->_vertexCount,
                            .edgeCount = masterContext->_edgeCount,
                            .sendCount = sendCountPerActor.contains(worker)
                                             ? sendCountPerActor.at(worker)
                                             : 0,
                            .aggregators = aggregators});
  }
  return out;
}
auto Computing::receive(actor::ActorPID sender,
                        message::ConductorMessages message)
    -> std::optional<std::unique_ptr<ExecutionState>> {
  if (not conductor.workers.contains(sender) or
      not std::holds_alternative<ResultT<message::GlobalSuperStepFinished>>(
          message)) {
    return std::make_unique<FatalError>(conductor);
  }
  auto gssFinished =
      std::get<ResultT<message::GlobalSuperStepFinished>>(message);
  if (not gssFinished.ok()) {
    return std::make_unique<FatalError>(conductor);
  }
  respondedWorkers.emplace(sender);
  messageAccumulation.add(gssFinished.get());

  if (respondedWorkers == conductor.workers) {
    masterContext->_vertexCount = messageAccumulation.vertexCount;
    masterContext->_edgeCount = messageAccumulation.edgeCount;
    masterContext->_aggregators->resetValues();
    for (auto aggregator :
         VPackArrayIterator(messageAccumulation.aggregators.slice())) {
      masterContext->_aggregators->aggregateValues(aggregator);
    }

    auto postGss = _postGlobalSuperStep();

    if (postGss.finished) {
      conductor.timing.gss.back().finish();
      masterContext->postApplication();
      conductor.timing.computation.finish();
      // TODO GORDO-1510
      // conductor._feature.metrics()->pregelConductorsRunningNumber->fetch_sub(1);
      if (conductor.specifications.storeResults) {
        return std::make_unique<Storing>(conductor);
      }
      return std::make_unique<ProduceAQLResults>(conductor);
    }

    conductor.timing.gss.back().finish();
    masterContext->_globalSuperstep++;
    return std::make_unique<Computing>(
        conductor, std::move(masterContext),
        std::move(messageAccumulation.sendCountPerActor));
  }

  return std::nullopt;
};

auto Computing::_postGlobalSuperStep() -> PostGlobalSuperStepResult {
  // workers are done if all messages were processed and no active vertices
  // are left to process
  bool done = messageAccumulation.activeCount == 0 &&
              messageAccumulation.messageStats.allMessagesProcessed();
  auto proceed = masterContext->postGlobalSuperstep();
  return PostGlobalSuperStepResult{
      .finished = !proceed || done ||
                  masterContext->_globalSuperstep >=
                      conductor.specifications.maxSuperstep};
}

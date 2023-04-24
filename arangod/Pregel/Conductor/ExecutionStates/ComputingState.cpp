#include "ComputingState.h"

#include "Pregel/Conductor/ExecutionStates/FatalErrorState.h"
#include "Pregel/Conductor/ExecutionStates/ProduceAQLResultsState.h"
#include "Pregel/Conductor/ExecutionStates/State.h"
#include "Pregel/Conductor/ExecutionStates/StoringState.h"
#include "Pregel/Conductor/State.h"
#include "Pregel/MasterContext.h"
#include "CanceledState.h"

using namespace arangodb::pregel::conductor;

Computing::Computing(
    ConductorState& conductor, std::unique_ptr<MasterContext> masterContext,
    std::unordered_map<actor::ActorPID, uint64_t> sendCountPerActor)
    : conductor{conductor},
      masterContext{std::move(masterContext)},
      sendCountPerActor{std::move(sendCountPerActor)} {}

auto Computing::messages()
    -> std::unordered_map<actor::ActorPID, worker::message::WorkerMessages> {
  if (masterContext->_globalSuperstep == 0) {
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
    -> std::optional<StateChange> {
  if (std::holds_alternative<message::Cancel>(message)) {
    auto newState = std::make_unique<Canceled>(conductor);
    auto stateName = newState->name();

    return StateChange{
        .statusMessage = pregel::message::Canceled{.state = stateName},
        .metricsMessage =
            pregel::metrics::message::ConductorFinished{
                .previousState = pregel::metrics::message::PreviousState::COMPUTING},
        .newState = std::move(newState)};
  }

  if (not conductor.workers.contains(sender) or
      not std::holds_alternative<ResultT<message::GlobalSuperStepFinished>>(
          message)) {
    auto newState = std::make_unique<FatalError>(conductor);
    auto stateName = newState->name();
    return StateChange{
        .statusMessage = pregel::message::InFatalError{.state = stateName},
        .metricsMessage =
            pregel::metrics::message::ConductorFinished{
                .previousState = pregel::metrics::message::PreviousState::COMPUTING},
        .newState = std::move(newState)};
  }
  auto gssFinished =
      std::get<ResultT<message::GlobalSuperStepFinished>>(message);
  if (not gssFinished.ok()) {
    auto newState = std::make_unique<FatalError>(conductor);
    auto stateName = newState->name();
    return StateChange{
        .statusMessage = pregel::message::InFatalError{.state = stateName},
        .metricsMessage =
            pregel::metrics::message::ConductorFinished{
                .previousState = pregel::metrics::message::PreviousState::COMPUTING},
        .newState = std::move(newState)};
  }
  respondedWorkers.emplace(sender);
  messageAccumulation.add(gssFinished.get());
  for (auto const& count : gssFinished.get().sendCountPerActor) {
    sendCountPerActorForNextGss[count.receiver] += count.sendCount;
  }

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
      masterContext->postApplication();
      if (conductor.specifications.storeResults) {
        auto newState = std::make_unique<Storing>(conductor);
        auto stateName = newState->name();
        return StateChange{
            .statusMessage =
                pregel::message::StoringStarted{.state = stateName},
            .metricsMessage =
                pregel::metrics::message::ConductorStoringStarted{},
            .newState = std::move(newState)};
      }

      auto newState = std::make_unique<ProduceAQLResults>(conductor);
      auto stateName = newState->name();
      return StateChange{
          .statusMessage = pregel::message::StoringStarted{.state = stateName},
          .metricsMessage = pregel::metrics::message::ConductorStoringStarted{},
          .newState = std::move(newState)};
    }

    masterContext->_globalSuperstep++;
    auto gss = masterContext->_globalSuperstep;
    VPackBuilder aggregators;
    aggregators.openObject();
    masterContext->_aggregators->serializeValues(aggregators);
    aggregators.close();
    auto vertexCount = masterContext->vertexCount();
    auto edgeCount = masterContext->edgeCount();
    auto newState =
        std::make_unique<Computing>(conductor, std::move(masterContext),
                                    std::move(sendCountPerActorForNextGss));
    auto stateName = newState->name();
    return StateChange{.statusMessage =
                           pregel::message::GlobalSuperStepStarted{
                               .gss = gss,
                               .vertexCount = vertexCount,
                               .edgeCount = edgeCount,
                               .aggregators = std::move(aggregators),
                               .state = stateName},
                       .metricsMessage = std::nullopt,
                       .newState = std::move(newState)};
  }

  return std::nullopt;
}

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

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
    std::unordered_map<actor::ActorPID, uint64_t> sendCountPerActor,
    uint64_t totalSendMessagesCount, uint64_t totalReceivedMessagesCount)
    : conductor{conductor},
      masterContext{std::move(masterContext)},
      sendCountPerActor{std::move(sendCountPerActor)},
      totalSendMessagesCount{totalSendMessagesCount},
      totalReceivedMessagesCount{totalReceivedMessagesCount} {}

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
auto Computing::cancel(arangodb::pregel::actor::ActorPID sender,
                       message::ConductorMessages message)
    -> std::optional<StateChange> {
  auto newState = std::make_unique<Canceled>(conductor);
  auto stateName = newState->name();

  return StateChange{
      .statusMessage = pregel::message::Canceled{.state = stateName},
      .metricsMessage =
          pregel::metrics::message::ConductorFinished{
              .previousState =
                  pregel::metrics::message::PreviousState::COMPUTING},
      .newState = std::move(newState)};
}
auto Computing::receive(actor::ActorPID sender,
                        message::ConductorMessages message)
    -> std::optional<StateChange> {
  if (not conductor.workers.contains(sender) or
      not std::holds_alternative<ResultT<message::GlobalSuperStepFinished>>(
          message)) {
    auto newState = std::make_unique<FatalError>(conductor);
    auto stateName = newState->name();
    return StateChange{
        .statusMessage =
            pregel::message::InFatalError{
                .state = stateName,
                .errorMessage =
                    fmt::format("In {}: Received unexpected message {} from {}",
                                name(), inspection::json(message), sender)},
        .metricsMessage =
            pregel::metrics::message::ConductorFinished{
                .previousState =
                    pregel::metrics::message::PreviousState::COMPUTING},
        .newState = std::move(newState)};
  }
  auto gssFinished =
      std::get<ResultT<message::GlobalSuperStepFinished>>(message);
  if (not gssFinished.ok()) {
    auto newState = std::make_unique<FatalError>(conductor);
    auto stateName = newState->name();
    return StateChange{
        .statusMessage =
            pregel::message::InFatalError{
                .state = stateName,
                .errorMessage = fmt::format(
                    "In {}: Received error {} from {}", name(),
                    inspection::json(gssFinished.errorMessage()), sender)},
        .metricsMessage =
            pregel::metrics::message::ConductorFinished{
                .previousState =
                    pregel::metrics::message::PreviousState::COMPUTING},
        .newState = std::move(newState)};
  }
  LOG_TOPIC("543aa", INFO, Logger::PREGEL) << fmt::format(
      "Conductor Actor: Global super step {} finished on worker {}",
      masterContext->_globalSuperstep, sender);
  respondedWorkers.emplace(sender);
  _aggregateMessage(std::move(gssFinished.get()));

  if (respondedWorkers == conductor.workers) {
    masterContext->_vertexCount = vertexCount;
    masterContext->_edgeCount = edgeCount;
    masterContext->_aggregators->resetValues();
    for (auto aggregator : VPackArrayIterator(aggregators.slice())) {
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
    auto newState = std::make_unique<Computing>(
        conductor, std::move(masterContext),
        std::move(sendCountPerActorForNextGss), totalSendMessagesCount,
        totalReceivedMessagesCount);
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

auto Computing::_aggregateMessage(message::GlobalSuperStepFinished msg)
    -> void {
  totalSendMessagesCount += msg.sendMessagesCount;
  totalReceivedMessagesCount += msg.receivedMessagesCount;
  activeCount += msg.activeCount;
  vertexCount += msg.vertexCount;
  edgeCount += msg.edgeCount;
  // TODO directly aggregate in here when aggregators have an inspector
  VPackBuilder newAggregators;
  {
    VPackArrayBuilder ab(&newAggregators);
    if (!aggregators.isEmpty()) {
      newAggregators.add(VPackArrayIterator(aggregators.slice()));
    }
    newAggregators.add(msg.aggregators.slice());
  }
  aggregators = newAggregators;
  for (auto const& count : msg.sendCountPerActor) {
    sendCountPerActorForNextGss[count.receiver] += count.sendCount;
  }
}

auto Computing::_postGlobalSuperStep() -> PostGlobalSuperStepResult {
  // workers are done if all messages were processed and no active vertices
  // are left to process
  bool done =
      activeCount == 0 && totalSendMessagesCount == totalReceivedMessagesCount;
  auto proceed = masterContext->postGlobalSuperstep();
  return PostGlobalSuperStepResult{
      .finished = !proceed || done ||
                  masterContext->_globalSuperstep >=
                      conductor.specifications.maxSuperstep};
}

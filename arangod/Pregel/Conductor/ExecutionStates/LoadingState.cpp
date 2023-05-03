#include "LoadingState.h"

#include "Pregel/AggregatorHandler.h"
#include "Pregel/Conductor/ExecutionStates/ComputingState.h"
#include "Pregel/Conductor/ExecutionStates/FatalErrorState.h"
#include "Pregel/Conductor/State.h"
#include "Pregel/MasterContext.h"
#include "CanceledState.h"

using namespace arangodb::pregel::conductor;

Loading::Loading(ConductorState& conductor,
                 std::unordered_map<ShardID, actor::ActorPID> actorForShard)
    : conductor{conductor}, actorForShard{std::move(actorForShard)} {}

auto Loading::messages()
    -> std::unordered_map<actor::ActorPID, worker::message::WorkerMessages> {
  auto messages =
      std::unordered_map<actor::ActorPID, worker::message::WorkerMessages>{};
  for (auto const& worker : conductor.workers) {
    messages.emplace(worker, worker::message::LoadGraph{
                                 .responsibleActorPerShard = actorForShard});
  }
  return messages;
}

auto Loading::cancel(arangodb::pregel::actor::ActorPID sender,
                     message::ConductorMessages message)
    -> std::optional<StateChange> {
  auto newState = std::make_unique<Canceled>(conductor);
  auto stateName = newState->name();

  return StateChange{
      .statusMessage = pregel::message::Canceled{.state = stateName},
      .metricsMessage =
          pregel::metrics::message::ConductorFinished{
              .previousState =
                  pregel::metrics::message::PreviousState::LOADING},
      .newState = std::move(newState)};
}

auto Loading::receive(actor::ActorPID sender,
                      message::ConductorMessages message)
    -> std::optional<StateChange> {
  if (not conductor.workers.contains(sender) or
      not std::holds_alternative<ResultT<message::GraphLoaded>>(message)) {
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
                    pregel::metrics::message::PreviousState::LOADING},
        .newState = std::move(newState)};
  }
  auto graphLoaded = std::get<ResultT<message::GraphLoaded>>(message);
  if (not graphLoaded.ok()) {
    auto newState = std::make_unique<FatalError>(conductor);
    auto stateName = newState->name();
    return StateChange{
        .statusMessage =
            pregel::message::InFatalError{
                .state = stateName,
                .errorMessage = fmt::format(
                    "In {}: Received error {} from {}", name(),
                    inspection::json(graphLoaded.errorMessage()), sender)},
        .metricsMessage =
            pregel::metrics::message::ConductorFinished{
                .previousState =
                    pregel::metrics::message::PreviousState::LOADING},
        .newState = std::move(newState)};
  }
  respondedWorkers.emplace(sender);
  totalVerticesCount += graphLoaded.get().vertexCount;
  totalEdgesCount += graphLoaded.get().edgeCount;

  if (respondedWorkers == conductor.workers) {
    auto masterContext = conductor.algorithm->masterContextUnique(
        totalVerticesCount, totalEdgesCount,
        std::make_unique<AggregatorHandler>(conductor.algorithm.get()),
        conductor.specifications.userParameters.slice());
    auto newState = std::make_unique<Computing>(
        conductor, std::move(masterContext),
        std::unordered_map<actor::ActorPID, uint64_t>{}, 0, 0);
    auto stateName = newState->name();
    return StateChange{
        .statusMessage =
            pregel::message::ComputationStarted{.state = stateName},
        .metricsMessage = pregel::metrics::message::ConductorComputingStarted{},
        .newState = std::move(newState)};
  }

  return std::nullopt;
}

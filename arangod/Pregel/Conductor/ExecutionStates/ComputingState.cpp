#include "ComputingState.h"

using namespace arangodb::pregel::conductor;

Computing::Computing(ConductorState& conductor,
                     std::unique_ptr<MasterContext> masterContext)
    : conductor{conductor}, masterContext{std::move(masterContext)} {
  // TODO somewhere call masterContext->preApplication();
}

Computing::~Computing() {}

auto Computing::message() -> worker::message::WorkerMessages {
  return worker::message::WorkerMessages{};
};
auto Computing::receive(actor::ActorPID sender,
                        message::ConductorMessages message)
    -> std::optional<std::unique_ptr<ExecutionState>> {
  return std::nullopt;
};

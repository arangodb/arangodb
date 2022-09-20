#include "InitialState.h"
#include "Pregel/Conductor/Conductor.h"

using namespace arangodb::pregel::conductor;

Initial::Initial(Conductor& conductor) : conductor{conductor} {}

auto Initial::run() -> std::optional<std::unique_ptr<State>> {
  return std::make_unique<Loading>(conductor);
}

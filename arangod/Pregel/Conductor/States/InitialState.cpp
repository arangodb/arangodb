#include "InitialState.h"
#include "Pregel/Conductor/Conductor.h"
#include "Pregel/WorkerConductorMessages.h"

using namespace arangodb::pregel::conductor;

Initial::Initial(Conductor& conductor) : conductor{conductor} {}

auto Initial::run() -> void { conductor.changeState(StateType::Loading); }

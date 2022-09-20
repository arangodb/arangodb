#include "InErrorState.h"

#include <chrono>

#include "Pregel/Conductor/Conductor.h"
#include "Pregel/WorkerConductorMessages.h"

using namespace arangodb::pregel::conductor;

InError::InError(Conductor& conductor, std::chrono::seconds const& ttl)
    : conductor{conductor} {
  expiration = std::chrono::system_clock::now() + ttl;
}

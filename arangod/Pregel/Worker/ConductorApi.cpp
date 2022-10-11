#include "ConductorApi.h"

using namespace arangodb::pregel::worker;

auto ConductorApi::graphLoaded(ResultT<GraphLoaded> const& data) const
    -> Result {
  return _connection
      ->post(
          Destination{Destination::Type::server, _server},
          ModernMessage{.executionNumber = _executionNumber, .payload = data})
      .get();
}

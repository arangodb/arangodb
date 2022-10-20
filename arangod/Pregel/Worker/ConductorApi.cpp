#include "ConductorApi.h"

using namespace arangodb::pregel::worker;

auto ConductorApi::send(MessagePayload data) const -> Result {
  return _connection
      ->post(
          Destination{Destination::Type::server, _server},
          ModernMessage{.executionNumber = _executionNumber, .payload = data})
      .get();
}

#include "ConductorApi.h"

using namespace arangodb::pregel::worker;

auto ConductorApi::send(MessagePayload data) const -> Result {
  auto response = _connection
                      ->post(Destination{Destination::Type::server, _server},
                             ModernMessage{.executionNumber = _executionNumber,
                                           .payload = data})
                      .get();
  if (response.fail()) {
    return Result{response.errorNumber(),
                  fmt::format("Got unsuccessful response from Conductor after "
                              "sending message {}: {}",
                              velocypack::serialize(data).toJson(),
                              response.errorMessage())};
  }
  return Result{};
}

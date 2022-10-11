#include "DirectConnection.h"

#include "Pregel/PregelFeature.h"

using namespace arangodb::pregel;

auto DirectConnection::send(Destination const& destination,
                            ModernMessage&& message) const
    -> futures::Future<ResultT<ModernMessage>> {
  return _feature.process(message, _vocbaseGuard.database());
}

auto DirectConnection::post(Destination const& destination,
                            ModernMessage&& message) const
    -> futures::Future<Result> {
  auto response = _feature.process(message, _vocbaseGuard.database());
  if (response.fail()) {
    return Result{response.errorNumber(), response.errorMessage()};
  }
  return Result{};
}

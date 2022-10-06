#include "DirectConnectionToConductor.h"
#include "Pregel/PregelFeature.h"
#include "Pregel/Messaging/Message.h"
#include "Pregel/Conductor/Conductor.h"

using namespace arangodb::pregel;

auto DirectConnectionToConductor::send(Destination const& destination,
                                       ModernMessage&& message) const
    -> futures::Future<ResultT<ModernMessage>> {
  return Result{};
}

auto DirectConnectionToConductor::post(Destination const& destination,
                                       ModernMessage&& message) const
    -> futures::Future<Result> {
  // TODO add same cases a in send fct when post is used for more messages
  auto conductor = _feature.conductor(message.executionNumber);
  if (!conductor) {
    VPackBuilder serialized;
    serialize(serialized, message);
    return Result{TRI_ERROR_CURSOR_NOT_FOUND,
                  fmt::format("Handling direct request {} but conductor for "
                              "execution {} does not exist",
                              serialized.toJson(), message.executionNumber)};
  }
  return conductor->process(message.payload);
}

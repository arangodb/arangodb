#include "DirectConnection.h"
#include "Basics/Exceptions.h"
#include "Basics/voc-errors.h"
#include "Pregel/AlgoRegistry.h"
#include "Pregel/PregelFeature.h"
#include "Pregel/WorkerConductorMessages.h"

using namespace arangodb::pregel;

auto DirectConnection::send(Destination const& destination,
                            ModernMessage&& message)
    -> futures::Future<ResultT<ModernMessage>> {
  if (_feature.isStopping()) {
    return Result{TRI_ERROR_SHUTTING_DOWN};
  }

  if (std::holds_alternative<LoadGraph>(message.payload)) {
    try {
      auto created = AlgoRegistry::createWorker(
          _vocbaseGuard.database(),
          std::get<LoadGraph>(message.payload).details.slice(), _feature);
      TRI_ASSERT(created.get() != nullptr);
      _feature.addWorker(std::move(created), message.executionNumber);
    } catch (basics::Exception& e) {
      return Result{e.code(), e.message()};
    }
  }
  auto worker = _feature.worker(message.executionNumber);
  // TODO check for start cleanup
  if (!worker) {
    VPackBuilder serialized;
    serialize(serialized, message);
    return Result{TRI_ERROR_CURSOR_NOT_FOUND,
                  fmt::format("Handling direct request {} but worker for "
                              "execution {} does not exist",
                              serialized.toJson(), message.executionNumber)};
  }
  return worker->process(message.payload)
      .thenValue([&](auto response) -> futures::Future<ResultT<ModernMessage>> {
        if (response.fail()) {
          VPackBuilder serialized;
          serialize(serialized, message);
          return Result{
              response.errorNumber(),
              fmt::format(
                  "Processing direct request failed: Execution {}: {}: {}",
                  message.executionNumber, response.errorMessage(),
                  serialized.toJson())};
        }
        return response;
      });
}

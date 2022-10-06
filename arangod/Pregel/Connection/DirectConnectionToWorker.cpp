#include "DirectConnectionToWorker.h"
#include "Basics/Exceptions.h"
#include "Basics/voc-errors.h"
#include "Pregel/AlgoRegistry.h"
#include "Pregel/PregelFeature.h"
#include "Pregel/Messaging/Message.h"

using namespace arangodb::pregel;

auto DirectConnectionToWorker::send(Destination const& destination,
                                    ModernMessage&& message) const
    -> futures::Future<ResultT<ModernMessage>> {
  if (std::holds_alternative<CreateWorker>(message.payload)) {
    try {
      auto created = AlgoRegistry::createWorker(
          _vocbaseGuard.database(), std::get<CreateWorker>(message.payload),
          _feature);
      TRI_ASSERT(created.get() != nullptr);
      _feature.addWorker(std::move(created), message.executionNumber);
      return ModernMessage{
          .executionNumber = message.executionNumber,
          .payload = WorkerCreated{.senderId = destination._id}};
    } catch (basics::Exception& e) {
      return Result{e.code(), e.message()};
    }
  }
  auto worker = _feature.worker(message.executionNumber);
  if (std::holds_alternative<Cleanup>(message.payload)) {
    if (!worker || _feature.isStopping()) {
      // either cleanup has already happended because of garbage collection
      // or cleanup is unnecessary because shutdown already started
      return ModernMessage{.executionNumber = message.executionNumber,
                           .payload = CleanupFinished{}};
    }
  }
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

auto DirectConnectionToWorker::post(Destination const& destination,
                                    ModernMessage&& message) const
    -> futures::Future<Result> {
  // TODO add same cases a in send fct when post is used for more messages
  auto worker = _feature.worker(message.executionNumber);
  if (!worker) {
    VPackBuilder serialized;
    serialize(serialized, message);
    return Result{TRI_ERROR_CURSOR_NOT_FOUND,
                  fmt::format("Handling direct request {} but worker for "
                              "execution {} does not exist",
                              serialized.toJson(), message.executionNumber)};
  }
  // TODO change process function to directly return a Result
  //      and move actual work to scheduler
  return worker->process(message.payload)
      .thenValue(
          [&](auto response) -> futures::Future<Result> { return Result{}; });
}

#include "NetworkConnection.h"
#include <cstdint>

#include "Logger/LogMacros.h"
#include "Network/NetworkFeature.h"
#include "Network/Methods.h"
#include "Pregel/WorkerConductorMessages.h"
#include "VocBase/voc-types.h"
#include "VocBase/vocbase.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "velocypack/Builder.h"
#include "velocypack/Slice.h"

using namespace arangodb::pregel;

NetworkConnection::NetworkConnection(std::string baseUrl,
                                     network::RequestOptions requestOptions,
                                     TRI_vocbase_t& vocbase)
    : _baseUrl{std::move(baseUrl)},
      _requestOptions{std::move(requestOptions)},
      _connectionPool{vocbase.server().getFeature<NetworkFeature>().pool()} {}

namespace {
auto serialize(ModernMessage const& message)
    -> arangodb::ResultT<VPackBuffer<uint8_t>> {
  VPackBuffer<uint8_t> messageBuffer;
  VPackBuilder serializedMessage(messageBuffer);
  try {
    serialize(serializedMessage, message);
  } catch (arangodb::basics::Exception& e) {
    return {arangodb::Result{TRI_ERROR_INTERNAL,
                             fmt::format("REST message cannot be serialized")}};
  }
  return messageBuffer;
}

auto errorHandling(arangodb::network::Response const& message)
    -> arangodb::ResultT<VPackSlice> {
  if (message.fail()) {
    return {arangodb::Result{
        TRI_ERROR_INTERNAL,
        fmt::format("REST request to worker failed: {}",
                    arangodb::fuerte::to_string(message.error))}};
  }
  if (message.statusCode() >= 400) {
    return {arangodb::Result{
        TRI_ERROR_FAILED,
        fmt::format("REST request to worker returned an error code {}: {}",
                    message.statusCode(), message.slice().toJson())}};
  }
  return message.slice();
}

auto deserialize(VPackSlice slice) -> arangodb::ResultT<ModernMessage> {
  try {
    return deserialize<ModernMessage>(slice);
  } catch (arangodb::basics::Exception& e) {
    return {
        arangodb::Result{TRI_ERROR_INTERNAL,
                         fmt::format("REST response cannot be deserialized: {}",
                                     slice.toJson())}};
  }
}

}  // namespace

auto Destination::toString() const -> std::string {
  return fmt::format("{}:{}", typeString[_type], _id);
}

auto NetworkConnection::send(Destination const& destination,
                             ModernMessage&& message)
    -> futures::Future<ResultT<ModernMessage>> {
  auto messageBuffer = serialize(message);
  if (messageBuffer.fail()) {
    return {arangodb::Result{messageBuffer.errorNumber(),
                             messageBuffer.errorMessage()}};
  }
  auto request = network::sendRequestRetry(
      _connectionPool, destination.toString(), fuerte::RestVerb::Post,
      _baseUrl + Utils::modernMessagingPath, std::move(messageBuffer.get()),
      _requestOptions);

  return std::move(request).thenValue(
      [](auto&& result) -> ResultT<ModernMessage> {
        auto slice = errorHandling(result);
        if (slice.fail()) {
          return Result{slice.errorNumber(), slice.errorMessage()};
        }
        return deserialize(slice.get());
      });
}

auto NetworkConnection::sendWithoutRetry(Destination const& destination,
                                         ModernMessage&& message)
    -> futures::Future<Result> {
  auto messageBuffer = serialize(message);
  if (messageBuffer.fail()) {
    return {arangodb::Result{messageBuffer.errorNumber(),
                             messageBuffer.errorMessage()}};
  }
  auto request = network::sendRequest(
      _connectionPool, destination.toString(), fuerte::RestVerb::Post,
      _baseUrl + Utils::modernMessagingPath, std::move(messageBuffer.get()),
      _requestOptions);

  return std::move(request).thenValue([](auto&& result) -> Result {
    auto slice = errorHandling(result);
    if (slice.fail()) {
      return Result{slice.errorNumber(), slice.errorMessage()};
    }
    return Result{};
  });
}

#include "NetworkConnection.h"

#include "Logger/LogMacros.h"
#include "Network/NetworkFeature.h"
#include "Network/Methods.h"
#include "Pregel/WorkerConductorMessages.h"
#include "VocBase/voc-types.h"
#include "VocBase/vocbase.h"
#include "ApplicationFeatures/ApplicationServer.h"

using namespace arangodb::pregel;

Connection::Connection(ServerID destinationId, std::string baseUrl,
                       network::RequestOptions requestOptions,
                       network::ConnectionPool* connectionPool)
    : _destinationId{std::move(destinationId)},
      _baseUrl{std::move(baseUrl)},
      _requestOptions{std::move(requestOptions)},
      _connectionPool{connectionPool} {}
auto Connection::create(ServerID const& destinationId,
                        std::string const& baseUrl, TRI_vocbase_t& vocbase)
    -> Connection {
  network::RequestOptions reqOpts;
  reqOpts.timeout = network::Timeout(5.0 * 60.0);
  reqOpts.database = vocbase.name();
  auto const& nf = vocbase.server().getFeature<NetworkFeature>();
  return Connection{destinationId, baseUrl, reqOpts, nf.pool()};
}

auto Connection::post(ModernMessage&& message)
    -> futures::Future<ResultT<ModernMessage>> {
  VPackBuffer<uint8_t> messageBuffer;
  VPackBuilder serializedMessage(messageBuffer);
  try {
    serialize(serializedMessage, message);
  } catch (basics::Exception& e) {
    return {Result{TRI_ERROR_INTERNAL,
                   fmt::format("REST message cannot be serialized")}};
  }
  auto request = network::sendRequestRetry(
      _connectionPool, "server:" + _destinationId, fuerte::RestVerb::Post,
      _baseUrl + Utils::modernMessagingPath, std::move(messageBuffer),
      _requestOptions);

  return std::move(request).thenValue(
      [](auto&& result) -> futures::Future<ResultT<ModernMessage>> {
        if (result.fail()) {
          return {Result{TRI_ERROR_INTERNAL,
                         fmt::format("REST request to worker failed: {}",
                                     fuerte::to_string(result.error))}};
        }
        if (result.statusCode() >= 400) {
          return {
              Result{TRI_ERROR_FAILED,
                     fmt::format(
                         "REST request to worker returned an error code {}: {}",
                         result.statusCode(), result.slice().toJson())}};
        }
        try {
          return deserialize<ModernMessage>(result.slice());
        } catch (basics::Exception& e) {
          return {Result{TRI_ERROR_INTERNAL,
                         fmt::format("REST response cannot be deserialized: {}",
                                     result.slice().toJson())}};
        }
      });
}

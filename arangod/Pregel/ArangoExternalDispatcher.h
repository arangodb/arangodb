#pragma once

#include "Actor/ActorPID.h"
#include "Actor/Message.h"
#include "CrashHandler/CrashHandler.h"
#include "Assertions/ProdAssert.h"
#include "Inspection/VPackWithErrorT.h"
#include "Logger/LogMacros.h"
#include "Network/ConnectionPool.h"
#include "Network/Methods.h"

namespace arangodb::pregel {

struct NetworkMessage {
  actor::ActorPID sender;
  actor::ActorPID receiver;
  VPackBuilder payload;
};
template<typename Inspector>
auto inspect(Inspector& f, NetworkMessage& x) {
  return f.object(x).fields(f.field("sender", x.sender),
                            f.field("receiver", x.receiver),
                            f.field("payload", x.payload));
}

struct ArangoExternalDispatcher {
  static auto errorHandling(arangodb::network::Response const& message)
      -> arangodb::ResultT<VPackSlice> {
    if (message.fail()) {
      return {arangodb::Result{
          TRI_ERROR_INTERNAL,
          fmt::format("REST request failed: {}",
                      arangodb::fuerte::to_string(message.error))}};
    }
    if (message.statusCode() >= 400) {
      return {arangodb::Result{
          TRI_ERROR_FAILED,
          fmt::format("REST request returned an error code {}: {}",
                      message.statusCode(), message.slice().toJson())}};
    }
    return message.slice();
  }

  ArangoExternalDispatcher(std::string url,
                           network::ConnectionPool* connectionPool)
      : connectionPool(connectionPool), baseURL{std::move(url)} {}

  auto send(actor::ActorPID sender, actor::ActorPID receiver,
            arangodb::velocypack::SharedSlice msg) -> network::FutureRes {
    auto networkMessage =
        NetworkMessage{.sender = sender,
                       .receiver = receiver,
                       .payload = velocypack::Builder(msg.slice())};
    auto serialized = inspection::serializeWithErrorT(networkMessage);
    ADB_PROD_ASSERT(serialized.ok());
    auto builder = velocypack::Builder(serialized.get().slice());
    return network::sendRequestRetry(
        this->connectionPool,
        // TODO: what about "shard:"?
        std::string{"server:"} + receiver.server, fuerte::RestVerb::Post,
        baseURL, builder.bufferRef(),
        network::RequestOptions{.database = receiver.database,
                                .timeout = timeout});
  }

  auto operator()(actor::ActorPID sender, actor::ActorPID receiver,
                  arangodb::velocypack::SharedSlice msg) -> void {
    send(sender, receiver, msg)
        .thenValue([sender, receiver, this](auto&& result) -> void {
          auto out = errorHandling(result);
          if (out.fail()) {
            auto error = actor::ActorError{actor::NetworkError{
                .message = (std::string)out.errorMessage()}};
            auto payload = inspection::serializeWithErrorT(error);
            ADB_PROD_ASSERT(payload.ok());
            send(receiver, sender, payload.get())
                .thenValue([](auto&& result) -> void {
                  // if sending the error does also not work, we just print the
                  // error
                  auto out = errorHandling(result);
                  if (out.fail()) {
                    LOG_TOPIC("ae1e9", INFO, Logger::PREGEL)
                        << fmt::format("Error in network communication: {}",
                                       out.errorMessage());
                  }
                });
          }
        });
  }

  network::ConnectionPool* connectionPool;
  std::string baseURL;
  network::Timeout timeout{5.0 * 60};
};

}  // namespace arangodb::pregel

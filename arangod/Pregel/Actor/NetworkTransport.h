////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Julia Volmer
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "ActorPID.h"

#include "velocypack/SharedSlice.h"
#include "Network/ConnectionPool.h"
#include "Network/Methods.h"
#include "fuerte/types.h"

#include "Pregel/Messaging/Message.h"

#include <string>

namespace {
// TODO: this function is stolen from Networkconnection.cpp
// and we should not have duplicates
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

}  // namespace

namespace arangodb::pregel::actor {

struct NetworkTransport {
  NetworkTransport(network::ConnectionPool* connectionPool)
      : connectionPool(connectionPool) {}

  auto operator()(ActorPID sender, ActorPID receiver,
                  velocypack::SharedSlice msg) const -> void {
    auto requestOptions = network::RequestOptions{
        .database = receiver.databaseName, .timeout = timeout};
    auto networkMessage = ModernMessage{
        .executionNumber = ExecutionNumber{0},
        .payload = NetworkMessage{.sender = sender,
                                  .receiver = receiver,
                                  .payload = velocypack::Builder(msg.slice())}};
    auto serialized = inspection::serializeWithErrorT(networkMessage);

    if (not serialized.ok()) {
      LOG_DEVEL << "error serializing message";
      return;
    }

    auto builder = velocypack::Builder(serialized.get().slice());
    LOG_DEVEL << fmt::format("trying to send a message from {} to {} ({})",
                             sender, receiver, builder.slice().toJson());

    if (this->connectionPool == nullptr) {
      LOG_DEVEL << " connection pool is null";
      return;
    }

    auto request = network::sendRequestRetry(
        this->connectionPool,
        // TODO: what about "shard:"?
        std::string{"server:"} + receiver.server, fuerte::RestVerb::Post,
        baseURL, builder.bufferRef(), requestOptions);

    LOG_DEVEL << "send";
    std::move(request).thenValue([](auto&& result) -> void {
      auto out = errorHandling(result);
      if (out.fail()) {
        std::cerr << "oops" << out.errorNumber() << " " << out.errorMessage();
        //        std::abort();
      }
    });
  }

  network::ConnectionPool* connectionPool;
  // timeout magic constant has to be configurable
  network::Timeout timeout{5.0 * 60};
  // baseURL should be configurable
  std::string baseURL{"/_api/pregel/"};
};

}  // namespace arangodb::pregel::actor

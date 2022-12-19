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

#include <string>                                               \

namespace arangodb::pregel::actor {

struct NetworkTransport {
  NetworkTransport(network::ConnectionPool* connectionPool)
      : connectionPool(connectionPool) {}

  auto operator()(ActorPID sender, ActorPID receiver,
                  velocypack::SharedSlice msg) -> void {

    auto requestOptions = network::RequestOptions{.timeout = timeout, .database = receiver.dabaseName};
    auto request =
        network::sendRequestRetry(connectionPool,
                                  // TODO: what about "shard:"?
                                  "server:"s + receiver.server,
                                  fuerte::RestVerb::Post,
                                  baseURL,
                                  msg.buffer(), requestOptions);

    std::move(request).thenValue([](auto&& result) -> Result {
      auto out = errorHandling(result);
      if (out.fail()) {
        std::cerr << "oops" << out.errorNumber() << " " << out.errorMessage();
        std::abort();
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

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018-2019 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_NETWORK_METHODS_H
#define ARANGOD_NETWORK_METHODS_H 1

#include "Basics/Result.h"
#include "Futures/Future.h"
#include "Network/types.h"

#include <fuerte/message.h>
#include <velocypack/Buffer.h>
#include <chrono>

namespace arangodb {
namespace network {

/// Response data structure
struct Response {
  DestinationId destination;
  fuerte::Error error;  /// connectivity error
  std::unique_ptr<arangodb::fuerte::Response> response;
};
using FutureRes = arangodb::futures::Future<Response>;

/// @brief send a request to a given destination
FutureRes sendRequest(DestinationId const& destination, arangodb::fuerte::RestVerb type,
                      std::string const& path, velocypack::Buffer<uint8_t> payload,
                      Timeout timeout, Headers const& headers = {});

/// @brief send a request to a given destination, retry under certain conditions
/// a retry will be triggered if the connection was lost our could not be established
/// optionally a retry will be performed in the case of until timeout is exceeded
FutureRes sendRequestRetry(DestinationId const& destination,
                           arangodb::fuerte::RestVerb type, std::string const& path,
                           velocypack::Buffer<uint8_t> payload, Timeout timeout,
                           Headers const& headers = {}, bool retryNotFound = false);

}  // namespace network
}  // namespace arangodb

#endif

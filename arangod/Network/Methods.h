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
#include "Basics/StaticStrings.h"
#include "Futures/Future.h"
#include "Network/ConnectionPool.h"
#include "Network/types.h"

#include <fuerte/message.h>
#include <velocypack/Buffer.h>
#include <chrono>

namespace arangodb {
class ClusterInfo;

namespace network {
class ConnectionPool;

/// Response data structure
struct Response {
  DestinationId destination;
  fuerte::Error error;  /// connectivity error
  std::shared_ptr<arangodb::fuerte::Response> response;
  
  bool ok() const {
    return fuerte::Error::NoError == this->error;
  }
  
  bool fail() const { return !ok(); }
    
  // returns a slice of the payload if there was no error
  velocypack::Slice slice() const {
    if (error == fuerte::Error::NoError && response) {
      return response->slice();
    }
    return velocypack::Slice(); // none slice
  }
  
 public:
  std::string destinationShard() const; /// @brief shardId or empty
  std::string serverId() const;         /// @brief server ID
};
using FutureRes = arangodb::futures::Future<Response>;

// Container for optional (often defaulted) parameters
struct RequestOptions {
  Timeout timeout = Timeout(120.0);
  std::string contentType = StaticStrings::MimeTypeVPack;
  std::string acceptType = StaticStrings::MimeTypeVPack;
  bool retryNotFound = false;
};

/// @brief send a request to a given destination
///
/// deprecated, use alternative signature
FutureRes sendRequest(ConnectionPool* pool, DestinationId const& destination,
                      arangodb::fuerte::RestVerb type, std::string const& path,
                      velocypack::Buffer<uint8_t> payload, Timeout timeout,
                      Headers headers = {});

/// @brief send a request to a given destination
FutureRes sendRequest(ConnectionPool* pool, DestinationId const& destination,
                      arangodb::fuerte::RestVerb type, std::string const& path,
                      velocypack::Buffer<uint8_t> payload = {},
                      Headers headers = {}, RequestOptions options = {});

/// @brief send a request to a given destination, retry under certain conditions
/// a retry will be triggered if the connection was lost our could not be established
/// optionally a retry will be performed in the case of until timeout is exceeded
///
/// deprecated, use alternative signature
FutureRes sendRequestRetry(ConnectionPool* pool, DestinationId const& destination,
                           arangodb::fuerte::RestVerb type, std::string const& path,
                           velocypack::Buffer<uint8_t> payload, Timeout timeout,
                           Headers headers = {}, bool retryNotFound = false);

/// @brief send a request to a given destination, retry under certain conditions
/// a retry will be triggered if the connection was lost our could not be established
/// optionally a retry will be performed in the case of until timeout is exceeded
FutureRes sendRequestRetry(ConnectionPool* pool, DestinationId const& destination,
                           arangodb::fuerte::RestVerb type, std::string const& path,
                           velocypack::Buffer<uint8_t> payload = {},
                           Headers headers = {}, RequestOptions options = {});

using Sender =
    std::function<FutureRes(DestinationId const&, arangodb::fuerte::RestVerb, std::string const&,
                            velocypack::Buffer<uint8_t>, Timeout, Headers)>;

}  // namespace network
}  // namespace arangodb

#endif

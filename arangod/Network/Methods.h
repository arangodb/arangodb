////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
  fuerte::Error error = fuerte::Error::ConnectionCanceled;
  std::unique_ptr<arangodb::fuerte::Request> request;
  std::unique_ptr<arangodb::fuerte::Response> response;

  [[nodiscard]] bool ok() const {
    return fuerte::Error::NoError == this->error;
  }

  [[nodiscard]] bool fail() const { return !ok(); }

  // returns a slice of the payload if there was no error
  [[nodiscard]] velocypack::Slice slice() const {
    if (error == fuerte::Error::NoError && response) {
      return response->slice();
    }
    return velocypack::Slice();  // none slice
  }

  fuerte::StatusCode statusCode() const {
    if (error == fuerte::Error::NoError && response) {
      return response->statusCode();
    }
    return fuerte::StatusUndefined;
  }

  /// @brief Build a Result that contains
  ///   - no error if everything went well, otherwise
  ///   - the error from the body, if available, otherwise
  ///   - the HTTP error, if available, otherwise
  ///   - the fuerte error, if there was a connectivity error.
  [[nodiscard]] Result combinedResult() const;

 public:
  [[nodiscard]] std::string destinationShard() const;  /// @brief shardId or empty
  [[nodiscard]] std::string serverId() const;          /// @brief server ID
};

static_assert(std::is_nothrow_move_constructible<Response>::value, "");
using FutureRes = arangodb::futures::Future<Response>;

static constexpr Timeout TimeoutDefault = Timeout(120.0);

// Container for optional (often defaulted) parameters
struct RequestOptions {
  std::string database;
  std::string contentType;  // uses vpack by default
  std::string acceptType;   // uses vpack by default
  fuerte::StringMap parameters;
  Timeout timeout = TimeoutDefault;
  bool retryNotFound = false;  // retry if answers is "datasource not found"
  bool skipScheduler = false;  // do not use Scheduler queue

  template <typename K, typename V>
  RequestOptions& param(K&& key, V&& val) {
    this->parameters.insert_or_assign(std::forward<K>(key), std::forward<V>(val));
    return *this;
  }
};

/// @brief send a request to a given destination
/// This method must not throw under penalty of ...
FutureRes sendRequest(ConnectionPool* pool, DestinationId destination,
                      arangodb::fuerte::RestVerb type, std::string path,
                      velocypack::Buffer<uint8_t> payload = {},
                      RequestOptions const& options = {}, Headers headers = {});

/// @brief send a request to a given destination, retry under certain conditions
/// a retry will be triggered if the connection was lost our could not be
/// established optionally a retry will be performed in the case of until
/// timeout is exceeded This method must not throw under penalty of ...
FutureRes sendRequestRetry(ConnectionPool* pool, DestinationId destination,
                           arangodb::fuerte::RestVerb type, std::string path,
                           velocypack::Buffer<uint8_t> payload = {},
                           RequestOptions const& options = {}, Headers headers = {});

using Sender =
    std::function<FutureRes(DestinationId const&, arangodb::fuerte::RestVerb, std::string const&,
                            velocypack::Buffer<uint8_t>, RequestOptions const& options, Headers)>;

}  // namespace network
}  // namespace arangodb

#endif

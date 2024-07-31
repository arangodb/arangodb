////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Assertions/Assert.h"
#include "GeneralServer/RequestLane.h"
#include "Network/types.h"

#include <fuerte/types.h>

#include <string>

namespace arangodb::network {

static constexpr Timeout TimeoutDefault = Timeout(120.0);

// Container for optional (often defaulted) parameters
struct RequestOptions {
  std::string database;
  std::string contentType;  // uses vpack by default
  std::string acceptType;   // uses vpack by default
  fuerte::StringMap parameters;
  Timeout timeout = TimeoutDefault;
  // retry if answer is "datasource not found"
  bool retryNotFound = false;
  // do not use Scheduler queue
  bool skipScheduler = false;
  // send x-arango-hlc header with outgoing request, so that that peer can
  // update its own HLC value to at least the value of our HLC
  bool sendHLCHeader = true;
  // transparently handle content-encoding. enabling this will automatically
  // uncompress responses that have the `Content-Encoding: gzip|deflate` header
  // set.
  bool handleContentEncoding = true;
  // allow to compress the request
  bool allowCompression = true;
  RequestLane continuationLane = RequestLane::CONTINUATION;

  // Normally this is empty, if it is set to the ID of a server in the
  // cluster, we will direct a read operation to a shard not as usual to
  // the leader, but rather to the server given here. This is read for
  // the "allowDirtyReads" options when we want to read from followers.
  std::string overrideDestination;

  template<typename K, typename V>
  RequestOptions& param(K&& key, V&& val) {
    TRI_ASSERT(!std::string_view{val}.empty());  // cannot parse it on receiver
    this->parameters.insert_or_assign(std::forward<K>(key),
                                      std::forward<V>(val));
    return *this;
  }
};

}  // namespace arangodb::network

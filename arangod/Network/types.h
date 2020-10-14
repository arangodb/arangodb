////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_NETWORK_TYPES_H
#define ARANGOD_NETWORK_TYPES_H 1

#include <fuerte/types.h>
#include <chrono>

namespace arangodb {
namespace network {

struct Response;
typedef std::string DestinationId;

using Headers = std::map<std::string, std::string>;
using Timeout = std::chrono::duration<double>;

struct EndpointSpec {
  std::string shardId;
  std::string serverId;
  std::string endpoint;
};

}  // namespace network
}  // namespace arangodb

#endif

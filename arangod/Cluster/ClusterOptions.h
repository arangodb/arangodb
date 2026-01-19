////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2025 ArangoDB GmbH, Cologne, Germany
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

#include <cstdint>
#include <string>
#include <vector>

#include "Cluster/ServerState.h"

namespace arangodb {

struct ClusterOptions {
  std::vector<std::string> agencyEndpoints;
  std::string agencyPrefix;
  std::string myRole;
  ServerState::RoleEnum requestedRole = ServerState::RoleEnum::ROLE_UNDEFINED;
  std::string myEndpoint;
  std::string myAdvertisedEndpoint;
  std::string apiJwtPolicy = "jwt-compat";

  // hard-coded limit for maximum replicationFactor value
  static constexpr std::uint32_t kMaxReplicationFactor = 10;

  std::uint32_t connectivityCheckInterval = 3600;  // seconds
  std::uint32_t writeConcern = 1;
  std::uint32_t defaultReplicationFactor = 1;
  std::uint32_t systemReplicationFactor = 2;
  std::uint32_t minReplicationFactor = 1;
  std::uint32_t maxReplicationFactor = kMaxReplicationFactor;
  std::uint32_t maxNumberOfShards = 1000;
  std::uint32_t maxNumberOfMoveShards =
      10;  // maximum number of shards to be moved per rebalance operation
           // if value = 0, no move shards operations will be scheduled

  // The following value indicates what HTTP status code should be returned if
  // a configured write concern cannot currently be fulfilled. The old
  // behavior (currently the default) means that a 403 Forbidden
  // with an error of 1004 ERROR_ARANGO_READ_ONLY is returned. It is possible to
  // adjust the behavior so that an HTTP 503 Service Unavailable with an error
  // of 1429 ERROR_REPLICATION_WRITE_CONCERN_NOT_FULFILLED is returned.
  std::uint32_t statusCodeFailedWriteConcern = 403;
  bool createWaitsForSyncReplication = true;
  bool forceOneShard = false;
  bool unregisterOnShutdown = false;
  bool enableCluster = false;
  bool requirePersistedId = false;
  /// @brief coordinator timeout for index creation. defaults to 4 days
  double indexCreationTimeout = 72.0 * 3600.0;
  double noHeartbeatDelayBeforeShutdown = 1800.0;  // seconds
};

}  // namespace arangodb

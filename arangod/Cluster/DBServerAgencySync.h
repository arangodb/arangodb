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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Basics/Common.h"
#include "Basics/Result.h"
#include "Basics/VelocyPackHelper.h"
#include "Containers/FlatHashMap.h"
#include "Containers/FlatHashSet.h"
#include "RestServer/arangod.h"

namespace arangodb {

namespace replication2 {
namespace maintenance {
struct LogStatus;
}
namespace replicated_state {
struct StateStatus;
}
class LogId;
}  // namespace replication2

class HeartbeatThread;
struct ShardID;

struct DBServerAgencySyncResult {
  bool success;
  std::string errorMessage;
  uint64_t planIndex;
  uint64_t currentIndex;

  DBServerAgencySyncResult() : success(false), planIndex(0), currentIndex(0) {}

  DBServerAgencySyncResult(bool s, uint64_t pi, uint64_t ci)
      : success(s), planIndex(pi), currentIndex(ci) {}

  DBServerAgencySyncResult(bool s, std::string const& e, uint64_t pi,
                           uint64_t ci)
      : success(s), errorMessage(e), planIndex(pi), currentIndex(ci) {}
};

class DBServerAgencySync {
  DBServerAgencySync(DBServerAgencySync const&) = delete;
  DBServerAgencySync& operator=(DBServerAgencySync const&) = delete;

 public:
  explicit DBServerAgencySync(ArangodServer& server,
                              HeartbeatThread* heartbeat);

  void work();

  // equivalent of ReplicatedLogStatusMapByDatabase
  using LocalLogsMap = std::unordered_map<
      std::string,
      std::unordered_map<arangodb::replication2::LogId,
                         arangodb::replication2::maintenance::LogStatus>>;

  // equivalent of ShardIdToLogIdMapByDatabase
  using LocalShardsToLogsMap = std::unordered_map<
      std::string, std::unordered_map<ShardID, arangodb::replication2::LogId>>;

  /**
   * @brief Get copy of current local state
   * @param  collections  Builder to fill to
   */
  Result getLocalCollections(
      containers::FlatHashSet<std::string> const& dirty,
      containers::FlatHashMap<std::string, std::shared_ptr<VPackBuilder>>&
          collections,
      LocalLogsMap& replLogs, LocalShardsToLogsMap& localShardIdToLogId);

  double requestTimeout() const noexcept;

 private:
  DBServerAgencySyncResult execute();

 private:
  ArangodServer& _server;
  HeartbeatThread* _heartbeat;
  double _requestTimeout;
};
}  // namespace arangodb

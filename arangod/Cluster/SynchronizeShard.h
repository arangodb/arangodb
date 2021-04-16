////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Kaveh Vahedipour
/// @author Matthew Von-Maszewski
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_MAINTENANCE_SYNCHRONIZE_SHARD_H
#define ARANGODB_MAINTENANCE_SYNCHRONIZE_SHARD_H

#include "Basics/ResultT.h"
#include "Cluster/ActionBase.h"
#include "Cluster/ActionDescription.h"
#include "Replication/utilities.h"
#include "VocBase/voc-types.h"

#include <chrono>

namespace arangodb {
namespace network {
class ConnectionPool;
}

class LogicalCollection;
struct SyncerId;

namespace maintenance {

class SynchronizeShard : public ActionBase {
 public:
  SynchronizeShard(MaintenanceFeature&, ActionDescription const& d);

  virtual ~SynchronizeShard();

  bool first() override final;

  void setState(ActionState state) override final;

  std::string const& clientInfoString() const;

  arangodb::replutils::LeaderInfo const& leaderInfo() const;
  void setLeaderInfo(arangodb::replutils::LeaderInfo const& leaderInfo);

 private:
  arangodb::Result getReadLock(network::ConnectionPool* pool,
                               std::string const& endpoint, std::string const& database,
                               std::string const& collection, std::string const& clientId,
                               uint64_t rlid, bool soft, double timeout);

  arangodb::Result startReadLockOnLeader(std::string const& endpoint,
                                         std::string const& database,
                                         std::string const& collection,
                                         std::string const& clientId, uint64_t& rlid,
                                         bool soft, double timeout = 300.0);

  arangodb::ResultT<TRI_voc_tick_t> catchupWithReadLock(
      std::string const& ep, std::string const& database, LogicalCollection const& collection,
      std::string const& clientId, std::string const& shard,
      std::string const& leader, TRI_voc_tick_t lastLogTick, VPackBuilder& builder);

  arangodb::Result catchupWithExclusiveLock(
      std::string const& ep, std::string const& database,
      LogicalCollection& collection, std::string const& clientId,
      std::string const& shard, std::string const& leader, SyncerId syncerId,
      TRI_voc_tick_t lastLogTick, VPackBuilder& builder);

  /// @brief Short, informative description of the replication client, passed to the server
  std::string _clientInfoString;

  /// @brief information about the leader, reused across multiple replication steps
  arangodb::replutils::LeaderInfo _leaderInfo;
};

}  // namespace maintenance
}  // namespace arangodb

#endif

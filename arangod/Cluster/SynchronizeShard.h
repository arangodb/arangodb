////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2018 ArangoDB GmbH, Cologne, Germany
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

#include "ActionBase.h"
#include "ActionDescription.h"
#include "Cluster/ResultT.h"
#include "VocBase/voc-types.h"

#include <chrono>

namespace arangodb {

class LogicalCollection;

namespace maintenance {

class SynchronizeShard : public ActionBase {
 public:
  SynchronizeShard(MaintenanceFeature&, ActionDescription const& d);

  virtual ~SynchronizeShard();

  virtual bool first() override final;

  virtual void setState(ActionState state) override final;

 private:
  arangodb::Result getReadLock(std::string const& endpoint, std::string const& database,
                               std::string const& collection, std::string const& clientId,
                               uint64_t rlid, bool soft, double timeout = 300.0);

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
      std::string const& ep, std::string const& database, LogicalCollection const& collection,
      std::string const& clientId, std::string const& shard,
      std::string const& leader, TRI_voc_tick_t lastLogTick, VPackBuilder& builder);
};

}  // namespace maintenance
}  // namespace arangodb

#endif

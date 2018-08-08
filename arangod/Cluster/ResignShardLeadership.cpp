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

#include "ResignShardLeadership.h"
#include "MaintenanceFeature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/FollowerInfo.h"
#include "Transaction/StandaloneContext.h"
#include "Transaction/Methods.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/Methods/Collections.h"
#include "VocBase/Methods/Databases.h"

#include <velocypack/Compare.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>


using namespace arangodb::application_features;
using namespace arangodb::maintenance;
using namespace arangodb::methods;

ResignShardLeadership::ResignShardLeadership(
  MaintenanceFeature& feature, ActionDescription const& desc)
  : ActionBase(feature, desc) {

  if (!desc.has(DATABASE)) {
    LOG_TOPIC(ERR, Logger::MAINTENANCE)
      << "ResignShardLeadership: database must be specified";
    setState(FAILED);
  }

  if (!desc.has(SHARD)) {
    LOG_TOPIC(ERR, Logger::MAINTENANCE)
      << "ResignShardLeadership: shard must be specified";
    setState(FAILED);
  }

}

ResignShardLeadership::~ResignShardLeadership() {};



bool ResignShardLeadership::first() {

  auto const& database = _description.get(DATABASE);
  auto const& collection = _description.get(SHARD);

  LOG_TOPIC(DEBUG, Logger::MAINTENANCE)
    << "trying to withdraw as leader of shard '" << database << "/" << collection;

  auto vocbase = Databases::lookup(database);
  if (vocbase == nullptr) {
    std::string errorMsg("ResignShardLeadership: Failed to lookup database ");
    errorMsg += database;
    _result.reset(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND, errorMsg);
    LOG_TOPIC(ERR, Logger::MAINTENANCE) << errorMsg;
    setState(FAILED);
    return false;
  }

  auto col = vocbase->lookupCollection(collection);
  if (col == nullptr) {
    std::string errorMsg("EnsureIndex: Failed to lookup local collection ");
    errorMsg += collection + " in database " + database;
    _result.reset(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, errorMsg);
    LOG_TOPIC(ERR, Logger::MAINTENANCE) << errorMsg;
    setState(FAILED);
    return false;
  }
  // This starts a write transaction, just to wait for any ongoing
  // write transaction on this shard to terminate. We will then later
  // report to Current about this resignation. If a new write operation
  // starts in the meantime (which is unlikely, since no coordinator that
  // has seen the _ will start a new one), it is doomed, and we ignore the
  // problem, since similar problems can arise in failover scenarios anyway.

  try {
    // we know the shard exists locally!

    col->followers()->setTheLeader("LEADER_NOT_YET_KNOWN");  // resign
    // Note that it is likely that we will be a follower for this shard
    // with another leader in due course. However, we do not know the
    // name of the new leader yet. This setting will make us a follower
    // for now but we will not accept any replication operation from any
    // leader, until we have negotiated a deal with it. Then the actual
    // name of the leader will be set.

    // Get write transaction on collection
    auto ctx = std::make_shared<transaction::StandaloneContext>(*vocbase);
    transaction::Methods trx(ctx, {}, {collection}, {}, transaction::Options());

    Result res = trx.begin();

    if (!res.ok()) {
      THROW_ARANGO_EXCEPTION(res);
     }
  } catch (std::exception const& e) {
    std::string errorMsg( "ResignLeadership: exception thrown when resigning:");
    errorMsg += e.what();
    LOG_TOPIC(ERR, Logger::MAINTENANCE) << errorMsg;
    _result = Result(TRI_ERROR_INTERNAL, errorMsg);
    setState(FAILED);
    return false;
  }

  setState(COMPLETE);
  return false;

}

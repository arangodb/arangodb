////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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

#include "UpdateCollection.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/FollowerInfo.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/Methods/Collections.h"
#include "VocBase/Methods/Databases.h"


using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::maintenance;
using namespace arangodb::methods;

UpdateCollection::UpdateCollection(
  MaintenanceFeature& feature, ActionDescription const& desc) :
  ActionBase(feature, desc) {
  TRI_ASSERT(desc.has(COLLECTION));
  TRI_ASSERT(desc.has(DATABASE));
  TRI_ASSERT(desc.has(LEADER));
  TRI_ASSERT(desc.has(LOCAL_LEADER));
}

void handleLeadership(
  LogicalCollection& collection, std::string const& localLeader,
  std::string const& plannedLeader) {
  
  auto& followers = collection.followers();
      
  if (plannedLeader.empty()) { // Planned to lead
    if (!localLeader.empty()) {  // We were not leader, assume leadership
      followers->setTheLeader(std::string());
      followers->clear();
    } else {
      // If someone (the Supervision most likely) has thrown
      // out a follower from the plan, then the leader
      // will not notice until it fails to replicate an operation
      // to the old follower. This here is to drop such a follower
      // from the local list of followers. Will be reported
      // to Current in due course. This is not needed for 
      // correctness but is a performance optimization.
    }  
  } else { // Planned to follow
    if (localLeader.empty()) {
      // Note that the following does not delete the follower list
      // and that this is crucial, because in the planned leader 
      // resign case, updateCurrentForCollections will report the
      // resignation together with the old in-sync list to the
      // agency. If this list would be empty, then the supervision
      // would be very angry with us!
      followers->setTheLeader(plannedLeader);
    }
    // Note that if we have been a follower to some leader
    // we do not immediately adjust the leader here, even if
    // the planned leader differs from what we have set locally.
    // The setting must only be adjusted once we have
    // synchronized with the new leader and negotiated
    // a leader/follower relationship!
  }
  
}


UpdateCollection::~UpdateCollection() {};

bool UpdateCollection::first() {

  auto const& database   = _description.get(DATABASE);
  auto const& collection = _description.get(COLLECTION);
  auto const& plannedLeader = _description.get(LEADER);
  auto const& localLeader = _description.get(LOCAL_LEADER);
  auto const& props = properties();

  auto vocbase = Databases::lookup(database);
  if (vocbase == nullptr) {
    std::string errorMsg("UpdateCollection: Failed to lookup database ");
    errorMsg += database;
    _result.reset(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND, errorMsg);
    return false;
  }

  Result found = methods::Collections::lookup(
    vocbase, collection, [&](LogicalCollection& coll) {
      LOG_TOPIC(DEBUG, Logger::MAINTENANCE)
        << "Updating local collection " + collection;

      // We adjust local leadership, note that the planned
      // resignation case is not handled here, since then
      // ourselves does not appear in shards[shard] but only
      // "_" + ourselves. See below
      // under "Drop local shards" to see the proper handling
      // of this case. Place is marked with *** in comments.
      handleLeadership(coll, localLeader, plannedLeader);
      _result = Collections::updateProperties(&coll, props);
    });
  
  if (found.fail()) {
    std::string errorMsg("UpdateCollection: Failed to lookup local collection ");
    errorMsg += collection + "in database " + database;
    _result = actionError(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND, errorMsg);
  }
  
  return false;

}

arangodb::Result UpdateCollection::kill(Signal const& signal) {
  return actionError(
    TRI_ERROR_ACTION_OPERATION_UNABORTABLE, "Cannot kill UpdateCollection action");
}

arangodb::Result UpdateCollection::progress(double& progress) {
  progress = 0.5;
  return arangodb::Result(TRI_ERROR_NO_ERROR);
}



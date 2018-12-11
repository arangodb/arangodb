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

#include "UpdateCollection.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/FollowerInfo.h"
#include "Utils/DatabaseGuard.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/Methods/Collections.h"
#include "VocBase/Methods/Databases.h"
#include "Cluster/MaintenanceFeature.h"


using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::maintenance;
using namespace arangodb::methods;

UpdateCollection::UpdateCollection(
  MaintenanceFeature& feature, ActionDescription const& desc) :
  ActionBase(feature, desc) {

  std::stringstream error;

  _labels.emplace(FAST_TRACK);

  if (!desc.has(COLLECTION)) {
    error << "collection must be specified. ";
  }
  TRI_ASSERT(desc.has(COLLECTION));

  if (!desc.has(SHARD)) {
    error << "shard must be specified. ";
  }
  TRI_ASSERT(desc.has(SHARD));

  if (!desc.has(DATABASE)) {
    error << "database must be specified. ";
  }
  TRI_ASSERT(desc.has(DATABASE));

  if (!desc.has(THE_LEADER)) {
    error << "leader must be specified. ";
  }
  TRI_ASSERT(desc.has(THE_LEADER));

  if (!desc.has(LOCAL_LEADER)) {
    error << "local leader must be specified. ";
  }
  TRI_ASSERT(desc.has(LOCAL_LEADER));

  if (!desc.has(FOLLOWERS_TO_DROP)) {
    error << "followersToDrop must be specified. ";
  }
  TRI_ASSERT(desc.has(FOLLOWERS_TO_DROP));

  if (!error.str().empty()) {
    LOG_TOPIC(ERR, Logger::MAINTENANCE) << "UpdateCollection: " << error.str();
    _result.reset(TRI_ERROR_INTERNAL, error.str());
    setState(FAILED);
  }

}

void handleLeadership(
  LogicalCollection& collection, std::string const& localLeader,
  std::string const& plannedLeader, std::string const& followersToDrop) {

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
      // to Current in due course.
      if (!followersToDrop.empty()) {
        std::vector<std::string> ftd = arangodb::basics::StringUtils::split(
            followersToDrop, ',');
        for (auto const& s : ftd) {
          followers->remove(s);
        }
      }
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

  auto const& database      = _description.get(DATABASE);
  auto const& collection    = _description.get(COLLECTION);
  auto const& shard         = _description.get(SHARD);
  auto const& plannedLeader = _description.get(THE_LEADER);
  auto const& localLeader   = _description.get(LOCAL_LEADER);
  auto const& followersToDrop = _description.get(FOLLOWERS_TO_DROP);
  auto const& props = properties();

  try {
    DatabaseGuard guard(database);
    auto vocbase = &guard.database();

    Result found = methods::Collections::lookup(
      vocbase,
      shard,
      [&](std::shared_ptr<LogicalCollection> const& coll)->void {
        TRI_ASSERT(coll);
        LOG_TOPIC(DEBUG, Logger::MAINTENANCE)
          << "Updating local collection " + shard;

        // We adjust local leadership, note that the planned
        // resignation case is not handled here, since then
        // ourselves does not appear in shards[shard] but only
        // "_" + ourselves.
        handleLeadership(*coll, localLeader, plannedLeader, followersToDrop);
        _result = Collections::updateProperties(*coll, props, false); // always a full-update

        if (!_result.ok()) {
          LOG_TOPIC(ERR, Logger::MAINTENANCE) << "failed to update properties"
            " of collection " << shard << ": " << _result.errorMessage();
        }
      });

    if (found.fail()) {
      std::stringstream error;
      error << "failed to lookup local collection " << shard
            << "in database " + database;
      LOG_TOPIC(ERR, Logger::MAINTENANCE) << error.str();
      _result = actionError(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, error.str());
    }
  } catch (std::exception const& e) {
    std::stringstream error;

    error << "action " << _description << " failed with exception " << e.what();
    LOG_TOPIC(WARN, Logger::MAINTENANCE) << "UpdateCollection: " << error.str();
    _result.reset(TRI_ERROR_INTERNAL, error.str());
  }

  if (_result.fail()) {
    _feature.storeShardError(database, collection, shard,
      _description.get(SERVER_ID), _result);
  }

  notify();

  return false;
}

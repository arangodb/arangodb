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
/// @author Kaveh Vahedipour
/// @author Matthew Von-Maszewski
////////////////////////////////////////////////////////////////////////////////

#include "UpdateCollection.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/FollowerInfo.h"
#include "Cluster/MaintenanceFeature.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "Replication2/AgencyCollectionSpecification.h"
#include "Replication2/StateMachines/Document/DocumentFollowerState.h"
#include "Replication2/StateMachines/Document/DocumentLeaderState.h"
#include "RestServer/DatabaseFeature.h"
#include "Transaction/ClusterUtils.h"
#include "Utils/DatabaseGuard.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/Methods/Collections.h"
#include "VocBase/Methods/Databases.h"

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::maintenance;
using namespace arangodb::methods;

UpdateCollection::UpdateCollection(MaintenanceFeature& feature,
                                   ActionDescription const& desc)
    : ActionBase(feature, desc),
      ShardDefinition(desc.get(DATABASE), desc.get(SHARD)) {
  std::stringstream error;

  _labels.emplace(FAST_TRACK);

  if (!desc.has(COLLECTION)) {
    error << "collection must be specified. ";
  }
  TRI_ASSERT(desc.has(COLLECTION));

  if (!ShardDefinition::isValid()) {
    error << "database and shard must be specified. ";
  }

  if (!desc.has(FOLLOWERS_TO_DROP)) {
    error << "followersToDrop must be specified. ";
  }
  TRI_ASSERT(desc.has(FOLLOWERS_TO_DROP));

  if (!error.str().empty()) {
    LOG_TOPIC("a6e4c", ERR, Logger::MAINTENANCE)
        << "UpdateCollection: " << error.str();
    result(TRI_ERROR_INTERNAL, error.str());
    setState(FAILED);
  }
}

UpdateCollection::~UpdateCollection() = default;

bool UpdateCollection::first() {
  auto const& database = getDatabase();
  auto const& collection = _description.get(COLLECTION);
  auto const& shard = getShard();
  auto const& followersToDrop = _description.get(FOLLOWERS_TO_DROP);
  auto const& props = properties();
  Result res;

  std::string from;
  _description.get("from", from);

  try {
    auto& df = _feature.server().getFeature<DatabaseFeature>();
    DatabaseGuard guard(df, database);
    auto& vocbase = guard.database();

    std::shared_ptr<LogicalCollection> coll;
    Result found = methods::Collections::lookup(vocbase, shard, coll);
    if (found.ok()) {
      TRI_ASSERT(coll);
      LOG_TOPIC("60543", DEBUG, Logger::MAINTENANCE)
          << "Updating local collection " << shard;

      // If someone (the Supervision most likely) has thrown
      // out a follower from the plan, then the leader
      // will not notice until it fails to replicate an operation
      // to the old follower. This here is to drop such a follower
      // from the local list of followers. Will be reported
      // to Current in due course.
      if (!followersToDrop.empty()) {
        TRI_IF_FAILURE("Maintenance::doNotRemoveUnPlannedFollowers") {
          LOG_TOPIC("de342", INFO, Logger::MAINTENANCE)
              << "Skipping check for followers not in Plan because of failure "
                 "point.";
          return false;
        }
        auto& followers = coll->followers();
        std::vector<std::string> ftd =
            arangodb::basics::StringUtils::split(followersToDrop, ',');
        for (auto const& s : ftd) {
          followers->remove(s);
        }
      }

      res.reset(std::invoke([&, coll = std::move(coll)]() mutable -> Result {
        if (vocbase.replicationVersion() == replication::Version::TWO) {
          return updateCollectionReplication2(
              shard, collection, _description.properties()->sharedSlice(),
              std::move(coll));
        }

        OperationOptions options(ExecContext::current());
        return Collections::updateProperties(*coll, props, options)
            .waitAndGet();
      }));
      result(res);

      if (!res.ok()) {
        LOG_TOPIC("c3733", ERR, Logger::MAINTENANCE)
            << "failed to update properties"
               " of collection "
            << shard << ": " << res.errorMessage();
      }

    } else {
      std::stringstream error;
      error << "failed to lookup local collection " << shard << "in database "
            << database;
      LOG_TOPIC("620fb", ERR, Logger::MAINTENANCE) << error.str();
      res = actionError(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, error.str());
      result(res);
    }
  } catch (std::exception const& e) {
    std::stringstream error;

    error << "action " << _description << " failed with exception " << e.what();
    LOG_TOPIC("79442", WARN, Logger::MAINTENANCE)
        << "UpdateCollection: " << error.str();
    res.reset(TRI_ERROR_INTERNAL, error.str());
    result(res);
  }

  if (res.fail()) {
    if (res.is(TRI_ERROR_REPLICATION_REPLICATED_LOG_NOT_THE_LEADER) ||
        res.is(TRI_ERROR_REPLICATION_REPLICATED_STATE_NOT_FOUND)) {
      // Temporary unavailability of the replication2 leader should not stop
      // this server from updating the shard eventually.
      // TODO prevent busy loop and wait for log to become ready (CINFRA-831).
      std::this_thread::sleep_for(std::chrono::milliseconds{50});
    } else {
      _feature.storeShardError(database, collection, shard,
                               _description.get(SERVER_ID), res);
    }
  }

  return false;
}
void UpdateCollection::setState(ActionState state) {
  if ((COMPLETE == state || FAILED == state) && _state != state) {
    _feature.unlockShard(getShard());
  }
  ActionBase::setState(state);
}

auto UpdateCollection::updateCollectionReplication2(
    ShardID const& shard, CollectionID const& collection,
    velocypack::SharedSlice props,
    std::shared_ptr<LogicalCollection> coll) noexcept -> Result {
  return basics::catchToResult([coll = std::move(coll),
                                props = std::move(props), &collection,
                                &shard]() mutable {
    return coll->getDocumentStateLeader()
        ->modifyShard(shard, collection, std::move(props))
        .waitAndGet();
  });
}

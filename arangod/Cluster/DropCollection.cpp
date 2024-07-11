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

#include "DropCollection.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/Exceptions.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/MaintenanceFeature.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "Replication2/ReplicatedState/ReplicatedState.h"
#include "Replication2/StateMachines/Document/DocumentFollowerState.h"
#include "Replication2/StateMachines/Document/DocumentLeaderState.h"
#include "RestServer/DatabaseFeature.h"
#include "Utils/DatabaseGuard.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/Methods/Collections.h"
#include "VocBase/Methods/Databases.h"
#include "VocBase/vocbase.h"

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::maintenance;
using namespace arangodb::methods;

DropCollection::DropCollection(MaintenanceFeature& feature,
                               ActionDescription const& d)
    : ActionBase(feature, d), ShardDefinition(d.get(DATABASE), d.get(SHARD)) {
  std::stringstream error;

  if (!ShardDefinition::isValid()) {
    error << "database and shard must be specified. ";
  }

  if (!error.str().empty()) {
    LOG_TOPIC("c7e42", ERR, Logger::MAINTENANCE)
        << "DropCollection: " << error.str();
    result(TRI_ERROR_INTERNAL, error.str());
    setState(FAILED);
  }
}

DropCollection::~DropCollection() = default;

bool DropCollection::first() {
  auto const& database = getDatabase();
  auto const& shard = getShard();

  LOG_TOPIC("a2961", DEBUG, Logger::MAINTENANCE)
      << "DropCollection: dropping local shard '" << database << "/" << shard;

  std::string from;
  _description.get("from", from);

  // Database still there?
  auto& df = _feature.server().getFeature<DatabaseFeature>();
  try {
    // note: will throw if database is already deleted.
    // exception TRI_ERROR_ARANGO_DATABASE_NOT_FOUND will be handled
    // properly below
    DatabaseGuard guard(df, database);
    auto& vocbase = guard.database();

    std::shared_ptr<LogicalCollection> coll;
    Result found = methods::Collections::lookup(vocbase, shard, coll);
    if (found.ok()) {
      TRI_ASSERT(coll);
      LOG_TOPIC("03e2f", DEBUG, Logger::MAINTENANCE)
          << "Dropping local collection " << shard;

      if (vocbase.replicationVersion() == replication::Version::TWO) {
        result(dropCollectionReplication2(shard, coll));
      } else {
        // both flags should not be necessary here, as we are only dealing with
        // shard names here and not actual cluster-wide collection names
        CollectionDropOptions dropOptions{.allowDropSystem = true,
                                          .allowDropGraphCollection = true};
        result(Collections::drop(*coll, dropOptions));
      }

      // it is safe here to clear our replication failure statistics even
      // if the collection could not be dropped. the drop attempt alone
      // should be reason enough to zero our stats
      _feature.removeReplicationError(database, shard);
    } else {
      std::stringstream error;

      error << "failed to lookup local collection " << database << "/" << shard;
      LOG_TOPIC("02722", ERR, Logger::MAINTENANCE)
          << "DropCollection: " << error.str() << " found " << found;
      result(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND, error.str());

      return false;
    }
  } catch (basics::Exception const& e) {
    if (e.code() != TRI_ERROR_ARANGO_DATABASE_NOT_FOUND) {
      // any error but "database not found" will be reported properly.
      // "database not found" is expected here for already-dropped
      // databases.
      std::stringstream error;

      error << "action " << _description << " failed with exception "
            << e.what();
      LOG_TOPIC("761d2", ERR, Logger::MAINTENANCE) << error.str();
      result(e.code(), error.str());

      return false;
    }
    // TRI_ERROR_ARANGO_DATABASE_NOT_FOUND will fallthrough here,
    // intentionally
  } catch (std::exception const& e) {
    std::stringstream error;

    error << "action " << _description << " failed with exception " << e.what();
    LOG_TOPIC("9dbd8", ERR, Logger::MAINTENANCE) << error.str();
    result(TRI_ERROR_INTERNAL, error.str());

    return false;
  }

  // We're removing the shard version from MaintenanceFeature before notifying
  // for new Maintenance run. This should make sure that the next round does not
  // get rejected.
  _feature.delShardVersion(shard);

  return false;
}

void DropCollection::setState(ActionState state) {
  if ((COMPLETE == state || FAILED == state) && _state != state) {
    _feature.unlockShard(getShard());
  }
  ActionBase::setState(state);
}

Result DropCollection::dropCollectionReplication2(
    ShardID const& shard, std::shared_ptr<LogicalCollection>& coll) {
  TRI_ASSERT(coll != nullptr) << shard;

  auto res = basics::catchToResult([&]() {
    auto leader = coll->getDocumentStateLeader();
    return leader->dropShard(shard).waitAndGet();
  });

  if (res.is(TRI_ERROR_REPLICATION_REPLICATED_LOG_NOT_THE_LEADER) ||
      res.is(TRI_ERROR_REPLICATION_REPLICATED_STATE_NOT_FOUND)) {
    // TODO prevent busy loop and wait for log to become ready (CINFRA-831).
    std::this_thread::sleep_for(std::chrono::milliseconds{50});
  }

  return res;
}

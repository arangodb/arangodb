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

#include <velocypack/Iterator.h>

#include "EnsureIndex.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/StaticStrings.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/Maintenance.h"
#include "Cluster/MaintenanceFeature.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "Replication2/StateMachines/Document/DocumentFollowerState.h"
#include "Replication2/StateMachines/Document/DocumentLeaderState.h"
#include "RestServer/DatabaseFeature.h"
#include "Utils/DatabaseGuard.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/Methods/Databases.h"

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::maintenance;
using namespace arangodb::methods;

EnsureIndex::EnsureIndex(MaintenanceFeature& feature,
                         ActionDescription const& desc)
    : ActionBase(feature, desc) {
  std::stringstream error;

  if (!desc.has(DATABASE)) {
    error << "database must be specified. ";
  }
  TRI_ASSERT(desc.has(DATABASE));

  if (!desc.has(COLLECTION)) {
    error << "cluster-wide collection must be specified. ";
  }
  TRI_ASSERT(desc.has(COLLECTION));

  if (!desc.has(SHARD)) {
    error << "shard must be specified. ";
  }
  TRI_ASSERT(desc.has(SHARD));

  if (!properties().hasKey(ID)) {
    error << "index properties must include id. ";
  }
  TRI_ASSERT(properties().hasKey(ID));

  if (!desc.has(StaticStrings::IndexType)) {
    error << "index type must be specified - discriminatory. ";
  }
  TRI_ASSERT(desc.has(StaticStrings::IndexType));

  if (!desc.has(FIELDS)) {
    error << "index fields must be specified - discriminatory. ";
  }
  TRI_ASSERT(desc.has(FIELDS));

  if (!error.str().empty()) {
    LOG_TOPIC("8473a", ERR, Logger::MAINTENANCE)
        << "EnsureIndex: " << error.str();
    result(TRI_ERROR_INTERNAL, error.str());
    setState(FAILED);
  }
}

EnsureIndex::~EnsureIndex() = default;

// For local book keeping and reporting on /_admin/actions
arangodb::Result EnsureIndex::setProgress(double d) {
  _progress = d;
  return {};
}

void EnsureIndex::setState(ActionState state) {
  if (_description.isRunEvenIfDuplicate() &&
      (COMPLETE == state || FAILED == state) && _state != state) {
    // calling unlockShard here is safe, because nothing before it
    // can go throw. if some code is added before the unlock that
    // can throw, it must be made sure that the unlock is always called
    _feature.unlockShard(ShardID{_description.get(SHARD)});
  }
  ActionBase::setState(state);
}

bool EnsureIndex::first() {
  auto const& database = _description.get(DATABASE);
  auto const& collection = _description.get(COLLECTION);
  auto const& shard = _description.get(SHARD);
  auto const& id = properties().get(ID).copyString();

  try {  // now try to guard the database
    auto& df = _feature.server().getFeature<DatabaseFeature>();
    DatabaseGuard guard(df, database);
    auto vocbase = &guard.database();

    auto col = vocbase->lookupCollection(shard);
    if (col == nullptr) {
      std::stringstream error;
      error << "failed to lookup local collection " << shard << " in database "
            << database;
      LOG_TOPIC("12767", ERR, Logger::MAINTENANCE)
          << "EnsureIndex: " << error.str();
      result(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, error.str());
      return false;
    }

    VPackBuilder body;
    {
      VPackObjectBuilder b(&body);
      body.add(COLLECTION, VPackValue(shard));
      auto const props = properties();
      body.add(VPackObjectIterator(props));
    }

    if (_priority != maintenance::SLOW_OP_PRIORITY) {
      uint64_t docCount = 0;
      if (Result res = arangodb::maintenance::collectionCount(*col, docCount);
          res.fail()) {
        std::stringstream error;
        error << "failed to get count of local collection " << shard
              << " in database " << database << ": " << res.errorMessage();
        LOG_TOPIC("23561", WARN, Logger::MAINTENANCE)
            << "EnsureIndex: " << error.str();
        result(res.errorNumber(), error.str());
        return false;
      }

      if (docCount > 100000) {
        // This could be a larger job, let's reschedule ourselves with
        // priority SLOW_OP_PRIORITY:
        LOG_TOPIC("25a63", DEBUG, Logger::MAINTENANCE)
            << "EnsureIndex action found a shard with more than 100000 "
               "documents, "
               "will reschedule with slow priority, database: "
            << database << ", shard: " << shard;
        requeueMe(maintenance::SLOW_OP_PRIORITY);
        result(TRI_ERROR_ACTION_UNFINISHED,
               "EnsureIndex action rescheduled to slow operation priority");
        return false;
      }
      // continue with the job normally
    }

    auto res = std::invoke([&]() -> Result {
      auto lambda = std::make_shared<Indexes::ProgressTracker>(
          [this](double d) { return setProgress(d); });

      if (vocbase->replicationVersion() == replication::Version::TWO) {
        return ensureIndexReplication2(col, body.slice(), std::move(lambda));
      }
      auto index = VPackBuilder();
      auto res = methods::Indexes::ensureIndex(*col, body.slice(), true, index,
                                               std::move(lambda))
                     .waitAndGet();
      if (res.ok()) {
        indexCreationLogging(index.slice());
        setProgress(100.);
      }
      return res;
    });

    result(res);

    if (res.fail()) {
      std::stringstream error;
      error << "failed to ensure index " << body.slice().toJson() << " "
            << res.errorMessage();

      if (!res.is(TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED) &&
          !res.is(TRI_ERROR_BAD_PARAMETER)) {
        // "unique constraint violated" is an expected error that can happen at
        // any time. it does not justify logging and alerting DBAs. The error
        // will be passed back to the caller anyway, so not logging it seems to
        // be good.
        LOG_TOPIC("bc555", WARN, Logger::MAINTENANCE)
            << "EnsureIndex: " << _description << ", error: " << error.str();
      }

      VPackBuilder eb;
      {
        VPackObjectBuilder o(&eb);
        eb.add(StaticStrings::Error, VPackValue(true));
        eb.add(StaticStrings::ErrorMessage, VPackValue(res.errorMessage()));
        eb.add(StaticStrings::ErrorNum, VPackValue(res.errorNumber()));
        eb.add(ID, VPackValue(id));
      }

      LOG_TOPIC("397e2", DEBUG, Logger::MAINTENANCE)
          << "Reporting error " << eb.toJson();

      // FIXMEMAINTENANCE: If this action is refused due to missing
      // components in description, no IndexError gets produced. But
      // then, if you are missing components, such as database name, will
      // you be able to produce an IndexError?

      // Temporary unavailability of the replication2 leader should not stop
      // this server from creating the index eventually.
      if (res.is(TRI_ERROR_REPLICATION_REPLICATED_LOG_NOT_THE_LEADER) ||
          res.is(TRI_ERROR_REPLICATION_REPLICATED_STATE_NOT_FOUND)) {
        // TODO prevent busy loop and wait for log to become ready (CINFRA-831).
        std::this_thread::sleep_for(std::chrono::milliseconds{50});
      } else {
        _feature.storeIndexError(database, collection, shard, id, eb.steal());
      }
      result(TRI_ERROR_INTERNAL, error.str());
      return false;
    }

  } catch (std::exception const& e) {  // Guard failed?
    std::stringstream error;
    error << "action " << _description << " failed with exception " << e.what();
    LOG_TOPIC("445e5", WARN, Logger::MAINTENANCE)
        << "EnsureIndex: " << error.str();
    result(TRI_ERROR_INTERNAL, error.str());
    return false;
  }

  return false;
}

void EnsureIndex::indexCreationLogging(VPackSlice index) {
  VPackSlice created = index.get("isNewlyCreated");
  std::string log = std::string("Index ") + index.get(ID).copyString();
  log += (created.isBool() && created.getBool() ? std::string(" created")
                                                : std::string(" updated"));
  LOG_TOPIC("6e2cd", DEBUG, Logger::MAINTENANCE) << log;
}

auto EnsureIndex::ensureIndexReplication2(
    std::shared_ptr<LogicalCollection> coll, VPackSlice indexInfo,
    std::shared_ptr<methods::Indexes::ProgressTracker> progress) noexcept
    -> Result {
  auto maybeShardID = ShardID::shardIdFromString(coll->name());
  if (ADB_UNLIKELY(maybeShardID.fail())) {
    // This will only throw if we take a real collection here and not a shard.
    TRI_ASSERT(false) << "Tried to ensure index on Collection " << coll->name()
                      << " which is not considered a shard.";
    return maybeShardID.result();
  }
  return basics::catchToResult([&coll, shard = maybeShardID.get(), indexInfo,
                                progress = std::move(progress)]() mutable {
    return coll->getDocumentStateLeader()
        ->createIndex(shard, indexInfo, std::move(progress))
        .waitAndGet();
  });
}

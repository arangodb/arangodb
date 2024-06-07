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

#include "CreateCollection.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/StaticStrings.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/FollowerInfo.h"
#include "Cluster/MaintenanceFeature.h"
#include "Cluster/Utils/ShardID.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "RestServer/DatabaseFeature.h"
#include "Utils/DatabaseGuard.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/Methods/Collections.h"
#include "VocBase/Methods/Databases.h"
#include "VocBase/vocbase.h"
#include "Replication2/ReplicatedState/ReplicatedState.h"
#include "Replication2/StateMachines/Document/DocumentFollowerState.h"
#include "Replication2/StateMachines/Document/DocumentLeaderState.h"

#include <velocypack/Compare.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::maintenance;
using namespace arangodb::methods;

CreateCollection::CreateCollection(MaintenanceFeature& feature,
                                   ActionDescription const& desc)
    : ActionBase(feature, desc),
      ShardDefinition(desc.get(DATABASE), desc.get(SHARD)) {
  std::stringstream error;

  _labels.emplace(FAST_TRACK);

  if (!desc.has(COLLECTION)) {
    error << "cluster-wide collection must be specified. ";
  }
  TRI_ASSERT(desc.has(COLLECTION));

  if (!ShardDefinition::isValid()) {
    error << "database and shard must be specified. ";
  }

  if (!desc.has(THE_LEADER)) {
    error << "shard leader must be specified. ";
  }
  TRI_ASSERT(desc.has(THE_LEADER));

  if (!desc.has(SERVER_ID)) {
    error << "own server id must be specified. ";
  }
  TRI_ASSERT(desc.has(SERVER_ID));

  if (!properties().get(StaticStrings::DataSourceType).isNumber()) {
    error << "properties slice must specify collection type. ";
  }
  TRI_ASSERT(properties().hasKey(StaticStrings::DataSourceType) &&
             properties().get(StaticStrings::DataSourceType).isNumber())
      << properties().toJson() << desc;

  uint32_t const type =
      properties().get(StaticStrings::DataSourceType).getNumber<uint32_t>();
  if (type != TRI_COL_TYPE_DOCUMENT && type != TRI_COL_TYPE_EDGE) {
    error << "invalid collection type number. " << type;
  }
  TRI_ASSERT(type == TRI_COL_TYPE_DOCUMENT || type == TRI_COL_TYPE_EDGE);

  if (!error.str().empty()) {
    LOG_TOPIC("7c60f", ERR, Logger::MAINTENANCE)
        << "CreateCollection: " << error.str();
    result(TRI_ERROR_INTERNAL, error.str());
    setState(FAILED);
  }
}

CreateCollection::~CreateCollection() = default;

bool CreateCollection::first() {
  auto const& database = getDatabase();
  auto const& collection = _description.get(COLLECTION);
  auto const& shard = getShard();
  auto const& leader = _description.get(THE_LEADER);
  auto const& props = properties();

  std::string from;
  _description.get("from", from);

  LOG_TOPIC("21710", DEBUG, Logger::MAINTENANCE)
      << "CreateCollection: creating local shard '" << database << "/" << shard
      << "' for central '" << database << "/" << collection << "'";

  Result res;

  // Temporary unavailability of the replication2 leader should not stop this
  // server from creating the shard eventually.
  bool ignoreTemporaryError = false;

  try {  // now try to guard the vocbase
    auto& df = _feature.server().getFeature<DatabaseFeature>();
    DatabaseGuard guard(df, database);
    auto& vocbase = guard.database();

    TRI_col_type_e type = static_cast<TRI_col_type_e>(
        props.get(StaticStrings::DataSourceType).getNumber<uint32_t>());

    // Replication2 collection group
    auto group = getCollectionGroup(props);
    TRI_ASSERT(vocbase.replicationVersion() != replication::Version::TWO ||
               group != nullptr)
        << shard;

    // Replication2 log ID
    auto logId = std::invoke([&]() -> std::optional<replication2::LogId> {
      if (vocbase.replicationVersion() == replication::Version::TWO) {
        return getReplicatedLogId(shard, group, props);
      }
      return std::nullopt;
    });

    VPackBuilder docket;
    {
      VPackObjectBuilder d(&docket);
      for (auto const& i : VPackObjectIterator(props)) {
        std::string_view key = i.key.stringView();
        if (key == StaticStrings::DataSourceId ||
            key == StaticStrings::DataSourceName ||
            key == StaticStrings::DataSourceGuid ||
            key == StaticStrings::ObjectId) {
          if (key == StaticStrings::DataSourceGuid ||
              key == StaticStrings::ObjectId) {
            LOG_TOPIC("44577", WARN, Logger::MAINTENANCE)
                << "unexpected " << key << " in " << props.toJson();
          }
          continue;
        }
        docket.add(key, i.value);
      }
      docket.add(PLAN_ID, VPackValue(collection));

      if (vocbase.replicationVersion() == replication::Version::TWO) {
        TRI_ASSERT(logId.has_value()) << shard;
        docket.add(REPLICATED_STATE_ID, VPackValue(*logId));

        // For replication 2 the CollectionGroupProperties are not stored
        // in the collection anymore, but they are still required for
        // shard creation. So let us rewrite the Properties.
        fillGroupProperties(group, docket);
      }
    }

    res.reset(std::invoke([&]() {
      if (vocbase.replicationVersion() == replication::Version::TWO) {
        return createCollectionReplication2(vocbase, *logId, shard, type,
                                            docket.sharedSlice());
      }
      return createCollectionReplication1(vocbase, shard, type, docket.slice(),
                                          leader);
    }));
    result(res);

    if (res.fail()) {
      // If this is TRI_ERROR_ARANGO_DUPLICATE_NAME, then we assume that a
      // previous incarnation of ourselves has already done the work. This can
      // happen, if the timing of phaseOne runs is unfortunate with
      // asynchronous creation of shards. In this case, we do not report an
      // error and do not increase the version number of the shard in
      // `setState` below.
      if (res.is(TRI_ERROR_ARANGO_DUPLICATE_NAME)) {
        LOG_TOPIC("9db9c", DEBUG, Logger::MAINTENANCE)
            << "local collection " << database << "/" << shard
            << " already found, ignoring...";
        result(TRI_ERROR_NO_ERROR);
        _doNotIncrement = true;
        return false;
      }

      std::stringstream error;
      error << "creating local shard '" << database << "/" << shard
            << "' for central '" << database << "/" << collection
            << "' failed: " << res;
      if (res.is(TRI_ERROR_REPLICATION_REPLICATED_LOG_NOT_THE_LEADER) ||
          res.is(TRI_ERROR_REPLICATION_REPLICATED_STATE_NOT_FOUND)) {
        // Do not store this error
        // TODO prevent busy loop and wait for log to become ready (CINFRA-831).
        std::this_thread::sleep_for(std::chrono::milliseconds{50});
        ignoreTemporaryError = true;
        LOG_TOPIC("63688", DEBUG, Logger::MAINTENANCE) << error.str();
      } else {
        LOG_TOPIC("63687", ERR, Logger::MAINTENANCE) << error.str();
      }

      res.reset(TRI_ERROR_FAILED, error.str());
      result(res);
    }
  } catch (std::exception const& e) {
    std::stringstream error;

    error << "action " << _description << " failed with exception " << e.what();
    LOG_TOPIC("60514", WARN, Logger::MAINTENANCE) << error.str();
    res.reset(TRI_ERROR_FAILED, error.str());
    result(res);
  }

  if (res.fail() && !ignoreTemporaryError) {
    _feature.storeShardError(database, collection, shard,
                             _description.get(SERVER_ID), res);
  }

  LOG_TOPIC("4562c", DEBUG, Logger::MAINTENANCE)
      << "Create collection done, notifying Maintenance";

  return false;
}

void CreateCollection::setState(ActionState state) {
  if ((COMPLETE == state || FAILED == state) && _state != state) {
    // calling unlockShard here is safe, because nothing before it
    // can go throw. if some code is added before the unlock that
    // can throw, it must be made sure that the unlock is always called
    _feature.unlockShard(getShard());
    if (!_doNotIncrement) {
      _feature.incShardVersion(getShard());
    }
  }
  ActionBase::setState(state);
}

Result CreateCollection::createCollectionReplication1(
    TRI_vocbase_t& vocbase, ShardID const& shard, TRI_col_type_e collectionType,
    VPackSlice properties, std::string const& leader) {
  TRI_IF_FAILURE("create_collection_delay_follower_creation") {
    if (!leader.empty()) {
      // Make a race that the shard on the follower is not created more
      // likely
      std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
  }

  std::shared_ptr<LogicalCollection> col;
  OperationOptions options(ExecContext::current());
  auto res = Collections::createShard(vocbase, options, shard, collectionType,
                                      properties, col);
  if (col) {
    LOG_TOPIC("9db9a", DEBUG, Logger::MAINTENANCE)
        << "local collection " << vocbase.name() << "/" << shard
        << " successfully created";

    if (leader.empty()) {
      std::vector<std::string> noFollowers;
      col->followers()->takeOverLeadership(noFollowers, nullptr);
    } else {
      TRI_IF_FAILURE("create_collection_delay_follower_sync_start") {
        // Make a race that the shard on the follower is not in sync more
        // likely
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
      }
      col->followers()->setTheLeader(LEADER_NOT_YET_KNOWN);
    }
  }
  return res;
}

Result CreateCollection::createCollectionReplication2(
    TRI_vocbase_t& vocbase, replication2::LogId logId, ShardID const& shard,
    TRI_col_type_e collectionType, velocypack::SharedSlice properties) {
  auto res = Result{};

  auto state = vocbase.getReplicatedStateById(logId);
  if (state.ok()) {
    auto leaderState = std::dynamic_pointer_cast<
        replication2::replicated_state::document::DocumentLeaderState>(
        state.get()->getLeader());
    if (leaderState != nullptr) {
      res.reset(
          basics::catchToResult([&leaderState, &shard, collectionType,
                                 properties = std::move(properties)]() mutable {
            // It is necessary to block here to prevent creation of an
            // additional action while we are waiting for the shard to be
            // created.
            return leaderState
                ->createShard(shard, collectionType, std::move(properties))
                .waitAndGet();
          }));
    } else {
      res.reset(
          {TRI_ERROR_REPLICATION_REPLICATED_LOG_NOT_THE_LEADER,
           fmt::format("Leader of log {} not found while creating shard {}",
                       logId, shard)});
    }
  } else {
    res.reset(state.result());
  }

  return res;
}

std::shared_ptr<replication2::agency::CollectionGroupPlanSpecification const>
CreateCollection::getCollectionGroup(VPackSlice props) const {
  auto gid = std::invoke(
      [props]() -> std::optional<replication2::agency::CollectionGroupId> {
        if (auto gid = props.get("groupId"); gid.isUInt()) {
          return replication2::agency::CollectionGroupId{gid.getUInt()};
        }
        return std::nullopt;
      });

  if (!gid.has_value()) {
    return nullptr;
  }

  auto& ci = _feature.server().getFeature<ClusterFeature>().clusterInfo();
  return ci.getCollectionGroupById(*gid);
}

void CreateCollection::fillGroupProperties(
    std::shared_ptr<
        replication2::agency::CollectionGroupPlanSpecification const> const&
        group,
    VPackBuilder& builder) {
  builder.add(StaticStrings::NumberOfShards,
              VPackValue(group->attributes.immutableAttributes.numberOfShards));
  builder.add(StaticStrings::WriteConcern,
              VPackValue(group->attributes.mutableAttributes.writeConcern));
  builder.add(StaticStrings::WaitForSyncString,
              VPackValue(group->attributes.mutableAttributes.waitForSync));
  builder.add(
      StaticStrings::ReplicationFactor,
      VPackValue(group->attributes.mutableAttributes.replicationFactor));
}

replication2::LogId CreateCollection::getReplicatedLogId(
    ShardID const& shard,
    std::shared_ptr<
        replication2::agency::CollectionGroupPlanSpecification const> const&
        group,
    VPackSlice props) {
  auto shardsR2 = props.get("shardsR2");
  ADB_PROD_ASSERT(shardsR2.isArray()) << props.toJson();

  bool found = false;
  std::size_t index = 0;
  std::string cmpShard{shard};
  for (auto sid : VPackArrayIterator(shardsR2)) {
    if (sid.isEqualString(cmpShard)) {
      found = true;
      break;
    }
    ++index;
  }
  ADB_PROD_ASSERT(found) << shard << " " << shardsR2.toJson();
  ADB_PROD_ASSERT(index < group->shardSheaves.size())
      << " " << index << " " << shard << " " << group->shardSheaves.size();

  return group->shardSheaves[index].replicatedLog;
}

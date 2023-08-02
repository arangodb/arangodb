////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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

#include "CreateCollection.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/StaticStrings.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/FollowerInfo.h"
#include "Cluster/MaintenanceFeature.h"
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

  try {  // now try to guard the vocbase
    auto& df = _feature.server().getFeature<DatabaseFeature>();
    DatabaseGuard guard(df, database);
    auto& vocbase = guard.database();

    if (vocbase.replicationVersion() == replication::Version::TWO &&
        from == "maintenance") {
      return createReplication2Shard(collection, shard, props, vocbase);
    }

    TRI_col_type_e type = static_cast<TRI_col_type_e>(
        props.get(StaticStrings::DataSourceType).getNumber<uint32_t>());

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
      if (_description.has(maintenance::REPLICATED_LOG_ID)) {
        auto logId = replication2::LogId::fromString(
            _description.get(maintenance::REPLICATED_LOG_ID));
        TRI_ASSERT(logId.has_value());
        docket.add("replicatedStateId", VPackValue(*logId));
      }
      docket.add("planId", VPackValue(collection));
    }

    TRI_IF_FAILURE("create_collection_delay_follower_creation") {
      if (!leader.empty()) {
        // Make a race that the shard on the follower is not created more likely
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
      }
    }
    std::shared_ptr<LogicalCollection> col;
    OperationOptions options(ExecContext::current());
    res.reset(Collections::createShard(vocbase, options, shard, type,
                                       docket.slice(), col));
    result(res);
    if (col) {
      LOG_TOPIC("9db9a", DEBUG, Logger::MAINTENANCE)
          << "local collection " << database << "/" << shard
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

    if (res.fail()) {
      // If this is TRI_ERROR_ARANGO_DUPLICATE_NAME, then we assume that a
      // previous incarnation of ourselves has already done the work. This can
      // happen, if the timing of phaseOne runs is unfortunate with asynchronous
      // creation of shards. In this case, we do not report an error and do not
      // increase the version number of the shard in `setState` below.
      if (res.errorNumber() == TRI_ERROR_ARANGO_DUPLICATE_NAME) {
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
      LOG_TOPIC("63687", ERR, Logger::MAINTENANCE) << error.str();

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

  if (res.fail()) {
    _feature.storeShardError(database, collection, shard,
                             _description.get(SERVER_ID), res);
  }

  LOG_TOPIC("4562c", DEBUG, Logger::MAINTENANCE)
      << "Create collection done, notifying Maintenance";

  return false;
}

bool CreateCollection::createReplication2Shard(CollectionID const& collection,
                                               ShardID const& shard,
                                               VPackSlice props,
                                               TRI_vocbase_t& vocbase)
    const {  // special case for replication 2 on the leader
  auto logId = LogicalCollection::shardIdToStateId(shard);
  if (auto gid = props.get("groupId"); gid.isNumber()) {
    logId = replication2::LogId{gid.getUInt()};
    // Now look up the collection group
    auto& ci = _feature.server().getFeature<ClusterFeature>().clusterInfo();
    auto group = ci.getCollectionGroupById(
        replication2::agency::CollectionGroupId{logId.id()});
    if (group == nullptr) {
      return false;  // retry later
    }
    // now find the index of the shard in the collection group
    auto shardsR2 = props.get("shardsR2");
    ADB_PROD_ASSERT(shardsR2.isArray());
    bool found = false;
    std::size_t index = 0;
    for (auto x : VPackArrayIterator(shardsR2)) {
      if (x.isEqualString(shard)) {
        found = true;
        break;
      }
      ++index;
    }
    ADB_PROD_ASSERT(found);
    ADB_PROD_ASSERT(index < group->shardSheaves.size());
    logId = group->shardSheaves[index].replicatedLog;
  }
  auto state = vocbase.getReplicatedStateById(logId);
  if (state.ok()) {
    auto leaderState = std::dynamic_pointer_cast<
        replication2::replicated_state::document::DocumentLeaderState>(
        state.get()->getLeader());
    if (leaderState != nullptr) {
      // It is necessary to block here to prevent creation of an additional
      // action while we are waiting for the shard to be created.
      leaderState->createShard(shard, collection, _description.properties())
          .get();
    } else {
      // TODO prevent busy loop and wait for log to become ready.
      std::this_thread::sleep_for(std::chrono::milliseconds{50});
    }
  }

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

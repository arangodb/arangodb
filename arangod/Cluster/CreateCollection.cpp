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

#include "CreateCollection.h"
#include "MaintenanceFeature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/ClusterFeature.h"
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

#include <Replication2/ReplicatedLog/FakeLogFollower.h>
#include <velocypack/Compare.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::maintenance;
using namespace arangodb::methods;

constexpr auto WAIT_FOR_SYNC_REPL = "waitForSyncReplication";
constexpr auto ENF_REPL_FACT = "enforceReplicationFactor";

CreateCollection::CreateCollection(MaintenanceFeature& feature, ActionDescription const& desc)
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
             properties().get(StaticStrings::DataSourceType).isNumber());

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
  auto const& followerEncoded = _description.get(FOLLOWER_ID);

  auto const& props = properties();

  std::vector<std::string> follower;

  {
    // YOLO!
    auto followerDecoded = basics::StringUtils::decodeBase64(followerEncoded);
    auto slice = VPackSlice(reinterpret_cast<uint8_t const*>(followerDecoded.c_str()));
    VPackArrayIterator iter(slice);
    TRI_ASSERT(slice.isArray() && slice.length() >= 1);
    std::transform(std::next(iter.begin()), iter.end(), std::back_inserter(follower),
                   [](VPackSlice slice) { return slice.copyString(); });
  }

  LOG_DEVEL << "create collection with followers " << follower;

  LOG_TOPIC("21710", DEBUG, Logger::MAINTENANCE)
      << "CreateCollection: creating local shard '" << database << "/" << shard
      << "' for central '" << database << "/" << collection << "'";

  Result res;

  try {  // now try to guard the vocbase
    auto& df = _feature.server().getFeature<DatabaseFeature>();
    DatabaseGuard guard(df, database);
    auto& vocbase = guard.database();

    auto& cluster = _feature.server().getFeature<ClusterFeature>();

    bool waitForRepl =
        (props.get(WAIT_FOR_SYNC_REPL).isBool())
            ? props.get(WAIT_FOR_SYNC_REPL).getBool()
            : cluster.createWaitsForSyncReplication();

    bool enforceReplFact =
        (props.get(ENF_REPL_FACT).isBool())
            ? props.get(ENF_REPL_FACT).getBool()
            : true;

    TRI_col_type_e type = static_cast<TRI_col_type_e>(
        props.get(StaticStrings::DataSourceType).getNumber<uint32_t>());

    VPackBuilder docket;
    {
      VPackObjectBuilder d(&docket);
      for (auto const& i : VPackObjectIterator(props)) {
        auto const& key = i.key.copyString();
        if (key == ID || key == NAME || key == GLOB_UID || key == OBJECT_ID) {
          if (key == GLOB_UID || key == OBJECT_ID) {
            LOG_TOPIC("44577", WARN, Logger::MAINTENANCE)
                << "unexpected " << key << " in " << props.toJson();
          }
          continue;
        }
        docket.add(key, i.value);
      }
      docket.add("planId", VPackValue(collection));
    }

    std::shared_ptr<LogicalCollection> col;
    OperationOptions options(ExecContext::current());
    res.reset(Collections::create(vocbase, options, shard, type, docket.slice(),
                                  waitForRepl, enforceReplFact, false, col));
    result(res);
    if (col) {
      LOG_TOPIC("9db9a", DEBUG, Logger::MAINTENANCE)
          << "local collection " << database << "/" << shard
          << " successfully created";

      if (leader.empty()) {
        std::vector<std::string> noFollowers;
        col->followers()->takeOverLeadership(noFollowers, nullptr);
      } else {
        col->followers()->setTheLeader(leader);
      }
      if (vocbase.replicationVersion() == replication::Version::TWO) {
        using namespace replication2;
        using namespace replication2::replicated_log;
        if (auto logId = LogId::fromShardName(col->name())) {
          auto& log = vocbase.getReplicatedLogById(*logId);
          if (leader.empty()) {
            auto replicationFollowers =
                std::vector<std::shared_ptr<AbstractFollower>>{};
            replicationFollowers.reserve(follower.size());
            auto& networkFeature = vocbase.server().getFeature<NetworkFeature>();
            std::transform(follower.begin(), follower.end(),
                           std::back_inserter(replicationFollowers),
                           [&](auto const& followerId) {
                             return std::make_shared<FakeLogFollower>(
                                 networkFeature.pool(), followerId, vocbase.name(), *logId);
                           });

            LogLeader::TermData termData;
            termData.id = ServerState::instance()->getId();
            termData.writeConcern = col->writeConcern();
            termData.term = LogTerm{1};
            log.becomeLeader(termData, replicationFollowers);
          } else {
            log.becomeFollower(ServerState::instance()->getId(), LogTerm{1}, leader);
          }
        }
      }
    }

    if (res.fail()) {
      // If this is TRI_ERROR_ARANGO_DUPLICATE_NAME, then we assume that a previous
      // incarnation of ourselves has already done the work. This can happen, if
      // the timing of phaseOne runs is unfortunate with asynchronous creation of
      // shards.
      // In this case, we do not report an error and do not increase the version
      // number of the shard in `setState` below.
      if (res.errorNumber() == TRI_ERROR_ARANGO_DUPLICATE_NAME) {
        LOG_TOPIC("9db9c", INFO, Logger::MAINTENANCE)
        << "local collection " << database << "/" << shard
        << " already found, ignoring...";
        result(TRI_ERROR_NO_ERROR);
        _doNotIncrement = true;
        return false;
      }
      std::stringstream error;
      error << "creating local shard '" << database << "/" << shard << "' for central '"
            << database << "/" << collection << "' failed: " << res;
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

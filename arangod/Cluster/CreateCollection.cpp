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
    : ActionBase(feature, desc) {
  std::stringstream error;

  _labels.emplace(FAST_TRACK);

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
    _result.reset(TRI_ERROR_INTERNAL, error.str());
    setState(FAILED);
  }
}

CreateCollection::~CreateCollection() = default;

bool CreateCollection::first() {
  auto const& database = _description.get(DATABASE);
  auto const& collection = _description.get(COLLECTION);
  auto const& shard = _description.get(SHARD);
  auto const& leader = _description.get(THE_LEADER);
  auto const& props = properties();

  LOG_TOPIC("21710", DEBUG, Logger::MAINTENANCE)
      << "CreateCollection: creating local shard '" << database << "/" << shard
      << "' for central '" << database << "/" << collection << "'";

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
    _result = Collections::create(vocbase, options, shard, type, docket.slice(),
                                  waitForRepl, enforceReplFact, false, col);

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
    }

    if (_result.fail()) {
      // If this is TRI_ERROR_ARANGO_DUPLICATE_NAME, then we assume that a previous
      // incarnation of ourselves has already done the work. This can happen, if
      // the timing of phaseOne runs is unfortunate with asynchronous creation of
      // shards.
      // In this case, we do not report an error and do not increase the version
      // number of the shard in `setState` below.
      if (_result.errorNumber() == TRI_ERROR_ARANGO_DUPLICATE_NAME) {
        LOG_TOPIC("9db9c", INFO, Logger::MAINTENANCE)
        << "local collection " << database << "/" << shard
        << " already found, ignoring...";
        _result.reset(TRI_ERROR_NO_ERROR);
        _doNotIncrement = true;
        return false;
      }
      std::stringstream error;
      error << "creating local shard '" << database << "/" << shard << "' for central '"
            << database << "/" << collection << "' failed: " << _result;
      LOG_TOPIC("63687", ERR, Logger::MAINTENANCE) << error.str();

      _result.reset(TRI_ERROR_FAILED, error.str());
    }

  } catch (std::exception const& e) {
    std::stringstream error;

    error << "action " << _description << " failed with exception " << e.what();
    LOG_TOPIC("60514", WARN, Logger::MAINTENANCE) << error.str();
    _result.reset(TRI_ERROR_FAILED, error.str());
  }

  if (_result.fail()) {
    _feature.storeShardError(database, collection, shard,
                             _description.get(SERVER_ID), _result);
  }

  LOG_TOPIC("4562c", DEBUG, Logger::MAINTENANCE)
      << "Create collection done, notifying Maintenance";

  return false;
}

void CreateCollection::setState(ActionState state) {
  if ((COMPLETE == state || FAILED == state) && _state != state) {
    TRI_ASSERT(_description.has(SHARD));
    _feature.unlockShard(_description.get(SHARD));
    if (!_doNotIncrement) {
      _feature.incShardVersion(_description.get(SHARD));
    }
  }
  ActionBase::setState(state);
}

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

#include "Cluster/Maintenance/Actions/UpgradeCollection.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/FollowerInfo.h"
#include "Cluster/Maintenance/MaintenanceFeature.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "Transaction/ClusterUtils.h"
#include "Utils/DatabaseGuard.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/Methods/Collections.h"
#include "VocBase/Methods/Databases.h"

namespace arangodb::maintenance {

UpgradeCollection::UpgradeCollection(MaintenanceFeature& feature, ActionDescription const& desc)
    : ActionBase(feature, desc) {
  std::stringstream error;

  _labels.emplace(FAST_TRACK);

  if (!desc.has(DATABASE)) {
    error << "database must be specified. ";
  }
  TRI_ASSERT(desc.has(DATABASE));

  if (!desc.has(COLLECTION)) {
    error << "collection must be specified. ";
  }
  TRI_ASSERT(desc.has(COLLECTION));

  if (!desc.has(SHARD)) {
    error << "shard must be specified. ";
  }
  TRI_ASSERT(desc.has(SHARD));

  if (!error.str().empty()) {
    LOG_TOPIC("a6e4c", ERR, Logger::MAINTENANCE)
        << "UpgradeCollection: " << error.str();
    _result.reset(TRI_ERROR_INTERNAL, error.str());
    setState(FAILED);
  }
}

UpgradeCollection::~UpgradeCollection() = default;

bool UpgradeCollection::first() {
  auto const& database = _description.get(DATABASE);
  auto const& collection = _description.get(COLLECTION);
  auto const& shard = _description.get(SHARD);
  auto const& props = properties();

  try {
    DatabaseGuard guard(database);
    auto& vocbase = guard.database();
    
    std::shared_ptr<LogicalCollection> coll;
    Result found = methods::Collections::lookup(vocbase, shard, coll);
    if (found.ok()) {
      TRI_ASSERT(coll);
      LOG_TOPIC("60543", DEBUG, Logger::MAINTENANCE)
          << "Updating local collection " + shard;

      LOG_DEVEL << "UpgradeCollection action on '" << coll->name() << "', id '" << coll->id() << "'";
      auto status = LogicalCollection::UpgradeStatus::fetch(*coll);    

      _result = methods::Collections::updateProperties(*coll, props);

      if (!_result.ok()) {
        LOG_TOPIC("c3733", ERR, Logger::MAINTENANCE)
            << "failed to update properties"
               " of collection "
            << shard << ": " << _result.errorMessage();
      }      
    } else {
      std::stringstream error;
      error << "failed to lookup local collection " << shard << "in database " + database;
      LOG_TOPIC("620fb", ERR, Logger::MAINTENANCE) << error.str();
      _result = actionError(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, error.str());
    }
  } catch (std::exception const& e) {
    std::stringstream error;

    error << "action " << _description << " failed with exception " << e.what();
    LOG_TOPIC("79442", WARN, Logger::MAINTENANCE)
        << "UpgradeCollection: " << error.str();
    _result.reset(TRI_ERROR_INTERNAL, error.str());
  }

  if (_result.fail()) {
    _feature.storeShardError(database, collection, shard,
                             _description.get(SERVER_ID), _result);
  }

  notify();

  return false;
}

}
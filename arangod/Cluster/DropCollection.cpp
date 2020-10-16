////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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

#include "DropCollection.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/Exceptions.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/MaintenanceFeature.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "RestServer/DatabaseFeature.h"
#include "Utils/DatabaseGuard.h"
#include "VocBase/Methods/Collections.h"
#include "VocBase/Methods/Databases.h"

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::maintenance;
using namespace arangodb::methods;

DropCollection::DropCollection(MaintenanceFeature& feature, ActionDescription const& d)
    : ActionBase(feature, d) {
  std::stringstream error;

  if (!d.has(SHARD)) {
    error << "shard must be specified. ";
  }
  TRI_ASSERT(d.has(SHARD));

  if (!d.has(DATABASE)) {
    error << "database must be specified. ";
  }
  TRI_ASSERT(d.has(DATABASE));

  if (!error.str().empty()) {
    LOG_TOPIC("c7e42", ERR, Logger::MAINTENANCE) << "DropCollection: " << error.str();
    _result.reset(TRI_ERROR_INTERNAL, error.str());
    setState(FAILED);
  }
}

DropCollection::~DropCollection() = default;

bool DropCollection::first() {
  auto const& database = _description.get(DATABASE);
  auto const& shard = _description.get(SHARD);

  LOG_TOPIC("a2961", DEBUG, Logger::MAINTENANCE)
      << "DropCollection: dropping local shard '" << database << "/" << shard;

  // Database still there?
  auto* vocbase = _feature.server().getFeature<DatabaseFeature>().lookupDatabase(database);
  if (vocbase != nullptr) {
    try {
      DatabaseGuard guard(database);
      auto& vocbase = guard.database();

      std::shared_ptr<LogicalCollection> coll;
      Result found = methods::Collections::lookup(vocbase, shard, coll);
      if (found.ok()) {
        TRI_ASSERT(coll);
        LOG_TOPIC("03e2f", DEBUG, Logger::MAINTENANCE)
          << "Dropping local collection " + shard;
        _result = Collections::drop(*coll, false, 2.5);
      } else {
        std::stringstream error;

        error << "failed to lookup local collection " << database << "/" << shard;
        LOG_TOPIC("02722", ERR, Logger::MAINTENANCE) << "DropCollection: " << error.str();
        _result.reset(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND, error.str());

        return false;
      }
    } catch (basics::Exception const& e) {
      if (e.code() != TRI_ERROR_ARANGO_DATABASE_NOT_FOUND) {
        // any error but database not found will be reported properly
        std::stringstream error;

        error << "action " << _description << " failed with exception " << e.what();
        LOG_TOPIC("761d2", ERR, Logger::MAINTENANCE) << error.str();
        _result.reset(e.code(), error.str());

        return false;
      }
      // TRI_ERROR_ARANGO_DATABASE_NOT_FOUND will fallthrough here, intentionally
    } catch (std::exception const& e) {
      std::stringstream error;

      error << "action " << _description << " failed with exception " << e.what();
      LOG_TOPIC("9dbd8", ERR, Logger::MAINTENANCE) << error.str();
      _result.reset(TRI_ERROR_INTERNAL, error.str());

      return false;
    }
  }

  // We're removing the shard version from MaintenanceFeature before notifying
  // for new Maintenance run. This should make sure that the next round does not
  // get rejected.
  _feature.delShardVersion(shard);

  return false;
}

void DropCollection::setState(ActionState state) {
  if ((COMPLETE == state || FAILED == state) && _state != state) {
    _feature.unlockShard(_description.get(SHARD));
  }
  ActionBase::setState(state);
}

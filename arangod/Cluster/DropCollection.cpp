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

#include "DropCollection.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/MaintenanceFeature.h"
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

  _labels.emplace(FAST_TRACK);

  if (!d.has(COLLECTION)) {
    error << "collection must be specified. ";
  }
  TRI_ASSERT(d.has(COLLECTION));

  if (!d.has(DATABASE)) {
    error << "database must be specified. ";
  }
  TRI_ASSERT(d.has(DATABASE));

  if (!error.str().empty()) {
    LOG_TOPIC("c7e42", ERR, Logger::MAINTENANCE) << "DropCollectio: " << error.str();
    _result.reset(TRI_ERROR_INTERNAL, error.str());
    setState(FAILED);
  }
}

DropCollection::~DropCollection(){};

bool DropCollection::first() {
  auto const& database = _description.get(DATABASE);
  auto const& collection = _description.get(COLLECTION);

  LOG_TOPIC("a2961", DEBUG, Logger::MAINTENANCE)
      << "DropCollection: dropping local shard '" << database << "/" << collection;

  try {
    DatabaseGuard guard(database);
    auto& vocbase = guard.database();
    Result found = methods::Collections::lookup(
        vocbase, collection, [&](std::shared_ptr<LogicalCollection> const& coll) -> void {
          TRI_ASSERT(coll);
          LOG_TOPIC("03e2f", DEBUG, Logger::MAINTENANCE)
              << "Dropping local collection " + collection;
          _result = Collections::drop(*coll, false, 120);
        });

    if (found.fail()) {
      std::stringstream error;

      error << "failed to lookup local collection " << database << "/" << collection;
      LOG_TOPIC("02722", ERR, Logger::MAINTENANCE) << "DropCollection: " << error.str();
      _result.reset(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND, error.str());

      return false;
    }

  } catch (std::exception const& e) {
    std::stringstream error;

    error << " action " << _description << " failed with exception " << e.what();
    LOG_TOPIC("9dbd8", ERR, Logger::MAINTENANCE) << error.str();
    _result.reset(TRI_ERROR_INTERNAL, error.str());

    return false;
  }

  // We're removing the shard version from MaintenanceFeature before notifying
  // for new Maintenance run. This should make sure that the next round does not
  // get rejected.
  _feature.delShardVersion(collection);
  notify();

  return false;
}

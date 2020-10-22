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

#include <velocypack/Iterator.h>

#include "EnsureIndex.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/StaticStrings.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/MaintenanceFeature.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "Utils/DatabaseGuard.h"
#include "VocBase/Methods/Collections.h"
#include "VocBase/Methods/Databases.h"
#include "VocBase/Methods/Indexes.h"

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::maintenance;
using namespace arangodb::methods;

EnsureIndex::EnsureIndex(MaintenanceFeature& feature, ActionDescription const& desc)
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
    LOG_TOPIC("8473a", ERR, Logger::MAINTENANCE) << "EnsureIndex: " << error.str();
    _result.reset(TRI_ERROR_INTERNAL, error.str());
    setState(FAILED);
  }
}

EnsureIndex::~EnsureIndex() = default;

bool EnsureIndex::first() {
  arangodb::Result res;

  auto const& database = _description.get(DATABASE);
  auto const& collection = _description.get(COLLECTION);
  auto const& shard = _description.get(SHARD);
  auto const& id = properties().get(ID).copyString();

  VPackBuilder body;

  try {  // now try to guard the database

    DatabaseGuard guard(database);
    auto vocbase = &guard.database();

    auto col = vocbase->lookupCollection(shard);
    if (col == nullptr) {
      std::stringstream error;
      error << "failed to lookup local collection " << shard << " in database " + database;
      LOG_TOPIC("12767", ERR, Logger::MAINTENANCE) << "EnsureIndex: " << error.str();
      _result.reset(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, error.str());
      return false;
    }

    auto const props = properties();
    {
      VPackObjectBuilder b(&body);
      body.add(COLLECTION, VPackValue(shard));
      body.add(VPackObjectIterator(props));
    }

    VPackBuilder index;
    _result = methods::Indexes::ensureIndex(col.get(), body.slice(), true, index);

    if (_result.ok()) {
      VPackSlice created = index.slice().get("isNewlyCreated");
      std::string log = std::string("Index ") + id;
      log += (created.isBool() && created.getBool() ? std::string(" created")
                                                    : std::string(" updated"));
      LOG_TOPIC("6e2cd", DEBUG, Logger::MAINTENANCE) << log;
    } else {
      std::stringstream error;
      error << "failed to ensure index " << body.slice().toJson() << " "
            << _result.errorMessage();

      if (!_result.is(TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED) &&
          !_result.is(TRI_ERROR_BAD_PARAMETER)) {
        // "unique constraint violated" is an expected error that can happen at any time.
        // it does not justify logging and alerting DBAs. The error will be passed back
        // to the caller anyway, so not logging it seems to be good.
        LOG_TOPIC("bc555", WARN, Logger::MAINTENANCE) << "EnsureIndex: " << _description << ", error: " << error.str();
      }

      VPackBuilder eb;
      {
        VPackObjectBuilder o(&eb);
        eb.add(StaticStrings::Error, VPackValue(true));
        eb.add(StaticStrings::ErrorMessage, VPackValue(_result.errorMessage()));
        eb.add(StaticStrings::ErrorNum, VPackValue(_result.errorNumber()));
        eb.add(ID, VPackValue(id));
      }

      LOG_TOPIC("397e2", DEBUG, Logger::MAINTENANCE) << "Reporting error " << eb.toJson();

      // FIXMEMAINTENANCE: If this action is refused due to missing
      // components in description, no IndexError gets produced. But
      // then, if you are missing components, such as database name, will
      // you be able to produce an IndexError?

      _feature.storeIndexError(database, collection, shard, id, eb.steal());
      _result.reset(TRI_ERROR_INTERNAL, error.str());
      return false;
    }

  } catch (std::exception const& e) {  // Guard failed?
    std::stringstream error;
    error << "action " << _description << " failed with exception " << e.what();
    LOG_TOPIC("445e5", WARN, Logger::MAINTENANCE) << "EnsureIndex: " << error.str();
    _result.reset(TRI_ERROR_INTERNAL, error.str());
    return false;
  }

  return false;
}

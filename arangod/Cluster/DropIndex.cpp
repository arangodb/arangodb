////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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

#include "DropIndex.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/ClusterFeature.h"
#include "Utils/DatabaseGuard.h"
#include "VocBase/Methods/Collections.h"
#include "VocBase/Methods/Databases.h"
#include "VocBase/Methods/Indexes.h"

using namespace arangodb::application_features;
using namespace arangodb::maintenance;
using namespace arangodb::methods;
using namespace arangodb;

DropIndex::DropIndex(MaintenanceFeature& feature, ActionDescription const& d)
    : ActionBase(feature, d) {
  std::stringstream error;

  if (!d.has(COLLECTION)) {
    error << "collection must be specified. ";
  }
  TRI_ASSERT(d.has(COLLECTION));

  if (!d.has(DATABASE)) {
    error << "database must be specified. ";
  }
  TRI_ASSERT(d.has(DATABASE));

  if (!d.has(INDEX)) {
    error << "index id must be stecified. ";
  }
  TRI_ASSERT(d.has(INDEX));

  if (!error.str().empty()) {
    LOG_TOPIC(ERR, Logger::MAINTENANCE) << "DropIndex: " << error.str();
    _result.reset(TRI_ERROR_INTERNAL, error.str());
    setState(FAILED);
  }
}

DropIndex::~DropIndex(){};

bool DropIndex::first() {
  auto const& database = _description.get(DATABASE);
  auto const& collection = _description.get(COLLECTION);
  auto const& id = _description.get(INDEX);

  VPackBuilder index;
  index.add(VPackValue(_description.get(INDEX)));

  try {
    DatabaseGuard guard(database);
    auto vocbase = &guard.database();

    auto col = vocbase->lookupCollection(collection);
    if (col == nullptr) {
      std::stringstream error;
      error << "failed to lookup local collection " << collection
            << " in database " << database;
      LOG_TOPIC(ERR, Logger::MAINTENANCE) << "DropIndex: " << error.str();
      _result.reset(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, error.str());
      return false;
    }

    LOG_TOPIC(DEBUG, Logger::MAINTENANCE)
        << "Dropping local index " + collection + "/" + id;
    _result = Indexes::drop(col.get(), index.slice());

  } catch (std::exception const& e) {
    std::stringstream error;

    error << "action " << _description << " failed with exception " << e.what();
    LOG_TOPIC(ERR, Logger::MAINTENANCE) << "DropIndex " << error.str();
    _result.reset(TRI_ERROR_INTERNAL, error.str());

    return false;
  }

  notify();
  return false;
}

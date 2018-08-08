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
#include "VocBase/Methods/Collections.h"
#include "VocBase/Methods/Indexes.h"
#include "VocBase/Methods/Databases.h"

using namespace arangodb::application_features;
using namespace arangodb::maintenance;
using namespace arangodb::methods;

DropIndex::DropIndex(
  MaintenanceFeature& feature, ActionDescription const& d) :
  ActionBase(feature, d) {

  if (!d.has(COLLECTION)) {
    LOG_TOPIC(ERR, Logger::MAINTENANCE)
      << "DropIndex: collection must be specified";
    setState(FAILED);
  }
  TRI_ASSERT(d.has(COLLECTION));

  if (!d.has(DATABASE)) {
    LOG_TOPIC(ERR, Logger::MAINTENANCE)
      << "DropIndex: database must be specified";
    setState(FAILED);
  }
  TRI_ASSERT(d.has(DATABASE));

  if (!d.has(INDEX)) {
    LOG_TOPIC(ERR, Logger::MAINTENANCE)
      << "DropIndex: index id must be stecified";
    setState(FAILED);
  }
  TRI_ASSERT(d.has(INDEX));

}

DropIndex::~DropIndex() {};

bool DropIndex::first() {

  auto const& database = _description.get(DATABASE);
  auto const& collection = _description.get(COLLECTION);
  auto const& id = _description.get(INDEX);

  VPackBuilder index;
  index.add(VPackValue(_description.get(INDEX)));

  auto vocbase = Databases::lookup(database);
  if (vocbase == nullptr) {
    std::string errorMsg("DropIndex: Failed to lookup database ");
    errorMsg += database;
    _result.reset(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND, errorMsg);
    LOG_TOPIC(ERR, Logger::MAINTENANCE) << errorMsg;
    setState(FAILED);
    return false;
  }

  auto col = vocbase->lookupCollection(collection);
  if (col == nullptr) {
    std::string errorMsg("EnsureIndex: Failed to lookup local collection ");
    errorMsg += collection + " in database " + database;
    _result.reset(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, errorMsg);
    LOG_TOPIC(ERR, Logger::MAINTENANCE) << errorMsg;
    setState(FAILED);
    return false;
  }

  Result found = methods::Collections::lookup(
    vocbase, collection, [&](LogicalCollection& coll) {
      LOG_TOPIC(DEBUG, Logger::MAINTENANCE)
        << "Dropping local index " + collection + "/" + id;
      _result = Indexes::drop(&coll, index.slice());
    });

  if (found.fail()) {
    std::string errorMsg("DropIndex: Failed to lookup local collection ");
    errorMsg += collection + "in database " + database;
    LOG_TOPIC(ERR, Logger::MAINTENANCE) << errorMsg;
    _result.reset(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND, errorMsg);
    setState(FAILED);
    return false;
  }
<<<<<<< HEAD

  setState(COMPLETE);
  return false;
  
}
=======
>>>>>>> 5914ff9492a16f3af8b1655752bf6a58ffa72b81

  return false;

}

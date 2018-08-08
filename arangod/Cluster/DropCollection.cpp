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
#include "VocBase/Methods/Collections.h"
#include "VocBase/Methods/Databases.h"

using namespace arangodb::application_features;
using namespace arangodb::maintenance;
using namespace arangodb::methods;

DropCollection::DropCollection(
  MaintenanceFeature& feature, ActionDescription const& d) :
  ActionBase(feature, d) {

  if (!d.has(COLLECTION)) {
    LOG_TOPIC(ERR, Logger::MAINTENANCE)
      << "DropCollection: collection must be specified";
    setState(FAILED);
  }
  TRI_ASSERT(d.has(COLLECTION));

  if (!d.has(DATABASE)) {
    LOG_TOPIC(ERR, Logger::MAINTENANCE)
      << "DropCollection: database must be specified";
    setState(FAILED);
  }
  TRI_ASSERT(d.has(DATABASE));
  
}

DropCollection::~DropCollection() {};

bool DropCollection::first() {

  auto const& database = _description.get(DATABASE);
  auto const& collection = _description.get(COLLECTION);

  auto vocbase = Databases::lookup(database);
  if (vocbase == nullptr) {
    std::string errorMsg("failed to lookup database ");
    errorMsg += database;
    _result.reset(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND, errorMsg);
    LOG_TOPIC(ERR, Logger::MAINTENANCE)
      << "DropCollection: failed to drop local collection " << database
      << "/" << collection << ": " << errorMsg;
    setState(FAILED);
    return false;
  }

  Result found = methods::Collections::lookup(
    vocbase, collection, [&](LogicalCollection& coll) {
      LOG_TOPIC(DEBUG, Logger::MAINTENANCE)
        << "Dropping local collection " + collection;
      _result = Collections::drop(vocbase, &coll, false, 120);
    });

  if (found.fail()) {
    std::string errorMsg("DropCollection: failed to lookup local collection ");
    errorMsg += database + "/" + collection;
    _result.reset(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND, errorMsg);
    LOG_TOPIC(ERR, Logger::MAINTENANCE) << errorMsg;
    setState(FAILED);
    return false;
  }

  setState(COMPLETE);
  return false;
  
}

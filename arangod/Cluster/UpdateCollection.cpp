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

#include "UpdateCollection.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/ClusterFeature.h"
#include "VocBase/Methods/Collections.h"
#include "VocBase/Methods/Databases.h"

using namespace arangodb::application_features;
using namespace arangodb::maintenance;
using namespace arangodb::methods;

constexpr auto WAIT_FOR_SYNC_REPL = "waitForSyncReplication";
constexpr auto ENF_REPL_FACT = "enforceReplicationFactor";

UpdateCollection::UpdateCollection(ActionDescription const& d) :
  ActionBase(d, arangodb::maintenance::FOREGROUND) {
  TRI_ASSERT(d.has(COLLECTION));
  TRI_ASSERT(d.has(DATABASE));
}

UpdateCollection::~UpdateCollection() {};

arangodb::Result UpdateCollection::run(
  std::chrono::duration<double> const&, bool& finished) {
  arangodb::Result res;

  auto const& database = _description.get(DATABASE);
  auto const& collection = _description.get(COLLECTION);
  auto const& properties = _description.properties();

  auto vocbase = Databases::lookup(database);
  if (vocbase == nullptr) {
    std::string errorMsg("UpdateCollection: Failed to lookup database ");
    errorMsg += database;
    return actionError(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND, errorMsg);
  }

  Result found = methods::Collections::lookup(
    vocbase, collection, [&](LogicalCollection* coll) {
      LOG_TOPIC(DEBUG, Logger::MAINTENANCE)
        << "Updating local collection " + collection;
      res = Collections::updateProperties(coll, properties);
    });
  
  return res;
}

arangodb::Result UpdateCollection::kill(Signal const& signal) {
  return actionError(
    TRI_ERROR_ACTION_OPERATION_UNABORTABLE, "Cannot kill UpdateCollection action");
}

arangodb::Result UpdateCollection::progress(double& progress) {
  progress = 0.5;
  return arangodb::Result(TRI_ERROR_NO_ERROR);
}



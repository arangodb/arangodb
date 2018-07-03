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

#include <velocypack/Iterator.h>

#include "EnsureIndex.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/ClusterFeature.h"
#include "VocBase/Methods/Collections.h"
#include "VocBase/Methods/Databases.h"
#include "VocBase/Methods/Indexes.h"

using namespace arangodb::application_features;
using namespace arangodb::maintenance;
using namespace arangodb::methods;

EnsureIndex::EnsureIndex(
  MaintenanceFeature& feature, ActionDescription const& desc) :
  ActionBase(feature, desc) {
  TRI_ASSERT(properties().hasKey(ID));
  TRI_ASSERT(desc.has(COLLECTION));
  TRI_ASSERT(desc.has(DATABASE));
}

EnsureIndex::~EnsureIndex() {};

bool EnsureIndex::first() {
  arangodb::Result res;

  auto const& database = _description.get(DATABASE);
  auto const& collection = _description.get(COLLECTION);
  auto const& id = properties().get(ID).copyString();

  auto* vocbase = Databases::lookup(database);
  if (vocbase == nullptr) {
    std::string errorMsg("EnsureIndex: Failed to lookup database ");
    errorMsg += database;
    _result.reset(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND, errorMsg);
    return false;
  }

  auto col = vocbase->lookupCollection(collection);
  if (col == nullptr) {
    std::string errorMsg("EnsureIndex: Failed to lookup local collection ");
    errorMsg += collection + " in database " + database;
    _result.reset(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, errorMsg);
    return false;
  }

  auto const props = properties();
  VPackBuilder body;
  { VPackObjectBuilder b(&body);
    body.add(COLLECTION, VPackValue(collection));
    for (auto const& i : VPackObjectIterator(props)) {
      body.add(i.key.copyString(), i.value);
    }}

  VPackBuilder index;
  _result = methods::Indexes::ensureIndex(col.get(), body.slice(), true, index);
  
  if (_result.ok()) {
    VPackSlice created = index.slice().get("isNewlyCreated");
    std::string log =  std::string("Index ") + id;
    log += (created.isBool() && created.getBool() ? std::string(" created")
            : std::string(" updated"));
    LOG_TOPIC(DEBUG, Logger::MAINTENANCE) << log;
  } else {
    LOG_TOPIC(ERR, Logger::MAINTENANCE)
      << "Failed to ensure index " << body.slice().toJson();
  }
  
  return false;
}

arangodb::Result EnsureIndex::kill(Signal const& signal) {
  return actionError(
    TRI_ERROR_ACTION_OPERATION_UNABORTABLE, "Cannot kill EnsureIndex action");
}

arangodb::Result EnsureIndex::progress(double& progress) {
  progress = 0.5;
  return arangodb::Result(TRI_ERROR_NO_ERROR);
}



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

#include <velocypack/Iterator.h>

#include "EnsureIndex.h"

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

EnsureIndex::EnsureIndex(
  MaintenanceFeature& feature, ActionDescription const& desc) :
  ActionBase(feature, desc) {

  if (!desc.has(DATABASE)) {
    LOG_TOPIC(ERR, Logger::MAINTENANCE)
      << "CreateCollection: database must be specified";
    fail();
  }
  TRI_ASSERT(desc.has(DATABASE));

  if (!desc.has(COLLECTION)) {
    LOG_TOPIC(ERR, Logger::MAINTENANCE)
      << "CreateCollection: cluster-wide collection must be specified";
    fail();
  }
  TRI_ASSERT(desc.has(COLLECTION));

  if (!properties().hasKey(ID)) {
    LOG_TOPIC(ERR, Logger::MAINTENANCE)
      << "EnsureIndex: index properties must include id";
    fail();
  }
  TRI_ASSERT(properties().hasKey(ID));

  if (!desc.has(TYPE)) {
    LOG_TOPIC(ERR, Logger::MAINTENANCE)
      << "EnsureIndex: index type must be specified - discriminatory";
    fail();
  }
  TRI_ASSERT(desc.has(TYPE));

  if (!desc.has(FIELDS)) {
    LOG_TOPIC(ERR, Logger::MAINTENANCE)
      << "EnsureIndex: index fields must be specified - discriminatory";
    fail();
  }
  TRI_ASSERT(desc.has(FIELDS));

}

EnsureIndex::~EnsureIndex() {};

bool EnsureIndex::first() {

  ActionBase::first();
  
  arangodb::Result res;

  auto const& database = _description.get(DATABASE);
  auto const& collection = _description.get(COLLECTION);
  auto const& id = properties().get(ID).copyString();

  auto* vocbase = Databases::lookup(database);
  if (vocbase == nullptr) {
    std::string errorMsg("EnsureIndex: Failed to lookup database ");
    errorMsg += database;
    _result.reset(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND, errorMsg);
    fail();
    return false;
  }

  VPackBuilder body;
  
  try { // now try to guard the database
    
    DatabaseGuard guard(*vocbase);
    
    auto col = vocbase->lookupCollection(collection);
    if (col == nullptr) {
      std::string errorMsg("EnsureIndex: Failed to lookup local collection ");
      errorMsg += collection + " in database " + database;
      _result.reset(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, errorMsg);
      fail();
      return false;
    }
    
    auto const props = properties();
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
      fail();
      return false;
    }
    
  } catch (std::exception const& e) { // Guard failed?
    LOG_TOPIC(WARN, Logger::MAINTENANCE)
      << "action " << _description << " failed with exception " << e.what();     
      fail();
      return false;
  }

  complete();
  return false;
    
}

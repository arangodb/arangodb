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

#include "CreateDatabase.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/VelocyPackHelper.h"
#include "RestServer/DatabaseFeature.h"
#include "VocBase/Methods/Databases.h"

using namespace arangodb::application_features;
using namespace arangodb::maintenance;
using namespace arangodb::methods;

CreateDatabase::CreateDatabase(arangodb::MaintenanceFeature & feature,
                 std::shared_ptr<ActionDescription_t> const & description,
                 std::shared_ptr<VPackBuilder> const & properties)
  : MaintenanceAction(feature, description, properties) {
  TRI_ASSERT(description->end()!=description->find(DATABASE));
}

CreateDatabase::~CreateDatabase() {};

bool CreateDatabase::first() {

  VPackSlice users;
  auto db_it = _description->find(DATABASE);

  if (_description->end() != db_it) {
    auto* systemVocbase =
      ApplicationServer::getFeature<DatabaseFeature>("Database")->systemDatabase();

    if (systemVocbase == nullptr) {
      LOG_TOPIC(FATAL, Logger::AGENCY) << "could not determine _system database";
      FATAL_ERROR_EXIT();
    }

    _result = Databases::create(db_it->second, users, _properties->slice());
  } else {
    _result.reset(TRI_ERROR_BAD_PARAMETER, "CreateDatabase called without required \"database\" field.");
  } // else

  // false means no more processing
  return false;

}

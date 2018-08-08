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

#include "DropDatabase.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/VelocyPackHelper.h"
#include "RestServer/DatabaseFeature.h"
#include "VocBase/Methods/Databases.h"

using namespace arangodb::application_features;
using namespace arangodb::methods;
using namespace arangodb::maintenance;

DropDatabase::DropDatabase(
  MaintenanceFeature& feature, ActionDescription const& desc)
  : ActionBase(feature, desc) {

  if (!desc.has(DATABASE)) {
    LOG_TOPIC(ERR, Logger::MAINTENANCE)
      << "DropDatabase: database must be specified";
    fail();
  }
  TRI_ASSERT(desc.has(DATABASE));

}

DropDatabase::~DropDatabase() {};

bool DropDatabase::first() {

  ActionBase::first();

  auto const& database = _description.get(DATABASE);
  auto* systemVocbase =
    ApplicationServer::getFeature<DatabaseFeature>("Database")->systemDatabase();
  if (systemVocbase == nullptr) {
    LOG_TOPIC(FATAL, Logger::AGENCY) << "could not determine _system database";
    FATAL_ERROR_EXIT();
  }

  auto result = Databases::drop(systemVocbase, database);
  if (!result.ok()) {
    _result = result;
    LOG_TOPIC(ERR, Logger::AGENCY)
      << "DropDatabase: dropping database " << database << " failed: "
      << result.errorMessage();
    fail();
    return false;
  }
  
  complete();
  return false;

}

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

CreateDatabase::CreateDatabase(ActionDescription const& d) :
  ActionBase(d, arangodb::maintenance::FOREGROUND) {
  TRI_ASSERT(d.has("database"));
}

CreateDatabase::~CreateDatabase() {};

arangodb::Result CreateDatabase::run(
  std::chrono::duration<double> const&, bool& finished) {

  arangodb::Result res;
  VPackSlice users;
  auto const& database = _description.get(DATABASE);
  auto const& options = _description.properties();
  auto* systemVocbase =
    ApplicationServer::getFeature<DatabaseFeature>("Database")->systemDatabase();

  if (systemVocbase == nullptr) {
    LOG_TOPIC(FATAL, Logger::AGENCY) << "could not determine _system database";
    FATAL_ERROR_EXIT();
  }

  return Databases::create(database, users, options);

}

arangodb::Result CreateDatabase::kill(Signal const& signal) {
  arangodb::Result res;
  return res;
}

arangodb::Result CreateDatabase::progress(double& progress) {
  arangodb::Result res;
  return res;
}



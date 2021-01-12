////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
#include "Basics/StaticStrings.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/MaintenanceFeature.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "RestServer/DatabaseFeature.h"
#include "Utils/DatabaseGuard.h"
#include "Utils/ExecContext.h"
#include "Utils/OperationOptions.h"
#include "VocBase/Methods/Databases.h"

using namespace arangodb::application_features;
using namespace arangodb::methods;
using namespace arangodb::maintenance;
using namespace arangodb;

DropDatabase::DropDatabase(MaintenanceFeature& feature, ActionDescription const& desc)
    : ActionBase(feature, desc) {
  std::stringstream error;

  _labels.emplace(FAST_TRACK);

  if (!desc.has(DATABASE)) {
    error << "database must be specified";
  }
  TRI_ASSERT(desc.has(DATABASE));

  if (!error.str().empty()) {
    LOG_TOPIC("103f0", ERR, Logger::MAINTENANCE) << "DropDatabase: " << error.str();
    _result.reset(TRI_ERROR_INTERNAL, error.str());
    setState(FAILED);
  }
}

DropDatabase::~DropDatabase() = default;

bool DropDatabase::first() {
  std::string const database = _description.get(DATABASE);
  LOG_TOPIC("22779", DEBUG, Logger::MAINTENANCE) << "DropDatabase: dropping " << database;

  try {
    auto& df = _feature.server().getFeature<DatabaseFeature>();
    DatabaseGuard guard(df, StaticStrings::SystemDatabase);
    auto vocbase = &guard.database();

    _result = Databases::drop(ExecContext::current(), vocbase, database);
    if (!_result.ok() && _result.errorNumber() != TRI_ERROR_ARANGO_DATABASE_NOT_FOUND) {
      LOG_TOPIC("f46b7", ERR, Logger::AGENCY) << "DropDatabase: dropping database " << database
                                     << " failed: " << _result.errorMessage();
      return false;
    }
  } catch (std::exception const& e) {
    std::stringstream error;
    error << "action " << _description << " failed with exception " << e.what();
    _result.reset(TRI_ERROR_INTERNAL, error.str());
    return false;
  }

  return false;
}

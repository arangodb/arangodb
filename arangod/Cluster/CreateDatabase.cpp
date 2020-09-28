////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
#include "ActionBase.h"
#include "MaintenanceFeature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/StaticStrings.h"
#include "Basics/VelocyPackHelper.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "RestServer/DatabaseFeature.h"
#include "Utils/DatabaseGuard.h"
#include "Utils/OperationOptions.h"
#include "VocBase/Methods/Databases.h"

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::maintenance;
using namespace arangodb::methods;

CreateDatabase::CreateDatabase(MaintenanceFeature& feature, ActionDescription const& desc)
    : ActionBase(feature, desc) {
  std::stringstream error;

  _labels.emplace(FAST_TRACK);

  if (!desc.has(DATABASE)) {
    error << "database must be specified.";
  }
  TRI_ASSERT(desc.has(DATABASE));

  if (!error.str().empty()) {
    LOG_TOPIC("751ce", ERR, Logger::MAINTENANCE) << "CreateDatabase: " << error.str();
    _result.reset(TRI_ERROR_INTERNAL, error.str());
    setState(FAILED);
  }
}

CreateDatabase::~CreateDatabase() = default;

bool CreateDatabase::first() {
  VPackSlice users;
  auto database = _description.get(DATABASE);

  LOG_TOPIC("953b1", INFO, Logger::MAINTENANCE) << "CreateDatabase: creating database " << database;

  try {
    DatabaseGuard guard(StaticStrings::SystemDatabase);

    // Assertion in constructor makes sure that we have DATABASE.
    auto& server = _feature.server();
    _result = Databases::create(server, ExecContext::current(),
                                _description.get(DATABASE), users, properties());
    if (!_result.ok() && _result.errorNumber() != TRI_ERROR_ARANGO_DUPLICATE_NAME) {
      LOG_TOPIC("5fb67", ERR, Logger::MAINTENANCE)
          << "CreateDatabase: failed to create database " << database << ": " << _result;

      _feature.storeDBError(database, _result);
    } else {
      LOG_TOPIC("997c8", INFO, Logger::MAINTENANCE)
          << "CreateDatabase: database  " << database << " created";
    }
  } catch (std::exception const& e) {
    std::stringstream error;
    error << "action " << _description << " failed with exception " << e.what();
    LOG_TOPIC("fa073", ERR, Logger::MAINTENANCE) << "CreateDatabase: " << error.str();
    _result.reset(TRI_ERROR_INTERNAL, error.str());
    _feature.storeDBError(database, _result);
  }

  return false;
}

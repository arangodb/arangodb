////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include "CheckVersionFeature.h"

#include "Basics/FileUtils.h"
#include "Logger/Logger.h"
#include "Logger/LoggerFeature.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "Replication/ReplicationFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "Rest/Version.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "VocBase/vocbase.h"
#include "Basics/exitcodes.h"

#include <velocypack/Parser.h>
#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::basics;
using namespace arangodb::options;

CheckVersionFeature::CheckVersionFeature(
    ApplicationServer* server, int* result,
    std::vector<std::string> const& nonServerFeatures)
    : ApplicationFeature(server, "CheckVersion"),
      _checkVersion(false),
      _result(result),
      _nonServerFeatures(nonServerFeatures) {
  setOptional(false);
  requiresElevatedPrivileges(false);
  startsAfter("Database");
  startsAfter("StorageEngine");
  startsAfter("Logger");
  startsAfter("Replication");
}

void CheckVersionFeature::collectOptions(
    std::shared_ptr<ProgramOptions> options) {
  options->addSection("database", "Configure the database");

  options->addOldOption("check-version", "database.check-version");

  options->addHiddenOption("--database.check-version",
                           "checks the versions of the database and exit",
                           new BooleanParameter(&_checkVersion));
}

CheckVersionFeature::CheckVersionResult
CheckVersionFeature::checkVersionFileForDB(TRI_vocbase_t* vocbase,
                                           StorageEngine* engine,
                                           uint32_t currentVersion) const {
  std::string const versionFilePath = engine->versionFilename(vocbase->id());
  if (!FileUtils::exists(versionFilePath) ||
      !FileUtils::isRegularFile(versionFilePath)) {
    // File not existing
    LOG_TOPIC(DEBUG, arangodb::Logger::FIXME)
        << "version file (" << versionFilePath << ") not found";
    return NO_VERSION_FILE;
  }
  std::string const versionFileData = FileUtils::slurp(versionFilePath);
  LOG_TOPIC(DEBUG, arangodb::Logger::FIXME) << "found version file "
                                            << versionFileData;
  std::shared_ptr<VPackBuilder> versionBuilder;
  try {
    versionBuilder = VPackParser::fromJson(versionFileData);
  } catch (...) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "Cannot parse VERSION file '"
                                            << versionFilePath << "': '"
                                            << versionFileData << "'";
    return CANNOT_PARSE_VERSION_FILE;
  }
  VPackSlice version = versionBuilder->slice();
  if (!version.isObject() || !version.hasKey("version")) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "Cannot parse VERSION file '"
                                            << versionFilePath << "': '"
                                            << versionFileData << "'";
    return CANNOT_PARSE_VERSION_FILE;
  }
  version = version.get("version");
  if (!version.isNumber()) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "Cannot parse VERSION file '"
                                            << versionFilePath << "': '"
                                            << versionFileData << "'";
    return CANNOT_PARSE_VERSION_FILE;
  }
  uint32_t lastVersion = version.getNumericValue<uint32_t>();

  // Integer division
  if (lastVersion / 100 == currentVersion / 100) {
    LOG_TOPIC(DEBUG, arangodb::Logger::FIXME) << "version match: last version" << lastVersion << ", current version" << currentVersion;
    return VERSION_MATCH;
  }
  if (lastVersion > currentVersion) {
    LOG_TOPIC(DEBUG, arangodb::Logger::FIXME) << "downgrade: last version" << lastVersion << ", current version" << currentVersion;
    return DOWNGRADE_NEEDED;
  }
  TRI_ASSERT(lastVersion < currentVersion);
  LOG_TOPIC(DEBUG, arangodb::Logger::FIXME) << "upgrade: last version" << lastVersion << ", current version" << currentVersion;
  return UPGRADE_NEEDED;
}

void CheckVersionFeature::validateOptions(
    std::shared_ptr<ProgramOptions> options) {
  if (!_checkVersion) {
    return;
  }

  // This disables cluster feature, so the role will not be set
  ApplicationServer::forceDisableFeatures(_nonServerFeatures);
  // HardCode it to single server
  ServerState::instance()->setRole(ServerState::ROLE_SINGLE);

  LoggerFeature* logger =
      ApplicationServer::getFeature<LoggerFeature>("Logger");
  logger->disableThreaded();
  
  ReplicationFeature* replicationFeature =
      ApplicationServer::getFeature<ReplicationFeature>("Replication");
  replicationFeature->disableReplicationApplier();

  DatabaseFeature* databaseFeature =
      ApplicationServer::getFeature<DatabaseFeature>("Database");
  databaseFeature->enableCheckVersion();
}

void CheckVersionFeature::start() {
  if (!_checkVersion) {
    return;
  }

  // check the version
  if (DatabaseFeature::DATABASE->isInitiallyEmpty()) {
    *_result = EXIT_SUCCESS;
  } else {
    checkVersion();
  }

  // and force shutdown
  server()->beginShutdown();
  
  usleep(1 * 1000 * 1000);  
  TRI_EXIT_FUNCTION(EXIT_SUCCESS, nullptr);
}

void CheckVersionFeature::checkVersion() {
  *_result = 1;

  // run version check
  LOG_TOPIC(TRACE, arangodb::Logger::FIXME) << "starting version check";

  DatabaseFeature* databaseFeature = application_features::ApplicationServer::getFeature<DatabaseFeature>("Database");

  int32_t currentVersion = arangodb::rest::Version::getNumericServerVersion();
  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  auto checkDBVersion = [&] (TRI_vocbase_t* vocbase) {
    auto res = checkVersionFileForDB(vocbase, engine, currentVersion);
    switch (res) {
      case VERSION_MATCH:
        // all good
        break;
      case UPGRADE_NEEDED:
        // this is safe to do even if further databases will be checked
        // because we will never set the status back to success
        *_result = 3;
        LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "Database version check failed for '"
                   << vocbase->name() << "': upgrade needed";
        break;
      case DOWNGRADE_NEEDED:
        if (*_result == 1) {
          // this is safe to do even if further databases will be checked
          // because we will never set the status back to success
          *_result = 2;
          LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "Database version check failed for '"
                     << vocbase->name() << "': downgrade needed";
        }
        break;
      default:
        LOG_TOPIC(FATAL, arangodb::Logger::FIXME) << "Database version check failed for '"
                   << vocbase->name()
                   << "'. Please inspect the logs for any errors";
        FATAL_ERROR_EXIT_CODE(TRI_EXIT_VERSION_CHECK_FAILED);
    }
  };
  databaseFeature->enumerateDatabases(checkDBVersion);

  LOG_TOPIC(DEBUG, arangodb::Logger::FIXME) << "final result of version check: " << *_result;

  if (*_result == 1) {
    *_result = EXIT_SUCCESS;
  } else if (*_result > 1) {
    if (*_result == 2) {
      // downgrade needed
      LOG_TOPIC(FATAL, arangodb::Logger::FIXME) << "Database version check failed: downgrade needed";
      FATAL_ERROR_EXIT_CODE(TRI_EXIT_DOWNGRADE_REQUIRED);
    } else if (*_result == 3) {
      LOG_TOPIC(FATAL, arangodb::Logger::FIXME) << "Database version check failed: upgrade needed";
      FATAL_ERROR_EXIT_CODE(TRI_EXIT_UPGRADE_REQUIRED);
    } else {
      LOG_TOPIC(FATAL, arangodb::Logger::FIXME) << "Database version check failed";
      FATAL_ERROR_EXIT_CODE(TRI_EXIT_VERSION_CHECK_FAILED);
    }
    FATAL_ERROR_EXIT_CODE(*_result);
  }
}

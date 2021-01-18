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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include "CheckVersionFeature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "ApplicationFeatures/EnvironmentFeature.h"
#include "Basics/FileUtils.h"
#include "Basics/application-exit.h"
#include "Basics/exitcodes.h"
#include "Cluster/ServerState.h"
#include "FeaturePhases/BasicFeaturePhaseServer.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerFeature.h"
#include "Logger/LoggerStream.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "Replication/ReplicationFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/DatabasePathFeature.h"
#include "RestServer/ServerIdFeature.h"
#include "RestServer/SystemDatabaseFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "VocBase/Methods/Version.h"
#include "VocBase/vocbase.h"

using namespace arangodb::application_features;
using namespace arangodb::basics;
using namespace arangodb::options;

namespace arangodb {

CheckVersionFeature::CheckVersionFeature(application_features::ApplicationServer& server,
                                         int* result,
                                         std::vector<std::type_index> const& nonServerFeatures)
    : ApplicationFeature(server, "CheckVersion"),
      _checkVersion(false),
      _result(result),
      _nonServerFeatures(nonServerFeatures) {
  setOptional(false);
  startsAfter<BasicFeaturePhaseServer>();

  startsAfter<DatabaseFeature>();
  startsAfter<DatabasePathFeature>();
  startsAfter<EngineSelectorFeature>();
  startsAfter<ServerIdFeature>();
  startsAfter<SystemDatabaseFeature>();
}

void CheckVersionFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addSection("database", "Configure the database");

  options->addOldOption("check-version", "database.check-version");

  options->addOption("--database.check-version",
                     "checks the versions of the database and exit",
                     new BooleanParameter(&_checkVersion),
                     arangodb::options::makeDefaultFlags(arangodb::options::Flags::Hidden,
                                                  arangodb::options::Flags::Command));
}

void CheckVersionFeature::validateOptions(std::shared_ptr<ProgramOptions> options) {
  if (!_checkVersion) {
    return;
  }

  // hard-code our role to a single server instance, because
  // noone else will set our role
  ServerState::instance()->setRole(ServerState::ROLE_SINGLE);

  server().forceDisableFeatures(_nonServerFeatures);

  LoggerFeature& logger = server().getFeature<LoggerFeature>();
  logger.disableThreaded();

  ReplicationFeature& replicationFeature = server().getFeature<ReplicationFeature>();
  replicationFeature.disableReplicationApplier();

  DatabaseFeature& databaseFeature = server().getFeature<DatabaseFeature>();
  databaseFeature.enableCheckVersion();

  // we can turn off all warnings about environment here, because they
  // wil show up on a regular start later anyway
  server().disableFeatures(
      std::vector<std::type_index>{std::type_index(typeid(EnvironmentFeature))});
}

void CheckVersionFeature::start() {
  if (!_checkVersion) {
    return;
  }

  // check the version
  if (server().getFeature<DatabaseFeature>().isInitiallyEmpty()) {
    LOG_TOPIC("e9df6", TRACE, arangodb::Logger::STARTUP)
        << "skipping version check because database directory was initially "
           "empty";
    *_result = EXIT_SUCCESS;
  } else {
    checkVersion();
  }

  // and force shutdown
  server().beginShutdown();

  std::this_thread::sleep_for(std::chrono::seconds(1));
  TRI_EXIT_FUNCTION(EXIT_SUCCESS, nullptr);
}

void CheckVersionFeature::checkVersion() {
  *_result = 1;

  // run version check
  LOG_TOPIC("449fd", TRACE, arangodb::Logger::STARTUP) << "starting version check";

  DatabasePathFeature& databasePathFeature = server().getFeature<DatabasePathFeature>();

  LOG_TOPIC("73006", TRACE, arangodb::Logger::STARTUP)
      << "database path is: '" << databasePathFeature.directory() << "'";

  // can do this without a lock as this is the startup
  DatabaseFeature& databaseFeature = server().getFeature<DatabaseFeature>();

  bool ignoreDatafileErrors = databaseFeature.ignoreDatafileErrors();

  // iterate over all databases
  for (auto& name : databaseFeature.getDatabaseNames()) {
    TRI_vocbase_t* vocbase = databaseFeature.lookupDatabase(name);
    methods::VersionResult res = methods::Version::check(vocbase);
    TRI_ASSERT(vocbase != nullptr);

    if (res.status == methods::VersionResult::CANNOT_PARSE_VERSION_FILE ||
        res.status == methods::VersionResult::CANNOT_READ_VERSION_FILE) {
      if (ignoreDatafileErrors) {
        // try to install a fresh new, empty VERSION file instead
        if (methods::Version::write(vocbase, std::map<std::string, bool>(), true)
                .ok()) {
          // give it another try
          res = methods::Version::check(vocbase);
        }
      } else {
        LOG_TOPIC("ecd13", WARN, Logger::STARTUP)
            << "in order to automatically fix the VERSION file on startup, "
            << "please start the server with option "
               "`--database.ignore-datafile-errors true`";
      }
    } else if (res.status == methods::VersionResult::NO_VERSION_FILE) {
      // try to install a fresh new, empty VERSION file instead
      if (methods::Version::write(vocbase, std::map<std::string, bool>(), true).ok()) {
        // give it another try
        res = methods::Version::check(vocbase);
      }
    }

    LOG_TOPIC("53cbb", DEBUG, Logger::STARTUP) << "version check return status " << res.status;
    if (res.status < 0) {
      LOG_TOPIC("52f16", FATAL, arangodb::Logger::FIXME)
          << "Database version check failed for '" << vocbase->name()
          << "'. Please inspect the logs for any errors. If there are no "
             "obvious issues in the logs, please retry with option "
             "`--log.level startup=trace`";
      FATAL_ERROR_EXIT_CODE(TRI_EXIT_VERSION_CHECK_FAILED);
    } else if (res.status == methods::VersionResult::DOWNGRADE_NEEDED) {
      // this is safe to do even if further databases will be checked
      // because we will never set the status back to success
      *_result = 3;
      LOG_TOPIC("ef6ca", WARN, arangodb::Logger::FIXME)
          << "Database version check failed for '" << vocbase->name()
          << "': downgrade needed";
    } else if (res.status == methods::VersionResult::UPGRADE_NEEDED && *_result == 1) {
      // this is safe to do even if further databases will be checked
      // because we will never set the status back to success
      *_result = 2;
      LOG_TOPIC("b7514", WARN, arangodb::Logger::FIXME)
          << "Database version check failed for '" << vocbase->name()
          << "': upgrade needed";
    }
  }

  LOG_TOPIC("382bb", DEBUG, Logger::STARTUP) << "final result of version check: " << *_result;

  if (*_result == 1) {
    *_result = EXIT_SUCCESS;
  } else if (*_result > 1) {
    if (*_result == 3) {
      // downgrade needed
      LOG_TOPIC("290c2", FATAL, Logger::FIXME)
          << "Database version check failed: downgrade needed";
      FATAL_ERROR_EXIT_CODE(TRI_EXIT_DOWNGRADE_REQUIRED);
    } else if (*_result == 2) {
      LOG_TOPIC("40e37", FATAL, Logger::FIXME)
          << "Database version check failed: upgrade needed";
      FATAL_ERROR_EXIT_CODE(TRI_EXIT_UPGRADE_REQUIRED);
    } else {
      LOG_TOPIC("13e92", FATAL, Logger::FIXME) << "Database version check failed";
      FATAL_ERROR_EXIT_CODE(TRI_EXIT_VERSION_CHECK_FAILED);
    }
    FATAL_ERROR_EXIT_CODE(*_result);
  }
}

}  // namespace arangodb

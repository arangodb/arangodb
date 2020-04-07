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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "LockfileFeature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/Exceptions.h"
#include "Basics/FileUtils.h"
#include "Basics/application-exit.h"
#include "Basics/exitcodes.h"
#include "Basics/files.h"
#include "FeaturePhases/BasicFeaturePhaseServer.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "RestServer/DatabasePathFeature.h"

using namespace arangodb::basics;

namespace arangodb {

LockfileFeature::LockfileFeature(application_features::ApplicationServer& server)
    : ApplicationFeature(server, "Lockfile") {
  setOptional(false);
  startsAfter<application_features::BasicFeaturePhaseServer>();
}

void LockfileFeature::start() {
  // build lockfile name
  auto& database = server().getFeature<DatabasePathFeature>();
  _lockFilename = database.subdirectoryName("LOCK");

  TRI_ASSERT(!_lockFilename.empty());

  int res = TRI_VerifyLockFile(_lockFilename.c_str());

  if (res != TRI_ERROR_NO_ERROR) {
    std::string otherPID;
    try {
      otherPID = FileUtils::slurp(_lockFilename);
    } catch (...) {
    }
    if (otherPID.empty()) {
      LOG_TOPIC("5e4c0", FATAL, arangodb::Logger::FIXME)
          << "failed to read/write lockfile, please check the file permissions "
             "of the lockfile '"
          << _lockFilename << "'";
    } else {
      LOG_TOPIC("1f4eb", FATAL, arangodb::Logger::FIXME)
          << "database is locked by process " << otherPID
          << "; please stop it first and check that the lockfile '" << _lockFilename
          << "' goes away. If you are sure no other arangod process is "
             "running, please remove the lockfile '"
          << _lockFilename << "' and try again";
    }
    FATAL_ERROR_EXIT_CODE(TRI_EXIT_COULD_NOT_LOCK);
  }

  if (TRI_ExistsFile(_lockFilename.c_str())) {
    res = TRI_UnlinkFile(_lockFilename.c_str());

    if (res != TRI_ERROR_NO_ERROR) {
      LOG_TOPIC("ffea2", FATAL, arangodb::Logger::FIXME)
          << "failed to remove an abandoned lockfile in the database "
             "directory, please check the file permissions of the lockfile '"
          << _lockFilename << "': " << TRI_errno_string(res);
      FATAL_ERROR_EXIT_CODE(TRI_EXIT_COULD_NOT_LOCK);
    }
  }
  res = TRI_CreateLockFile(_lockFilename.c_str());

  if (res != TRI_ERROR_NO_ERROR) {
    LOG_TOPIC("c2704", FATAL, arangodb::Logger::FIXME)
        << "failed to lock the database directory using '" << _lockFilename
        << "': " << TRI_errno_string(res);
    FATAL_ERROR_EXIT_CODE(TRI_EXIT_COULD_NOT_LOCK);
  }

  auto cleanup = std::make_unique<CleanupFunctions::CleanupFunction>(
      [&](int code, void* data) { TRI_DestroyLockFile(_lockFilename.c_str()); });
  CleanupFunctions::registerFunction(std::move(cleanup));
}

void LockfileFeature::unprepare() {
  TRI_DestroyLockFile(_lockFilename.c_str());
}

}  // namespace arangodb

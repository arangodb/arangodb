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
#include "Basics/Exceptions.h"
#include "Basics/FileUtils.h"
#include "Basics/files.h"
#include "RestServer/DatabasePathFeature.h"

using namespace arangodb;
using namespace arangodb::basics;

LockfileFeature::LockfileFeature(
    application_features::ApplicationServer* server)
    : ApplicationFeature(server, "Lockfile") {
  setOptional(false);
  requiresElevatedPrivileges(false);
  startsAfter("DatabasePath");
}

void LockfileFeature::start() {
  // build lockfile name
  auto database = application_features::ApplicationServer::getFeature<DatabasePathFeature>("DatabasePath");
  _lockFilename = database->subdirectoryName("LOCK");

  TRI_ASSERT(!_lockFilename.empty());

  int res = TRI_VerifyLockFile(_lockFilename.c_str());

  if (res != TRI_ERROR_NO_ERROR) {
    LOG(FATAL) << "database is locked, please check the lock file '" << _lockFilename << "'";
    FATAL_ERROR_EXIT();
  }
  
  if (TRI_ExistsFile(_lockFilename.c_str())) {
    TRI_UnlinkFile(_lockFilename.c_str());
  }
  
  if (res != TRI_ERROR_NO_ERROR) {
    LOG(FATAL)
        << "cannot lock the database directory, please check the lock file '"
        << _lockFilename << "': " << TRI_errno_string(res);
    FATAL_ERROR_EXIT();
  }
  
  res = TRI_CreateLockFile(_lockFilename.c_str());

  if (res != TRI_ERROR_NO_ERROR) {
    LOG(FATAL)
        << "cannot lock the database directory, please check the lock file '"
        << _lockFilename << "': " << TRI_errno_string(res);
    FATAL_ERROR_EXIT();
  }
}

void LockfileFeature::unprepare() {
  TRI_DestroyLockFile(_lockFilename.c_str());
}

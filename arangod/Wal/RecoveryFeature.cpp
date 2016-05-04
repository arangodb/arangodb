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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "RecoveryFeature.h"

#include "Basics/Exceptions.h"
#include "Logger/Logger.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "Wal/LogfileManager.h"

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::basics;
using namespace arangodb::options;
using namespace arangodb::wal;

RecoveryFeature::RecoveryFeature(ApplicationServer* server)
    : ApplicationFeature(server, "Recovery") {

  setOptional(false);
  requiresElevatedPrivileges(false);
  startsAfter("Database"); 
  startsAfter("LogfileManager");
#ifdef ARANGODB_ENABLE_ROCKSDB
  startsAfter("RocksDB");
#endif
}

/// @brief run the recovery procedure
/// this is called after the logfiles have been scanned completely and
/// recovery state has been build. additionally, all databases have been
/// opened already so we can use collections
void RecoveryFeature::start() {
  LogfileManager* logfileManager = ApplicationServer::getFeature<LogfileManager>("LogfileManager");

  TRI_ASSERT(!logfileManager->allowWrites());

  int res = logfileManager->runRecovery();
  
  if (res != TRI_ERROR_NO_ERROR) {
    LOG(FATAL) << "unable to finish WAL recovery: " << TRI_errno_string(res);
    FATAL_ERROR_EXIT();
  }

  if (!logfileManager->open()) {
    // if we got here, the LogfileManager has already logged a fatal error and we can simply abort
    FATAL_ERROR_EXIT();
  }
}


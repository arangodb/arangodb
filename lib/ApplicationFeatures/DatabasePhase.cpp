////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "DatabasePhase.h"

namespace arangodb {
namespace application_features {

DatabaseFeaturePhase::DatabaseFeaturePhase(ApplicationServer& server)
    : ApplicationFeaturePhase(server, "DatabasePhase") {
  setOptional(false);
  startsAfter("BasicsPhase");

  startsAfter("Authentication");
  startsAfter("CacheManager");
  startsAfter("CheckVersion");
  startsAfter("Database");
  startsAfter("EngineSelector");
  startsAfter("Flush");
  startsAfter("InitDatabase");
#ifdef USE_ENTERPRISE
  startsAfter("Ldap");
#endif
  startsAfter("Lockfile");
  startsAfter("MMFilesCompaction");
  startsAfter("MMFilesEngine");
  startsAfter("MMFilesLogfileManager");
  startsAfter("MMFilesPersistentIndex");
  startsAfter("MMFilesWalRecovery");
  startsAfter("Replication");
  startsAfter("RocksDBEngine");
  startsAfter("RocksDBOption");
  startsAfter("RocksDBRecoveryManager");
  startsAfter("ServerId");
  startsAfter("StorageEngine");
  startsAfter("SystemDatabase");
  startsAfter("TransactionManager");
  startsAfter("ViewTypes");
}

} // application_features
} // arangodb

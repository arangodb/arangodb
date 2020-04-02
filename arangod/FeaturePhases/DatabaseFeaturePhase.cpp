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

#include "DatabaseFeaturePhase.h"

#include "Cache/CacheManagerFeature.h"
#include "FeaturePhases/BasicFeaturePhaseServer.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "Replication/ReplicationFeature.h"
#include "RestServer/CheckVersionFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/FlushFeature.h"
#include "RestServer/InitDatabaseFeature.h"
#include "RestServer/LockfileFeature.h"
#include "RestServer/ServerIdFeature.h"
#include "RestServer/SystemDatabaseFeature.h"
#include "RestServer/ViewTypesFeature.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "RocksDBEngine/RocksDBRecoveryManager.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/RocksDBOptionFeature.h"
#include "StorageEngine/StorageEngineFeature.h"
#include "Transaction/ManagerFeature.h"

#ifdef USE_ENTERPRISE
#include "Enterprise/Ldap/LdapFeature.h"
#endif

namespace arangodb {
namespace application_features {

DatabaseFeaturePhase::DatabaseFeaturePhase(ApplicationServer& server)
    : ApplicationFeaturePhase(server, "DatabasePhase") {
  setOptional(false);
  startsAfter<BasicFeaturePhaseServer>();

  startsAfter<AuthenticationFeature>();
  startsAfter<CacheManagerFeature>();
  startsAfter<CheckVersionFeature>();
  startsAfter<DatabaseFeature>();
  startsAfter<EngineSelectorFeature>();
  startsAfter<FlushFeature>();
  startsAfter<InitDatabaseFeature>();
  startsAfter<LockfileFeature>();
  startsAfter<ReplicationFeature>();
  startsAfter<RocksDBEngine>();
  startsAfter<RocksDBOptionFeature>();
  startsAfter<RocksDBRecoveryManager>();
  startsAfter<ServerIdFeature>();
  startsAfter<StorageEngineFeature>();
  startsAfter<SystemDatabaseFeature>();
  startsAfter<transaction::ManagerFeature>();
  startsAfter<ViewTypesFeature>();

#ifdef USE_ENTERPRISE
  startsAfter<LdapFeature>();
#endif
}

}  // namespace application_features
}  // namespace arangodb

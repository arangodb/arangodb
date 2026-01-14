////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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
#include "GeneralServer/AuthenticationFeature.h"
#include "Replication/ReplicationFeature.h"
#include "RestServer/CheckVersionFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/FlushFeature.h"
#include "RestServer/InitDatabaseFeature.h"
#include "RestServer/LockfileFeature.h"
#include "RestServer/ServerIdFeature.h"
#include "RestServer/SystemDatabaseFeature.h"
#include "RestServer/VectorIndexFeature.h"
#include "RestServer/ViewTypesFeature.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "RocksDBEngine/RocksDBRecoveryManager.h"
#include "StorageEngine/StorageEngineFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "Transaction/ManagerFeature.h"

namespace arangodb::application_features {

DatabaseFeaturePhase::DatabaseFeaturePhase(ArangodServer& server)
    : ApplicationFeaturePhase{server, *this} {
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
  startsAfter<RocksDBRecoveryManager>();
  startsAfter<ServerIdFeature>();
  startsAfter<StorageEngineFeature>();
  startsAfter<SystemDatabaseFeature>();
  startsAfter<transaction::ManagerFeature>();
  startsAfter<ViewTypesFeature>();
  startsAfter<VectorIndexFeature>();
}

}  // namespace arangodb::application_features

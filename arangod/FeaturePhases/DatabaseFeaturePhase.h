////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "ApplicationFeatures/ApplicationFeaturePhase.h"

namespace arangodb {

class AuthenticationFeature;
class CacheManagerFeature;
class CheckVersionFeature;
class DatabaseFeature;
class EngineSelectorFeature;
class FlushFeature;
class InitDatabaseFeature;
class LockfileFeature;
class ReplicationFeature;
class RocksDBEngine;
class RocksDBRecoveryManager;
class ServerIdFeature;
class StorageEngineFeature;
class SystemDatabaseFeature;
class ViewTypesFeature;
#ifdef USE_ENTERPRISE
class LdapFeature;
#endif

namespace transaction {
class ManagerFeature;
}
namespace application_features {

class BasicFeaturePhaseServer;

class DatabaseFeaturePhase : public ApplicationFeaturePhase {
 public:
  static constexpr std::string_view name() noexcept { return "DatabasePhase"; }

  template<typename Server>
  explicit DatabaseFeaturePhase(Server& server)
      : ApplicationFeaturePhase(
            server, Server::template id<DatabaseFeaturePhase>(), name()) {
    setOptional(false);
    startsAfter<BasicFeaturePhaseServer, Server>();

    startsAfter<AuthenticationFeature, Server>();
    startsAfter<CacheManagerFeature, Server>();
    startsAfter<CheckVersionFeature, Server>();
    startsAfter<DatabaseFeature, Server>();
    startsAfter<EngineSelectorFeature, Server>();
    startsAfter<FlushFeature, Server>();
    startsAfter<InitDatabaseFeature, Server>();
    startsAfter<LockfileFeature, Server>();
    startsAfter<ReplicationFeature, Server>();
    startsAfter<RocksDBEngine, Server>();
    startsAfter<RocksDBRecoveryManager, Server>();
    startsAfter<ServerIdFeature, Server>();
    startsAfter<StorageEngineFeature, Server>();
    startsAfter<SystemDatabaseFeature, Server>();
    startsAfter<transaction::ManagerFeature, Server>();
    startsAfter<ViewTypesFeature, Server>();

#ifdef USE_ENTERPRISE
    startsAfter<LdapFeature, Server>();
#endif
  }
};

}  // namespace application_features
}  // namespace arangodb

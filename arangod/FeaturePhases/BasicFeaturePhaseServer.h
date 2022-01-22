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
namespace application_features {

class GreetingsFeaturePhase;
class DaemonFeature;
class DatabasePathFeature;
class EnvironmentFeature;
class FileDescriptorsFeature;
class LanguageFeature;
class MaxMapCountFeature;
class NonceFeature;
class PrivilegeFeature;
class SchedulerFeature;
class ShardingFeature;
class SslFeature;
class SupervisorFeature;
class TempFeature;
#ifdef _WIN32
class WindowsServiceFeature;
#endif

#ifdef USE_ENTERPRISE
class AuditFeature;
class EncryptionFeature;
#endif

class BasicFeaturePhaseServer : public ApplicationFeaturePhase {
 public:
  static constexpr std::string_view name() noexcept { return "BasicsPhase"; }

  template<typename Server>
  explicit BasicFeaturePhaseServer(Server& server)
      : ApplicationFeaturePhase(
            server, Server::template id<BasicFeaturePhaseServer>(), name()) {
    setOptional(false);
    startsAfter<GreetingsFeaturePhase, Server>();

    startsAfter<DaemonFeature, Server>();
    startsAfter<DatabasePathFeature, Server>();
    startsAfter<EnvironmentFeature, Server>();
#ifdef TRI_HAVE_GETRLIMIT
    startsAfter<FileDescriptorsFeature, Server>();
#endif
    startsAfter<LanguageFeature, Server>();
    startsAfter<MaxMapCountFeature, Server>();
    startsAfter<NonceFeature, Server>();
    startsAfter<PrivilegeFeature, Server>();
    startsAfter<SchedulerFeature, Server>();
    startsAfter<ShardingFeature, Server>();
    startsAfter<SslFeature, Server>();
    startsAfter<SupervisorFeature, Server>();
    startsAfter<TempFeature, Server>();

#ifdef _WIN32
    startsAfter<WindowsServiceFeature, Server>();
#endif

#ifdef USE_ENTERPRISE
    startsAfter<AuditFeature, Server>();
    startsAfter<EncryptionFeature, Server>();
#endif
  }
};

}  // namespace application_features
}  // namespace arangodb

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

#include "BasicFeaturePhaseServer.h"
#include "ApplicationFeatures/ApplicationServer.h"

namespace arangodb::application_features {

BasicFeaturePhaseServer::BasicFeaturePhaseServer(ArangodServer& server)
    : ApplicationFeaturePhase{server, *this} {
  setOptional(false);
  startsAfter<GreetingsFeaturePhase, ArangodServer>();

  if constexpr (ArangodServer::contains<DaemonFeature>()) {
    startsAfter<DaemonFeature, ArangodServer>();
  }
  if constexpr (ArangodServer::contains<SupervisorFeature>()) {
    startsAfter<SupervisorFeature, ArangodServer>();
  }
  startsAfter<CpuUsageFeature, ArangodServer>();
  startsAfter<DatabasePathFeature, ArangodServer>();
  startsAfter<EnvironmentFeature, ArangodServer>();
  startsAfter<LanguageFeature, ArangodServer>();
  startsAfter<MaxMapCountFeature, ArangodServer>();
  startsAfter<NonceFeature, ArangodServer>();
  startsAfter<PrivilegeFeature, ArangodServer>();
  startsAfter<SchedulerFeature, ArangodServer>();
  startsAfter<SharedPRNGFeature, ArangodServer>();
  startsAfter<ShardingFeature, ArangodServer>();
  startsAfter<SslFeature, ArangodServer>();
  startsAfter<TempFeature, ArangodServer>();

  if constexpr (ArangodServer::contains<FileDescriptorsFeature>()) {
    startsAfter<FileDescriptorsFeature, ArangodServer>();
  }
  if constexpr (ArangodServer::contains<AuditFeature>()) {
    startsAfter<AuditFeature, ArangodServer>();
  }
  if constexpr (ArangodServer::contains<EncryptionFeature>()) {
    startsAfter<EncryptionFeature, ArangodServer>();
  }
}

}  // namespace arangodb::application_features

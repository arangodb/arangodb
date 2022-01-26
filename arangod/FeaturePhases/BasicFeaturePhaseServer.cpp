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

#include "BasicFeaturePhaseServer.h"
#include "ApplicationFeatures/ApplicationServer.h"

namespace arangodb::application_features {

BasicFeaturePhaseServer::BasicFeaturePhaseServer(ArangodServer& server)
    : ApplicationFeaturePhase{server, *this} {
  setOptional(false);
  startsAfter<GreetingsFeaturePhase, ArangodServer>();

  startsAfter<DaemonFeature, ArangodServer>();
  startsAfter<DatabasePathFeature, ArangodServer>();
  startsAfter<EnvironmentFeature, ArangodServer>();
#ifdef TRI_HAVE_GETRLIMIT
  startsAfter<FileDescriptorsFeature, ArangodServer>();
#endif
  startsAfter<LanguageFeature, ArangodServer>();
  startsAfter<MaxMapCountFeature, ArangodServer>();
  startsAfter<NonceFeature, ArangodServer>();
  startsAfter<PrivilegeFeature, ArangodServer>();
  startsAfter<SchedulerFeature, ArangodServer>();
  startsAfter<ShardingFeature, ArangodServer>();
  startsAfter<SslFeature, ArangodServer>();
  startsAfter<SupervisorFeature, ArangodServer>();
  startsAfter<TempFeature, ArangodServer>();

#ifdef _WIN32
  startsAfter<WindowsServiceFeature, ArangodServer>();
#endif

#ifdef USE_ENTERPRISE
  startsAfter<AuditFeature, ArangodServer>();
  startsAfter<EncryptionFeature, ArangodServer>();
#endif
}

}  // namespace arangodb::application_features

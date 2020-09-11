////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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

#include "ApplicationFeatures/DaemonFeature.h"
#include "ApplicationFeatures/EnvironmentFeature.h"
#include "ApplicationFeatures/GreetingsFeaturePhase.h"
#include "ApplicationFeatures/LanguageFeature.h"
#include "ApplicationFeatures/MaxMapCountFeature.h"
#include "ApplicationFeatures/NonceFeature.h"
#include "ApplicationFeatures/PrivilegeFeature.h"
#include "ApplicationFeatures/SupervisorFeature.h"
#include "ApplicationFeatures/TempFeature.h"
#include "RestServer/DatabasePathFeature.h"
#include "RestServer/FileDescriptorsFeature.h"
#include "Scheduler/SchedulerFeature.h"
#include "Sharding/ShardingFeature.h"
#include "Ssl/SslFeature.h"

#ifdef _WIN32
#include "ApplicationFeatures/WindowsServiceFeature.h"
#endif

#ifdef USE_ENTERPRISE
#include "Enterprise/Audit/AuditFeature.h"
#include "Enterprise/Encryption/EncryptionFeature.h"
#endif

namespace arangodb {
namespace application_features {

BasicFeaturePhaseServer::BasicFeaturePhaseServer(ApplicationServer& server)
    : ApplicationFeaturePhase(server, "BasicsPhase") {
  setOptional(false);
  startsAfter<GreetingsFeaturePhase>();

  startsAfter<DaemonFeature>();
  startsAfter<DatabasePathFeature>();
  startsAfter<EnvironmentFeature>();
  startsAfter<FileDescriptorsFeature>();
  startsAfter<LanguageFeature>();
  startsAfter<MaxMapCountFeature>();
  startsAfter<NonceFeature>();
  startsAfter<PrivilegeFeature>();
  startsAfter<SchedulerFeature>();
  startsAfter<ShardingFeature>();
  startsAfter<SslFeature>();
  startsAfter<SupervisorFeature>();
  startsAfter<TempFeature>();

#ifdef _WIN32
  startsAfter<WindowsServiceFeature>();
#endif

#ifdef USE_ENTERPRISE
  startsAfter<AuditFeature>();
  startsAfter<EncryptionFeature>();
#endif
}

}  // namespace application_features
}  // namespace arangodb

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

#include "ApplicationFeatures/GreetingsFeaturePhase.h"
#include "ApplicationFeatures/LanguageFeature.h"
#include "ApplicationFeatures/TempFeature.h"
#include "RestServer/CpuUsageFeature.h"
#include "RestServer/DaemonFeature.h"
#include "RestServer/DatabasePathFeature.h"
#include "RestServer/EnvironmentFeature.h"
#include "RestServer/FileDescriptorsFeature.h"
#include "RestServer/MaxMapCountFeature.h"
#include "RestServer/NonceFeature.h"
#include "RestServer/PrivilegeFeature.h"
#include "RestServer/SharedPRNGFeature.h"
#include "RestServer/SupervisorFeature.h"
#include "Scheduler/SchedulerFeature.h"
#include "Sharding/ShardingFeature.h"
#include "Ssl/SslFeature.h"

#ifdef USE_ENTERPRISE
#include "Enterprise/Audit/AuditFeature.h"
#include "Enterprise/Encryption/EncryptionFeature.h"
#endif

namespace arangodb::application_features {

BasicFeaturePhaseServer::BasicFeaturePhaseServer(ArangodServer& server)
    : ApplicationFeaturePhase{server, *this} {
  setOptional(false);
  startsAfter<GreetingsFeaturePhase>();

  if (server.hasFeature<DaemonFeature>()) {
    startsAfter<DaemonFeature>();
  }
  if (server.hasFeature<SupervisorFeature>()) {
    startsAfter<SupervisorFeature>();
  }
  startsAfter<CpuUsageFeature>();
  startsAfter<DatabasePathFeature>();
  startsAfter<EnvironmentFeature>();
  startsAfter<LanguageFeature>();
  startsAfter<MaxMapCountFeature>();
  startsAfter<NonceFeature>();
  startsAfter<PrivilegeFeature>();
  startsAfter<SchedulerFeature>();
  startsAfter<SharedPRNGFeature>();
  startsAfter<ShardingFeature>();
  startsAfter<SslFeature>();
  startsAfter<TempFeature>();

  if (server.hasFeature<FileDescriptorsFeature>()) {
    startsAfter<FileDescriptorsFeature>();
  }
  if (server.hasFeature<AuditFeature>()) {
    startsAfter<AuditFeature>();
  }
  if (server.hasFeature<EncryptionFeature>()) {
    startsAfter<EncryptionFeature>();
  }
}

}  // namespace arangodb::application_features

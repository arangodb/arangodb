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

#include "BasicPhase.h"

using namespace arangodb;
using namespace arangodb::application_features;

BasicFeaturePhase::BasicFeaturePhase(ApplicationServer* server)
    : ApplicationFeaturePhase(server, "BasicsPhase") {
  setOptional(false);
  startsAfter("Audit");
  startsAfter("Config");
  startsAfter("Daemon");
  startsAfter("DatabasePath");
  startsAfter("Environment");
  startsAfter("FileDescriptors");
  startsAfter("Greetings");
  startsAfter("Jemalloc");
  startsAfter("Language");
  startsAfter("LoggerBuffer");
  startsAfter("Logger");
  startsAfter("MaxMapCount");
  startsAfter("Nonce");
  startsAfter("PageSize");
  startsAfter("Privilege");
  startsAfter("Random");
  startsAfter("Scheduler");
  startsAfter("ServerId");
  startsAfter("ShellColors");
  startsAfter("Ssl");
  startsAfter("Supervisor");
  startsAfter("Temp");
  startsAfter("Version");
  startsAfter("WindowsService");
  startsAfter("Workmonitor");
}

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

namespace arangodb {
namespace application_features {

BasicFeaturePhase::BasicFeaturePhase(ApplicationServer& server, bool isClient)
    : ApplicationFeaturePhase(server, "BasicsPhase") {
  setOptional(false);
  startsAfter("GreetingsPhase");
  startsAfter("Sharding");

#ifdef USE_ENTERPRISE
  startsAfter("Encryption");
#endif
  startsAfter("Ssl");

  if (isClient) {
    startsAfter("Client");
  }

  if (!isClient) {
    startsAfter("Audit");
    startsAfter("Daemon");
    startsAfter("DatabasePath");
    startsAfter("Environment");
    startsAfter("FileDescriptors");
    startsAfter("Language");
    startsAfter("MaxMapCount");
    startsAfter("Nonce");
    startsAfter("PageSize");
    startsAfter("Privilege");
    startsAfter("Scheduler");
    startsAfter("Supervisor");
    startsAfter("Temp");
    startsAfter("WindowsService");
  }
}

} // application_features
} // arangodb

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "GreetingsFeature.h"
#include "Rest/Version.h"

using namespace arangodb;

GreetingsFeature::GreetingsFeature(
    application_features::ApplicationServer* server, char const* progname)
    : ApplicationFeature(server, "Greetings"), _progname(progname) {
  setOptional(false);
  requiresElevatedPrivileges(false);
  startsAfter("Logger");
}

void GreetingsFeature::prepare() {
  if (strcmp(_progname, "arangod") == 0) {
    LOG(INFO) << "" << rest::Version::getVerboseVersionString();
  }
}

void GreetingsFeature::unprepare() {
  if (strcmp(_progname, "arangod") == 0) {
    LOG(INFO) << "ArangoDB has been shut down";
  }
}

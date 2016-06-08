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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include "ApplicationFeatures/ShutdownFeature.h"

#include "Logger/Logger.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"

using namespace arangodb;
using namespace arangodb::options;

ShutdownFeature::ShutdownFeature(
    application_features::ApplicationServer* server,
    std::vector<std::string> const& features)
    : ApplicationFeature(server, "Shutdown") {
  setOptional(true);
  requiresElevatedPrivileges(false);
  startsAfter("Logger");

  for (auto feature : features) {
    if (feature != "Logger") {
      startsAfter(feature);
    }
  }
}

void ShutdownFeature::start() { server()->beginShutdown(); }

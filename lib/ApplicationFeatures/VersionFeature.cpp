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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "ApplicationFeatures/VersionFeature.h"

#include "ApplicationFeatures/ShellColorsFeature.h"
#include "ProgramOptions/ProgramOptions.h"
#include "Rest/Version.h"

#include <iostream>

using namespace arangodb::rest;
using namespace arangodb::options;

namespace arangodb {

VersionFeature::VersionFeature(application_features::ApplicationServer& server)
    : ApplicationFeature(server, "Version"), _printVersion(false) {
  setOptional(false);

  startsAfter<ShellColorsFeature>();
}

void VersionFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addOption("--version", "reports the version and exits",
                     new BooleanParameter(&_printVersion),
                     arangodb::options::makeDefaultFlags(arangodb::options::Flags::Command));
}

void VersionFeature::validateOptions(std::shared_ptr<ProgramOptions>) {
  if (_printVersion) {
    std::cout << Version::getServerVersion() << std::endl
              << std::endl
              << Version::getDetailed() << std::endl;
    exit(EXIT_SUCCESS);
  }
}

}  // namespace arangodb

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

#include "ApplicationFeatures/TempFeature.h"

#include "Basics/files.h"
#include "Logger/Logger.h"
#include "ProgramOptions2/ProgramOptions.h"
#include "ProgramOptions2/Section.h"

using namespace arangodb;
using namespace arangodb::options;

TempFeature::TempFeature(application_features::ApplicationServer* server,
                         std::string const& appname)
    : ApplicationFeature(server, "Temp"), _path(), _appname(appname) {
  setOptional(false);
  requiresElevatedPrivileges(false);
  startsAfter("Logger");
}

void TempFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  LOG_TOPIC(TRACE, Logger::STARTUP) << name() << "::collectOptions";

  options->addSection(Section("temp", "Configure the temporary files",
                              "temp options", false, false));

  options->addOption("--temp.path", "path for temporary files",
                     new StringParameter(&_path));
}

void TempFeature::prepare() {
  LOG_TOPIC(TRACE, Logger::STARTUP) << name() << "::prepare";

  TRI_SetApplicationName(_appname.c_str());
}

void TempFeature::start() {
  LOG_TOPIC(TRACE, Logger::STARTUP) << name() << "::start";

  if (!_path.empty()) {
    TRI_SetUserTempPath((char*)_path.c_str());
  }
}

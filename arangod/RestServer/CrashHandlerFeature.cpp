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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "RestServer/CrashHandlerFeature.h"

#include "ApplicationFeatures/ApplicationFeature.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"
#include "RestServer/DatabasePathFeature.h"

using namespace arangodb::options;

namespace arangodb {

CrashHandlerFeature::CrashHandlerFeature(
    application_features::ApplicationServer& server,
    std::shared_ptr<crash_handler::DumpManager> dumpManager)
    : application_features::ApplicationFeature{server, *this},
      _dumpManager(std::move(dumpManager)) {
  setOptional(false);
  // Feature must start after DatabasePathFeature
  // otherwise it won't be able to set the crashes directory
  startsAfter<DatabasePathFeature>();
}

void CrashHandlerFeature::start() {
  if (_enabled) {
    auto const path = server().getFeature<DatabasePathFeature>().directory();
    _dumpManager->setCrashesDirectory(path);
  }
}

void CrashHandlerFeature::collectOptions(
    std::shared_ptr<ProgramOptions> options) {
  options->addOption(
      "--crash-handler.enable-dumps",
      "Enable crash dump logging to write crash information to disk.",
      new BooleanParameter(&_enabled),
      options::makeDefaultFlags(
          options::Flags::DefaultNoComponents, options::Flags::OnCoordinator,
          options::Flags::OnDBServer, options::Flags::OnAgent,
          options::Flags::OnSingle));
}

std::shared_ptr<crash_handler::DumpManager>
CrashHandlerFeature::getDumpManager() const {
  return _dumpManager;
}

}  // namespace arangodb

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

#include "ApplicationFeatures/ApplicationServer.h"
#include "ApplicationFeatures/GreetingsFeaturePhase.h"
#include "CrashHandler/CrashHandlerRegistry.h"
#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"

using namespace arangodb;
using namespace arangodb::options;

CrashHandlerFeature::CrashHandlerFeature(Server& server,
                                         CrashHandlerRegistry* crashHandler)
    : ArangodFeature{server, *this},
      _crashHandler(crashHandler),
      _enabled(true) {
  setOptional(false);
  // This feature should start very early
  startsAfter<GreetingsFeaturePhase>();
}

void CrashHandlerFeature::collectOptions(
    std::shared_ptr<ProgramOptions> options) {
  options->addOption(
      "--crash-handler.enabled",
      "Enable crash dump logging to write crash information to disk.",
      new BooleanParameter(&_enabled),
      arangodb::options::makeDefaultFlags(
          arangodb::options::Flags::DefaultNoComponents,
          arangodb::options::Flags::OnCoordinator,
          arangodb::options::Flags::OnDBServer,
          arangodb::options::Flags::OnAgent,
          arangodb::options::Flags::OnSingle));
}

void CrashHandlerFeature::setDatabaseDirectory(std::string path) {
  if (_enabled && _crashHandler != nullptr) {
    _crashHandler->setDatabaseDirectory(std::move(path));
  }
}

std::vector<std::string> CrashHandlerFeature::listCrashes() const {
  if (!_enabled || _crashHandler == nullptr) {
    return {};
  }
  return _crashHandler->listCrashes();
}

std::unordered_map<std::string, std::string>
CrashHandlerFeature::getCrashContents(std::string_view crashId) const {
  if (!_enabled || _crashHandler == nullptr) {
    return {};
  }
  return _crashHandler->getCrashContents(crashId);
}

bool CrashHandlerFeature::deleteCrash(std::string_view crashId) const {
  if (!_enabled || _crashHandler == nullptr) {
    return false;
  }
  return _crashHandler->deleteCrash(crashId);
}

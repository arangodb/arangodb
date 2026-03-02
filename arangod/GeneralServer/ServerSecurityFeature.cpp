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

#include "ApplicationFeatures/ApplicationServer.h"
#include "ApplicationFeatures/GreetingsFeaturePhase.h"
#include "GeneralServer/ServerSecurityFeature.h"
#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"
#include "Utils/ExecContext.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::options;

ServerSecurityFeature::ServerSecurityFeature(
    application_features::ApplicationServer& server)
    : ApplicationFeature{server, *this} {
  setOptional(false);
  startsAfter<application_features::GreetingsFeaturePhase>();
}

void ServerSecurityFeature::collectOptions(
    std::shared_ptr<ProgramOptions> options) {
  options->addOption(
      "--server.harden",
      "Lock down REST APIs that reveal version information or server "
      "internals for non-admin users.",
      new BooleanParameter(&_options.hardenedRestApi));
}

bool ServerSecurityFeature::isRestApiHardened() const noexcept {
  return _options.hardenedRestApi;
}

bool ServerSecurityFeature::canAccessHardenedApi() const noexcept {
  bool allowAccess = !isRestApiHardened();

  if (!allowAccess) {
    ExecContext const& exec = ExecContext::current();
    if (exec.isAdminUser()) {
      // also allow access if there is not authentication
      // enabled or when the user is an administrator
      allowAccess = true;
    }
  }
  return allowAccess;
}

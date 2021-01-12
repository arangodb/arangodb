////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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

#include "GeneralServer/ServerSecurityFeature.h"
#include "ApplicationFeatures/GreetingsFeaturePhase.h"
#include "Logger/Logger.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "Utils/ExecContext.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::options;

ServerSecurityFeature::ServerSecurityFeature(application_features::ApplicationServer& server)
    : ApplicationFeature(server, "ServerSecurity"),
      _enableFoxxApi(true),
      _enableFoxxStore(true),
      _hardenedRestApi(false) {
  setOptional(false);
  startsAfter<application_features::GreetingsFeaturePhase>();
}

void ServerSecurityFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addSection("server", "Server features");
  options->addOption("--server.harden",
                     "lock down REST APIs that reveal version information or server "
                     "internals for non-admin users",
                     new BooleanParameter(&_hardenedRestApi))
                     .setIntroducedIn(30500);

  options->addSection("foxx", "Configure Foxx");
  options->addOption("--foxx.api", "enables Foxx management REST APIs",
                     new BooleanParameter(&_enableFoxxApi),
                     arangodb::options::makeFlags(
                     arangodb::options::Flags::DefaultNoComponents,
                     arangodb::options::Flags::OnCoordinator,
                     arangodb::options::Flags::OnSingle))
                     .setIntroducedIn(30500);
  options->addOption("--foxx.store", "enables Foxx store in web interface",
                     new BooleanParameter(&_enableFoxxStore),
                     arangodb::options::makeFlags(
                     arangodb::options::Flags::DefaultNoComponents,
                     arangodb::options::Flags::OnCoordinator,
                     arangodb::options::Flags::OnSingle))
                     .setIntroducedIn(30500);

}

bool ServerSecurityFeature::isFoxxApiDisabled() const {
  return !_enableFoxxApi;
}

bool ServerSecurityFeature::isFoxxStoreDisabled() const {
  return !_enableFoxxStore || !_enableFoxxApi;
}

bool ServerSecurityFeature::isRestApiHardened() const {
  return _hardenedRestApi;
}

bool ServerSecurityFeature::canAccessHardenedApi() const {
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

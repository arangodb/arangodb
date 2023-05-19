////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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

#include "ApplicationFeatures/ApplicationServer.h"
#include "GeneralServer/ServerSecurityFeature.h"
#include "Logger/Logger.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "Utils/ExecContext.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::options;

ServerSecurityFeature::ServerSecurityFeature(Server& server)
    : ArangodFeature{server, *this},
      _enableFoxxApi(true),
      _enableFoxxStore(true),
      _hardenedRestApi(false),
      _foxxAllowInstallFromRemote(false) {
  setOptional(false);
  startsAfter<application_features::GreetingsFeaturePhase>();
}

void ServerSecurityFeature::collectOptions(
    std::shared_ptr<ProgramOptions> options) {
  options
      ->addOption(
          "--server.harden",
          "Lock down REST APIs that reveal version information or server "
          "internals for non-admin users.",
          new BooleanParameter(&_hardenedRestApi))
      .setIntroducedIn(30500);

  options
      ->addOption("--foxx.api",
                  "Whether to enable the Foxx management REST APIs.",
                  new BooleanParameter(&_enableFoxxApi),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnCoordinator,
                      arangodb::options::Flags::OnSingle))
      .setIntroducedIn(30500);

  options
      ->addOption("--foxx.store",
                  "Whether to enable the Foxx store in the web interface.",
                  new BooleanParameter(&_enableFoxxStore),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnCoordinator,
                      arangodb::options::Flags::OnSingle))
      .setIntroducedIn(30500);

  options
      ->addOption(
          "--foxx.allow-install-from-remote",
          "Allow installing Foxx apps from remote URLs other than GitHub.",
          new BooleanParameter(&_foxxAllowInstallFromRemote),
          arangodb::options::makeFlags(
              arangodb::options::Flags::DefaultNoComponents,
              arangodb::options::Flags::OnCoordinator,
              arangodb::options::Flags::OnSingle))
      .setIntroducedIn(30805);
}

void ServerSecurityFeature::disableFoxxApi() noexcept {
  _enableFoxxApi = false;
}

bool ServerSecurityFeature::isFoxxApiDisabled() const noexcept {
  return !_enableFoxxApi;
}

bool ServerSecurityFeature::isFoxxStoreDisabled() const noexcept {
  return !_enableFoxxStore || !_enableFoxxApi;
}

bool ServerSecurityFeature::isRestApiHardened() const noexcept {
  return _hardenedRestApi;
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

bool ServerSecurityFeature::foxxAllowInstallFromRemote() const noexcept {
  return _foxxAllowInstallFromRemote;
}

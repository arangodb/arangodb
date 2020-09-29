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
      _enableWebInterface(true),
      _enableFoxxApi(true),
      _enableFoxxStore(true),
      _enableFoxxApps(true),
      _enableJavaScriptTasksApi(true),
      _enableJavaScriptTransactionsApi(true),
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
  
  options->addOption("--server.web-interface",
                     "enable admin web interface at /_admin/aardvark",
                     new BooleanParameter(&_enableWebInterface))
                     .setIntroducedIn(30704)
                     .setIntroducedIn(30800);
  
  options->addOption("--server.tasks-api",
                     "enable JavaScript tasks API /_api/tasks",
                     new BooleanParameter(&_enableJavaScriptTasksApi))
                     .setIntroducedIn(30704)
                     .setIntroducedIn(30800);
  
  options->addOption("--server.transactions-api",
                     "enable JavaScript transaction POST API /_api/transactions",
                     new BooleanParameter(&_enableJavaScriptTransactionsApi))
                     .setIntroducedIn(30704)
                     .setIntroducedIn(30800);

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

  options->addOption("--foxx.apps", "enables the usage of user-defined Foxx apps",
                     new BooleanParameter(&_enableFoxxApps),
                     arangodb::options::makeFlags(
                     arangodb::options::Flags::DefaultNoComponents,
                     arangodb::options::Flags::OnCoordinator,
                     arangodb::options::Flags::OnSingle))
                     .setIntroducedIn(30704)
                     .setIntroducedIn(30800);
}

bool ServerSecurityFeature::canAccessHardenedApi() const {
  // also allow access if there is not authentication
  // enabled or when the user is an administrator
  return !isRestApiHardened() || ExecContext::current().isAdminUser();
}

bool ServerSecurityFeature::isRestApiHardened() const {
  return _hardenedRestApi;
}

bool ServerSecurityFeature::enableWebInterface() const {
  // the web interface usage can be configured independently of other Foxx apps!
  return _enableWebInterface;
}

bool ServerSecurityFeature::enableFoxxApi() const {
  return _enableFoxxApi && _enableFoxxApps;
}

bool ServerSecurityFeature::enableFoxxStore() const {
  return _enableFoxxStore && _enableFoxxApi && _enableFoxxApps;
}

bool ServerSecurityFeature::enableFoxxApps() const {
  return _enableFoxxApps;
}

bool ServerSecurityFeature::enableJavaScriptTasksApi() const {
  return _enableJavaScriptTasksApi;
}

bool ServerSecurityFeature::enableJavaScriptTransactionsApi() const {
  return _enableJavaScriptTransactionsApi;
}

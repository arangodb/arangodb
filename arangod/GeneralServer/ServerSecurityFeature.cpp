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
#include "Auth/Rbac/Actions.h"
#include "GeneralServer/ServerSecurityFeature.h"
#include "GeneralServer/ServerSecurityOptionsProvider.h"
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
  arangodb::security::ServerSecurityOptionsProvider provider;
  provider.declareOptions(options, _options);
}

void ServerSecurityFeature::disableFoxxApi() noexcept {
  _options.enableFoxxApi = false;
}

bool ServerSecurityFeature::isFoxxApiDisabled() const noexcept {
  return !_options.enableFoxxApi;
}

bool ServerSecurityFeature::isFoxxStoreDisabled() const noexcept {
  return !_options.enableFoxxStore || !_options.enableFoxxApi;
}

bool ServerSecurityFeature::isRestApiHardened() const noexcept {
  return _options.hardenedRestApi;
}

bool ServerSecurityFeature::foxxAllowInstallFromRemote() const noexcept {
  return _options.foxxAllowInstallFromRemote;
}

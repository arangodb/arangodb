////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2026 ArangoDB GmbH, Hyderabad, India
/// Copyright 2026 triAGENS GmbH, Hyderabad, India
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
/// Copyright holder is ArangoDB GmbH, Hyderabad, India
///
////////////////////////////////////////////////////////////////////////////////

#include "ServerSecurityOptionsProvider.h"

#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"

namespace arangodb::security {

using namespace arangodb::options;

void ServerSecurityOptionsProvider::declareOptions(
    std::shared_ptr<ProgramOptions> options,
    ServerSecurityFeatureOptions& opts) {
  options->addOption(
      "--server.harden",
      "Lock down REST APIs that reveal version information or server "
      "internals for non-admin users.",
      new BooleanParameter(&opts.hardenedRestApi));

  options->addOption("--foxx.api", "Enable the Foxx management API.",
                     new BooleanParameter(&opts.enableFoxxApi),
                     makeFlags(Flags::DefaultNoComponents, Flags::OnCoordinator,
                               Flags::OnSingle));

  options->addOption("--foxx.store",
                     "Enable the Foxx store in the web interface.",
                     new BooleanParameter(&opts.enableFoxxStore),
                     makeFlags(Flags::DefaultNoComponents, Flags::OnCoordinator,
                               Flags::OnSingle));

  options->addOption(
      "--foxx.allow-install-from-remote",
      "Allow installing Foxx apps from remote URLs other than Github.",
      new BooleanParameter(&opts.foxxAllowInstallFromRemote),
      makeFlags(Flags::DefaultNoComponents, Flags::OnCoordinator,
                Flags::OnSingle));
}

void ServerSecurityOptionsProvider::validateOptions(
    std::shared_ptr<ProgramOptions> /*options*/,
    ServerSecurityFeatureOptions& /*opts*/) {}

}  // namespace arangodb::security

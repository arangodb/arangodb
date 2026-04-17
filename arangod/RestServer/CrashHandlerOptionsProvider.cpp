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

#include "CrashHandlerOptionsProvider.h"

#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"

namespace arangodb::crash_handler {

using namespace arangodb::options;

void CrashHandlerOptionsProvider::declareOptions(
    std::shared_ptr<ProgramOptions> options, CrashHandlerFeatureOptions& opts) {
  options->addOption(
      "--crash-handler.enable-dumps",
      "Enable crash dump logging to write crash information to disk.",
      new BooleanParameter(&opts.enabled),
      makeDefaultFlags(Flags::DefaultNoComponents, Flags::OnCoordinator,
                       Flags::OnDBServer, Flags::OnAgent, Flags::OnSingle));
}

void CrashHandlerOptionsProvider::validateOptions(
    std::shared_ptr<ProgramOptions> /*options*/,
    CrashHandlerFeatureOptions& /*opts*/) {}

}  // namespace arangodb::crash_handler

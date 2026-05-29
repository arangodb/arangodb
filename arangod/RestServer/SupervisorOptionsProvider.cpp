////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#include "SupervisorOptionsProvider.h"

#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"

namespace arangodb {

using namespace arangodb::options;

void SupervisorOptionsProvider::declareOptions(
    std::shared_ptr<ProgramOptions> options, SupervisorFeatureOptions& opts) {
  options
      ->addOption(
          "--supervisor",
          "Start the server in supervisor mode. Requires --pid-file to be set.",
          new BooleanParameter(&opts.supervisor),
          makeDefaultFlags(Flags::Uncommon))
      .setLongDescription(R"(Runs an arangod process as supervisor with another
arangod process as child, which acts as the server. In the event that the server
unexpectedly terminates due to an internal error, the supervisor automatically
restarts the server. Enabling this option implies that the server runs as a
daemon.)");
}

}  // namespace arangodb

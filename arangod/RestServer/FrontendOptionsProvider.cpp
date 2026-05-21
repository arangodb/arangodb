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

#include "FrontendOptionsProvider.h"

#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"

namespace arangodb::frontend {

using namespace arangodb::options;

void FrontendOptionsProvider::declareOptions(
    std::shared_ptr<ProgramOptions> options, FrontendFeatureOptions& opts) {
  options->addSection("web-interface", "browser-based frontend");

  options->addOldOption("frontend.version-check",
                        "web-interface.version-check");

  options->addOption("--web-interface.version-check",
                     "Alert the user if new versions are available.",
                     new BooleanParameter(&opts.versionCheck),
                     arangodb::options::makeFlags(
                         arangodb::options::Flags::DefaultNoComponents,
                         arangodb::options::Flags::OnCoordinator,
                         arangodb::options::Flags::OnSingle,
                         arangodb::options::Flags::Uncommon));
}

}  // namespace arangodb::frontend

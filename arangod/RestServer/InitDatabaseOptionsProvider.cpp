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

#include "InitDatabaseOptionsProvider.h"

#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"

namespace arangodb::init_database {

using namespace arangodb::options;

void InitDatabaseOptionsProvider::declareOptions(
    std::shared_ptr<ProgramOptions> options, InitDatabaseFeatureOptions& opts) {
  options->addOption("--database.init-database",
                     "Initialize an empty database.",
                     new BooleanParameter(&opts.initDatabase),
                     makeDefaultFlags(Flags::Uncommon, Flags::Command));

  options->addOption("--database.restore-admin",
                     "Reset the admin users and set a new password.",
                     new BooleanParameter(&opts.restoreAdmin),
                     makeDefaultFlags(Flags::Uncommon, Flags::Command));

  options->addOption(
      "--database.password", "The initial password of the root user.",
      new StringParameter(&opts.password), makeDefaultFlags(Flags::Uncommon));
}

}  // namespace arangodb::init_database

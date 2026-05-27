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

#include "VersionOptionsProvider.h"

#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"

namespace arangodb {

using namespace arangodb::options;

void VersionOptionsProvider::declareOptions(
    std::shared_ptr<ProgramOptions> options, VersionFeatureOptions& opts) {
  options->addOption(
      "--version",
      "Print the version and other related information, then exit.",
      new BooleanParameter(&opts.printVersion),
      arangodb::options::makeDefaultFlags(arangodb::options::Flags::Command));

  options
      ->addOption("--version-json",
                  "Print the version and other related information in JSON "
                  "format, then exit.",
                  new BooleanParameter(&opts.printVersionJson),
                  arangodb::options::makeDefaultFlags(
                      arangodb::options::Flags::Command))
      .setIntroducedIn(30900);
}

}  // namespace arangodb

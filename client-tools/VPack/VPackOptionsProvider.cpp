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

#include "VPackOptionsProvider.h"

#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"

namespace arangodb {

using namespace arangodb::options;

void VPackOptionsProvider::declareOptionsImpl(
    std::shared_ptr<ProgramOptions> opts, VPackFeatureOptions& options) {
  std::unordered_set<std::string> const inputTypes{
      {"json", "json-hex", "vpack", "vpack-hex"}};
  std::unordered_set<std::string> const outputTypes{
      {"json", "json-pretty", "vpack", "vpack-hex"}};

  opts->addOption("--input-file",
                  "The input file (leave empty or use \"-\" for stdin).",
                  new StringParameter(&options.inputFile));

  opts->addOption("--output-file",
                  "The output file (leave empty or use \"+\" for stdout).",
                  new StringParameter(&options.outputFile));

  opts->addOption("--input-type", "The input format.",
                  new DiscreteValuesParameter<StringParameter>(
                      &options.inputType, inputTypes))
      .setIntroducedIn(30800);

  opts->addOption("--output-type", "The output format.",
                  new DiscreteValuesParameter<StringParameter>(
                      &options.outputType, outputTypes))
      .setIntroducedIn(30800);

  opts->addOption(
          "--fail-on-non-json",
          "Raise an error when trying to emit non-JSON types to JSON output.",
          new BooleanParameter(&options.failOnNonJson))
      .setIntroducedIn(30800);
}

}  // namespace arangodb

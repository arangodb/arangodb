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

#include "Shell/ShellOptionsProvider.h"

#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"

namespace arangodb {

using namespace arangodb::options;

void ShellOptionsProvider::declareOptionsImpl(
    std::shared_ptr<ProgramOptions> opts, ShellFeatureOptions& options) {
  opts->addSection("javascript", "JavaScript engine");

  opts->addOption(
      "--javascript.execute",
      "Execute the JavaScript code from the specified file.",
      new VectorParameter<StringParameter>(&options.executeScripts));

  opts->addOption(
      "--javascript.execute-string",
      "Execute the JavaScript code from the specified string.",
      new VectorParameter<StringParameter>(&options.executeStrings));

  opts->addOption(
      "--javascript.check-syntax",
      "Check the syntax of the JavaScript code from the specified file.",
      new VectorParameter<StringParameter>(&options.checkSyntaxFiles));

  opts->addOption("--javascript.unit-tests",
                  "Do not start as a shell, run unit tests instead.",
                  new VectorParameter<StringParameter>(&options.unitTests));

  opts->addOption("--javascript.unit-test-filter",
                  "Filter the test cases in the test suite.",
                  new StringParameter(&options.unitTestFilter));
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  opts->addOption(
      "--javascript.script-parameter", "Script parameter.",
      new VectorParameter<StringParameter>(&options.scriptParameters));
#endif
#ifdef ARANGODB_ENABLE_FAILURE_TESTS
  opts->addOption(
      "--client.failure-points",
      "The failure point to set during shell startup (requires compilation "
      "with failure points support).",
      new VectorParameter<StringParameter>(&options.failurePoints));
#endif
}

}  // namespace arangodb

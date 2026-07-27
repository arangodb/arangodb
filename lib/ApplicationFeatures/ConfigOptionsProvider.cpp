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

#include "ConfigOptionsProvider.h"

#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"

namespace arangodb {

using namespace arangodb::options;

void ConfigOptionsProvider::declareOptions(
    std::shared_ptr<options::ProgramOptions> options,
    ConfigFeatureOptions& opts) {
  options->addOption("--configuration,-c",
                     "The configuration file or \"none\".",
                     new StringParameter(&opts.file));

  options->addOption(
      "--config", "The configuration file or \"none\".",
      new StringParameter(&opts.file),
      arangodb::options::makeDefaultFlags(arangodb::options::Flags::Uncommon));

  options->addOption(
      "--define,-D",
      "Define a value for a `@key@` entry in the configuration file using the "
      "syntax `\"key=value\"`.",
      new VectorParameter<StringParameter>(&opts.defines),
      arangodb::options::makeDefaultFlags(arangodb::options::Flags::Uncommon));

  options->addOption(
      "--check-configuration", "Check the configuration and exit.",
      new BooleanParameter(&opts.checkConfiguration),
      arangodb::options::makeDefaultFlags(arangodb::options::Flags::Uncommon,
                                          arangodb::options::Flags::Command));

  options->addOption(
      "--honor-nsswitch",
      "Allow hostname lookup configuration via /etc/nsswitch.conf if on "
      "Linux/glibc.",
      new BooleanParameter(&opts.honorNsswitch),
      arangodb::options::makeDefaultFlags(arangodb::options::Flags::Uncommon));
}

}  // namespace arangodb

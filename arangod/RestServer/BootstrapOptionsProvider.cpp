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

#include "BootstrapOptionsProvider.h"

#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"

namespace arangodb::bootstrap {

using namespace arangodb::options;

void BootstrapOptionsProvider::declareOptions(
    std::shared_ptr<ProgramOptions> options, BootstrapFeatureOptions& opts) {
  options->addOption(
      "--hund", "Make ArangoDB bark on startup.",
      new BooleanParameter(&opts.bark),
      arangodb::options::makeDefaultFlags(arangodb::options::Flags::Uncommon));
}

}  // namespace arangodb::bootstrap

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

#include "EngineSelectorOptionsProvider.h"

#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"
#include "EngineSelectorFeature.h"

namespace arangodb::engine_selector {

using namespace arangodb::options;

void EngineSelectorOptionsProvider::declareOptions(
    std::shared_ptr<ProgramOptions> options,
    EngineSelectorFeatureOptions& opts) {
  options
      ->addOption("--server.storage-engine",
                  "The storage engine type "
                  "(note that the MMFiles engine is unavailable since "
                  "v3.7.0 and cannot be used anymore).",
                  new DiscreteValuesParameter<StringParameter>(
                      &opts.engineName, EngineSelectorFeature::availableEngineNames()))
      .setLongDescription(R"(ArangoDB's storage engine is based on RocksDB, see
http://rocksdb.org. It is the only available engine from ArangoDB v3.7 onwards.

The storage engine type needs to be the same for an entire deployment.
Live switching of storage engines on already installed systems isn't supported.
Configuring the wrong engine (not matching the previously used one) results
in the server refusing to start. You may use `auto` to let ArangoDB choose the
previously used one.)");
}

}  // namespace arangodb::engine_selector

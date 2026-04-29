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

#include "VectorIndexOptionsProvider.h"

#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"

namespace arangodb::vector_index {

using namespace arangodb::options;

void VectorIndexOptionsProvider::declareOptions(
    std::shared_ptr<ProgramOptions> programOptions,
    VectorIndexFeatureOptions& opts) {
  programOptions
      ->addOption("--vector-index",
                  "Enable the vector index feature. "
                  "Once in use, this option cannot be turned off again.",
                  new BooleanParameter(&opts.useVectorIndex),
                  makeFlags(Flags::DefaultNoComponents, Flags::OnCoordinator,
                            Flags::OnDBServer, Flags::OnSingle))
      .setIntroducedIn(31204)
      .setLongDescription(R"(This startup option should not be enabled for
  Agents in a cluster as it has no effect on them other than that you need to
  leave the option enabled.)");

  programOptions->addOldOption("--experimental-vector-index", "--vector-index");
}

}  // namespace arangodb::vector_index

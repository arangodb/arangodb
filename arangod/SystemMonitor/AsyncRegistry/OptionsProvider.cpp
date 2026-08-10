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

#include "SystemMonitor/AsyncRegistry/OptionsProvider.h"

#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"

namespace arangodb::async_registry {

using namespace arangodb::options;

void OptionsProvider::declareOptionsImpl(std::shared_ptr<ProgramOptions> opts,
                                         FeatureOptions& options) {
  opts->addSection("async-registry", "Options for the async-registry");

  opts->addOption(
          "--async-registry.cleanup-timeout",
          "Timeout in seconds between async-registry garbage collection "
          "swipes.",
          new SizeTParameter(&options.gc_timeout, /*base*/ 1,
                             /*minValue*/ 1))
      .setLongDescription(R"(Each thread that is involved in the async-registry
needs to garbage collect its finished async function calls regularly. This
option controls how often this is done in seconds. This can possibly be
performance relevant because each involved thread acquires a lock.)");
}

}  // namespace arangodb::async_registry

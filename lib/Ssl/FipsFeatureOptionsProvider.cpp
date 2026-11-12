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

#include "FipsFeatureOptionsProvider.h"

#include "Basics/application-exit.h"
#include "Logger/Logger.h"
#include "Logger/LogMacros.h"

#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"

namespace arangodb {

using namespace arangodb::options;

void FipsFeatureOptionsProvider::declareOptionsImpl(
    std::shared_ptr<ProgramOptions> options, FipsFeatureOptions& opts) {
  options->addSection("fips", "Options to control fips behaviour");
  options->addOption(
      "--server.fips-enable", "Forces the server to load the fips module.",
      new BooleanParameter(&opts.enableFips),
      arangodb::options::makeFlags(
          arangodb::options::Flags::DefaultNoComponents,
          arangodb::options::Flags::OnAll, arangodb::options::Flags::Uncommon));
}

void FipsFeatureOptionsProvider::validateOptionsImpl(
    std::shared_ptr<ProgramOptions>,
    [[maybe_unused]] FipsFeatureOptions& options) {
#ifndef USE_FIPS
  if (options.enableFips) {
    LOG_TOPIC("f4567", FATAL, arangodb::Logger::SSL)
        << "This build does not support FIPS. "
        << "Remove the --server.fips-enable options or use a FIPS supported "
           "build.";
    FATAL_ERROR_EXIT();
  }
#endif  // USE_FIPS
}

}  // namespace arangodb

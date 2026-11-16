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
  options
      ->addOption("--server.fips-enable",
                  "Whether to restrict the server to FIPS 140-3 approved "
                  "cryptography by loading the OpenSSL FIPS provider.",
                  new BooleanParameter(&opts.enableFips),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnAll,
                      arangodb::options::Flags::Uncommon))
      .setIntroducedIn(40000)
      .setLongDescription(R"(If set to `true`, the server loads the OpenSSL
FIPS provider on startup, before any TLS context is created, and makes it the
default for all cryptographic operations. Only FIPS-approved algorithms can be
used afterwards, for incoming TLS connections as well as for the server's own
outgoing connections, e.g. between cluster nodes.

The provider is loaded from `/opt/lib/ossl-modules/fips.so` with the
configuration in `/opt/ssl/fips-only.cnf`, which includes the
`/opt/ssl/fipsmodule.cnf` file generated for the installed module with
`openssl fipsinstall`. The server refuses to start if any of these files is
missing or if the provider cannot be loaded. In the FIPS Docker images, setting
the `USE_FIPS` environment variable generates `fipsmodule.cnf` and sets this
option.

This option is only available in builds with FIPS support. Other builds refuse
to start if it is set to `true`.)");
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

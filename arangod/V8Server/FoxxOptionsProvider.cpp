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

#include "FoxxOptionsProvider.h"

#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"

namespace arangodb {

using namespace arangodb::options;

void FoxxOptionsProvider::declareOptionsImpl(
    std::shared_ptr<options::ProgramOptions> options,
    FoxxFeatureOptions& opts) {
  options->addSection("foxx", "Foxx services");

  options->addOldOption("server.foxx-queues", "foxx.queues");
  options->addOldOption("server.foxx-queues-poll-interval",
                        "foxx.queues-poll-interval");

  options
      ->addOption("--foxx.queues", "Enable or disable Foxx queues.",
                  new BooleanParameter(&opts.queuesEnabled),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnCoordinator,
                      arangodb::options::Flags::OnSingle))
      .setLongDescription(R"(If set to `true`, the Foxx queues are available
and jobs in the queues are executed asynchronously.

If set to `false`, the queue manager is disabled and any jobs are prevented from
being processed, which may reduce CPU load a bit.)");

  options
      ->addOption("--foxx.queues-poll-interval",
                  "The poll interval for the Foxx queue manager (in seconds)",
                  new DoubleParameter(&opts.queuesPollInterval),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnCoordinator,
                      arangodb::options::Flags::OnSingle))
      .setLongDescription(R"(Lower values lead to more immediate and more
frequent Foxx queue job execution, but make the queue thread wake up and query
the queues more often. If set to a low value, the queue thread might cause
CPU load.

If you don't use Foxx queues much, then you may increase this value to make the
queues thread wake up less.)");

  options
      ->addOption("--foxx.force-update-on-startup",
                  "Ensure that all Foxx services are synchronized before "
                  "completing the startup sequence.",
                  new BooleanParameter(&opts.startupWaitForSelfHeal),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnCoordinator,
                      arangodb::options::Flags::OnSingle))
      .setLongDescription(R"(If set to `true`, all Foxx services in all
databases are synchronized between multiple Coordinators during the startup
sequence. This ensures that all Foxx services are up-to-date when a Coordinator
reports itself as ready.

In case the option is set to `false` (i.e. no waiting), the Coordinator
completes the startup sequence faster, and the Foxx services are propagated
lazily. Until the initialization procedure has completed for the local Foxx
apps, any request to a Foxx app is responded to with an HTTP 500 error and a
message `waiting for initialization of Foxx services in this database`. This can
cause an unavailability window for Foxx services on Coordinator startup for the
initial requests to Foxx apps until the app propagation has completed.

If you don't use Foxx, you should set this option to `false` to benefit from a
faster Coordinator startup. Deployments relying on Foxx apps being available as
soon as a Coordinator is integrated or responding should set this option to
`true`.

The option only has an effect for cluster setups. On single servers all
Foxx apps are available from the very beginning.

**Note**: ArangoDB 3.8 changes the default value to `false` for this option.
In previous versions, this option had a default value of `true`.)");

  options
      ->addOption("--foxx.enable", "Enable Foxx.",
                  new BooleanParameter(&opts.foxxEnabled),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnCoordinator,
                      arangodb::options::Flags::OnSingle))
      .setIntroducedIn(31005)
      .setLongDescription(R"(If set to `false`, access to any custom Foxx
services in the deployment will be forbidden. Access to ArangoDB's built-in
web interface will still be possible though.

**Note**: when setting this option to `false`, the management API for Foxx
services will automatically be disabled as well. This is the same as manually
setting the startup option `--foxx.api false`.)");
}

void FoxxOptionsProvider::validateOptionsImpl(
    std::shared_ptr<options::ProgramOptions> /*opts*/,
    FoxxFeatureOptions& options) {
  if (options.queuesPollInterval < 0.1) {
    options.queuesPollInterval = 0.1;
  }
}

}  // namespace arangodb

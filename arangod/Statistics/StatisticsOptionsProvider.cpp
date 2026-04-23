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

#include "StatisticsOptionsProvider.h"

#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"
#include "Statistics/ConnectionStatistics.h"
#include "Statistics/RequestStatistics.h"
#include "Statistics/StatisticsFeature.h"

namespace arangodb::statistics {

using namespace arangodb::options;

void StatisticsOptionsProvider::declareOptions(
    std::shared_ptr<ProgramOptions> options, StatisticsFeatureOptions& opts) {
  options->addOldOption("server.disable-statistics", "server.statistics");

  options
      ->addOption("--server.statistics",
                  "Whether to enable statistics gathering and statistics APIs.",
                  new BooleanParameter(&opts.statistics))
      .setLongDescription(R"(If you set this option to `false`, then ArangoDB's
statistics gathering is turned off. Statistics gathering causes regular
background CPU activity, memory usage, and writes to the storage engine, so
using this option to turn statistics off might relieve heavily-loaded instances
a bit.

A side effect of setting this option to `false` is that no statistics are
shown in the dashboard of ArangoDB's web interface, and that the REST API for
server statistics at `/_admin/statistics` returns HTTP 404.)");

  options
      ->addOption("--server.statistics-history",
                  "Whether to store statistics in the database.",
                  new BooleanParameter(&opts.statisticsHistory),
                  arangodb::options::makeDefaultFlags(
                      arangodb::options::Flags::Dynamic))
      .setLongDescription(R"(If you set this option to `false`, then ArangoDB's
statistics gathering is turned off. Statistics gathering causes regular
background CPU activity, memory usage, and writes to the storage engine, so
using this option to turn statistics off might relieve heavily-loaded instances
a bit.

If set to `false`, no statistics are shown in the dashboard of ArangoDB's
web interface, but the current statistics are available and can be queried
using the REST API for server statistics at `/_admin/statistics`.
This is less intrusive than setting the `--server.statistics` option to
`false`.)");

  options
      ->addOption(
          "--server.statistics-all-databases",
          "Provide cluster statistics in the web interface for all databases.",
          new BooleanParameter(&opts.statisticsAllDatabases),
          arangodb::options::makeFlags(
              arangodb::options::Flags::DefaultNoComponents,
              arangodb::options::Flags::OnCoordinator))
      .setIntroducedIn(30800);
}

}  // namespace arangodb::statistics

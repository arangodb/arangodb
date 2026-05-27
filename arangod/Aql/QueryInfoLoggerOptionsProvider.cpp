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

#include "QueryInfoLoggerOptionsProvider.h"

#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"

namespace arangodb::aql {

using namespace arangodb::options;

void QueryInfoLoggerOptionsProvider::declareOptions(
    std::shared_ptr<ProgramOptions> options, QueryInfoLoggerOptions& opts) {
  options
      ->addOption("--query.collection-logger-max-buffered-queries",
                  "The maximum number of queries to buffer for query "
                  "collection logging.",
                  new UInt64Parameter(&opts.maxBufferedQueries),
                  makeDefaultFlags(Flags::DefaultNoComponents,
                                   Flags::OnCoordinator, Flags::OnSingle))
      .setIntroducedIn(31202);

  options
      ->addOption(
          "--query.collection-logger-enabled",
          "Whether or not to enable logging of queries to a system collection.",
          new BooleanParameter(&opts.logEnabled),
          makeDefaultFlags(Flags::DefaultNoComponents, Flags::OnCoordinator,
                           Flags::OnSingle))
      .setIntroducedIn(31202);

  options
      ->addOption("--query.collection-logger-include-system-database",
                  "Whether or not to include _system database queries in query "
                  "collection logging.",
                  new BooleanParameter(&opts.logSystemDatabaseQueries),
                  makeDefaultFlags(Flags::DefaultNoComponents,
                                   Flags::OnCoordinator, Flags::OnSingle))
      .setIntroducedIn(31202);

  options
      ->addOption("--query.collection-logger-all-slow-queries",
                  "Whether or not to include all slow queries in query "
                  "collection logging.",
                  new BooleanParameter(&opts.logSlowQueries),
                  makeDefaultFlags(Flags::DefaultNoComponents,
                                   Flags::OnCoordinator, Flags::OnSingle))
      .setIntroducedIn(31202);

  options
      ->addOption(
          "--query.collection-logger-probability",
          "The probability with which queries are included in query collection "
          "logging (in percent).",
          new DoubleParameter(&opts.logProbability, 1.0, 0.0, 100.0),
          makeDefaultFlags(Flags::DefaultNoComponents, Flags::OnCoordinator,
                           Flags::OnSingle))
      .setIntroducedIn(31202)
      .setLongDescription(
          R"(A value of `100` logs all queries, whereas a value of `1`
approximately logs every 100th query and ignores the rest. The minimum value
is `0` and means no logging.)");

  options
      ->addOption("--query.collection-logger-retention-time",
                  "The time duration (in seconds) for with which queries are "
                  "kept in the "
                  "_queries system collection before they are purged.",
                  new DoubleParameter(&opts.retentionTime, 1.0, 1.0),
                  makeDefaultFlags(Flags::DefaultNoComponents,
                                   Flags::OnCoordinator, Flags::OnSingle))
      .setIntroducedIn(31202);

  options
      ->addOption("--query.collection-logger-push-interval",
                  "The interval (in milliseconds) in which query information "
                  "is flushed to the _queries system collection.",
                  new UInt64Parameter(&opts.pushInterval),
                  makeDefaultFlags(Flags::DefaultNoComponents,
                                   Flags::OnCoordinator, Flags::OnSingle))
      .setIntroducedIn(31202);

  options
      ->addOption("--query.collection-logger-cleanup-interval",
                  "The interval (in milliseconds) in which query information "
                  "is purged from the _queries system collection.",
                  new UInt64Parameter(&opts.cleanupInterval, 1, 1000),
                  makeDefaultFlags(Flags::DefaultNoComponents,
                                   Flags::OnCoordinator, Flags::OnSingle))
      .setIntroducedIn(31202);
}

}  // namespace arangodb::aql

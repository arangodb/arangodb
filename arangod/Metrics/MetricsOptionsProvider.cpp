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

#include "Metrics/MetricsOptionsProvider.h"

#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"

namespace arangodb::metrics {

using namespace arangodb::options;

void MetricsOptionsProvider::declareOptions(
    std::shared_ptr<ProgramOptions> opts, MetricsOptions& options) {
  opts->addOption(
      "--server.export-metrics-api", "Whether to enable the metrics API.",
      new BooleanParameter(&options.exportAPI),
      arangodb::options::makeDefaultFlags(arangodb::options::Flags::Uncommon));

  opts->addOption("--server.export-read-write-metrics",
                  "Whether to enable metrics for document reads and writes.",
                  new BooleanParameter(&options.exportReadWriteMetrics),
                  arangodb::options::makeDefaultFlags(
                      arangodb::options::Flags::Uncommon))
      .setLongDescription(R"(Enabling this option exposes the following
additional metrics via the `GET /_admin/metrics` endpoint:

- `arangodb_document_writes_total`
- `arangodb_document_writes_replication_total`
- `arangodb_document_insert_time`
- `arangodb_document_read_time`
- `arangodb_document_update_time`
- `arangodb_document_replace_time`
- `arangodb_document_remove_time`
- `arangodb_collection_truncates_total`
- `arangodb_collection_truncates_replication_total`
- `arangodb_collection_truncate_time`
)");

  opts->addOption(
          "--server.ensure-whitespace-metrics-format",
          "Set to `true` to ensure whitespace between the exported metric "
          "value and the preceding token (metric name or labels) in the "
          "metrics output.",
          new BooleanParameter(&options.ensureWhitespace),
          arangodb::options::makeDefaultFlags(
              arangodb::options::Flags::Uncommon))
      .setIntroducedIn(31006)
      .setLongDescription(R"(Using the whitespace characters in the output may
be required to make the metrics output compatible with some processing tools,
although Prometheus itself doesn't need it.)");

  std::unordered_set<std::string> modes = {"disabled", "enabled-per-shard",
                                           "enabled-per-shard-per-user"};
  opts->addOption("--server.export-shard-usage-metrics",
                  "Whether or not to export shard usage metrics.",
                  new DiscreteValuesParameter<StringParameter>(
                      &options.usageTrackingModeString, modes),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnDBServer))
      .setIntroducedIn(31200)
      .setLongDescription(R"(This option can be used to make DB-Servers export
detailed shard usage metrics.

- By default, this option is set to `disabled` so that no shard usage metrics
  are exported.

- Set the option to `enabled-per-shard` to make DB-Servers collect per-shard
  usage metrics whenever a shard is accessed.

- Set this option to `enabled-per-shard-per-user` to make DB-Servers collect
  usage metrics per shard and per user whenever a shard is accessed.

Note that enabling shard usage metrics can produce a lot of metrics if there
are many shards and/or users in the system.)");
}

void MetricsOptionsProvider::validateOptions(
    std::shared_ptr<ProgramOptions> opts, MetricsOptions& options) {
  if (options.usageTrackingModeString == "enabled-per-shard") {
    options.usageTrackingMode = UsageTrackingMode::kEnabledPerShard;
  } else if (options.usageTrackingModeString == "enabled-per-shard-per-user") {
    options.usageTrackingMode = UsageTrackingMode::kEnabledPerShardPerUser;
  } else {
    options.usageTrackingMode = UsageTrackingMode::kDisabled;
  }
}

}  // namespace arangodb::metrics

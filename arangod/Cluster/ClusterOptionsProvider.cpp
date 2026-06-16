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

#include "ClusterOptionsProvider.h"

#include "Basics/application-exit.h"
#include "Logger/Logger.h"
#include "Logger/LogMacros.h"
#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"

#include <limits>

namespace arangodb {

using namespace arangodb::options;

void ClusterOptionsProvider::declareOptions(
    std::shared_ptr<ProgramOptions> options, ClusterOptions& opts) {
  options->addSection("cluster", "cluster");

  options->addObsoleteOption("--cluster.username",
                             "username used for cluster-internal communication",
                             true);
  options->addObsoleteOption("--cluster.password",
                             "password used for cluster-internal communication",
                             true);
  options->addObsoleteOption("--cluster.disable-dispatcher-kickstarter",
                             "The dispatcher feature isn't available anymore; "
                             "Use ArangoDBStarter for this now!",
                             true);
  options->addObsoleteOption("--cluster.disable-dispatcher-frontend",
                             "The dispatcher feature isn't available anymore; "
                             "Use ArangoDB Starter for this now!",
                             true);
  options->addObsoleteOption(
      "--cluster.dbserver-config",
      "The dbserver-config is not available anymore, Use ArangoDBStarter",
      true);
  options->addObsoleteOption(
      "--cluster.coordinator-config",
      "The coordinator-config is not available anymore, Use ArangoDBStarter",
      true);
  options->addObsoleteOption("--cluster.data-path",
                             "path to cluster database directory", true);
  options->addObsoleteOption("--cluster.log-path",
                             "path to log directory for the cluster", true);
  options->addObsoleteOption("--cluster.arangod-path",
                             "path to the arangod for the cluster", true);
  options->addObsoleteOption("--cluster.my-local-info",
                             "this server's local info", false);
  options->addObsoleteOption("--cluster.my-id", "this server's id", false);

  options->addObsoleteOption("--cluster.agency-prefix", "agency prefix", false);

  options->addOption(
      "--cluster.require-persisted-id",
      "If set to `true`, then the instance only starts if a UUID file is found "
      "in the database directory on startup. This ensures that the instance is "
      "started using an already existing database directory and not a new one. "
      "For the first start, you must either create the UUID file manually or "
      "set the option to `false` for the initial startup.",
      new BooleanParameter(&opts.requirePersistedId));

  options
      ->addOption("--cluster.agency-endpoint",
                  "Agency endpoint(s) to connect to.",
                  new VectorParameter<StringParameter>(&opts.agencyEndpoints),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnCoordinator,
                      arangodb::options::Flags::OnDBServer))
      .setLongDescription(R"(You can specify this option multiple times to let
the server use a cluster of Agency servers.

Endpoints have the following pattern:

- `tcp://ipv4-address:port` - TCP/IP endpoint, using IPv4
- `tcp://[ipv6-address]:port` - TCP/IP endpoint, using IPv6
- `ssl://ipv4-address:port` - TCP/IP endpoint, using IPv4, SSL encryption
- `ssl://[ipv6-address]:port` - TCP/IP endpoint, using IPv6, SSL encryption

You must specify at least one endpoint or ArangoDB refuses to start. It is
recommended to specify at least two endpoints, so that ArangoDB has an
alternative endpoint if one of them becomes unavailable:

`--cluster.agency-endpoint tcp://192.168.1.1:4001
--cluster.agency-endpoint tcp://192.168.1.2:4002 ...`)");

  options
      ->addOption("--cluster.my-role", "This server's role.",
                  new StringParameter(&opts.myRole))
      .setLongDescription(R"(For a cluster, the possible values are `DBSERVER`
(backend data server) and `COORDINATOR` (frontend server for external and
application access).)");

  options
      ->addOption("--cluster.my-address",
                  "This server's endpoint for cluster-internal communication.",
                  new StringParameter(&opts.myEndpoint),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnCoordinator,
                      arangodb::options::Flags::OnDBServer))
      .setLongDescription(R"(If specified, the endpoint needs to be in one of
the following formats:

- `tcp://ipv4-address:port` - TCP/IP endpoint, using IPv4
- `tcp://[ipv6-address]:port` - TCP/IP endpoint, using IPv6
- `ssl://ipv4-address:port` - TCP/IP endpoint, using IPv4, SSL encryption
- `ssl://[ipv6-address]:port` - TCP/IP endpoint, using IPv6, SSL encryption

If you don't specify an endpoint, the server looks up its internal endpoint
address in the Agency. If no endpoint can be found in the Agency for the
server's ID, ArangoDB refuses to start.

**Examples**

Listen only on the interface with the address `192.168.1.1`:

`--cluster.my-address tcp://192.168.1.1:8530`

Listen on all IPv4 and IPv6 addresses which are configured on port `8530`:

`--cluster.my-address ssl://[::]:8530`)");

  options
      ->addOption("--cluster.my-advertised-endpoint",
                  "This server's advertised endpoint for external "
                  "communication (optional, e.g. aan external IP address or "
                  "load balancer).",
                  new StringParameter(&opts.myAdvertisedEndpoint),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnCoordinator,
                      arangodb::options::Flags::OnDBServer))
      .setLongDescription(R"(If specified, the endpoint needs to be in one of
the following formats:

- `tcp://ipv4-address:port` - TCP/IP endpoint, using IPv4
- `tcp://[ipv6-address]:port` - TCP/IP endpoint, using IPv6
- `ssl://ipv4-address:port` - TCP/IP endpoint, using IPv4, SSL encryption
- `ssl://[ipv6-address]:port` - TCP/IP endpoint, using IPv6, SSL encryption

If you don't specify an advertised endpoint, no external endpoint is
advertised.

**Examples**

If an external interface is available to this server, you can specify it to
communicate with external software / drivers:

`--cluster.my-advertised-endpoint tcp://some.public.place:8530`

All specifications of endpoints apply.)");

  options
      ->addOption("--cluster.write-concern",
                  "The global default write concern used for writes to new "
                  "collections.",
                  new UInt32Parameter(&opts.writeConcern, /*base*/ 1,
                                      /*minValue*/ 1,
                                      /*maxValue*/
                                      ClusterOptions::kMaxReplicationFactor),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnCoordinator))
      .setLongDescription(R"(This value is used as the default write concern
for databases, which in turn is used as the default for collections.

**Warning**: If you use multiple Coordinators, use the same value on all
Coordinators.)");

  options
      ->addOption("--cluster.system-replication-factor",
                  "The default replication factor for system collections.",
                  new UInt32Parameter(
                      &opts.systemReplicationFactor, /*base*/ 1,
                      /*minValue*/ 1,
                      /*maxValue*/ ClusterOptions::kMaxReplicationFactor),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnCoordinator))
      .setLongDescription(R"(**Warning**: If you use multiple Coordinators, use
the same value on all Coordinators.)");

  options
      ->addOption("--cluster.default-replication-factor",
                  "The default replication factor for non-system collections.",
                  new UInt32Parameter(
                      &opts.defaultReplicationFactor, /*base*/ 1,
                      /*minValue*/ 1,
                      /*maxValue*/ ClusterOptions::kMaxReplicationFactor),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnCoordinator))
      .setLongDescription(R"(If you don't set this option, it defaults to the
value of the `--cluster.min-replication-factor` option. If set, the value must
be between the values of `--cluster.min-replication-factor` and
`--cluster.max-replication-factor`.

Note that you can still adjust the replication factor per collection. This value
is only the default value used for new collections if no replication factor is
specified when creating a collection.

**Warning**: If you use multiple Coordinators, use the same value on all
Coordinators.)");

  options
      ->addOption("--cluster.min-replication-factor",
                  "The minimum replication factor for new collections.",
                  new UInt32Parameter(
                      &opts.minReplicationFactor, /*base*/ 1,
                      /*minValue*/ 1,
                      /*maxValue*/ ClusterOptions::kMaxReplicationFactor),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnCoordinator))
      .setLongDescription(R"(If you change the value of this setting and
restart the servers, no changes are applied to existing collections that would
violate the new setting.

**Warning**: If you use multiple Coordinators, use the same value on all
Coordinators.)");

  options
      ->addOption("--cluster.max-replication-factor",
                  "The maximum replication factor for new collections "
                  "(0 = unrestricted).",
                  // 10 is a hard-coded max value for the replication factor
                  new UInt32Parameter(
                      &opts.maxReplicationFactor, /*base*/ 1,
                      /*minValue*/ 0,
                      /*maxValue*/ ClusterOptions::kMaxReplicationFactor),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnCoordinator))
      .setLongDescription(R"(If you change the value of this setting and
restart the servers, no changes are applied to existing collections that would
violate the new setting.

**Warning**: If you use multiple Coordinators, use the same value on all
Coordinators.)");

  options
      ->addOption(
          "--cluster.max-number-of-shards",
          "The maximum number of shards that can be configured when creating "
          "new collections (0 = unrestricted).",
          new UInt32Parameter(&opts.maxNumberOfShards, /*base*/ 1,
                              /*minValue*/ 1),
          arangodb::options::makeFlags(
              arangodb::options::Flags::DefaultNoComponents,
              arangodb::options::Flags::OnCoordinator))
      .setLongDescription(R"(If you change the value of this setting and
restart the servers, no changes are applied to existing collections that would
violate the new setting.

**Warning**: If you use multiple Coordinators, use the same value on all
Coordinators.)");

  options
      ->addOption("--cluster.force-one-shard",
                  "Force the OneShard mode for all new collections.",
                  new BooleanParameter(&opts.forceOneShard),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnCoordinator,
                      arangodb::options::Flags::OnDBServer,
                      arangodb::options::Flags::Enterprise))
      .setLongDescription(R"(If set to `true`, forces the cluster into creating
all future collections with only a single shard and using the same DB-Server
as these collections' shards leader. All collections created this way are
eligible for specific AQL query optimizations that can improve query performance
and provide advanced transactional guarantees.

**Warning**: Use the same value on all Coordinators and all DB-Servers!)");

  options->addOption(
      "--cluster.create-waits-for-sync-replication",
      "Let the active Coordinator wait for all replicas to create collections.",
      new BooleanParameter(&opts.createWaitsForSyncReplication),
      arangodb::options::makeFlags(
          arangodb::options::Flags::DefaultNoComponents,
          arangodb::options::Flags::OnCoordinator,
          arangodb::options::Flags::OnDBServer,
          arangodb::options::Flags::Uncommon));

  options->addOption(
      "--cluster.index-create-timeout",
      "The amount of time (in seconds) the Coordinator waits for an index to "
      "be created before giving up.",
      new DoubleParameter(&opts.indexCreationTimeout),
      arangodb::options::makeFlags(
          arangodb::options::Flags::DefaultNoComponents,
          arangodb::options::Flags::OnCoordinator,
          arangodb::options::Flags::Uncommon));

  options
      ->addOption(
          "--cluster.api-jwt-policy",
          "Controls the access permissions required for accessing "
          "/_admin/cluster REST APIs (jwt-all = JWT required to access all "
          "operations, jwt-write = JWT required for POST/PUT/DELETE "
          "operations, jwt-compat = 3.7 compatibility mode)",
          new DiscreteValuesParameter<StringParameter>(
              &opts.apiJwtPolicy,
              std::unordered_set<std::string>{"jwt-all", "jwt-write",
                                              "jwt-compat"}),
          arangodb::options::makeFlags(
              arangodb::options::Flags::DefaultNoComponents,
              arangodb::options::Flags::OnCoordinator))
      .setIntroducedIn(30800)
      .setLongDescription(R"(The possible values for the option are:

- `jwt-all`: requires a valid JWT for all accesses to `/_admin/cluster` and its
  sub-routes. If you use this configuration, the **Cluster** and **Nodes**
  sections of the web interface are disabled, as they rely on the ability to
  read data from several cluster APIs.

- `jwt-write`: requires a valid JWT for write accesses (all HTTP methods except
  GET) to `/_admin/cluster`. You can use this setting to allow privileged users
  to read data from the cluster APIs, but not to do any modifications.
  Modifications (carried out by write accesses) are then only possible by
  requests with a valid JWT.

  All existing permission checks for the cluster API routes are still in effect
  with this setting, meaning that read operations without a valid JWT may still
  require dedicated other permissions (as in v3.7).

- `jwt-compat`: no **additional** access checks are in place for the cluster
  APIs. However, all existing permissions checks for the cluster API routes are
  still in effect with this setting, meaning that all operations may still
  require dedicated other permissions (as in v3.7).

The default value is `jwt-compat`, which means that this option does not cause
any extra JWT checks compared to v3.7.)");

  options
      ->addOption("--cluster.max-number-of-move-shards",
                  "The number of shards to be moved per rebalance operation. "
                  "If set to 0, no shards are moved.",
                  new UInt32Parameter(&opts.maxNumberOfMoveShards),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnCoordinator))
      .setIntroducedIn(30900)
      .setLongDescription(R"(This option limits the maximum number of move
shards operations that can be made when the **Rebalance Shards** button is
clicked in the web interface. For backwards compatibility, the default value is
`10`. A value of `0` disables the button.)");

  options
      ->addOption(
          "--cluster.failed-write-concern-status-code",
          "The HTTP status code to send if a shard has not enough in-sync "
          "replicas to fulfill the write concern.",
          new DiscreteValuesParameter<UInt32Parameter>(
              &opts.statusCodeFailedWriteConcern, {403, 503}),
          arangodb::options::makeFlags(
              arangodb::options::Flags::DefaultNoComponents,
              arangodb::options::Flags::OnDBServer))
      .setIntroducedIn(31100)
      .setLongDescription(R"(The default behavior is to return an HTTP
`403 Forbidden` status code. You can set the option to `503` to return a
`503 Service Unavailable`.)");

  options
      ->addOption("--cluster.connectivity-check-interval",
                  "The interval (in seconds) in which cluster-internal "
                  "connectivity checks are performed.",
                  new UInt32Parameter(&opts.connectivityCheckInterval),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnCoordinator,
                      arangodb::options::Flags::OnDBServer))
      .setLongDescription(R"(Setting this option to a value greater than
zero makes Coordinators and DB-Servers run period connectivity checks
with approximately the specified frequency. The first connectivity check
is carried out approximately 15 seconds after server start.
Note that a random delay is added to the interval on each server, so that
different servers do not execute their connectivity checks all at the
same time.
Setting this option to a value of zero disables these connectivity checks.)")
      .setIntroducedIn(31104);

  options
      ->addOption(
          "--cluster.no-heartbeat-delay-before-shutdown",
          "The delay (in seconds) before shutting down a coordinator "
          "if no heartbeat can be sent. Set to 0 to deactivate this shutdown",
          new DoubleParameter(&opts.noHeartbeatDelayBeforeShutdown, 1.0, 0.0,
                              std::numeric_limits<double>::max(), true, true),
          arangodb::options::makeFlags(
              arangodb::options::Flags::DefaultNoComponents,
              arangodb::options::Flags::OnCoordinator,
              arangodb::options::Flags::OnDBServer))
      .setLongDescription(
          R"(Setting this option to a value greater than zero will
let a coordinator which cannot send a heartbeat to the agency for the specified time
shut down. This is necessary to prevent that a coordinator survives longer than the
agency supervision has patience before it removes the coordinator from the agency
meta data. Without this it would be possible that a coordinator is still running
transactions and committing them, which could, for example, render Hot Backups
inconsistent.)")
      .setIntroducedIn(31204);
}

void ClusterOptionsProvider::validateOptions(
    std::shared_ptr<ProgramOptions> opts, ClusterOptions& options) {
  if (options.forceOneShard) {
    options.maxNumberOfShards = 1;
  }

  TRI_ASSERT(options.minReplicationFactor > 0);
  if (!opts->processingResult().touched("cluster.default-replication-factor")) {
    // no default replication factor set. now use the minimum value, which is
    // guaranteed to be at least 1
    options.defaultReplicationFactor = options.minReplicationFactor;
  }

  if (!opts->processingResult().touched("cluster.system-replication-factor")) {
    // no system replication factor set. now make sure it is between min and max
    if (options.systemReplicationFactor > options.maxReplicationFactor) {
      options.systemReplicationFactor = options.maxReplicationFactor;
    } else if (options.systemReplicationFactor < options.minReplicationFactor) {
      options.systemReplicationFactor = options.minReplicationFactor;
    }
  }

  if (options.defaultReplicationFactor > options.maxReplicationFactor ||
      options.defaultReplicationFactor < options.minReplicationFactor) {
    LOG_TOPIC("5af7e", FATAL, arangodb::Logger::CLUSTER)
        << "Invalid value for `--cluster.default-replication-factor`. Must not "
           "be lower than `--cluster.min-replication-factor` or higher than "
           "`--cluster.max-replication-factor`";
    FATAL_ERROR_EXIT();
  }

  if (options.systemReplicationFactor > options.maxReplicationFactor ||
      options.systemReplicationFactor < options.minReplicationFactor) {
    LOG_TOPIC("6cf0c", FATAL, arangodb::Logger::CLUSTER)
        << "Invalid value for `--cluster.system-replication-factor`. Must not "
           "be lower than `--cluster.min-replication-factor` or higher than "
           "`--cluster.max-replication-factor`";
    FATAL_ERROR_EXIT();
  }

  options.enableCluster = !options.agencyEndpoints.empty();

  options.agencyPrefix = "arango";

  if (options.systemReplicationFactor == 0) {
    LOG_TOPIC("cb945", FATAL, arangodb::Logger::CLUSTER)
        << "system replication factor must be greater 0";
    FATAL_ERROR_EXIT();
  }

  constexpr std::uint32_t minConnectivityCheckInterval = 10;  // seconds
  if (options.connectivityCheckInterval > 0 &&
      options.connectivityCheckInterval < minConnectivityCheckInterval) {
    options.connectivityCheckInterval = minConnectivityCheckInterval;
    LOG_TOPIC("08b46", WARN, Logger::CLUSTER)
        << "configured value for `--cluster.connectivity-check-interval` is "
           "too low and was automatically adjusted to minimum value "
        << minConnectivityCheckInterval;
  }
}

}  // namespace arangodb

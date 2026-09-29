////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
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

#include "ReplicationFeature.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "ApplicationFeatures/CommunicationFeaturePhase.h"
#include "Cluster/ServerState.h"
#include "FeaturePhases/BasicFeaturePhaseServer.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "Metrics/Counter.h"
#include "Metrics/CounterBuilder.h"
#include "Metrics/GaugeBuilder.h"
#include "Metrics/IRegistry.h"
#include "ProgramOptions/ProgramOptions.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/ServerIdFeature.h"
#include "RestServer/SystemDatabaseFeature.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "RocksDBEngine/RocksDBRecoveryManager.h"

using namespace arangodb::application_features;

DECLARE_COUNTER(arangodb_replication_cluster_inventory_requests_total,
                "(DC-2-DC only) Number of times the database and collection "
                "overviews have been requested.");
DECLARE_GAUGE(arangodb_replication_clients, uint64_t,
              "Number of replication clients connected");

namespace arangodb {

ReplicationFeature::ReplicationFeature(
    application_features::ApplicationServer& server,
    application_features::CommunicationFeaturePhase& comm,
    metrics::IRegistry& metricsRegistry)
    : ReplicationFeature(server, comm, metricsRegistry, ReplicationOptions{}) {}

ReplicationFeature::ReplicationFeature(
    application_features::ApplicationServer& server,
    application_features::CommunicationFeaturePhase& comm,
    metrics::IRegistry& metricsRegistry, ReplicationOptions options)
    : application_features::ApplicationFeature{server, *this},
      _options(std::move(options)),
      _connectionCache{comm, httpclient::ConnectionCache::Options{5, 120}},
      _parallelTailingInvocations(0),
      _inventoryRequests(metricsRegistry.add(
          arangodb_replication_cluster_inventory_requests_total{})),
      _clients(metricsRegistry.add(arangodb_replication_clients{})) {
  setOptional(true);
  startsAfter<BasicFeaturePhaseServer>();

  startsAfter<DatabaseFeature>();
  startsAfter<RocksDBEngine>();
  startsAfter<RocksDBRecoveryManager>();
  startsAfter<ServerIdFeature>();
  startsAfter<SystemDatabaseFeature>();
}

ReplicationFeature::~ReplicationFeature() = default;

void ReplicationFeature::prepare() {
  if (ServerState::instance()->isCoordinator()) {
    setEnabled(false);
    return;
  }
}

httpclient::ConnectionCache& ReplicationFeature::connectionCache() {
  return _connectionCache;
}

/// @brief track the number of (parallel) tailing operations
/// will throw an exception if the number of concurrently running operations
/// would exceed the configured maximum
void ReplicationFeature::trackTailingStart() {
  if (++_parallelTailingInvocations > _options.maxParallelTailingInvocations &&
      _options.maxParallelTailingInvocations > 0) {
    // we are above the configured maximum
    --_parallelTailingInvocations;
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_RESOURCE_LIMIT,
        "too many parallel invocations of WAL tailing operations");
  }
}

/// @brief count down the number of parallel tailing operations
/// must only be called after a successful call to trackTailingstart
void ReplicationFeature::trackTailingEnd() noexcept {
  --_parallelTailingInvocations;
}

void ReplicationFeature::trackInventoryRequest() noexcept {
  ++_inventoryRequests;
}

double ReplicationFeature::checkConnectTimeout(double value) const {
  if (_options.forceConnectTimeout) {
    return _options.connectTimeout;
  }
  return value;
}

double ReplicationFeature::checkRequestTimeout(double value) const {
  if (_options.forceRequestTimeout) {
    return _options.requestTimeout;
  }
  return value;
}

bool ReplicationFeature::syncByRevision() const noexcept {
  return _options.syncByRevision;
}

bool ReplicationFeature::autoRepairRevisionTrees() const noexcept {
  return _options.autoRepairRevisionTrees;
}

#ifdef ARANGODB_USE_GOOGLE_TESTS
// only used during testing
void ReplicationFeature::autoRepairRevisionTrees(bool value) noexcept {
  _options.autoRepairRevisionTrees = value;
}
#endif

/// @brief returns the connect timeout for replication requests
double ReplicationFeature::connectTimeout() const {
  return _options.connectTimeout;
}

double ReplicationFeature::requestTimeout() const {
  return _options.requestTimeout;
}

}  // namespace arangodb

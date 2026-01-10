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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <mutex>
#include <unordered_set>

#include "ApplicationFeatures/ApplicationFeature.h"
#include "Cluster/ClusterOptions.h"
#include "Cluster/ServerState.h"
#include "Containers/FlatHashMap.h"
#include "Containers/FlatHashSet.h"
#include "Metrics/Fwd.h"
#include "Scheduler/Scheduler.h"

namespace arangodb {
namespace application_features {
class CommunicationFeaturePhase;
class DatabaseFeaturePhase;
}  // namespace application_features
namespace metrics {
class MetricsFeature;
}
namespace network {
class ConnectionPool;
}

class AgencyCache;
class AgencyCallback;
class AgencyCallbackRegistry;
class ClusterInfo;
class DatabaseFeature;
class HeartbeatThread;

class ClusterFeature : public ArangodFeature {
 public:
  static constexpr std::string_view name() noexcept { return "Cluster"; }

  explicit ClusterFeature(Server& server);
  ~ClusterFeature();

  void collectOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void validateOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void prepare() override final;
  void start() override final;
  void stop() override final;
  void beginShutdown() override final;
  void unprepare() override final;

  void allocateMembers();
  void shutdown();

  std::vector<std::string> agencyEndpoints() const {
    return _options.agencyEndpoints;
  }

  std::string agencyPrefix() const { return _options.agencyPrefix; }

  AgencyCache& agencyCache();

  /// @return role argument as it was supplied by a user
  std::string const& myRole() const noexcept { return _options.myRole; }

  void notify();

  AgencyCallbackRegistry* agencyCallbackRegistry() const {
    return _agencyCallbackRegistry.get();
  }

  std::string const agencyCallbacksPath() const {
    return "/_api/agency/agency-callbacks";
  }

  std::string const clusterRestPath() const { return "/_api/cluster"; }

  void setUnregisterOnShutdown(bool);
  bool createWaitsForSyncReplication() const noexcept {
    return _options.createWaitsForSyncReplication;
  }
  std::uint32_t writeConcern() const noexcept { return _options.writeConcern; }
  std::uint32_t systemReplicationFactor() const noexcept {
    return _options.systemReplicationFactor;
  }
  std::uint32_t defaultReplicationFactor() const noexcept {
    return _options.defaultReplicationFactor;
  }
#ifdef ARANGODB_USE_GOOGLE_TESTS
  void defaultReplicationFactor(std::uint32_t value) noexcept {
    _options.defaultReplicationFactor = value;
  }
#endif

  std::uint32_t maxNumberOfShards() const noexcept {
    return _options.maxNumberOfShards;
  }
  std::uint32_t minReplicationFactor() const noexcept {
    return _options.minReplicationFactor;
  }
  std::uint32_t maxReplicationFactor() const noexcept {
    return _options.maxReplicationFactor;
  }
  std::uint32_t maxNumberOfMoveShards() const {
    return _options.maxNumberOfMoveShards;
  }
  bool forceOneShard() const noexcept { return _options.forceOneShard; }
  /// @brief index creation timeout in seconds. note: this used to be
  /// a configurable parameter in previous versions, but is now hard-coded.
  double indexCreationTimeout() const noexcept {
    return _options.indexCreationTimeout;
  }

  std::shared_ptr<HeartbeatThread> heartbeatThread();

  ClusterInfo& clusterInfo();

  /// @brief permissions required to access /_admin/cluster REST API endpoint:
  /// - "jwt-all"    = JWT required to access all operations
  /// - "jwt-write"  = JWT required to access post/put/delete operations
  /// - "jwt-compat" = compatibility mode = same permissions as in 3.7
  std::string const& apiJwtPolicy() const noexcept {
    return _options.apiJwtPolicy;
  }

  std::uint32_t statusCodeFailedWriteConcern() const {
    return _options.statusCodeFailedWriteConcern;
  }

  /// @brief get deployment ID (single server or cluster ID)
  ResultT<std::string> getDeploymentId() const;

  metrics::Counter& followersDroppedCounter() {
    TRI_ASSERT(_followersDroppedCounter != nullptr);
    return *_followersDroppedCounter;
  }
  metrics::Counter& followersRefusedCounter() {
    TRI_ASSERT(_followersRefusedCounter != nullptr);
    return *_followersRefusedCounter;
  }
  metrics::Counter& followersWrongChecksumCounter() {
    TRI_ASSERT(_followersWrongChecksumCounter != nullptr);
    return *_followersWrongChecksumCounter;
  }
  metrics::Counter& syncTreeRebuildCounter() {
    TRI_ASSERT(_syncTreeRebuildCounter != nullptr);
    return *_syncTreeRebuildCounter;
  }
  metrics::Counter& potentiallyDirtyDocumentReadsCounter() {
    TRI_ASSERT(_potentiallyDirtyDocumentReadsCounter != nullptr);
    return *_potentiallyDirtyDocumentReadsCounter;
  }
  metrics::Counter& dirtyReadQueriesCounter() {
    TRI_ASSERT(_dirtyReadQueriesCounter != nullptr);
    return *_dirtyReadQueriesCounter;
  }

  /**
   * @brief Add databases to dirty list
   */
  void addDirty(std::string const& database);
  void addDirty(containers::FlatHashSet<std::string> const& databases,
                bool callNotify);
  void addDirty(
      containers::FlatHashMap<std::string, std::shared_ptr<VPackBuilder>> const&
          changeset);
  std::unordered_set<std::string> allDatabases() const;

  /**
   * @brief Swap out the list of dirty databases
   *        This method must not be called by any other mechanism than
   *        the very start of a single maintenance run.
   */
  containers::FlatHashSet<std::string> dirty();

  /**
   * @brief Check database for dirtyness
   */
  bool isDirty(std::string const& database) const;

  /// @brief hand out async agency comm connection pool pruning:
  void pruneAsyncAgencyConnectionPool();

  /// the following methods may also be called from tests

  void shutdownHeartbeatThread();
  /// @brief wait for the AgencyCache to shut down
  /// note: this may be called multiple times during shutdown
  void shutdownAgencyCache();
  /// @brief wait for the Plan and Current syncer to shut down
  /// note: this may be called multiple times during shutdown
  void waitForSyncersToStop();

#ifdef ARANGODB_USE_GOOGLE_TESTS
  void setSyncerShutdownCode(ErrorCode code) { _syncerShutdownCode = code; }
#endif

  metrics::Histogram<metrics::LogScale<uint64_t>>&
  agency_comm_request_time_ms() {
    return _agency_comm_request_time_ms;
  }

 protected:
  void startHeartbeatThread(AgencyCallbackRegistry* agencyCallbackRegistry,
                            uint64_t interval_ms,
                            uint64_t maxFailsBeforeWarning,
                            double noHeartbeatDelayBeforeShutdown,
                            std::string const& endpoints);

 private:
  ClusterFeature(Server& server, metrics::MetricsFeature& metrics,
                 DatabaseFeature& database, size_t registration);
  void reportRole(ServerState::RoleEnum);
  void scheduleConnectivityCheck(std::uint32_t inSeconds);
  void runConnectivityCheck();

  ClusterOptions _options;

  ErrorCode _syncerShutdownCode = TRI_ERROR_SHUTTING_DOWN;
  std::unique_ptr<ClusterInfo> _clusterInfo;
  std::shared_ptr<HeartbeatThread> _heartbeatThread;
  std::unique_ptr<AgencyCache> _agencyCache;
  uint64_t _heartbeatInterval = 0;
  std::unique_ptr<AgencyCallbackRegistry> _agencyCallbackRegistry;
  metrics::MetricsFeature& _metrics;
  metrics::Histogram<metrics::LogScale<uint64_t>>& _agency_comm_request_time_ms;
  std::unique_ptr<network::ConnectionPool> _asyncAgencyCommPool;
  metrics::Counter* _followersDroppedCounter = nullptr;
  metrics::Counter* _followersRefusedCounter = nullptr;
  metrics::Counter* _followersWrongChecksumCounter = nullptr;
  // note: this metric is only there for downwards-compatibility reasons. it
  // will always have a value of 0.
  metrics::Counter* _followersTotalRebuildCounter = nullptr;
  metrics::Counter* _syncTreeRebuildCounter = nullptr;
  metrics::Counter* _potentiallyDirtyDocumentReadsCounter = nullptr;
  metrics::Counter* _dirtyReadQueriesCounter = nullptr;
  std::shared_ptr<AgencyCallback> _hotbackupRestoreCallback = nullptr;

  /// @brief lock for dirty database list
  mutable std::mutex _dirtyLock;
  /// @brief dirty databases, where a job could not be posted)
  containers::FlatHashSet<std::string> _dirtyDatabases;

  std::mutex _connectivityCheckMutex;
  Scheduler::WorkHandle _connectivityCheck;
  metrics::Counter* _connectivityCheckFailsCoordinators = nullptr;
  metrics::Counter* _connectivityCheckFailsDBServers = nullptr;
};

}  // namespace arangodb

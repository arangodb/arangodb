////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
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

#ifndef ARANGOD_CLUSTER_CLUSTER_FEATURE_H
#define ARANGOD_CLUSTER_CLUSTER_FEATURE_H 1

#include "Basics/Common.h"

#include "ApplicationFeatures/ApplicationFeature.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ServerState.h"
#include "Network/NetworkFeature.h"
#include "RestServer/Metrics.h"

namespace arangodb {

class AgencyCache;
class AgencyCallbackRegistry;
class HeartbeatThread;

class ClusterFeature : public application_features::ApplicationFeature {
 public:
  explicit ClusterFeature(application_features::ApplicationServer& server);
  ~ClusterFeature();

  void collectOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void validateOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void prepare() override final;
  void start() override final;
  void stop() override final;
  void beginShutdown() override final;
  void unprepare() override final;

  void allocateMembers();

  std::vector<std::string> agencyEndpoints() const { return _agencyEndpoints; }

  std::string agencyPrefix() const { return _agencyPrefix; }

  AgencyCache& agencyCache();

  /// @return role argument as it was supplied by a user
  std::string const& myRole() const noexcept { return _myRole; }

  void notify();

  AgencyCallbackRegistry* agencyCallbackRegistry() const {
    return _agencyCallbackRegistry.get();
  }

  std::string const agencyCallbacksPath() const {
    return "/_api/agency/agency-callbacks";
  }

  std::string const clusterRestPath() const { return "/_api/cluster"; }

  void setUnregisterOnShutdown(bool);
  bool createWaitsForSyncReplication() const {
    return _createWaitsForSyncReplication;
  }
  std::uint32_t writeConcern() const { return _writeConcern; }
  std::uint32_t systemReplicationFactor() const { return _systemReplicationFactor; }
  std::uint32_t defaultReplicationFactor() const { return _defaultReplicationFactor; }
  std::uint32_t maxNumberOfShards() const { return _maxNumberOfShards; }
  std::uint32_t minReplicationFactor() const { return _minReplicationFactor; }
  std::uint32_t maxReplicationFactor() const { return _maxReplicationFactor; }
  double indexCreationTimeout() const { return _indexCreationTimeout; }
  bool forceOneShard() const { return _forceOneShard; }

  std::shared_ptr<HeartbeatThread> heartbeatThread();

  ClusterInfo& clusterInfo();

  /// @brief permissions required to access /_admin/cluster REST API endpoint:
  /// - "jwt-all"    = JWT required to access all operations
  /// - "jwt-write"  = JWT required to access post/put/delete operations
  /// - "jwt-compat" = compatibility mode = same permissions as in 3.7
  std::string const& apiJwtPolicy() const noexcept { return _apiJwtPolicy; }
  
  Counter& followersDroppedCounter() { return _followersDroppedCounter->get(); }
  Counter& followersRefusedCounter() { return _followersRefusedCounter->get(); }
  Counter& followersWrongChecksumCounter() { return _followersWrongChecksumCounter->get(); }

  /**
   * @brief Add databases to dirty list
   */
  void addDirty(std::string const& database);
  void addDirty(std::unordered_set<std::string> const& databases, bool callNotify);
  void addDirty(std::unordered_map<std::string, std::shared_ptr<VPackBuilder>> const& changeset);
  std::unordered_set<std::string> allDatabases() const;

  /**
   * @brief Swap out the list of dirty databases
   *        This method must not be called by any other mechanism than
   *        the very start of a single maintenance run.
   */
  std::unordered_set<std::string> dirty();

  /**
   * @brief Check database for dirtyness
   */
  bool isDirty(std::string const& database) const;

  /// @brief hand out async agency comm connection pool pruning:
  void pruneAsyncAgencyConnectionPool() {
    _asyncAgencyCommPool->pruneConnections();
  }
  
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

  Histogram<log_scale_t<uint64_t>>& agency_comm_request_time_ms() { return _agency_comm_request_time_ms; }

 protected:
  void startHeartbeatThread(AgencyCallbackRegistry* agencyCallbackRegistry,
                            uint64_t interval_ms, uint64_t maxFailsBeforeWarning,
                            std::string const& endpoints);

 private:
  void reportRole(ServerState::RoleEnum);

  std::vector<std::string> _agencyEndpoints;
  std::string _agencyPrefix;
  std::string _myRole;
  std::string _myEndpoint;
  std::string _myAdvertisedEndpoint;
  std::string _apiJwtPolicy;
  std::uint32_t _writeConcern = 1;             // write concern
  std::uint32_t _defaultReplicationFactor = 0; // a value of 0 means it will use the min replication factor
  std::uint32_t _systemReplicationFactor = 2;
  std::uint32_t _minReplicationFactor = 1;     // minimum replication factor (0 = unrestricted)
  std::uint32_t _maxReplicationFactor = 10;    // maximum replication factor (0 = unrestricted)
  std::uint32_t _maxNumberOfShards = 1000;     // maximum number of shards (0 = unrestricted)
  ErrorCode _syncerShutdownCode = TRI_ERROR_SHUTTING_DOWN;
  bool _createWaitsForSyncReplication = true;
  bool _forceOneShard = false;
  bool _unregisterOnShutdown = false;
  bool _enableCluster = false;
  bool _requirePersistedId = false;
  double _indexCreationTimeout = 3600.0;
  std::unique_ptr<ClusterInfo> _clusterInfo;
  std::shared_ptr<HeartbeatThread> _heartbeatThread;
  std::unique_ptr<AgencyCache> _agencyCache;
  uint64_t _heartbeatInterval = 0;
  std::unique_ptr<AgencyCallbackRegistry> _agencyCallbackRegistry;
  ServerState::RoleEnum _requestedRole = ServerState::RoleEnum::ROLE_UNDEFINED;
  Histogram<log_scale_t<uint64_t>>& _agency_comm_request_time_ms;
  std::unique_ptr<network::ConnectionPool> _asyncAgencyCommPool;
  std::optional<std::reference_wrapper<Counter>> _followersDroppedCounter;
  std::optional<std::reference_wrapper<Counter>> _followersRefusedCounter;
  std::optional<std::reference_wrapper<Counter>> _followersWrongChecksumCounter;
  std::shared_ptr<AgencyCallback> _hotbackupRestoreCallback;

  /// @brief lock for dirty database list
  mutable arangodb::Mutex _dirtyLock;
  /// @brief dirty databases, where a job could not be posted)
  std::unordered_set<std::string> _dirtyDatabases;
};

}  // namespace arangodb

#endif

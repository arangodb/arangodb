////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2018 ArangoDB GmbH, Cologne, Germany
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

namespace arangodb {

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

  std::vector<std::string> agencyEndpoints() const { return _agencyEndpoints; }

  std::string agencyPrefix() const { return _agencyPrefix; }

  /// @return role argument as it was supplied by a user
  std::string const& myRole() const noexcept { return _myRole; }

  void syncDBServerStatusQuo();
  
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
  std::uint32_t systemReplicationFactor() { return _systemReplicationFactor; }
  std::uint32_t defaultReplicationFactor() { return _defaultReplicationFactor; }
  std::uint32_t maxNumberOfShards() const { return _maxNumberOfShards; }
  std::uint32_t minReplicationFactor() const { return _minReplicationFactor; }
  std::uint32_t maxReplicationFactor() const { return _maxReplicationFactor; }
  double indexCreationTimeout() const { return _indexCreationTimeout; }
  bool forceOneShard() const { return _forceOneShard; }

  std::shared_ptr<HeartbeatThread> heartbeatThread();

  ClusterInfo& clusterInfo();

 protected:
  void startHeartbeatThread(AgencyCallbackRegistry* agencyCallbackRegistry,
                            uint64_t interval_ms, uint64_t maxFailsBeforeWarning,
                            std::string const& endpoints);
  
  void shutdownHeartbeatThread();

 private:
  void reportRole(ServerState::RoleEnum);

  std::vector<std::string> _agencyEndpoints;
  std::string _agencyPrefix;
  std::string _myRole;
  std::string _myEndpoint;
  std::string _myAdvertisedEndpoint;
  std::uint32_t _writeConcern = 1;             // write concern
  std::uint32_t _defaultReplicationFactor = 0; // a value of 0 means it will use the min replication factor 
  std::uint32_t _systemReplicationFactor = 2;
  std::uint32_t _minReplicationFactor = 1;     // minimum replication factor (0 = unrestricted)
  std::uint32_t _maxReplicationFactor = 10;    // maximum replication factor (0 = unrestricted)
  std::uint32_t _maxNumberOfShards = 1000;     // maximum number of shards (0 = unrestricted)
  bool _createWaitsForSyncReplication = true;
  bool _forceOneShard = false;
  bool _unregisterOnShutdown = false;
  bool _enableCluster = false;
  bool _requirePersistedId = false;
  double _indexCreationTimeout = 3600.0;
  std::unique_ptr<ClusterInfo> _clusterInfo;
  std::shared_ptr<HeartbeatThread> _heartbeatThread;
  uint64_t _heartbeatInterval = 0;
  std::unique_ptr<AgencyCallbackRegistry> _agencyCallbackRegistry;
  ServerState::RoleEnum _requestedRole = ServerState::RoleEnum::ROLE_UNDEFINED;
};

}  // namespace arangodb

#endif

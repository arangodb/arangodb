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
  void beginShutdown() override final;
  void unprepare() override final;

  std::vector<std::string> agencyEndpoints() const { return _agencyEndpoints; }

  std::string agencyPrefix() const { return _agencyPrefix; }

  /// @return role argument as it was supplied by a user
  std::string const& myRole() const noexcept { return _myRole; }

  void syncDBServerStatusQuo();

 protected:
  void startHeartbeatThread(AgencyCallbackRegistry* agencyCallbackRegistry,
                            uint64_t interval_ms, uint64_t maxFailsBeforeWarning,
                            const std::string& endpoints);

 private:
  std::vector<std::string> _agencyEndpoints;
  std::string _agencyPrefix;
  std::string _myRole;
  std::string _myEndpoint;
  std::string _myAdvertisedEndpoint;
  std::uint32_t _systemReplicationFactor = 2;
  std::uint32_t _defaultReplicationFactor = 1; // default replication factor for non-system collections
  bool _createWaitsForSyncReplication = true;
  double _indexCreationTimeout = 3600.0;

  void reportRole(ServerState::RoleEnum);

 public:
  AgencyCallbackRegistry* agencyCallbackRegistry() const {
    return _agencyCallbackRegistry.get();
  }

  std::string const agencyCallbacksPath() const {
    return "/_api/agency/agency-callbacks";
  };

  std::string const clusterRestPath() const { return "/_api/cluster"; };

  void setUnregisterOnShutdown(bool);
  bool createWaitsForSyncReplication() const {
    return _createWaitsForSyncReplication;
  };
  double indexCreationTimeout() const { return _indexCreationTimeout; }
  uint32_t systemReplicationFactor() { return _systemReplicationFactor; };
  std::size_t defaultReplicationFactor() { return _defaultReplicationFactor; };

  void stop() override final;

 private:
  bool _unregisterOnShutdown;
  bool _enableCluster;
  bool _requirePersistedId;
  std::shared_ptr<HeartbeatThread> _heartbeatThread;
  uint64_t _heartbeatInterval;
  std::unique_ptr<AgencyCallbackRegistry> _agencyCallbackRegistry;
  ServerState::RoleEnum _requestedRole;
};

}  // namespace arangodb

#endif

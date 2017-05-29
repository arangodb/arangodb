////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
  explicit ClusterFeature(application_features::ApplicationServer*);
  ~ClusterFeature();

 public:
  void collectOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void validateOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void prepare() override final;
  void start() override final;
  void unprepare() override final;

  std::string agencyPrefix() {
    return _agencyPrefix;
  }

  double syncReplTimeoutFactor() {
    return _syncReplTimeoutFactor;
  }

 private:
  std::vector<std::string> _agencyEndpoints;
  std::string _agencyPrefix;
  std::string _myLocalInfo;
  std::string _myId;
  std::string _myRole;
  std::string _myAddress;
  std::string _dataPath;
  std::string _logPath;
  std::string _arangodPath;
  std::string _dbserverConfig;
  std::string _coordinatorConfig;
  uint32_t _systemReplicationFactor = 2;
  bool _createWaitsForSyncReplication = true;
  double _syncReplTimeoutFactor = 1.0;

 private:
  void reportRole(ServerState::RoleEnum);

 public:
  AgencyCallbackRegistry* agencyCallbackRegistry() const {
    return _agencyCallbackRegistry.get();
  }

  std::string const agencyCallbacksPath() {
    return "/_api/agency/agency-callbacks";
  };

  void setUnregisterOnShutdown(bool);
  bool createWaitsForSyncReplication() { return _createWaitsForSyncReplication; };

  void stop() override final;

 private:
  bool _unregisterOnShutdown;
  bool _enableCluster;
  std::shared_ptr<HeartbeatThread> _heartbeatThread;
  uint64_t _heartbeatInterval;
  bool _disableHeartbeat;
  std::unique_ptr<AgencyCallbackRegistry> _agencyCallbackRegistry;
  ServerState::RoleEnum _requestedRole;
};
}

#endif

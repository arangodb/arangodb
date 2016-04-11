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

namespace arangodb {
class HeartbeatThread;

class ClusterFeature : public application_features::ApplicationFeature {
 public:
  ClusterFeature(application_features::ApplicationServer*);
  ~ClusterFeature();

 public:
  // disable the heartbeat (used for testing)
#warning TODO
  // void disableHeartbeat() { _disableHeartbeat = true; }

  // whether or not the cluster is enabled
#warning TODO
  // inline bool enabled() const { return _enableCluster; }

 public:
  void collectOptions(std::shared_ptr<options::ProgramOptions>) override;
  void validateOptions(std::shared_ptr<options::ProgramOptions>) override;
  void prepare() override;
  void start() override;
  void stop() override;

private:
  std::vector<std::string> _agencyEndpoints;
  std::string _agencyPrefix;
  std::string _myLocalInfo;
  std::string _myId;
  std::string _myRole;
  std::string _myAddress;
  std::string _username;
  std::string _password;
  std::string _dataPath;
  std::string _logPath;
  std::string _arangodPath;
  std::string _agentPath;
  std::string _dbserverConfig;
  std::string _coordinatorConfig;
  bool _dispatcherFrontend;
  bool _kickstarter;


 private:
  bool _enableCluster;
  HeartbeatThread* _heartbeatThread;
  uint64_t _heartbeatInterval;
  bool _disableHeartbeat;
};
}

#endif

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGODB_REPLICATION_REPLICATION_FEATURE_H
#define ARANGODB_REPLICATION_REPLICATION_FEATURE_H 1

#include "ApplicationFeatures/ApplicationFeature.h"
#include "Cluster/ServerState.h"

struct TRI_vocbase_t;

namespace arangodb {

class GlobalReplicationApplier;
class GeneralResponse;

class ReplicationFeature final
    : public application_features::ApplicationFeature {
 public:
  explicit ReplicationFeature(application_features::ApplicationServer& server);

  void collectOptions(
      std::shared_ptr<options::ProgramOptions> options) override final;
  void validateOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void prepare() override final;
  void start() override final;
  void beginShutdown() override final;
  void stop() override final;
  void unprepare() override final;

  /// @brief return a pointer to the global replication applier
  GlobalReplicationApplier* globalReplicationApplier() const {
    TRI_ASSERT(_globalReplicationApplier != nullptr);
    return _globalReplicationApplier.get();
  }

  /// @brief disable replication appliers
  void disableReplicationApplier() {
    _replicationApplierAutoStart = false;
  }

  /// @brief start the replication applier for a single database
  void startApplier(TRI_vocbase_t* vocbase);

  /// @brief stop the replication applier for a single database
  void stopApplier(TRI_vocbase_t* vocbase);

  /// @brief automatic failover of replication using the agency
  bool isActiveFailoverEnabled() const {
    return _enableActiveFailover;
  }

  /// @brief set the x-arango-endpoint header
  static void setEndpointHeader(GeneralResponse*, arangodb::ServerState::Mode);

  /// @brief fill a response object with correct response for a follower
  static void prepareFollowerResponse(GeneralResponse*,
                                      arangodb::ServerState::Mode);

  static ReplicationFeature* INSTANCE;

 private:
  bool _replicationApplierAutoStart;

  /// Enable the active failover
  bool _enableActiveFailover;

  std::unique_ptr<GlobalReplicationApplier> _globalReplicationApplier;
};

}

#endif
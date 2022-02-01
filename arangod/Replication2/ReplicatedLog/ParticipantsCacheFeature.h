////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Alex Petenchea
/// @author Manuel Pöter
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "ApplicationFeatures/ApplicationFeature.h"
#include "Replication2/ReplicatedLog/FailureOracle.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "ApplicationFeatures/CommunicationFeaturePhase.h"
#include "FeaturePhases/DatabaseFeaturePhase.h"
#include "Cluster/AgencyCallbackRegistry.h"
#include "Cluster/AgencyCallback.h"
#include "Cluster/ServerState.h"
#include "Logger/LogMacros.h"

#include <unordered_map>

namespace arangodb::replication2 {

class ParticipantsCacheFeature final
    : public ArangodFeature,
      public FailureOracle,
      public std::enable_shared_from_this<ParticipantsCacheFeature> {
  static const std::string_view kParticipantsHealthPath;

 public:
  static constexpr std::string_view name() noexcept {
    return "ParticipantsCache";
  }

  explicit ParticipantsCacheFeature(Server& server);

  ~ParticipantsCacheFeature() override = default;

  void prepare() override;

  auto isServerFailed(std::string_view serverId) const noexcept
      -> bool override;

  void start() override;
  void stop() override;

 private:
  std::unordered_map<std::string, bool> isFailed;
  // ApplicationServer& agencyCallbackRegistry;
  // application_features::ApplicationServer& server;
  std::shared_ptr<AgencyCallback> agencyCallback;
};

}  // namespace arangodb::replication2

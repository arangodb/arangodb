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
/// @author Alexandru Petenchea
////////////////////////////////////////////////////////////////////////////////

#include "Replication2/ReplicatedState/ReplicatedStateFeature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ServerState.h"
#include "Cluster/MaintenanceFeature.h"
#include "RestServer/DatabaseFeature.h"

#include "Replication2/StateMachines/Document/DocumentStateMachineFeature.h"
#include "Replication2/StateMachines/Document/DocumentStateMachine.h"
#include "Replication2/StateMachines/Document/DocumentStateHandlersFactory.h"

using namespace arangodb::replication2::replicated_state::document;

void DocumentStateMachineFeature::prepare() {
  bool const enabled = ServerState::instance()->isDBServer();
  setEnabled(enabled);
}

void DocumentStateMachineFeature::start() {
  ArangodServer& s = server();
  auto& replicatedStateFeature = s.getFeature<ReplicatedStateAppFeature>();
  auto& clusterFeature = s.getFeature<ClusterFeature>();
  auto& maintenanceFeature = s.getFeature<MaintenanceFeature>();
  auto& databaseFeature = s.getFeature<DatabaseFeature>();

  replicatedStateFeature.registerStateType<DocumentState>(
      std::string{DocumentState::NAME},
      std::make_shared<DocumentStateHandlersFactory>(
          s, clusterFeature, maintenanceFeature, databaseFeature));
}

DocumentStateMachineFeature::DocumentStateMachineFeature(Server& server)
    : ArangodFeature{server, *this} {
  setOptional(true);
  startsAfter<ClusterFeature>();
  startsAfter<MaintenanceFeature>();
  startsAfter<ReplicatedStateAppFeature>();
  onlyEnabledWith<ClusterFeature>();
  onlyEnabledWith<ReplicatedStateAppFeature>();
}

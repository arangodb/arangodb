////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2022-2022 ArangoDB GmbH, Cologne, Germany
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

#include "Replication2/StateMachines/Document/DocumentStateHandlersFactory.h"

#include "Replication2/StateMachines/Document/DocumentStateAgencyHandler.h"
#include "Replication2/StateMachines/Document/DocumentStateShardHandler.h"
#include "Replication2/StateMachines/Document/DocumentStateTransactionHandler.h"

namespace arangodb::replication2::replicated_state::document {

DocumentStateHandlersFactory::DocumentStateHandlersFactory(
    ArangodServer& server, ClusterFeature& clusterFeature,
    MaintenanceFeature& maintenaceFeature, DatabaseFeature& databaseFeature)
    : _server(server),
      _clusterFeature(clusterFeature),
      _maintenanceFeature(maintenaceFeature),
      _databaseFeature(databaseFeature) {}

auto DocumentStateHandlersFactory::createAgencyHandler(GlobalLogIdentifier gid)
    -> std::shared_ptr<IDocumentStateAgencyHandler> {
  return std::make_shared<DocumentStateAgencyHandler>(std::move(gid), _server,
                                                      _clusterFeature);
}

auto DocumentStateHandlersFactory::createShardHandler(GlobalLogIdentifier gid)
    -> std::shared_ptr<IDocumentStateShardHandler> {
  return std::make_shared<DocumentStateShardHandler>(std::move(gid),
                                                     _maintenanceFeature);
}

auto DocumentStateHandlersFactory::createTransactionHandler(
    GlobalLogIdentifier gid)
    -> std::unique_ptr<IDocumentStateTransactionHandler> {
  return std::make_unique<DocumentStateTransactionHandler>(std::move(gid),
                                                           _databaseFeature);
}

}  // namespace arangodb::replication2::replicated_state::document

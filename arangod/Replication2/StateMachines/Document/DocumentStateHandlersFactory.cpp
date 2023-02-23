////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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

#include "Replication2/StateMachines/Document/DocumentStateHandlersFactory.h"

#include "Replication2/StateMachines/Document/CollectionReader.h"
#include "Replication2/StateMachines/Document/DocumentStateNetworkHandler.h"
#include "Replication2/StateMachines/Document/DocumentStateShardHandler.h"
#include "Replication2/StateMachines/Document/DocumentStateSnapshotHandler.h"
#include "Replication2/StateMachines/Document/DocumentStateTransactionHandler.h"
#include "Replication2/StateMachines/Document/DocumentStateTransaction.h"

#include "Cluster/AgencyCache.h"
#include "RestServer/DatabaseFeature.h"
#include "Transaction/ReplicatedContext.h"

namespace arangodb::replication2::replicated_state::document {
DocumentStateHandlersFactory::DocumentStateHandlersFactory(
    network::ConnectionPool* connectionPool,
    MaintenanceFeature& maintenanceFeature)
    : _connectionPool(connectionPool),
      _maintenanceFeature(maintenanceFeature) {}

auto DocumentStateHandlersFactory::createShardHandler(GlobalLogIdentifier gid)
    -> std::shared_ptr<IDocumentStateShardHandler> {
  return std::make_shared<DocumentStateShardHandler>(std::move(gid),
                                                     _maintenanceFeature);
}

auto DocumentStateHandlersFactory::createSnapshotHandler(
    TRI_vocbase_t& vocbase, GlobalLogIdentifier const& gid)
    -> std::unique_ptr<IDocumentStateSnapshotHandler> {
  return std::make_unique<DocumentStateSnapshotHandler>(
      std::make_unique<DatabaseSnapshotFactory>(vocbase));
}

auto DocumentStateHandlersFactory::createTransactionHandler(
    TRI_vocbase_t& vocbase, GlobalLogIdentifier gid)
    -> std::unique_ptr<IDocumentStateTransactionHandler> {
  return std::make_unique<DocumentStateTransactionHandler>(
      std::move(gid), &vocbase, shared_from_this());
}

auto DocumentStateHandlersFactory::createTransaction(
    DocumentLogEntry const& doc, TRI_vocbase_t& vocbase)
    -> std::shared_ptr<IDocumentStateTransaction> {
  auto options = transaction::Options();
  options.isFollowerTransaction = true;
  options.allowImplicitCollectionsForWrite = true;

  auto state = std::make_shared<SimpleRocksDBTransactionState>(vocbase, doc.tid,
                                                               options);

  auto ctx = std::make_shared<transaction::ReplicatedContext>(doc.tid, state);

  auto methods = std::make_unique<transaction::Methods>(
      std::move(ctx), doc.shardId,
      doc.operation == OperationType::kTruncate ? AccessMode::Type::EXCLUSIVE
                                                : AccessMode::Type::WRITE);
  methods->addHint(transaction::Hints::Hint::ALLOW_RANGE_DELETE);

  // TODO Why is GLOBAL_MANAGED necessary?
  methods->addHint(transaction::Hints::Hint::GLOBAL_MANAGED);

  auto res = methods->begin();
  if (res.fail()) {
    THROW_ARANGO_EXCEPTION(res);
  }

  return std::make_shared<DocumentStateTransaction>(std::move(methods));
}

auto DocumentStateHandlersFactory::createNetworkHandler(GlobalLogIdentifier gid)
    -> std::shared_ptr<IDocumentStateNetworkHandler> {
  return std::make_shared<DocumentStateNetworkHandler>(std::move(gid),
                                                       _connectionPool);
}

}  // namespace arangodb::replication2::replicated_state::document

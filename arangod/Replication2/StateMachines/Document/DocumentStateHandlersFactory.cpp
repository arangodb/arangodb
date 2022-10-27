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
#include "Replication2/StateMachines/Document/DocumentStateNetworkHandler.h"
#include "Replication2/StateMachines/Document/DocumentStateShardHandler.h"
#include "Replication2/StateMachines/Document/DocumentStateTransactionHandler.h"
#include "Replication2/StateMachines/Document/DocumentStateTransaction.h"

#include "Cluster/AgencyCache.h"
#include "RestServer/DatabaseFeature.h"
#include "Transaction/ReplicatedContext.h"

namespace arangodb::replication2::replicated_state::document {

DocumentStateHandlersFactory::DocumentStateHandlersFactory(
    ArangodServer& server, AgencyCache& agencyCache,
    network::ConnectionPool* connectionPool,
    MaintenanceFeature& maintenaceFeature, DatabaseFeature& databaseFeature)
    : _server(server),
      _agencyCache(agencyCache),
      _connectionPool(connectionPool),
      _maintenanceFeature(maintenaceFeature),
      _databaseFeature(databaseFeature) {}

auto DocumentStateHandlersFactory::createAgencyHandler(GlobalLogIdentifier gid)
    -> std::shared_ptr<IDocumentStateAgencyHandler> {
  return std::make_shared<DocumentStateAgencyHandler>(std::move(gid), _server,
                                                      _agencyCache);
}

auto DocumentStateHandlersFactory::createShardHandler(GlobalLogIdentifier gid)
    -> std::shared_ptr<IDocumentStateShardHandler> {
  return std::make_shared<DocumentStateShardHandler>(std::move(gid),
                                                     _maintenanceFeature);
}

auto DocumentStateHandlersFactory::createTransactionHandler(
    GlobalLogIdentifier gid)
    -> std::unique_ptr<IDocumentStateTransactionHandler> {
  try {
    return std::make_unique<DocumentStateTransactionHandler>(
        std::move(gid),
        std::make_unique<DatabaseGuard>(_databaseFeature, gid.database),
        shared_from_this());
  } catch (basics::Exception const& ex) {
    // TODO this is a temporary fix, see CINFRA-588
    if (ex.code() == TRI_ERROR_ARANGO_DATABASE_NOT_FOUND) {
      return nullptr;
    }
    throw;
  }
}

auto DocumentStateHandlersFactory::createTransaction(
    DocumentLogEntry const& doc, IDatabaseGuard const& dbGuard)
    -> std::shared_ptr<IDocumentStateTransaction> {
  TRI_ASSERT(doc.operation != OperationType::kCommit &&
             doc.operation != OperationType::kAbort);

  auto options = transaction::Options();
  options.isFollowerTransaction = true;
  options.allowImplicitCollectionsForWrite = true;

  auto state = std::make_shared<SimpleRocksDBTransactionState>(
      dbGuard.database(), doc.tid, options);

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

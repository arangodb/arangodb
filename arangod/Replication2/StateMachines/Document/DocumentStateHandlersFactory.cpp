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

#include "Replication2/StateMachines/Document/DocumentStateErrorHandler.h"
#include "Replication2/StateMachines/Document/DocumentStateNetworkHandler.h"
#include "Replication2/StateMachines/Document/DocumentStateShardHandler.h"
#include "Replication2/StateMachines/Document/DocumentStateSnapshotHandler.h"
#include "Replication2/StateMachines/Document/DocumentStateTransactionHandler.h"
#include "Replication2/StateMachines/Document/DocumentStateTransaction.h"
#include "Replication2/StateMachines/Document/MaintenanceActionExecutor.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "Logger/LogContextKeys.h"
#include "Transaction/OperationOrigin.h"
#include "Transaction/ReplicatedContext.h"

namespace arangodb::replication2::replicated_state::document {
DocumentStateHandlersFactory::DocumentStateHandlersFactory(
    network::ConnectionPool* connectionPool,
    MaintenanceFeature& maintenanceFeature, LoggerContext defaultLoggerContext)
    : _connectionPool(connectionPool),
      _maintenanceFeature(maintenanceFeature),
      _defaultLoggerContext(std::move(defaultLoggerContext)) {}

auto DocumentStateHandlersFactory::createShardHandler(TRI_vocbase_t& vocbase,
                                                      GlobalLogIdentifier gid)
    -> std::shared_ptr<IDocumentStateShardHandler> {
  auto maintenance = createMaintenanceActionExecutor(
      vocbase, gid, ServerState::instance()->getId());
  return std::make_shared<DocumentStateShardHandler>(vocbase, std::move(gid),
                                                     std::move(maintenance));
}

auto DocumentStateHandlersFactory::createSnapshotHandler(
    TRI_vocbase_t& vocbase, GlobalLogIdentifier gid)
    -> std::shared_ptr<IDocumentStateSnapshotHandler> {
  // TODO: this looks unsafe, because the vocbase that we have fetched above
  // is just a raw pointer. there may be a concurrent thread that deletes
  // the vocbase we just looked up.
  // this should be improved, by using `DatabaseFeature::useDatabase()`
  // instead, which returns a managed pointer.
  auto& ci = vocbase.server().getFeature<ClusterFeature>().clusterInfo();
  auto logger = createLogger(gid);
  return std::make_shared<DocumentStateSnapshotHandler>(
      std::make_unique<DatabaseSnapshotFactory>(vocbase), ci.rebootTracker(),
      std::move(gid), std::move(logger));
}

auto DocumentStateHandlersFactory::createTransactionHandler(
    TRI_vocbase_t& vocbase, GlobalLogIdentifier gid,
    std::shared_ptr<IDocumentStateShardHandler> shardHandler)
    -> std::unique_ptr<IDocumentStateTransactionHandler> {
  return std::make_unique<DocumentStateTransactionHandler>(
      gid, &vocbase, shared_from_this(), std::move(shardHandler));
}

auto DocumentStateHandlersFactory::createTransaction(
    TRI_vocbase_t& vocbase, TransactionId tid, ShardID const& shard,
    AccessMode::Type accessType) -> std::shared_ptr<IDocumentStateTransaction> {
  auto options = transaction::Options();
  options.isFollowerTransaction = true;
  options.allowImplicitCollectionsForWrite = true;

  auto origin = transaction::OperationOriginInternal{"replication transaction"};
  auto state = std::make_shared<SimpleRocksDBTransactionState>(vocbase, tid,
                                                               options, origin);

  auto ctx =
      std::make_shared<transaction::ReplicatedContext>(tid, state, origin);

  auto methods =
      std::make_unique<transaction::Methods>(std::move(ctx), shard, accessType);
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
  auto loggerContext = createLogger(gid);
  return std::make_shared<DocumentStateNetworkHandler>(
      std::move(gid), _connectionPool, std::move(loggerContext));
}

auto DocumentStateHandlersFactory::createMaintenanceActionExecutor(
    TRI_vocbase_t& vocbase, GlobalLogIdentifier gid, ServerID server)
    -> std::shared_ptr<IMaintenanceActionExecutor> {
  auto loggerContext = createLogger(gid);
  return std::make_shared<MaintenanceActionExecutor>(
      std::move(gid), std::move(server), _maintenanceFeature, vocbase,
      loggerContext);
}

auto DocumentStateHandlersFactory::createErrorHandler(GlobalLogIdentifier gid)
    -> std::shared_ptr<IDocumentStateErrorHandler> {
  return std::make_shared<DocumentStateErrorHandler>(
      createLogger(std::move(gid)));
}

auto DocumentStateHandlersFactory::createLogger(GlobalLogIdentifier gid)
    -> LoggerContext {
  return _defaultLoggerContext
      .with<logContextKeyDatabaseName>(std::move(gid.database))
      .with<logContextKeyLogId>(gid.id);
}

}  // namespace arangodb::replication2::replicated_state::document

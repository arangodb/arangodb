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

#include "Replication2/StateMachines/Document/DocumentStateStrategy.h"

#include <velocypack/Builder.h>

#include "Agency/AgencyStrings.h"
#include "Basics/ResultT.h"
#include "Cluster/ActionDescription.h"
#include "Cluster/AgencyCache.h"
#include "Cluster/CreateCollection.h"
#include "Cluster/Maintenance.h"
#include "Cluster/MaintenanceFeature.h"
#include "Cluster/MaintenanceStrings.h"
#include "Cluster/ServerState.h"
#include "RestServer/DatabaseFeature.h"
#include "Replication2/ReplicatedLog/LogCommon.h"
#include "RocksDBEngine/SimpleRocksDBTransactionState.h"
#include "Transaction/ManagerFeature.h"
#include "Transaction/Options.h"
#include "Transaction/ReplicatedContext.h"

#include <fmt/core.h>

using namespace arangodb::replication2::replicated_state::document;

DocumentStateAgencyHandler::DocumentStateAgencyHandler(
    GlobalLogIdentifier gid, ArangodServer& server,
    ClusterFeature& clusterFeature)
    : _gid(std::move(gid)), _server(server), _clusterFeature(clusterFeature) {}

auto DocumentStateAgencyHandler::getCollectionPlan(
    std::string const& collectionId) -> std::shared_ptr<VPackBuilder> {
  auto builder = std::make_shared<VPackBuilder>();
  auto path = cluster::paths::aliases::plan()
                  ->collections()
                  ->database(_gid.database)
                  ->collection(collectionId);
  _clusterFeature.agencyCache().get(*builder, path);

  ADB_PROD_ASSERT(!builder->isEmpty())
      << "Could not get collection from plan " << path->str();

  return builder;
}

auto DocumentStateAgencyHandler::reportShardInCurrent(
    std::string const& collectionId, std::string const& shardId,
    std::shared_ptr<velocypack::Builder> const& properties) -> Result {
  VPackBuilder localShard;
  {
    VPackObjectBuilder ob(&localShard);

    localShard.add(StaticStrings::Error, VPackValue(false));
    localShard.add(StaticStrings::ErrorMessage, VPackValue(std::string()));
    localShard.add(StaticStrings::ErrorNum, VPackValue(0));
    localShard.add(maintenance::SERVERS, VPackSlice::emptyArraySlice());
    localShard.add(StaticStrings::FailoverCandidates,
                   VPackSlice::emptyArraySlice());
  }

  AgencyOperation op(consensus::CURRENT_COLLECTIONS + _gid.database + "/" +
                         collectionId + "/" + shardId,
                     AgencyValueOperationType::SET, localShard.slice());
  AgencyPrecondition pr(consensus::PLAN_COLLECTIONS + _gid.database + "/" +
                            collectionId + "/shards/" + shardId,
                        AgencyPrecondition::Type::VALUE,
                        VPackSlice::emptyArraySlice());

  AgencyComm comm(_server);
  AgencyWriteTransaction currentTransaction(op, pr);
  AgencyCommResult r = comm.sendTransactionWithFailover(currentTransaction);
  return r.asResult();
}

DocumentStateShardHandler::DocumentStateShardHandler(
    GlobalLogIdentifier gid, MaintenanceFeature& maintenanceFeature)
    : _gid(std::move(gid)), _maintenanceFeature(maintenanceFeature) {}

auto DocumentStateShardHandler::stateIdToShardId(LogId logId) -> std::string {
  return fmt::format("s{}", logId);
}

auto DocumentStateShardHandler::createLocalShard(
    std::string const& collectionId,
    std::shared_ptr<velocypack::Builder> const& properties)
    -> ResultT<std::string> {
  auto shardId = stateIdToShardId(_gid.id);
  auto serverId = ServerState::instance()->getId();

  maintenance::ActionDescription actionDescription(
      std::map<std::string, std::string>{
          {maintenance::NAME, maintenance::CREATE_COLLECTION},
          {maintenance::COLLECTION, collectionId},
          {maintenance::SHARD, shardId},
          {maintenance::DATABASE, _gid.database},
          {maintenance::SERVER_ID, std::move(serverId)},
          {maintenance::THE_LEADER, "replication2"},
      },
      maintenance::HIGHER_PRIORITY, false, properties);

  maintenance::CreateCollection collectionCreator(_maintenanceFeature,
                                                  actionDescription);
  bool work = collectionCreator.first();
  if (work) {
    return ResultT<std::string>::error(
        TRI_ERROR_INTERNAL, fmt::format("Cannot create shard ID {}", shardId));
  }

  return ResultT<std::string>::success(std::move(shardId));
}

DocumentStateTransaction::DocumentStateTransaction(
    std::unique_ptr<transaction::Methods> methods)
    : _methods(std::move(methods)) {}

auto DocumentStateTransaction::apply(DocumentLogEntry const& entry)
    -> futures::Future<Result> {
  // TODO revisit checkUniqueConstraintsInPreflight and waitForSync
  auto opOptions = OperationOptions();
  opOptions.silent = true;
  opOptions.ignoreRevs = true;
  opOptions.isRestore = true;
  // opOptions.checkUniqueConstraintsInPreflight = true;
  opOptions.validate = false;
  opOptions.waitForSync = false;
  opOptions.indexOperationMode = IndexOperationMode::internal;

  auto fut =
      futures::Future<OperationResult>{std::in_place, Result{}, opOptions};
  switch (entry.operation) {
    case OperationType::kInsert:
      fut = _methods->insertAsync(entry.shardId, entry.data.slice(), opOptions);
      break;
    case OperationType::kUpdate:
      fut = _methods->updateAsync(entry.shardId, entry.data.slice(), opOptions);
      break;
    case OperationType::kReplace:
      fut =
          _methods->replaceAsync(entry.shardId, entry.data.slice(), opOptions);
      break;
    case OperationType::kRemove:
      fut = _methods->removeAsync(entry.shardId, entry.data.slice(), opOptions);
      break;
    case OperationType::kTruncate:
      // TODO Think about correctness and efficiency.
      fut = _methods->truncateAsync(entry.shardId, opOptions);
      break;
    default:
      return Result{
          TRI_ERROR_TRANSACTION_INTERNAL,
          fmt::format("Transaction of type {} with ID {} could not be applied",
                      to_string(entry.operation), entry.tid.id())};
  }

  TRI_ASSERT(fut.isReady()) << entry;
  return std::move(fut).get().result;
}

auto DocumentStateTransaction::commit() -> futures::Future<Result> {
  return _methods->commitAsync();
}

auto DocumentStateTransaction::abort() -> futures::Future<Result> {
  return _methods->abortAsync();
}

DocumentStateTransactionHandler::DocumentStateTransactionHandler(
    GlobalLogIdentifier gid, DatabaseFeature& databaseFeature)
    : _gid(std::move(gid)), _db(databaseFeature, _gid.database) {}

auto DocumentStateTransactionHandler::getTrx(TransactionId tid)
    -> std::shared_ptr<DocumentStateTransaction> {
  auto it = _transactions.find(tid);
  if (it == _transactions.end()) {
    return nullptr;
  }
  return it->second;
}

auto DocumentStateTransactionHandler::applyTransaction(DocumentLogEntry doc)
    -> Result {
  auto fut = futures::Future<Result>{Result{}};

  if (doc.operation == OperationType::kAbortAllOngoingTrx) {
    _transactions.clear();
    return Result{};
  }

  try {
    auto trx = ensureTransaction(doc);
    TRI_ASSERT(trx != nullptr);
    switch (doc.operation) {
      case OperationType::kInsert:
      case OperationType::kUpdate:
      case OperationType::kReplace:
      case OperationType::kRemove:
      case OperationType::kTruncate:
        fut = trx->apply(doc);
        break;
      case OperationType::kCommit:
        fut = trx->commit();
        removeTransaction(doc.tid);
        break;
      case OperationType::kAbort:
        fut = trx->abort();
        removeTransaction(doc.tid);
        break;
      case OperationType::kAbortAllOngoingTrx:
        LOG_DEVEL << "aborting all " << doc.tid;
        break;
      default:
        THROW_ARANGO_EXCEPTION(TRI_ERROR_TRANSACTION_DISALLOWED_OPERATION);
    }
    ADB_PROD_ASSERT(fut.isReady()) << doc;
    return fut.get();
  } catch (basics::Exception& e) {
    return Result{e.code(), e.message()};
  } catch (std::exception& e) {
    return Result{TRI_ERROR_TRANSACTION_INTERNAL, e.what()};
  }
}

auto DocumentStateTransactionHandler::ensureTransaction(DocumentLogEntry doc)
    -> std::shared_ptr<IDocumentStateTransaction> {
  auto tid = doc.tid;
  auto trx = getTrx(tid);
  if (trx != nullptr) {
    return trx;
  }

  TRI_ASSERT(doc.operation != OperationType::kCommit &&
             doc.operation != OperationType::kAbort);

  auto options = transaction::Options();
  options.isFollowerTransaction = true;
  options.allowImplicitCollectionsForWrite = true;

  auto state = std::make_shared<SimpleRocksDBTransactionState>(_db.database(),
                                                               tid, options);

  auto ctx = std::make_shared<transaction::ReplicatedContext>(tid, state);

  auto methods = std::make_unique<transaction::Methods>(
      std::move(ctx), doc.shardId, AccessMode::Type::WRITE);

  // TODO Why is GLOBAL_MANAGED necessary?
  methods->addHint(transaction::Hints::Hint::GLOBAL_MANAGED);

  auto res = methods->begin();
  if (res.fail()) {
    THROW_ARANGO_EXCEPTION(res);
  }

  trx = std::make_shared<DocumentStateTransaction>(std::move(methods));
  _transactions.emplace(tid, trx);

  return trx;
}

void DocumentStateTransactionHandler::removeTransaction(TransactionId tid) {
  _transactions.erase(tid);
}

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

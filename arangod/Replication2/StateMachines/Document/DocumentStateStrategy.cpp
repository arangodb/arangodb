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

#include "DocumentStateStrategy.h"
#include "DocumentLogEntry.h"

#include <velocypack/Builder.h>

#include "Agency/AgencyStrings.h"
#include "Basics/ResultT.h"
#include "Basics/StringUtils.h"
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
#include "Transaction/SmartContext.h"

#include <fmt/core.h>

using namespace arangodb::replication2::replicated_state::document;

DocumentStateAgencyHandler::DocumentStateAgencyHandler(ArangodServer& server,
                                                       AgencyCache& agencyCache)
    : _server(server), _agencyCache(agencyCache){};

auto DocumentStateAgencyHandler::getCollectionPlan(
    std::string const& database, std::string const& collectionId)
    -> std::shared_ptr<VPackBuilder> {
  auto builder = std::make_shared<VPackBuilder>();
  auto path = cluster::paths::aliases::plan()
                  ->collections()
                  ->database(database)
                  ->collection(collectionId);
  _agencyCache.get(*builder, path);

  // TODO better error handling
  // Maybe check the cache again after some time or log additional information
  TRI_ASSERT(!builder->isEmpty());

  return builder;
}

auto DocumentStateAgencyHandler::reportShardInCurrent(
    std::string const& database, std::string const& collectionId,
    std::string const& shardId,
    std::shared_ptr<velocypack::Builder> const& properties) -> Result {
  auto participants = properties->slice().get(maintenance::SHARDS).get(shardId);

  VPackBuilder localShard;
  {
    VPackObjectBuilder ob(&localShard);

    localShard.add(StaticStrings::Error, VPackValue(false));
    localShard.add(StaticStrings::ErrorMessage, VPackValue(std::string()));
    localShard.add(StaticStrings::ErrorNum, VPackValue(0));
    localShard.add(maintenance::SERVERS, participants);
    localShard.add(StaticStrings::FailoverCandidates, participants);
  }

  AgencyOperation op(consensus::CURRENT_COLLECTIONS + database + "/" +
                         collectionId + "/" + shardId,
                     AgencyValueOperationType::SET, localShard.slice());
  AgencyPrecondition pr(consensus::PLAN_COLLECTIONS + database + "/" +
                            collectionId + "/shards/" + shardId,
                        AgencyPrecondition::Type::VALUE, participants);

  AgencyComm comm(_server);
  AgencyWriteTransaction currentTransaction(op, pr);
  AgencyCommResult r = comm.sendTransactionWithFailover(currentTransaction);
  return r.asResult();
}

DocumentStateShardHandler::DocumentStateShardHandler(
    MaintenanceFeature& maintenanceFeature)
    : _maintenanceFeature(maintenanceFeature){};

auto DocumentStateShardHandler::stateIdToShardId(LogId logId) -> std::string {
  return fmt::format("s{}", logId);
}

auto DocumentStateShardHandler::createLocalShard(
    GlobalLogIdentifier const& gid, std::string const& collectionId,
    std::shared_ptr<velocypack::Builder> const& properties)
    -> ResultT<std::string> {
  auto shardId = stateIdToShardId(gid.id);

  // For the moment, use the shard information to get the leader
  auto participants = properties->slice().get(maintenance::SHARDS).get(shardId);
  TRI_ASSERT(participants.isArray());
  auto leaderId = participants[0].toString();
  auto serverId = ServerState::instance()->getId();
  auto shouldBeLeading = leaderId == serverId;

  maintenance::ActionDescription actionDescriptions(
      std::map<std::string, std::string>{
          {maintenance::NAME, maintenance::CREATE_COLLECTION},
          {maintenance::COLLECTION, collectionId},
          {maintenance::SHARD, shardId},
          {maintenance::DATABASE, gid.database},
          {maintenance::SERVER_ID, std::move(serverId)},
          {maintenance::THE_LEADER,
           shouldBeLeading ? "" : std::move(leaderId)}},
      shouldBeLeading ? maintenance::LEADER_PRIORITY
                      : maintenance::FOLLOWER_PRIORITY,
      false, properties);

  maintenance::CreateCollection collectionCreator(_maintenanceFeature,
                                                  actionDescriptions);
  bool work = collectionCreator.first();
  if (work) {
    return ResultT<std::string>::error(
        TRI_ERROR_INTERNAL, fmt::format("Cannot create shard ID {}", shardId));
  }

  return ResultT<std::string>::success(std::move(shardId));
}

DocumentStateTransaction::DocumentStateTransaction(
    TRI_vocbase_t* vocbase, DocumentLogEntry const& entry)
    : _entries{1, entry},
      _methods(nullptr),
      _results{},
      _shardId{entry.shardId},
      _tid{entry.tid} {
  _options = transaction::Options();
  _options.isReplication2Transaction = true;
  _options.isFollowerTransaction = true;
  _options.allowImplicitCollectionsForWrite = true;
  _state =
      std::make_shared<SimpleRocksDBTransactionState>(*vocbase, _tid, _options);
}

auto DocumentStateTransaction::getShardId() const -> ShardID {
  return _shardId;
}

auto DocumentStateTransaction::getOptions() const -> transaction::Options {
  return _options;
}

auto DocumentStateTransaction::getTid() const -> TransactionId { return _tid; }

auto DocumentStateTransaction::getLastOperation() const -> OperationType {
  return getLastEntry().operation;
}

auto DocumentStateTransaction::getState() const
    -> std::shared_ptr<TransactionState> {
  return _state;
}

auto DocumentStateTransaction::getLastPayload() const -> VPackSlice {
  return getLastEntry().data.slice();
}

auto DocumentStateTransaction::getMethods() const
    -> std::shared_ptr<transaction::Methods> {
  return _methods;
}

auto DocumentStateTransaction::getResult() const -> OperationResult const& {
  for (auto const& it : _results) {
    if (it.fail()) {
      return it;
    }
  }
  return _results.back();
}

auto DocumentStateTransaction::getLastEntry() const -> DocumentLogEntry {
  TRI_ASSERT(!_entries.empty());
  return _entries.back();
}

auto DocumentStateTransaction::getTrxCount() const -> std::size_t {
  return _entries.size();
}

void DocumentStateTransaction::setMethods(
    std::shared_ptr<transaction::Methods> methods) {
  _methods = std::move(methods);
}

void DocumentStateTransaction::appendResult(OperationResult result) {
  _results.push_back(std::move(result));
}

void DocumentStateTransaction::appendEntry(DocumentLogEntry entry) {
  _entries.push_back(std::move(entry));
}

DocumentStateTransactionHandler::DocumentStateTransactionHandler(
    DatabaseFeature& databaseFeature)
    : _databaseFeature(databaseFeature), _vocbase(nullptr) {}

void DocumentStateTransactionHandler::setDatabase(std::string const& database) {
  _vocbase = _databaseFeature.useDatabase(database);
  TRI_ASSERT(_vocbase != nullptr);
}

auto DocumentStateTransactionHandler::getTrx(TransactionId tid)
    -> std::shared_ptr<DocumentStateTransaction> {
  auto it = _transactions.find(tid);
  if (it == _transactions.end()) {
    return nullptr;
  }
  return it->second;
}

auto DocumentStateTransactionHandler::ensureTransaction(DocumentLogEntry entry)
    -> std::shared_ptr<IDocumentStateTransaction> {
  auto tid = entry.tid;
  if (auto trx = getTrx(tid); trx != nullptr) {
    trx->appendEntry(std::move(entry));
    return trx;
  }

  TRI_ASSERT(_vocbase != nullptr);
  auto trx =
      std::make_shared<DocumentStateTransaction>(_vocbase, std::move(entry));
  _transactions.emplace(tid, trx);
  return trx;
}

auto DocumentStateTransactionHandler::initTransaction(TransactionId tid)
    -> Result {
  if (auto trx = getTrx(tid); trx != nullptr) {
    transaction::Hints hints;
    hints.set(transaction::Hints::Hint::GLOBAL_MANAGED);
    auto state = trx->getState();
    state->setWriteAccessType();

    if (trx->getTrxCount() > 1) {
      // Streaming transaction already started, don't start it again.
      return Result{};
    }
    return state->beginTransaction(hints);
  }
  return Result{TRI_ERROR_TRANSACTION_NOT_FOUND,
                fmt::format("Could not find transaction ID {}", tid.id())};
}

auto DocumentStateTransactionHandler::startTransaction(TransactionId tid)
    -> Result {
  auto trx = getTrx(tid);
  if (trx == nullptr) {
    return Result{TRI_ERROR_TRANSACTION_NOT_FOUND,
                  fmt::format("Could not find transaction ID {}", tid.id())};
  }

  if (trx->getTrxCount() > 1) {
    // Streaming transaction already started.
    return Result{};
  }

  auto ctx = std::make_shared<transaction::ManagedContext>(tid, trx->getState(),
                                                           false, false, true);

  std::vector<std::string> const readCollections{};
  std::vector<std::string> const writeCollections = {
      std::string(trx->getShardId())};
  std::vector<std::string> const exclusiveCollections{};

  auto methods = std::make_shared<transaction::Methods>(
      ctx, readCollections, writeCollections, exclusiveCollections,
      trx->getOptions());

  auto res = methods->begin();
  trx->setMethods(std::move(methods));
  return res;
}

auto DocumentStateTransactionHandler::applyTransaction(TransactionId tid)
    -> futures::Future<Result> {
  auto trx = getTrx(tid);
  if (trx == nullptr) {
    return Result{TRI_ERROR_TRANSACTION_NOT_FOUND,
                  fmt::format("Could not find transaction ID {}", tid.id())};
  }

  auto methods = trx->getMethods();

  if (trx->getLastOperation() == OperationType::kInsert) {
    auto opOptions = OperationOptions();
    return methods
        ->insertAsync(trx->getShardId(), trx->getLastPayload(), opOptions)
        .thenValue([opOptions, trx = std::move(trx)](OperationResult&& opRes) {
          auto res = opRes.result;
          trx->appendResult(std::move(opRes));
          return res;
        });
  } else if (trx->getLastOperation() == OperationType::kUpdate) {
    auto opOptions = arangodb::OperationOptions();
    return methods
        ->updateAsync(trx->getShardId(), trx->getLastPayload(), opOptions)
        .thenValue([opOptions, trx = std::move(trx)](OperationResult&& opRes) {
          auto res = opRes.result;
          trx->appendResult(std::move(opRes));
          return res;
        });
  } else if (trx->getLastOperation() == OperationType::kReplace) {
    auto opOptions = arangodb::OperationOptions();
    return methods
        ->replaceAsync(trx->getShardId(), trx->getLastPayload(), opOptions)
        .thenValue([opOptions, trx = std::move(trx)](OperationResult&& opRes) {
          auto res = opRes.result;
          trx->appendResult(std::move(opRes));
          return res;
        });
  } else if (trx->getLastOperation() == OperationType::kRemove) {
    auto opOptions = arangodb::OperationOptions();
    return methods
        ->removeAsync(trx->getShardId(), trx->getLastPayload(), opOptions)
        .thenValue([opOptions, trx = std::move(trx)](OperationResult&& opRes) {
          auto res = opRes.result;
          trx->appendResult(std::move(opRes));
          return res;
        });
  } else if (trx->getLastOperation() == OperationType::kTruncate) {
    auto opOptions = arangodb::OperationOptions();
    return methods->truncateAsync(trx->getShardId(), opOptions)
        .thenValue([opOptions, trx = std::move(trx)](OperationResult&& opRes) {
          auto res = opRes.result;
          trx->appendResult(std::move(opRes));
          return res;
        });
  }

  return Result{
      TRI_ERROR_TRANSACTION_INTERNAL,
      fmt::format("Transaction of type {} with ID {} could not be applied",
                  to_string(trx->getLastOperation()), tid.id())};
}

auto DocumentStateTransactionHandler::finishTransaction(DocumentLogEntry entry)
    -> futures::Future<Result> {
  auto trx = getTrx(entry.tid);
  if (trx == nullptr) {
    return Result{
        TRI_ERROR_TRANSACTION_NOT_FOUND,
        fmt::format("Could not find transaction ID {}", entry.tid.id())};
  }

  auto methods = trx->getMethods();

  switch (entry.operation) {
    case OperationType::kCommit:
      // Try to commit the operation or abort in case it failed
      return methods->finishAsync(trx->getResult().result);
    case OperationType::kAbort:
      return methods->abortAsync();
    default:
      return Result{
          TRI_ERROR_TRANSACTION_INTERNAL,
          fmt::format("Transaction of type {} with ID {} could not be finished",
                      to_string(trx->getLastOperation()), entry.tid.id())};
  }
}

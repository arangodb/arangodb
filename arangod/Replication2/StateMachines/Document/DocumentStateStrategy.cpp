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

auto DocumentStateTransaction::apply() -> futures::Future<Result> {
  auto methods = getMethods();

  if (getLastOperation() == OperationType::kInsert) {
    auto opOptions = OperationOptions();
    return methods->insertAsync(getShardId(), getLastPayload(), opOptions)
        .thenValue(
            [opOptions, self = shared_from_this()](OperationResult&& opRes) {
              auto res = opRes.result;
              self->appendResult(std::move(opRes));
              return res;
            });
  } else if (getLastOperation() == OperationType::kUpdate) {
    auto opOptions = arangodb::OperationOptions();
    return methods->updateAsync(getShardId(), getLastPayload(), opOptions)
        .thenValue(
            [opOptions, self = shared_from_this()](OperationResult&& opRes) {
              auto res = opRes.result;
              self->appendResult(std::move(opRes));
              return res;
            });
  } else if (getLastOperation() == OperationType::kReplace) {
    auto opOptions = arangodb::OperationOptions();
    return methods->replaceAsync(getShardId(), getLastPayload(), opOptions)
        .thenValue(
            [opOptions, self = shared_from_this()](OperationResult&& opRes) {
              auto res = opRes.result;
              self->appendResult(std::move(opRes));
              return res;
            });
  } else if (getLastOperation() == OperationType::kRemove) {
    auto opOptions = arangodb::OperationOptions();
    return methods->removeAsync(getShardId(), getLastPayload(), opOptions)
        .thenValue(
            [opOptions, self = shared_from_this()](OperationResult&& opRes) {
              auto res = opRes.result;
              self->appendResult(std::move(opRes));
              return res;
            });
  } else if (getLastOperation() == OperationType::kTruncate) {
    auto opOptions = arangodb::OperationOptions();
    return methods->truncateAsync(getShardId(), opOptions)
        .thenValue(
            [opOptions, self = shared_from_this()](OperationResult&& opRes) {
              auto res = opRes.result;
              self->appendResult(std::move(opRes));
              return res;
            });
  }

  return Result{
      TRI_ERROR_TRANSACTION_INTERNAL,
      fmt::format("Transaction of type {} with ID {} could not be applied",
                  to_string(getLastOperation()), _tid.id())};
}

auto DocumentStateTransaction::finish() -> futures::Future<Result> {
  auto methods = getMethods();

  switch (getLastOperation()) {
    case OperationType::kCommit:
      // Try to commit the operation or abort in case it failed
      return methods->finishAsync(getResult().result);
    case OperationType::kAbort:
      return methods->abortAsync();
    default:
      return Result{
          TRI_ERROR_TRANSACTION_INTERNAL,
          fmt::format("Transaction of type {} with ID {} could not be finished",
                      to_string(getLastOperation()), getLastEntry().tid.id())};
  }
}

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
    GlobalLogIdentifier gid, DatabaseFeature& databaseFeature)
    : _gid(std::move(gid)),
      _vocbase(databaseFeature.useDatabase(_gid.database)) {
  ADB_PROD_ASSERT(_vocbase != nullptr) << _gid;
}

DocumentStateTransactionHandler::~DocumentStateTransactionHandler() {
  if (_vocbase) {
    _vocbase->release();
  }
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
  auto trx = getTrx(tid);

  if (trx != nullptr) {
    trx->appendEntry(std::move(entry));
    return trx;
  }

  if (entry.operation == kCommit || entry.operation == kAbort) {
    // Commit and Abort transactions are being replicated to all participants,
    // regardless whether they received any entries so far. In such case it is
    // ok to return a nullptr.
    return nullptr;
  }

  TRI_ASSERT(_vocbase != nullptr);
  trx = std::make_shared<DocumentStateTransaction>(_vocbase, std::move(entry));
  _transactions.emplace(tid, trx);

  transaction::Hints hints;
  hints.set(transaction::Hints::Hint::GLOBAL_MANAGED);
  auto state = trx->getState();
  state->setWriteAccessType();

  auto res = state->beginTransaction(hints);
  if (res.fail()) {
    THROW_ARANGO_EXCEPTION(res);
  }

  // TODO Use our own context class
  auto ctx = std::make_shared<transaction::ManagedContext>(tid, trx->getState(),
                                                           false, false, true);

  std::vector<std::string> const readCollections{};
  std::vector<std::string> const writeCollections = {
      std::string(trx->getShardId())};
  std::vector<std::string> const exclusiveCollections{};

  auto methods = std::make_shared<transaction::Methods>(
      ctx, readCollections, writeCollections, exclusiveCollections,
      trx->getOptions());

  res = methods->begin();
  trx->setMethods(std::move(methods));
  if (res.fail()) {
    THROW_ARANGO_EXCEPTION(res);
  }

  return trx;
}

DocumentStateHandlersFactory::DocumentStateHandlersFactory(
    ArangodServer& server, AgencyCache& agencyCache,
    MaintenanceFeature& maintenaceFeature, DatabaseFeature& databaseFeature)
    : _server(server),
      _agencyCache(agencyCache),
      _maintenanceFeature(maintenaceFeature),
      _databaseFeature(databaseFeature) {}

auto DocumentStateHandlersFactory::createAgencyHandler(GlobalLogIdentifier gid)
    -> std::shared_ptr<IDocumentStateAgencyHandler> {
  return std::make_shared<DocumentStateAgencyHandler>(_server, _agencyCache);
}

auto DocumentStateHandlersFactory::createShardHandler(GlobalLogIdentifier gid)
    -> std::shared_ptr<IDocumentStateShardHandler> {
  return std::make_shared<DocumentStateShardHandler>(_maintenanceFeature);
}

auto DocumentStateHandlersFactory::createTransactionHandler(
    GlobalLogIdentifier gid)
    -> std::shared_ptr<IDocumentStateTransactionHandler> {
  return std::make_shared<DocumentStateTransactionHandler>(std::move(gid),
                                                           _databaseFeature);
}

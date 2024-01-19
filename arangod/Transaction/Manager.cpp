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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "Manager.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Aql/Query.h"
#include "Aql/QueryList.h"
#include "Basics/Exceptions.h"
#include "Basics/ReadLocker.h"
#include "Basics/ScopeGuard.h"
#include "Basics/WriteLocker.h"
#include "Basics/system-functions.h"
#include "Basics/voc-errors.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ServerState.h"
#include "Futures/Utilities.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "Logger/LogMacros.h"
#include "Metrics/GaugeBuilder.h"
#include "Metrics/MetricsFeature.h"
#include "Network/Methods.h"
#include "Network/NetworkFeature.h"
#include "Network/Utils.h"
#include "RestServer/DatabaseFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/Helpers.h"
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
#include "Transaction/History.h"
#endif
#include "Transaction/ManagerFeature.h"
#include "Transaction/Methods.h"
#include "Transaction/SmartContext.h"
#include "Transaction/Status.h"
#include "Utils/CollectionNameResolver.h"
#include "Utils/ExecContext.h"
#include "VocBase/LogicalCollection.h"

#ifdef USE_ENTERPRISE
#include "Enterprise/VocBase/VirtualClusterSmartEdgeCollection.h"
#endif

#include <absl/strings/str_cat.h>
#include <fuerte/jwt.h>
#include <velocypack/Iterator.h>

#include <thread>

namespace {
bool authorized(std::string const& user) {
  auto const& exec = arangodb::ExecContext::current();
  if (exec.isSuperuser()) {
    return true;
  }
  return (user == exec.user());
}

std::string currentUser() { return arangodb::ExecContext::current().user(); }

}  // namespace

namespace arangodb::transaction {

Manager::Manager(ManagerFeature& feature)
    : _feature(feature),
      _nrRunning(0),
      _disallowInserts(false),
      _hotbackupCommitLockHeld(false),
      _streamingLockTimeout(feature.streamingLockTimeout()),
      _softShutdownOngoing(false) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  _history = std::make_unique<transaction::History>(/*maxSize*/ 256);
#endif
}

Manager::~Manager() = default;

void Manager::releaseTransactions() noexcept {
  std::unique_lock<std::mutex> guard(_hotbackupMutex);
  if (_hotbackupCommitLockHeld) {
    LOG_TOPIC("eeddd", TRACE, Logger::TRANSACTIONS)
        << "Releasing write lock to hold transactions.";
    _hotbackupCommitLock.unlockWrite();
    _hotbackupCommitLockHeld = false;
  }
}

std::shared_ptr<CounterGuard> Manager::registerTransaction(
    TransactionId transactionId, bool isReadOnlyTransaction,
    bool isFollowerTransaction) {
  // If isFollowerTransaction is set then either the transactionId should be
  // an isFollowerTransactionId or it should be a legacy transactionId:
  TRI_ASSERT(!isFollowerTransaction ||
             transactionId.isFollowerTransactionId() ||
             transactionId.isLegacyTransactionId());

  return std::make_shared<CounterGuard>(*this);
}

uint64_t Manager::getActiveTransactionCount() {
  return _nrRunning.load(std::memory_order_relaxed);
}

/*static*/ double Manager::ttlForType(ManagerFeature const& feature,
                                      Manager::MetaType type) {
  TRI_IF_FAILURE("transaction::Manager::shortTTL") { return 0.1; }

  if (type == Manager::MetaType::Tombstone) {
    return tombstoneTTL;
  }

  auto role = ServerState::instance()->getRole();
  if ((ServerState::isSingleServer(role) || ServerState::isCoordinator(role))) {
    TRI_IF_FAILURE("lowStreamingIdleTimeout") { return 5.0; }

    return feature.streamingIdleTimeout();
  }
  return idleTTLDBServer;
}

Manager::ManagedTrx::ManagedTrx(ManagerFeature const& feature, MetaType t,
                                double ttl,
                                std::shared_ptr<TransactionState> st,
                                arangodb::cluster::CallbackGuard rGuard)
    : type(t),
      intermediateCommits(false),
      wasExpired(false),
      sideUsers(0),
      finalStatus(Status::UNDEFINED),
      timeToLive(ttl),
      expiryTime(TRI_microtime() + Manager::ttlForType(feature, t)),
      state(std::move(st)),
      rGuard(std::move(rGuard)),
      user(::currentUser()),
      db(state ? state->vocbase().name() : ""),
      rwlock(_schedulerWrapper) {}

bool Manager::ManagedTrx::hasPerformedIntermediateCommits() const noexcept {
  return this->intermediateCommits;
}

bool Manager::ManagedTrx::expired() const noexcept {
  return this->expiryTime < TRI_microtime();
}

void Manager::ManagedTrx::updateExpiry() noexcept {
  this->expiryTime = TRI_microtime() + this->timeToLive;
}

Manager::ManagedTrx::~ManagedTrx() {
  if (type == MetaType::StandaloneAQL || state == nullptr) {
    return;  // not managed by us
  }
  if (!state->isRunning()) {
    return;
  }

  try {
    transaction::Options opts;
    transaction::ManagedContext ctx(TransactionId{2}, state,
                                    /*responsibleForCommit*/ true,
                                    /*cloned*/ false);
    transaction::Methods trx(std::shared_ptr<transaction::Context>(
                                 std::shared_ptr<transaction::Context>(), &ctx),
                             opts);  // own state now
    TRI_ASSERT(trx.state()->status() == transaction::Status::RUNNING);
    TRI_ASSERT(trx.isMainTransaction());
    std::ignore = trx.abort();
  } catch (...) {
    // obviously it is not good to consume all exceptions here,
    // but we are in a destructor and must never throw from here
  }
}

using namespace arangodb;

namespace {

bool extractCollections(VPackSlice collections, std::vector<std::string>& reads,
                        std::vector<std::string>& writes,
                        std::vector<std::string>& exclusives) {
  auto fillColls = [](VPackSlice const& slice, std::vector<std::string>& cols) {
    if (slice.isNone()) {  // ignore nonexistent keys
      return true;
    } else if (slice.isString()) {
      cols.emplace_back(slice.copyString());
      return true;
    }

    if (slice.isArray()) {
      for (VPackSlice val : VPackArrayIterator(slice)) {
        if (!val.isString() || val.getStringLength() == 0) {
          return false;
        }
        cols.emplace_back(val.copyString());
      }
      return true;
    }
    return false;
  };
  return fillColls(collections.get("read"), reads) &&
         fillColls(collections.get("write"), writes) &&
         fillColls(collections.get("exclusive"), exclusives);
}

Result buildOptions(VPackSlice trxOpts, transaction::Options& options,
                    std::vector<std::string>& reads,
                    std::vector<std::string>& writes,
                    std::vector<std::string>& exclusives) {
  Result res;
  // parse the collections to register
  if (!trxOpts.isObject()) {
    return res.reset(TRI_ERROR_BAD_PARAMETER, "missing 'collections'");
  }

  VPackSlice trxCollections = trxOpts.get("collections");
  if (!trxCollections.isObject()) {
    return res.reset(TRI_ERROR_BAD_PARAMETER, "missing 'collections'");
  }

  // extract the properties from the object
  options.fromVelocyPack(trxOpts);
  if (options.lockTimeout < 0.0) {
    return res.reset(TRI_ERROR_BAD_PARAMETER,
                     "<lockTimeout> needs to be positive");
  }

  bool isValid = extractCollections(trxCollections, reads, writes, exclusives);

  if (!isValid) {
    return res.reset(TRI_ERROR_BAD_PARAMETER,
                     "invalid 'collections' attribute");
  }
  return res;
}

}  // namespace

arangodb::cluster::CallbackGuard Manager::buildCallbackGuard(
    TransactionState const& state) {
  arangodb::cluster::CallbackGuard rGuard;

  if (ServerState::instance()->isDBServer()) {
    auto const& origin = state.options().origin;
    if (!origin.serverId.empty()) {
      auto& clusterFeature = _feature.server().getFeature<ClusterFeature>();
      auto& clusterInfo = clusterFeature.clusterInfo();
      rGuard = clusterInfo.rebootTracker().callMeOnChange(
          origin,
          [this, tid = state.id()]() {
            // abort the transaction once the coordinator goes away
            // Note that this can potentially block on the future, in
            // case we do not get the lock on the ManagedTrx object
            // quickly!
            abortManagedTrx(tid, std::string()).get();
          },
          "Transaction aborted since coordinator rebooted or failed.");
    }
  }
  return rGuard;
}

/// @brief register a transaction shard
/// @brief tid global transaction shard
/// @param cid the optional transaction ID (use 0 for a single shard trx)
void Manager::registerAQLTrx(std::shared_ptr<TransactionState> const& state) {
  if (_disallowInserts.load(std::memory_order_acquire)) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_SHUTTING_DOWN);
  }

  TRI_ASSERT(state != nullptr);
  arangodb::cluster::CallbackGuard rGuard = buildCallbackGuard(*state);

  TransactionId const tid = state->id();
  size_t bucket = getBucket(tid);
  {
    WRITE_LOCKER(writeLocker, _transactions[bucket]._lock);

    auto& buck = _transactions[bucket];

    double ttl = Manager::ttlForType(_feature, MetaType::StandaloneAQL);
    auto it = buck._managed.try_emplace(
        tid, std::make_shared<ManagedTrx>(_feature, MetaType::StandaloneAQL,
                                          ttl, state, std::move(rGuard)));
    if (!it.second) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_TRANSACTION_INTERNAL,
          std::string("transaction ID ") + std::to_string(tid.id()) +
              "' already used (while registering AQL trx)");
    }
  }
}

void Manager::unregisterAQLTrx(TransactionId tid) noexcept {
  size_t bucket = getBucket(tid);
  auto& buck = _transactions[bucket];

  std::shared_ptr<ManagedTrx> mtrx;

  {
    READ_LOCKER(readLocker, _transactions[bucket]._lock);
    auto it = buck._managed.find(tid);
    if (it == buck._managed.end()) {
      LOG_TOPIC("92a49", WARN, Logger::TRANSACTIONS)
          << "registered transaction " << tid << " not found";
      TRI_ASSERT(false);
      return;
    }
    std::shared_ptr<ManagedTrx> mtrx = it->second;  // copy shared_ptr!
  }

  /// we need to make sure no-one else is still using the TransactionState
  /// we try for a second, before we give up in despair:
  {
    auto guard =
        mtrx->rwlock.asyncTryLockExclusiveFor(std::chrono::milliseconds(1000))
            .get();
    if (!guard.isLocked()) {
      LOG_TOPIC("9f7d7", WARN, Logger::TRANSACTIONS)
          << "transaction " << tid << " is still in use";
      TRI_ASSERT(false);
      return;
    }
  }
  // It is possible that somebody else has in the meantime deleted the
  // transaction or set its state to MetaType::Tombstone!
  // TRI_ASSERT(mtrx->type == MetaType::StandaloneAQL);

  WRITE_LOCKER(writeLocker, _transactions[bucket]._lock);
  buck._managed.erase(tid);  // ignore if it was already gone!
  // Note that we have already destroyed `guard` and so `mtrx` and thus
  // the rwlock can now automatically be destroyed in the right order!
}

futures::Future<ResultT<TransactionId>> Manager::createManagedTrx(
    TRI_vocbase_t& vocbase, velocypack::Slice trxOpts,
    OperationOrigin operationOrigin, bool allowDirtyReads) {
  if (_softShutdownOngoing.load(std::memory_order_relaxed)) {
    co_return {TRI_ERROR_SHUTTING_DOWN};
  }
  transaction::Options options;
  std::vector<std::string> reads, writes, exclusives;

  Result res = buildOptions(trxOpts, options, reads, writes, exclusives);
  if (res.fail()) {
    co_return res;
  }
  if (ServerState::instance()->isCoordinator() && writes.empty() &&
      exclusives.empty()) {
    if (allowDirtyReads) {
      options.allowDirtyReads = true;
    }
    // If the header is not set, but the option said true, we accept this,
    // provided we are on a coordinator and there are only reading collections.
  } else {
    // If we are not on a coordinator or if there are writing or exclusive
    // collections, then there will be no dirty reads:
    options.allowDirtyReads = false;
  }

  co_return co_await createManagedTrx(vocbase, reads, writes, exclusives,
                                      std::move(options), operationOrigin);
}

futures::Future<Result> Manager::ensureManagedTrx(
    TRI_vocbase_t& vocbase, TransactionId tid, velocypack::Slice trxOpts,
    OperationOrigin operationOrigin, bool isFollowerTransaction) {
  TRI_ASSERT(
      (ServerState::instance()->isSingleServer() && !isFollowerTransaction) ||
      tid.isFollowerTransactionId() == isFollowerTransaction);
  transaction::Options options;
  std::vector<std::string> reads, writes, exclusives;

  Result res = buildOptions(trxOpts, options, reads, writes, exclusives);
  if (res.fail()) {
    co_return res;
  }

  co_return co_await ensureManagedTrx(vocbase, tid, reads, writes, exclusives,
                                      std::move(options), operationOrigin,
                                      /*ttl*/ 0.0);
}

transaction::Hints Manager::ensureHints(transaction::Options& options) const {
  transaction::Hints hints;
  hints.set(transaction::Hints::Hint::GLOBAL_MANAGED);
  if (isFollowerTransactionOnDBServer(options)) {
    hints.set(transaction::Hints::Hint::IS_FOLLOWER_TRX);
    if (options.isIntermediateCommitEnabled()) {
      // turn on intermediate commits on followers as well. otherwise huge
      // leader transactions could make the follower claim all memory and crash.
      hints.set(transaction::Hints::Hint::INTERMEDIATE_COMMITS);
    }
  }
  return hints;
}

futures::Future<Result> Manager::beginTransaction(
    transaction::Hints hints, std::shared_ptr<TransactionState>& state) {
  TRI_ASSERT(state != nullptr);
  Result res;

  try {
    res = co_await state->beginTransaction(
        hints);  // registers with transaction manager
  } catch (basics::Exception const& ex) {
    res.reset(ex.code(), ex.what());
  } catch (std::exception const& ex) {
    res.reset(TRI_ERROR_INTERNAL, ex.what());
  }

  TRI_ASSERT(res.ok() || !state->isRunning());
  co_return res;
}

Result Manager::prepareOptions(transaction::Options& options) {
  Result res;

  if (options.lockTimeout < 0.0) {
    return res.reset(TRI_ERROR_BAD_PARAMETER,
                     "lockTimeout needs to be greater than zero");
  }
  // enforce size limit per DBServer
  if (isFollowerTransactionOnDBServer(options)) {
    // if we are a follower, we reuse the leader's max transaction size and
    // slightly increase it. this is to ensure that the follower can process at
    // least as many data as the leader, even if the data representation is
    // slightly varied for network transport etc.
    if (options.maxTransactionSize != UINT64_MAX) {
      uint64_t adjust = options.maxTransactionSize / 10;
      if (adjust < UINT64_MAX - options.maxTransactionSize) {
        // now the transaction on the follower should be able to grow to at
        // least the size of the transaction on the leader.
        options.maxTransactionSize += adjust;
      }
    }
    // it is also important that we set this option, so that it is ok for two
    // different leaders to add "their" shards to the same follower transaction.
    // for example, if we have 3 database servers and 2 shards, so that
    // - db server 1 is the leader for shard A
    // - db server 2 is the leader for shard B
    // - db server 3 is the follower for both shard A and B,
    // then db server 1 may try to lazily start a follower transaction
    // on db server 3 for shard A, and db server 2 may try to do the same for
    // shard B. Both calls will only send data for "their" shards, so
    // effectively we need to add write collections to the transaction at
    // runtime whenever this happens. It is important that all these calls
    // succeed, because otherwise one of the calls would just drop db server 3
    // as a follower.
    options.allowImplicitCollectionsForWrite = true;

    // we should not have any locking conflicts on followers, generally. shard
    // locking should be performed on leaders first, which will then, eventually
    // replicate changes to followers. replication to followers is only done
    // once the locks have been acquired on the leader(s). so if there are any
    // locking issues, they are supposed to happen first on leaders, and not
    // affect followers.
    // Having said that, even on a follower it can happen that for example
    // an index is finalized on a shard. And then the collection could be
    // locked exclusively for some period of time. Therefore, we should not
    // set the locking timeout too low here. We choose 5 minutes as a
    // compromise:
    constexpr double followerLockTimeout = 300.0;
    if (options.lockTimeout == 0.0 ||
        options.lockTimeout >= followerLockTimeout) {
      options.lockTimeout = followerLockTimeout;
    }
  } else {
    // for all other transactions, apply a size limitation
    options.maxTransactionSize = std::min<size_t>(
        options.maxTransactionSize, _feature.streamingMaxTransactionSize());
  }

  return res;
}

futures::Future<Result> Manager::lockCollections(
    TRI_vocbase_t& vocbase, std::shared_ptr<TransactionState> state,
    std::vector<std::string> const& exclusiveCollections,
    std::vector<std::string> const& writeCollections,
    std::vector<std::string> const& readCollections) {
  Result res;
  CollectionNameResolver resolver(vocbase);

  auto lockCols = [&](std::vector<std::string> const& cols,
                      AccessMode::Type mode) -> futures::Future<bool> {
    for (auto const& cname : cols) {
      DataSourceId cid = DataSourceId::none();
      if (state->isCoordinator()) {
        cid = resolver.getCollectionIdCluster(cname);
      } else {  // only support local collections / shards
        cid = resolver.getCollectionIdLocal(cname);
      }

      if (cid.empty()) {
        // not found
        res.reset(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
                  std::string(TRI_errno_string(
                      TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND)) +
                      ": " + cname);
      } else {
#ifdef USE_ENTERPRISE
        if (state->isCoordinator()) {
          try {
            std::shared_ptr<LogicalCollection> col =
                resolver.getCollection(cname);
            if (col->isSmart() && col->type() == TRI_COL_TYPE_EDGE) {
              auto theEdge =
                  dynamic_cast<arangodb::VirtualClusterSmartEdgeCollection*>(
                      col.get());
              if (theEdge == nullptr) {
                THROW_ARANGO_EXCEPTION_MESSAGE(
                    TRI_ERROR_INTERNAL,
                    "cannot cast collection to smart edge collection");
              }
              res.reset(co_await state->addCollection(theEdge->getLocalCid(),
                                                      "_local_" + cname, mode,
                                                      /*lockUsage*/ false));
              if (res.fail()) {
                co_return false;
              }
              if (!col->isDisjoint()) {
                res.reset(co_await state->addCollection(theEdge->getFromCid(),
                                                        "_from_" + cname, mode,
                                                        /*lockUsage*/ false));
                if (res.fail()) {
                  co_return false;
                }
                res.reset(co_await state->addCollection(theEdge->getToCid(),
                                                        "_to_" + cname, mode,
                                                        /*lockUsage*/ false));
                if (res.fail()) {
                  co_return false;
                }
              }
            }
          } catch (basics::Exception const& ex) {
            res.reset(ex.code(), ex.what());
            co_return false;
          }
        }
#endif
        res.reset(co_await state->addCollection(cid, cname, mode,
                                                /*lockUsage*/ false));
      }

      if (res.fail()) {
        co_return false;
      }
    }
    co_return true;
  };

  if (!co_await lockCols(exclusiveCollections, AccessMode::Type::EXCLUSIVE) ||
      !co_await lockCols(writeCollections, AccessMode::Type::WRITE) ||
      !co_await lockCols(readCollections, AccessMode::Type::READ)) {
    if (res.fail()) {
      // error already set by callback function
      co_return res;
    }
    // no error set. so it must be "data source not found"
    co_return res.reset(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
  }

  co_return res;
}

bool Manager::isFollowerTransactionOnDBServer(
    transaction::Options const& options) const {
  return ServerState::instance()->isDBServer() && options.isFollowerTransaction;
}

/// @brief create managed transaction
futures::Future<ResultT<TransactionId>> Manager::createManagedTrx(
    TRI_vocbase_t& vocbase, std::vector<std::string> const& readCollections,
    std::vector<std::string> const& writeCollections,
    std::vector<std::string> const& exclusiveCollections, Options options,
    OperationOrigin operationOrigin) {
  // We cannot run this on FollowerTransactions.
  // They need to get injected the TransactionIds.
  TRI_ASSERT(!isFollowerTransactionOnDBServer(options));
  Result res;
  if (_disallowInserts.load(std::memory_order_acquire)) {
    co_return res.reset(TRI_ERROR_SHUTTING_DOWN);
  }

  // no transaction with ID exists yet, so start a new transaction
  res = prepareOptions(options);
  if (res.fail()) {
    co_return res;
  }

  ServerState::RoleEnum role = ServerState::instance()->getRole();
  TRI_ASSERT(ServerState::isSingleServerOrCoordinator(role));
  TransactionId tid = ServerState::isSingleServer(role)
                          ? TransactionId::createSingleServer()
                          : TransactionId::createCoordinator();

  auto maybeState = basics::catchToResultT([&] {
    StorageEngine& engine =
        vocbase.server().getFeature<EngineSelectorFeature>().engine();
    // now start our own transaction
    return engine.createTransactionState(vocbase, tid, options,
                                         operationOrigin);
  });
  if (!maybeState.ok()) {
    co_return std::move(maybeState).result();
  }
  auto& state = maybeState.get();

  TRI_ASSERT(state != nullptr);
  TRI_ASSERT(state->id() == tid);

  if (options.allowDirtyReads) {
    TRI_ASSERT(ServerState::instance()->isCoordinator());
    // Choose the replica we read from for all shards of all collections in
    // the reads list:
    containers::FlatHashSet<ShardID> shards;
    auto& ci = vocbase.server().getFeature<ClusterFeature>().clusterInfo();
    for (std::string const& collName : readCollections) {
      auto coll = ci.getCollection(vocbase.name(), collName);
      auto shardMap = coll->shardIds();
      for (auto const& p : *shardMap) {
        shards.emplace(p.first);
      }
    }
    state->chooseReplicas(shards);
  }

  // lock collections
  res = co_await lockCollections(vocbase, state, exclusiveCollections,
                                 writeCollections, readCollections);
  if (res.fail()) {
    co_return res;
  }

  // start the transaction
  auto hints = ensureHints(options);
  // We allow to do a fast locking round here
  // We can only do this because we KNOW that the tid is not
  // known to any other place yet.
  hints.set(transaction::Hints::Hint::ALLOW_FAST_LOCK_ROUND_CLUSTER);
  res = co_await beginTransaction(hints, state);
  if (res.fail()) {
    co_return res;
  }
  // Unset the FastLockRound hint, if for some reason we ever end up locking
  // something again for this transaction we cannot recover from a fast lock
  // failure
  hints.unset(transaction::Hints::Hint::ALLOW_FAST_LOCK_ROUND_CLUSTER);

  // During beginTransaction we may reroll the Transaction ID.
  tid = state->id();

  bool stored = storeManagedState(tid, std::move(state), /*ttl*/ 0.0);
  if (!stored) {
    co_return res.reset(TRI_ERROR_TRANSACTION_INTERNAL,
                        absl::StrCat("transaction id ", tid.id(),
                                     " already used (while creating)"));
  }

  LOG_TOPIC("d6807", DEBUG, Logger::TRANSACTIONS)
      << "created managed trx " << tid;

  co_return ResultT{tid};
}

/// @brief create managed transaction
futures::Future<Result> Manager::ensureManagedTrx(
    TRI_vocbase_t& vocbase, TransactionId tid,
    std::vector<std::string> const& readCollections,
    std::vector<std::string> const& writeCollections,
    std::vector<std::string> const& exclusiveCollections, Options options,
    OperationOrigin operationOrigin, double ttl) {
  Result res;
  if (_disallowInserts.load(std::memory_order_acquire)) {
    co_return res.reset(TRI_ERROR_SHUTTING_DOWN);
  }

  // This method should not be used in a single server. Note that single-server
  // transaction IDs will randomly be identified as follower transactions,
  // leader transactions, legacy transactions or coordinator transactions;
  // context is important.
  TRI_ASSERT(!ServerState::instance()->isSingleServer() ||
             ServerState::instance()->isGoogleTest());
  // We should never have `options.isFollowerTransaction == true`, but
  // `tid.isFollowerTransactionId() == false`.
  TRI_ASSERT(options.isFollowerTransaction == tid.isFollowerTransactionId() ||
             !options.isFollowerTransaction);
  options.isFollowerTransaction = tid.isFollowerTransactionId();

  LOG_TOPIC("7bd2d", DEBUG, Logger::TRANSACTIONS)
      << "managed trx creating: " << tid.id();

  if (transactionIdExists(tid)) {
    if (isFollowerTransactionOnDBServer(options)) {
      // it is ok for two different leaders to try to create the same
      // follower transaction on a leader.
      // for example, if we have 3 database servers and 2 shards, so that
      // - db server 1 is the leader for shard A
      // - db server 2 is the leader for shard B
      // - db server 3 is the follower for both shard A and B,
      // then db server 1 may try to lazily start a follower transaction
      // on db server 3, and db server 2 may try to do the same. It is
      // important that both succeed, because otherwise one of the calls
      // would just drop db server 3 as a follower.
      TRI_ASSERT(res.ok());
      co_return res;
    }

    co_return res.reset(TRI_ERROR_TRANSACTION_INTERNAL,
                        std::string("transaction id ") +
                            std::to_string(tid.id()) +
                            " already used (before creating)");
  }

  // no transaction with ID exists yet, so start a new transaction
  res = prepareOptions(options);
  if (res.fail()) {
    co_return res;
  }

  auto maybeState = basics::catchToResultT([&] {
    StorageEngine& engine =
        vocbase.server().getFeature<EngineSelectorFeature>().engine();
    // now start our own transaction
    return engine.createTransactionState(vocbase, tid, options,
                                         operationOrigin);
  });
  if (!maybeState.ok()) {
    co_return std::move(maybeState).result();
  }
  auto& state = maybeState.get();

  TRI_ASSERT(state != nullptr);
  TRI_ASSERT(state->id() == tid);

  // lock collections
  res = co_await lockCollections(vocbase, state, exclusiveCollections,
                                 writeCollections, readCollections);
  if (res.fail()) {
    co_return res;
  }

  // start the transaction
  auto hints = ensureHints(options);
  res = co_await beginTransaction(hints, state);
  if (res.fail()) {
    co_return res;
  }
  // The coordinator in some cases can reroll the Transaction id.
  // This however can not be allowed here, as this transaction ID
  // may be managed outside.
  TRI_ASSERT(state->id() == tid);

  bool stored = storeManagedState(tid, std::move(state), ttl);
  if (!stored) {
    if (isFollowerTransactionOnDBServer(options)) {
      TRI_ASSERT(res.ok());
      co_return res;
    }

    co_return res.reset(TRI_ERROR_TRANSACTION_INTERNAL,
                        std::string("transaction id ") +
                            std::to_string(tid.id()) +
                            " already used (while creating)");
  }

  LOG_TOPIC("d6806", DEBUG, Logger::TRANSACTIONS)
      << "created managed trx " << tid;

  co_return res;
}

/// @brief lease the transaction, increases nesting
futures::Future<std::shared_ptr<transaction::Context>> Manager::leaseManagedTrx(
    TransactionId tid, AccessMode::Type mode, bool isSideUser) {
  // simon: Two allowed scenarios:
  // 1. User sends concurrent write (CRUD) requests, (which was never intended
  // to be possible)
  //    but now we do have to kind of support it otherwise shitty apps break
  // 2. one does a bulk write within a el-cheapo / V8 transaction into
  // multiple shards
  //    on the same DBServer (still bad design).
  TRI_ASSERT(mode != AccessMode::Type::NONE);

  if (_disallowInserts.load(std::memory_order_acquire)) {
    co_return nullptr;
  }

  TRI_IF_FAILURE("leaseManagedTrxFail") { co_return nullptr; }

  auto role = ServerState::instance()->getRole();
  int64_t timeoutMilliseconds = -1;      // no timeout
  if (!ServerState::isDBServer(role)) {  // keep end time as small as possible
    timeoutMilliseconds = int64_t(1000 * _streamingLockTimeout);
  }
  // always serialize access on coordinator,
  // TransactionState::_knownServers is modified even for READ
  if (ServerState::isCoordinator(role)) {
    mode = AccessMode::Type::WRITE;
  }

  TRI_ASSERT(!isSideUser || AccessMode::isRead(mode));

  size_t bucket = getBucket(tid);
  std::shared_ptr<ManagedTrx> mtrx_ptr;
  MetaType type;
  {
    READ_LOCKER(locker, _transactions[bucket]._lock);

    auto it = _transactions[bucket]._managed.find(tid);
    if (it == _transactions[bucket]._managed.end()) {
      co_return nullptr;
    }

    mtrx_ptr = it->second;  // copy shared_ptr!
    if (mtrx_ptr->type == MetaType::Tombstone) {
      if (mtrx_ptr->finalStatus == transaction::Status::ABORTED) {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_TRANSACTION_ABORTED,
                                       "transaction " +
                                           std::to_string(tid.id()) +
                                           " has already been aborted");
      }
      co_return nullptr;
    }

    if (mtrx_ptr->expired() || !::authorized(mtrx_ptr->user)) {
      co_return nullptr;  // no need to return anything
    }
    type = mtrx_ptr->type;  // Keep a copy, since we must not access
                            // the members of mtrx (except rwlock) when
                            // we release the bucket _lock!
  }
  ManagedTrx& mtrx = *mtrx_ptr;
  // It is crucial to release the _transactions[bucket]._lock here before
  // trying to acquire the mtrx.rwlock, since otherwise we would run into
  // the danger to produce a deadlock, since some other thread might
  // have the mtrx.rwlock and then try to acquire the
  // _transactions[bucket]._lock!
  // On the other hand, we may now no longer access the members of mtrx,
  // (with the exception of rwlock), since they are protected by the bucket
  // _lock!

  if (AccessMode::isWriteOrExclusive(mode)) {
    if (type == MetaType::StandaloneAQL) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_TRANSACTION_DISALLOWED_OPERATION,
          "not allowed to write lock an AQL transaction");
    }
    auto guard = co_await mtrx.rwlock.asyncTryLockExclusiveFor(
        std::chrono::milliseconds(timeoutMilliseconds));
    if (guard.isLocked()) {
      auto managedContext = buildManagedContextUnderLock(tid, mtrx);
      // Get the bucket lock again:
      READ_LOCKER(locker, _transactions[bucket]._lock);
      // We may now access the members of mtrx again!
      if (mtrx.state->isReadOnlyTransaction()) {
        managedContext->setReadOnly();
      }
      // Now need to check again that nobody has soft deleted or aborted the
      // transaction in the meantime:
      // We may now access the mem
      auto it = _transactions[bucket]._managed.find(tid);
      if (it == _transactions[bucket]._managed.end()) {
        co_return nullptr;
      }
      if (mtrx.type == MetaType::Tombstone &&
          mtrx.finalStatus == transaction::Status::ABORTED) {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_TRANSACTION_ABORTED,
                                       "transaction " +
                                           std::to_string(tid.id()) +
                                           " has already been aborted");
      }
      guard.release();  // Sack the guard, keep the lock
      co_return managedContext;
    }
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_LOCKED, std::string("cannot write-lock, transaction ") +
                              std::to_string(tid.id()) + " is already in use");
  }
  TRI_ASSERT(mode == AccessMode::Type::READ);
  // even for side user leases, first try acquiring the read lock
  auto guard = co_await mtrx.rwlock.asyncTryLockSharedFor(
      std::chrono::milliseconds(timeoutMilliseconds));
  if (guard.isLocked()) {
    auto managedContext = buildManagedContextUnderLock(tid, mtrx);
    // Get the bucket lock again:
    READ_LOCKER(locker, _transactions[bucket]._lock);
    // We may now access the members of mtrx again!
    if (mtrx.state->isReadOnlyTransaction()) {
      managedContext->setReadOnly();
    }
    // Now need to check again that nobody has soft deleted or aborted the
    // transaction in the meantime:
    // We may now access the mem
    auto it = _transactions[bucket]._managed.find(tid);
    if (it == _transactions[bucket]._managed.end()) {
      co_return nullptr;
    }
    if (mtrx.type == MetaType::Tombstone &&
        mtrx.finalStatus == transaction::Status::ABORTED) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_TRANSACTION_ABORTED,
                                     "transaction " + std::to_string(tid.id()) +
                                         " has already been aborted");
    }
    guard.release();  // Sack the guard, keep the lock
    co_return managedContext;
  }
  if (isSideUser) {
    // number of side users is atomically increased under the bucket's read
    // lock. due to us holding the bucket's read lock here, there can be no
    // other threads concurrently aborting/commiting the transaction (these
    // operations acquire the write lock on the transaction's bucket).
    mtrx.sideUsers.fetch_add(1, std::memory_order_relaxed);
    // note: we are intentionally _not_ acquiring the lock on the
    // transaction here, as we expect another operation to have acquired it
    // already!
    try {
      std::shared_ptr<TransactionState> state = mtrx.state;
      TRI_ASSERT(state != nullptr);
      auto managedContext = std::make_shared<ManagedContext>(
          tid, std::move(state), TransactionContextSideUser{});
      if (mtrx.state->isReadOnlyTransaction()) {
        managedContext->setReadOnly();
      }
      co_return managedContext;
    } catch (...) {
      // roll back our increase of the number of side users
      auto previous = mtrx.sideUsers.fetch_sub(1, std::memory_order_relaxed);
      TRI_ASSERT(previous > 0);
      throw;
    }
  }

  THROW_ARANGO_EXCEPTION_MESSAGE(
      TRI_ERROR_LOCKED, std::string("cannot read-lock, transaction ") +
                            std::to_string(tid.id()) + " is already in use");
}

void Manager::returnManagedTrx(TransactionId tid, bool isSideUser) noexcept {
  bool isSoftAborted = false;

  {
    size_t bucket = getBucket(tid);
    WRITE_LOCKER(writeLocker, _transactions[bucket]._lock);

    auto it = _transactions[bucket]._managed.find(tid);
    if (it == _transactions[bucket]._managed.end() ||
        !::authorized(it->second->user)) {
      LOG_TOPIC("1d5b0", WARN, Logger::TRANSACTIONS)
          << "managed transaction " << tid << " not found";
      TRI_ASSERT(false);
      return;
    }

    TRI_ASSERT(it->second->state != nullptr);

    if (isSideUser) {
      // number of side users is atomically decreased under the bucket's read
      // lock. due to us holding the bucket's read lock here, there can be no
      // other threads concurrently aborting/commiting the transaction (these
      // operations acquire the write lock on the transaction's bucket).
      auto previous =
          it->second->sideUsers.fetch_sub(1, std::memory_order_relaxed);
      TRI_ASSERT(previous > 0);
      // note: we are intentionally _not_ releasing the lock on the
      // transaction here, because we have not acquired it before!
    } else {
      // garbageCollection might soft abort used transactions
      isSoftAborted = it->second->expiryTime == 0;
      if (!isSoftAborted) {
        it->second->updateExpiry();
      }

      it->second->rwlock.unlock();
    }
  }

  // it is important that we release the write lock for the bucket here,
  // because abortManagedTrx will call statusChangeWithTimeout, which will
  // call updateTransaction, which then will try to acquire the same
  // write lock

  TRI_IF_FAILURE("returnManagedTrxForceSoftAbort") { isSoftAborted = true; }

  if (isSoftAborted) {
    TRI_ASSERT(!isSideUser);
    abortManagedTrx(tid, "" /* any database */).get();
  }
}

/// @brief get the transaction state
transaction::Status Manager::getManagedTrxStatus(
    TransactionId tid, std::string const& database) const {
  size_t bucket = getBucket(tid);
  READ_LOCKER(writeLocker, _transactions[bucket]._lock);

  auto it = _transactions[bucket]._managed.find(tid);
  if (it == _transactions[bucket]._managed.end() ||
      !::authorized(it->second->user) || it->second->db != database) {
    return transaction::Status::UNDEFINED;
  }

  auto const& mtrx = *it->second;

  if (mtrx.type == MetaType::Tombstone) {
    return mtrx.finalStatus;
  } else if (!mtrx.expired() && mtrx.state != nullptr) {
    return transaction::Status::RUNNING;
  } else {
    return transaction::Status::ABORTED;
  }
}

futures::Future<Result> Manager::statusChangeWithTimeout(
    TransactionId tid, std::string const& database,
    transaction::Status status) {
  double startTime = 0.0;
  constexpr double maxWaitTime = 3.0;
  Result res;
  while (true) {
    res = co_await updateTransaction(tid, status, false, database);
    if (res.ok() || !res.is(TRI_ERROR_LOCKED)) {
      break;
    }
    double now = TRI_microtime();
    if (startTime <= 0.0001) {  // fp tolerance
      startTime = now;
    } else if (now - startTime > maxWaitTime) {
      // timeout
      break;
    }
    std::this_thread::yield();
  }
  co_return res;
}

futures::Future<Result> Manager::commitManagedTrx(TransactionId tid,
                                                  std::string const& database) {
  READ_LOCKER(guard, _hotbackupCommitLock);
  co_return co_await statusChangeWithTimeout(tid, database,
                                             transaction::Status::COMMITTED);
}

futures::Future<Result> Manager::abortManagedTrx(TransactionId tid,
                                                 std::string const& database) {
  co_return co_await statusChangeWithTimeout(tid, database,
                                             transaction::Status::ABORTED);
}

futures::Future<Result> Manager::updateTransaction(
    TransactionId tid, transaction::Status status, bool clearServers,
    std::string const& database) {
  TRI_ASSERT(status == transaction::Status::COMMITTED ||
             status == transaction::Status::ABORTED);

  LOG_TOPIC("7bd2f", DEBUG, Logger::TRANSACTIONS)
      << "managed trx " << tid << " updating to '"
      << (status == transaction::Status::COMMITTED ? "COMMITED" : "ABORTED")
      << "'";

  Result res;
  size_t bucket = getBucket(tid);
  bool wasExpired = false;
  auto buildErrorMessage = [](TransactionId tid, transaction::Status status,
                              bool found) -> std::string {
    std::string msg = "transaction " + std::to_string(tid.id());
    if (found) {
      msg += " inaccessible";
    } else {
      msg += " not found";
    }
    if (status == transaction::Status::COMMITTED) {
      msg += " on commit operation";
    } else {
      msg += " on abort operation";
    }
    return msg;
  };

  std::shared_ptr<ManagedTrx> mtrx_ptr;
  {
    WRITE_LOCKER(writeLocker, _transactions[bucket]._lock);

    auto& buck = _transactions[bucket];
    auto it = buck._managed.find(tid);
    if (it == buck._managed.end()) {
      // insert a tombstone for an aborted transaction that we never saw
      // before
      auto inserted = buck._managed.try_emplace(
          tid, std::make_shared<ManagedTrx>(
                   _feature, MetaType::Tombstone,
                   ttlForType(_feature, MetaType::Tombstone), nullptr,
                   arangodb::cluster::CallbackGuard{}));
      inserted.first->second->finalStatus = transaction::Status::ABORTED;
      inserted.first->second->db = database;
      co_return res.reset(TRI_ERROR_TRANSACTION_NOT_FOUND,
                          buildErrorMessage(tid, status, /*found*/ false));
    }

    mtrx_ptr = it->second;
    if (!::authorized(mtrx_ptr->user) ||
        (!database.empty() && mtrx_ptr->db != database)) {
      co_return res.reset(TRI_ERROR_TRANSACTION_NOT_FOUND,
                          buildErrorMessage(tid, status, /*found*/ true));
    }
  }
  // We are releasing the lock on the bucket here, because we must now
  // acquire the rwlock on the `ManagedTrx` object.

  // in order to modify the transaction's status, we need the write lock
  // here, plus we must ensure that the number of sideUsers is 0.
  auto tryGuard = co_await mtrx_ptr->rwlock.asyncTryLockExclusiveFor(
      std::chrono::milliseconds(100));
  bool canAccessTrx = tryGuard.isLocked();
  if (canAccessTrx) {
    canAccessTrx &= (mtrx_ptr->sideUsers.load(std::memory_order_relaxed) == 0);
  }
  if (!canAccessTrx) {
    std::string_view hint = " ";
    if (tid.isFollowerTransactionId()) {
      hint = " follower ";
    }

    std::string_view operation;
    if (status == transaction::Status::COMMITTED) {
      operation = "commit";
    } else {
      operation = "abort";
    }
    std::string msg = absl::StrCat("updating", hint, "transaction status on ",
                                   operation, " failed. transaction ",
                                   std::to_string(tid.id()), " is in use");
    LOG_TOPIC("dfc30", DEBUG, Logger::TRANSACTIONS) << msg;
    co_return res.reset(TRI_ERROR_LOCKED, std::move(msg));
  }
  TRI_ASSERT(tryGuard.isLocked());

  // A place to move the `TransactionState` out of the `ManagedTrx`:
  std::shared_ptr<TransactionState> state;
  {
    // Now we need to reacquire the lock for the bucket (and thus for the
    // members of the `ManagedTrx`:
    WRITE_LOCKER(writeLocker, _transactions[bucket]._lock);

    auto& buck = _transactions[bucket];
    auto it = buck._managed.find(tid);
    // Potentially, the transaction could be gone in the meantime, If
    // somebody else first got the lock, then our work is done with an error:
    if (it == buck._managed.end()) {
      // This will automatically first release `writeLocker`, then delete
      // `tryGuard`, thereby releasing the rwlock and then destroying
      // `mtrx_ptr` and thus the `ManagedTrx`.
      co_return res.reset(TRI_ERROR_TRANSACTION_NOT_FOUND,
                          buildErrorMessage(tid, status, /*found*/ false));
    }

    ManagedTrx& mtrx = *mtrx_ptr;

    if (mtrx.type == MetaType::StandaloneAQL) {
      co_return res.reset(TRI_ERROR_TRANSACTION_DISALLOWED_OPERATION,
                          "not allowed to change an AQL transaction");
    } else if (mtrx_ptr->type == MetaType::Tombstone) {
      TRI_ASSERT(mtrx.state == nullptr);
      // make sure everyone who asks gets the updated timestamp
      mtrx.updateExpiry();
      if (mtrx.finalStatus == status) {
        if (ServerState::instance()->isDBServer() &&
            tid.isFollowerTransactionId() &&
            mtrx.finalStatus == transaction::Status::ABORTED &&
            mtrx.intermediateCommits) {
          // we are trying to abort a follower transaction (again) that had
          // intermediate commits already. in this case we return a special
          // error code, which makes the leader drop us as a follower for all
          // shards in the transaction.
          co_return res.reset(
              TRI_ERROR_CLUSTER_FOLLOWER_TRANSACTION_COMMIT_PERFORMED);
        }
        co_return res;  // all good
      } else {
        std::string msg("transaction was already ");
        if (mtrx.wasExpired) {
          msg.append("expired");
        } else {
          msg.append(statusString(mtrx.finalStatus));
        }
        co_return res.reset(TRI_ERROR_TRANSACTION_DISALLOWED_OPERATION,
                            std::move(msg));
      }
    }
    TRI_ASSERT(mtrx.type == MetaType::Managed);

    if (mtrx.expired()) {
      // we will update the expire time of the tombstone shortly afterwards,
      // but we need to store the fact that this transaction originally
      // expired
      wasExpired = true;
      status = transaction::Status::ABORTED;
    }

    // Now take the `TransactionState` out of the `ManagedTrx`:
    std::swap(state, mtrx.state);
    TRI_ASSERT(mtrx.state == nullptr);
    // type is changed under the transaction's write lock and the bucket's
    // write lock
    mtrx.type = MetaType::Tombstone;
    if (state->numCommits() > 0) {
      // note that we have performed a commit or an intermediate commit.
      // this is necessary for follower transactions
      mtrx.intermediateCommits = true;
    }
    mtrx.wasExpired = wasExpired;
    mtrx.finalStatus = status;
    mtrx.updateExpiry();
    // it is sufficient to pretend that the operation already succeeded
  }

  // Now we have sorted out the Manager's data structures, so we can really
  // commit or abort the transaction:
  TRI_ASSERT(state);
  if (!state) {  // this should never happen
    co_return res.reset(TRI_ERROR_INTERNAL, "managed trx in an invalid state");
  }

  if (!state->isRunning()) {  // this also should not happen
    // We need to reacquire the lock for the bucket because of the
    // members of the `ManagedTrx`. Note that it is **not** a problem
    // if the entry in the bucket map has vanished in the meantime, since
    // we still hold `mtrx_ptr`!
    WRITE_LOCKER(writeLocker, _transactions[bucket]._lock);
    mtrx_ptr->finalStatus = Status::ABORTED;
    co_return res.reset(TRI_ERROR_TRANSACTION_ABORTED,
                        "transaction was not running");
  }

  bool isCoordinator = state->isCoordinator();
  bool intermediateCommits = state->numCommits() > 0;

  // We now need to use a `transactions::Methods` object to officially
  // commit the transaction. Note that we already have the rwlock in
  // write mode on the `ManagedTrx`, which we usually would get by
  // `leaseManagedTrx`. The `ManagedContext` which we are using here
  // will unregister the transaction in the Manager and thus free it,
  // so we release the guard here:
  tryGuard.release();
  transaction::Methods trx(
      std::make_shared<ManagedContext>(tid, std::move(state),
                                       /*responsibleForCommit*/ true,
                                       /*cloned*/ false),
      transaction::Options{});
  TRI_ASSERT(trx.state()->isRunning());
  TRI_ASSERT(trx.isMainTransaction());
  if (clearServers && !isCoordinator) {
    trx.state()->clearKnownServers();
  }
  if (status == transaction::Status::COMMITTED) {
    res = trx.commit();

    if (res.fail()) {  // set final status to aborted
      // Note that if the failure point TransactionCommitFail is used, then
      // the trx can still be running here.
      if (trx.state()->isRunning()) {
        // ignore return code here
        std::ignore = trx.abort();
      }
      // We need to reacquire the lock for the bucket because of the
      // members of the `ManagedTrx`. Note that it is **not** a problem
      // if the entry in the bucket map has vanished in the meantime, since
      // we still hold `mtrx_ptr`!
      WRITE_LOCKER(writeLocker, _transactions[bucket]._lock);
      mtrx_ptr->finalStatus = Status::ABORTED;
    }
  } else {
    res = trx.abort();
    if (intermediateCommits && ServerState::instance()->isDBServer() &&
        tid.isFollowerTransactionId()) {
      // we are trying to abort a follower transaction that had intermediate
      // commits already. in this case we return a special error code, which
      // makes the leader drop us as a follower for all shards in the
      // transaction.
      res.reset(TRI_ERROR_CLUSTER_FOLLOWER_TRANSACTION_COMMIT_PERFORMED);
    }
  }
  TRI_ASSERT(!trx.state()->isRunning());

  // Finally, our work is done. This will now release things in exactly
  // the right order: `trx` is destroyed, the `ManagedContext` created
  // above is destroyed as a consequence, the .
  co_return res;
}

/// @brief calls the callback function for each managed transaction
void Manager::iterateManagedTrx(
    std::function<void(TransactionId, ManagedTrx const&)> const& callback)
    const {
  // iterate over all active transactions
  for (size_t bucket = 0; bucket < numBuckets; ++bucket) {
    READ_LOCKER(locker, _transactions[bucket]._lock);

    auto& buck = _transactions[bucket];
    for (auto const& it : buck._managed) {
      if (it.second->type == MetaType::Managed) {
        // we only care about managed transactions here
        callback(it.first, *it.second);
      }
    }
  }
}

/// @brief collect forgotten transactions
bool Manager::garbageCollect(bool abortAll) {
  // This method is intentionally synchronous. We only call it on
  // shutdown, in tests, and in background threads.

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  // clear transaction history as well
  if (_history != nullptr) {
    _history->garbageCollect();
  }
#endif

  TRI_IF_FAILURE("transaction::Manager::noGC") { return false; }

  bool didWork = false;
  containers::SmallVector<TransactionId, 8> toAbort;
  containers::SmallVector<TransactionId, 8> toErase;

  uint64_t numAborted = 0;

  for (size_t bucket = 0; bucket < numBuckets; ++bucket) {
    struct Info {
      TransactionId id;
      std::shared_ptr<TransactionState> state;
      std::shared_ptr<ManagedTrx> mtrx_ptr;
    };
    std::vector<Info> inBucket;

    {
      if (abortAll) {
        _transactions[bucket]._lock.lockWrite();
      } else {
        _transactions[bucket]._lock.lockRead();
      }
      auto scope =
          scopeGuard([&]() noexcept { _transactions[bucket]._lock.unlock(); });

      for (auto& it : _transactions[bucket]._managed) {
        ManagedTrx& mtrx = *it.second;

        if (mtrx.type == MetaType::Managed) {
          TRI_ASSERT(mtrx.state != nullptr);
          if (abortAll || mtrx.expired()) {
            ++numAborted;
            inBucket.push_back(Info{
                .id = it.first,
                .state = mtrx.state,
                .mtrx_ptr = it.second,
            });
          }
        } else if (mtrx.type == MetaType::StandaloneAQL && mtrx.expired()) {
          LOG_TOPIC("7ad5f", INFO, Logger::TRANSACTIONS)
              << "expired AQL query transaction " << it.first;
        } else if (mtrx.type == MetaType::Tombstone && mtrx.expired()) {
          TRI_ASSERT(mtrx.state == nullptr);
          TRI_ASSERT(mtrx.finalStatus != transaction::Status::UNDEFINED);
          toErase.emplace_back(it.first);
        }
      }
    }

    for (auto& info : inBucket) {
      auto tryGuard =
          info.mtrx_ptr->rwlock
              .asyncTryLockExclusiveFor(std::chrono::milliseconds(100))
              .get();
      if (tryGuard.isLocked()) {
        TRI_ASSERT(info.state->isRunning());
        TRI_ASSERT(info.id == info.state->id());
        toAbort.emplace_back(info.id);
        LOG_TOPIC("7ad3f", INFO, Logger::TRANSACTIONS)
            << "aborting expired transaction " << info.id;
      } else if (abortAll) {  // transaction is in use but we want to
                              // abort
        LOG_TOPIC("92431", INFO, Logger::TRANSACTIONS)
            << "soft-aborting expired transaction " << info.id;
        info.mtrx_ptr->expiryTime = 0;  // soft-abort transaction
        didWork = true;
        LOG_TOPIC("7ad4f", INFO, Logger::TRANSACTIONS)
            << "soft aborting transaction " << info.id;
      }
    }
  }

  for (TransactionId tid : toAbort) {
    LOG_TOPIC("6fbaf", INFO, Logger::TRANSACTIONS) << "garbage collecting "
                                                   << "transaction " << tid;
    try {
      Result res =
          updateTransaction(tid, Status::ABORTED, /*clearSrvs*/ true).get();
      // updateTransaction can return TRI_ERROR_TRANSACTION_ABORTED when it
      // successfully aborts, so ignore this error.
      // we can also get the TRI_ERROR_LOCKED error in case we cannot
      // immediately acquire the lock on the transaction. this _can_ happen
      // infrequently, but is not an error
      if (res.fail() && !res.is(TRI_ERROR_TRANSACTION_ABORTED) &&
          !res.is(TRI_ERROR_CLUSTER_FOLLOWER_TRANSACTION_COMMIT_PERFORMED) &&
          !res.is(TRI_ERROR_LOCKED)) {
        LOG_TOPIC("0a07f", INFO, Logger::TRANSACTIONS)
            << "error while aborting "
               "transaction: "
            << res.errorMessage();
      }
      didWork = true;
    } catch (...) {
      // if this fails, this is no problem. we will simply try again in the
      // next round
    }
  }

  // track the number of hard-/soft-aborted transactions
  _feature.trackExpired(numAborted);

  // remove all expired tombstones
  for (TransactionId tid : toErase) {
    size_t bucket = getBucket(tid);
    WRITE_LOCKER(locker, _transactions[bucket]._lock);
    _transactions[bucket]._managed.erase(tid);
  }

  if (didWork) {
    LOG_TOPIC("e5b31", INFO, Logger::TRANSACTIONS)
        << "aborted expired transactions";
  }

  return didWork;
}

/// @brief abort all transactions matching
bool Manager::abortManagedTrx(
    std::function<bool(TransactionState const&, std::string const&)> cb) {
  // FIXME: This method should be asynchronous and a coroutine.
  containers::SmallVector<TransactionId, 8> toAbort;

  for (size_t bucket = 0; bucket < numBuckets; ++bucket) {
    // First get the list of managed transactions as shared_ptr copies:
    struct Info {
      TransactionId id;
      std::shared_ptr<ManagedTrx> mtrx_ptr;
      std::shared_ptr<TransactionState> state;
      std::string user;
    };
    std::vector<Info> inBucket;
    {
      READ_LOCKER(locker, _transactions[bucket]._lock);

      auto it = _transactions[bucket]._managed.begin();
      while (it != _transactions[bucket]._managed.end()) {
        ManagedTrx& mtrx = *it->second;
        if (mtrx.type == MetaType::Managed) {
          TRI_ASSERT(mtrx.state != nullptr);
          inBucket.push_back(Info{.id = it->first,
                                  .mtrx_ptr = it->second,
                                  .state = mtrx.state,
                                  .user = mtrx.user});
        }

        ++it;  // next
      }
    }

    // Now acquire for each the rwlock as read and call the callback to
    // see if it is to be aborted:
    for (auto& info : inBucket) {
      auto guard = info.mtrx_ptr->rwlock.asyncLockShared().get();  // FIXME
      if (guard.isLocked() && cb(*info.state, info.user)) {
        toAbort.emplace_back(info.id);
      }
    }
  }

  // Finally, we can actually abort all those filtered transaction:
  for (TransactionId tid : toAbort) {
    Result res =
        updateTransaction(tid, Status::ABORTED, /*clearSrvs*/ true).get();
    if (res.fail() &&
        !res.is(TRI_ERROR_CLUSTER_FOLLOWER_TRANSACTION_COMMIT_PERFORMED)) {
      LOG_TOPIC("2bf48", INFO, Logger::TRANSACTIONS)
          << "error aborting transaction " << tid << ": " << res.errorMessage();
    }
  }
  return !toAbort.empty();
}

void Manager::toVelocyPack(VPackBuilder& builder, std::string const& database,
                           std::string const& username, bool fanout) const {
  TRI_ASSERT(!builder.isClosed());

  if (fanout) {
    TRI_ASSERT(ServerState::instance()->isCoordinator());
    auto& ci = _feature.server().getFeature<ClusterFeature>().clusterInfo();

    NetworkFeature const& nf = _feature.server().getFeature<NetworkFeature>();
    network::ConnectionPool* pool = nf.pool();
    if (pool == nullptr) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_SHUTTING_DOWN);
    }

    std::vector<network::FutureRes> futures;
    auto auth = AuthenticationFeature::instance();

    network::RequestOptions options;
    options.database = database;
    options.timeout = network::Timeout(30.0);
    options.param("local", "true");

    VPackBuffer<uint8_t> body;

    for (auto const& coordinator : ci.getCurrentCoordinators()) {
      if (coordinator == ServerState::instance()->getId()) {
        // ourselves!
        continue;
      }

      network::Headers headers;
      if (auth != nullptr && auth->isActive()) {
        if (!username.empty()) {
          headers.try_emplace(
              StaticStrings::Authorization,
              "bearer " + fuerte::jwt::generateUserToken(
                              auth->tokenCache().jwtSecret(), username));
        } else {
          headers.try_emplace(StaticStrings::Authorization,
                              "bearer " + auth->tokenCache().jwtToken());
        }
      }

      auto f = network::sendRequestRetry(
          pool, "server:" + coordinator, fuerte::RestVerb::Get,
          "/_api/transaction", body, options, std::move(headers));
      futures.emplace_back(std::move(f));
    }

    if (!futures.empty()) {
      auto responses = futures::collectAll(futures).get();
      for (auto const& it : responses) {
        if (!it.hasValue()) {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_CLUSTER_BACKEND_UNAVAILABLE);
        }
        auto& res = it.get();
        if (res.statusCode() == fuerte::StatusOK) {
          VPackSlice slice = res.slice();
          if (slice.isObject()) {
            slice = slice.get("transactions");
            if (slice.isArray()) {
              builder.add(VPackArrayIterator(slice));
            }
          }
        }
      }
    }
  }

  // merge with local transactions
  iterateManagedTrx([&builder, &database](TransactionId tid,
                                          ManagedTrx const& trx) {
    if (::authorized(trx.user) && trx.db == database) {
      builder.openObject(true);
      builder.add("id", VPackValue(std::to_string(tid.id())));
      builder.add("state",
                  VPackValue(transaction::statusString(trx.state->status())));
      builder.close();
    }
  });
}

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
History& Manager::history() noexcept {
  TRI_ASSERT(_history != nullptr);
  return *_history;
}
#endif

Result Manager::abortAllManagedWriteTrx(std::string const& username,
                                        bool fanout) {
  LOG_TOPIC("bba16", INFO, Logger::QUERIES)
      << "aborting all " << (fanout ? "" : "local ") << "write transactions";
  Result res;

  DatabaseFeature& databaseFeature =
      _feature.server().getFeature<DatabaseFeature>();
  databaseFeature.enumerate([](TRI_vocbase_t* vocbase) {
    auto queryList = vocbase->queryList();
    TRI_ASSERT(queryList != nullptr);
    // we are only interested in killed write queries
    queryList->kill(
        [](aql::Query& query) { return query.isModificationQuery(); }, false);
  });

  // abort local transactions
  abortManagedTrx([](TransactionState const& state, std::string const& user) {
    return ::authorized(user) && !state.isReadOnlyTransaction();
  });

  if (fanout && ServerState::instance()->isCoordinator()) {
    auto& ci = _feature.server().getFeature<ClusterFeature>().clusterInfo();

    NetworkFeature const& nf = _feature.server().getFeature<NetworkFeature>();
    network::ConnectionPool* pool = nf.pool();
    if (pool == nullptr) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_SHUTTING_DOWN);
    }

    std::vector<network::FutureRes> futures;
    auto auth = AuthenticationFeature::instance();

    network::RequestOptions reqOpts;
    reqOpts.timeout = network::Timeout(30.0);
    reqOpts.param("local", "true");

    VPackBuffer<uint8_t> body;

    for (auto const& coordinator : ci.getCurrentCoordinators()) {
      if (coordinator == ServerState::instance()->getId()) {
        // ourselves!
        continue;
      }

      network::Headers headers;
      if (auth != nullptr && auth->isActive()) {
        if (!username.empty()) {
          headers.try_emplace(
              StaticStrings::Authorization,
              "bearer " + fuerte::jwt::generateUserToken(
                              auth->tokenCache().jwtSecret(), username));
        } else {
          headers.try_emplace(StaticStrings::Authorization,
                              "bearer " + auth->tokenCache().jwtToken());
        }
      }

      auto f = network::sendRequestRetry(
          pool, "server:" + coordinator, fuerte::RestVerb::Delete,
          "_api/transaction/write", body, reqOpts, std::move(headers));
      futures.emplace_back(std::move(f));
    }

    for (auto& f : futures) {
      network::Response const& resp = f.get();

      if (resp.statusCode() != fuerte::StatusOK) {
        VPackSlice slice = resp.slice();
        res.reset(network::resultFromBody(slice, TRI_ERROR_FAILED));
      }
    }
  }

  return res;
}

bool Manager::transactionIdExists(TransactionId const& tid) const {
  size_t bucket = getBucket(tid);
  // quick check whether ID exists
  READ_LOCKER(readLocker, _transactions[bucket]._lock);

  auto const& buck = _transactions[bucket];
  auto it = buck._managed.find(tid);
  return it != buck._managed.end();
}

bool Manager::storeManagedState(
    TransactionId const& tid, std::shared_ptr<arangodb::TransactionState> state,
    double ttl) {
  if (ttl <= 0) {
    ttl = Manager::ttlForType(_feature, MetaType::Managed);
  }

  TRI_ASSERT(state != nullptr);
  arangodb::cluster::CallbackGuard rGuard = buildCallbackGuard(*state);

  // add transaction to bucket
  size_t bucket = getBucket(tid);
  WRITE_LOCKER(writeLocker, _transactions[bucket]._lock);

  auto it = _transactions[bucket]._managed.try_emplace(
      tid, std::make_shared<ManagedTrx>(_feature, MetaType::Managed, ttl,
                                        std::move(state), std::move(rGuard)));
  return it.second;
}

std::shared_ptr<ManagedContext> Manager::buildManagedContextUnderLock(
    TransactionId tid, Manager::ManagedTrx& mtrx) {
  try {
    std::shared_ptr<TransactionState> state = mtrx.state;
    // the make_shared can throw, and in this case it is important that we
    // release the lock we have
    return std::make_shared<ManagedContext>(tid, std::move(state),
                                            /*responsibleForCommit*/ false,
                                            /*cloned*/ false);
  } catch (...) {
    // release lock in case something went wrong
    mtrx.rwlock.unlock();
    throw;
  }
}

Manager::TransactionCommitGuard Manager::getTransactionCommitGuard() {
  READ_LOCKER(guard, _hotbackupCommitLock);
  return guard;
}

CounterGuard::CounterGuard(Manager& manager) : _manager(manager) {
  _manager._nrRunning.fetch_add(1, std::memory_order_relaxed);
}

CounterGuard::~CounterGuard() {
  [[maybe_unused]] uint64_t r =
      _manager._nrRunning.fetch_sub(1, std::memory_order_relaxed);
  TRI_ASSERT(r > 0);
}

}  // namespace arangodb::transaction

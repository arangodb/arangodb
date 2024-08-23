////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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
#include "Auth/TokenCache.h"
#include "Assertions/ProdAssert.h"
#include "Basics/Exceptions.h"
#include "Basics/ReadLocker.h"
#include "Basics/ScopeGuard.h"
#include "Basics/WriteLocker.h"
#include "Basics/conversions.h"
#include "Basics/system-functions.h"
#include "Basics/voc-errors.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ServerState.h"
#include "Futures/Utilities.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "Logger/LogMacros.h"
#include "Network/Methods.h"
#include "Network/NetworkFeature.h"
#include "Network/Utils.h"
#include "RestServer/DatabaseFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "StorageEngine/TransactionCollection.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/Helpers.h"
#include "VocBase/vocbase.h"
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

#include <algorithm>
#include <optional>
#include <string_view>
#include <thread>

using namespace arangodb;

namespace {
bool authorized(std::string const& user) {
  auto const& exec = arangodb::ExecContext::current();
  if (exec.isSuperuser()) {
    return true;
  }
  return (user == exec.user());
}

std::string currentUser() { return arangodb::ExecContext::current().user(); }

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

namespace arangodb::transaction {

namespace {
struct MGMethods final : arangodb::transaction::Methods {
  MGMethods(std::shared_ptr<arangodb::transaction::Context> ctx,
            arangodb::transaction::Options const& opts)
      : Methods(std::move(ctx)) {}
};
}  // namespace

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

std::string_view Manager::typeName(MetaType type) {
  switch (type) {
    case MetaType::Managed:
      return "managed";
    case MetaType::StandaloneAQL:
      return "standalone-aql";
    case MetaType::Tombstone:
      return "tombstone";
  }
  return "unknown";
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

uint64_t Manager::getActiveTransactionCount() const noexcept {
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
      context(state ? buildContextFromState(*state, t) : ""),
      rwlock() {}

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
    MGMethods trx(std::shared_ptr<transaction::Context>(
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

bool Manager::ManagedTrx::hasPerformedIntermediateCommits() const noexcept {
  return intermediateCommits;
}

bool Manager::ManagedTrx::expired() const noexcept {
  return expiryTime < TRI_microtime();
}

void Manager::ManagedTrx::updateExpiry() noexcept {
  expiryTime = TRI_microtime() + timeToLive;
}

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
          [this, tid = state.id(), databaseName = state.vocbase().name()]() {
            // abort the transaction once the coordinator goes away
            abortManagedTrx(tid, databaseName).waitAndGet();
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
    auto it = buck._managed.try_emplace(tid, _feature, MetaType::StandaloneAQL,
                                        ttl, state, std::move(rGuard));
    if (!it.second) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_TRANSACTION_INTERNAL,
          absl::StrCat("transaction ID ", tid.id(),
                       "' already used (while registering AQL trx)"));
    }
  }
}

void Manager::unregisterAQLTrx(TransactionId tid) noexcept {
  size_t bucket = getBucket(tid);
  WRITE_LOCKER(writeLocker, _transactions[bucket]._lock);

  auto& buck = _transactions[bucket];
  auto it = buck._managed.find(tid);
  if (it == buck._managed.end()) {
    LOG_TOPIC("92a49", WARN, Logger::TRANSACTIONS)
        << "registered transaction " << tid << " not found";
    TRI_ASSERT(false);
    return;
  }
  TRI_ASSERT(it->second.type == MetaType::StandaloneAQL);

  /// we need to make sure no-one else is still using the TransactionState
  if (!it->second.rwlock.lockWrite(/*maxAttempts*/ 256)) {
    LOG_TOPIC("9f7d7", WARN, Logger::TRANSACTIONS)
        << "transaction " << tid << " is still in use";
    TRI_ASSERT(false);
    return;
  }

  buck._managed.erase(it);  // unlocking not necessary
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
    // if we are a follower, we allow transactions of arbitrary size.
    // this is because the max transaction size should be enforced already
    // when writing to the leader. the follower should simply accept
    // everything that the leader was able to apply successfully.
    options.maxTransactionSize = UINT64_MAX;

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

futures::Future<Result> Manager::addCollections(
    TRI_vocbase_t& vocbase, std::shared_ptr<TransactionState> state,
    std::vector<std::string> const& exclusiveCollections,
    std::vector<std::string> const& writeCollections,
    std::vector<std::string> const& readCollections) {
  Result res;
  CollectionNameResolver resolver(vocbase);

  auto addCols = [&](std::vector<std::string> const& cols,
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
                  absl::StrCat(
                      TRI_errno_string(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND),
                      ": ", cname));
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

  if (!co_await addCols(exclusiveCollections, AccessMode::Type::EXCLUSIVE) ||
      !co_await addCols(writeCollections, AccessMode::Type::WRITE) ||
      !co_await addCols(readCollections, AccessMode::Type::READ)) {
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
    // now start our own transaction
    return vocbase.engine().createTransactionState(vocbase, tid, options,
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

  // add collections
  res = co_await addCollections(vocbase, state, exclusiveCollections,
                                writeCollections, readCollections);
  if (res.fail()) {
    co_return res;
  }

  // start the transaction
  auto hints = ensureHints(options);
  // We allow to do a fast locking round here
  // We can only do this because we KNOW that the tid is not
  // known to any other place yet.
  if (!options.skipFastLockRound) {
    hints.set(transaction::Hints::Hint::ALLOW_FAST_LOCK_ROUND_CLUSTER);
  }
  res = co_await beginTransaction(hints, state);
  if (res.fail()) {
    co_return res;
  }
  // Unset the FastLockRound hint, if for some reason we ever end up locking
  // something again for this transaction we cannot recover from a fast lock
  // failure
  // note: we can unconditionally call unset here even if skipFastLockRound
  // was set. this does not do any harm.
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
                        absl::StrCat("transaction id ", tid.id(),
                                     " already used (before creating)"));
  }

  // no transaction with ID exists yet, so start a new transaction
  res = prepareOptions(options);
  if (res.fail()) {
    co_return res;
  }

  auto maybeState = basics::catchToResultT([&] {
    // now start our own transaction
    return vocbase.engine().createTransactionState(vocbase, tid, options,
                                                   operationOrigin);
  });
  if (!maybeState.ok()) {
    co_return std::move(maybeState).result();
  }
  auto& state = maybeState.get();

  TRI_ASSERT(state != nullptr);
  TRI_ASSERT(state->id() == tid);

  // add collections
  res = co_await addCollections(vocbase, state, exclusiveCollections,
                                writeCollections, readCollections);
  if (res.fail()) {
    co_return res;
  }

  // start the transaction
  auto hints = ensureHints(options);
  TRI_ASSERT(
      !hints.has(transaction::Hints::Hint::ALLOW_FAST_LOCK_ROUND_CLUSTER));
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
                        absl::StrCat("transaction id ", tid.id(),
                                     " already used (while creating)"));
  }

  LOG_TOPIC("d6806", DEBUG, Logger::TRANSACTIONS)
      << "created managed trx " << tid;

  co_return res;
}

namespace {
futures::Future<futures::Unit> sleepUsingSchedulerIfAvailable(
    std::string_view reason, std::chrono::milliseconds duration) {
#ifdef ARANGODB_USE_GOOGLE_TESTS
  constexpr bool alwaysHasScheduler = false;
#else
  constexpr bool alwaysHasScheduler = true;
#endif

  if (alwaysHasScheduler || SchedulerFeature::SCHEDULER) {
    TRI_ASSERT(SchedulerFeature::SCHEDULER != nullptr);
    return SchedulerFeature::SCHEDULER->delay(reason, duration);
  } else {
    std::this_thread::sleep_for(duration);
    return futures::Unit{};
  }
}
}  // namespace

/// @brief lease the transaction, increases nesting
futures::Future<std::shared_ptr<transaction::Context>> Manager::leaseManagedTrx(
    TransactionId tid, AccessMode::Type mode, bool isSideUser) {
  TRI_ASSERT(mode != AccessMode::Type::NONE);

  if (_disallowInserts.load(std::memory_order_acquire)) {
    co_return nullptr;
  }

  TRI_IF_FAILURE("leaseManagedTrxFail") { co_return nullptr; }

  auto role = ServerState::instance()->getRole();
  std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
  std::chrono::steady_clock::time_point endTime;
  if (!ServerState::isDBServer(role)) {  // keep end time as small as possible
    endTime =
        now + std::chrono::milliseconds(int64_t(1000 * _streamingLockTimeout));
  }
  // always serialize access on coordinator,
  // TransactionState::_knownServers is modified even for READ
  if (ServerState::isCoordinator(role)) {
    mode = AccessMode::Type::WRITE;
  }

  TRI_ASSERT(!isSideUser || AccessMode::isRead(mode));

  size_t bucket = getBucket(tid);
  int i = 0;
  do {
    READ_LOCKER(locker, _transactions[bucket]._lock);

    auto it = _transactions[bucket]._managed.find(tid);
    if (it == _transactions[bucket]._managed.end()) {
      // transaction actually not found
      co_return nullptr;
    }

    ManagedTrx& mtrx = it->second;
    if (mtrx.type == MetaType::Tombstone) {
      if (mtrx.finalStatus == transaction::Status::ABORTED) {
        THROW_ARANGO_EXCEPTION_MESSAGE(
            TRI_ERROR_TRANSACTION_ABORTED,
            absl::StrCat("transaction ", tid.id(),
                         " has already been aborted"));
      }

      TRI_ASSERT(mtrx.finalStatus == transaction::Status::COMMITTED);
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_TRANSACTION_DISALLOWED_OPERATION,
          absl::StrCat("transaction ", tid.id(),
                       " has already been committed"));
    }

    if (mtrx.expired()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_TRANSACTION_NOT_FOUND,
          absl::StrCat("transaction ", tid.id(), " has already expired"));
    }
    if (!isAuthorized(mtrx)) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_TRANSACTION_NOT_FOUND,
          absl::StrCat("not authorized to access transaction ", tid.id()));
    }

    if (AccessMode::isWriteOrExclusive(mode)) {
      if (mtrx.type == MetaType::StandaloneAQL) {
        THROW_ARANGO_EXCEPTION_MESSAGE(
            TRI_ERROR_TRANSACTION_DISALLOWED_OPERATION,
            "not allowed to write lock an AQL transaction");
      }
      if (mtrx.rwlock.tryLockWrite()) {
        auto managedContext = buildManagedContextUnderLock(tid, mtrx);
        if (mtrx.state->isReadOnlyTransaction()) {
          managedContext->setReadOnly();
        }
        co_return managedContext;
      }
      // continue the loop after a small pause
    } else {
      TRI_ASSERT(mode == AccessMode::Type::READ);
      // even for side user leases, first try acquiring the read lock
      if (mtrx.rwlock.tryLockRead()) {
        auto managedContext = buildManagedContextUnderLock(tid, mtrx);
        if (mtrx.state->isReadOnlyTransaction()) {
          managedContext->setReadOnly();
        }
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
          auto previous =
              mtrx.sideUsers.fetch_sub(1, std::memory_order_relaxed);
          TRI_ASSERT(previous > 0);
          throw;
        }
      }

      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_LOCKED, absl::StrCat("cannot read-lock, transaction ",
                                         tid.id(), " is already in use"));
    }

    locker.unlock();  // failure;

    // simon: never allow concurrent use of transactions
    // either busy loop until we get the lock or throw an error

    LOG_TOPIC("abd72", TRACE, Logger::TRANSACTIONS)
        << "transaction " << tid << " is already in use (RO)";

    TRI_ASSERT(endTime.time_since_epoch().count() == 0 ||
               !ServerState::instance()->isDBServer());

    auto now = std::chrono::steady_clock::now();
    if (!ServerState::isDBServer(role) && now > endTime) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_LOCKED, absl::StrCat("cannot write-lock, transaction ",
                                         tid.id(), " is already in use"));
    } else if ((i % 32) == 0) {
      LOG_TOPIC("9e972", DEBUG, Logger::TRANSACTIONS)
          << "waiting on trx write-lock " << tid;
      i = 0;
      if (_feature.server().isStopping()) {
        co_return nullptr;  // shutting down
      }
    }

    co_await sleepUsingSchedulerIfAvailable("managed-trx-lock-wait",
                                            std::chrono::milliseconds(10));
  } while (true);
}

void Manager::returnManagedTrx(TransactionId tid, bool isSideUser) noexcept {
  bool isSoftAborted = false;

  {
    size_t bucket = getBucket(tid);
    WRITE_LOCKER(writeLocker, _transactions[bucket]._lock);

    auto it = _transactions[bucket]._managed.find(tid);
    if (it == _transactions[bucket]._managed.end()) {
      LOG_TOPIC("1d5b0", WARN, Logger::TRANSACTIONS)
          << "managed transaction " << tid << " not found";
      TRI_ASSERT(false);
      return;
    }

    if (!isAuthorized(it->second)) {
      LOG_TOPIC("93894", WARN, Logger::TRANSACTIONS)
          << "not authorized to access managed transaction " << tid;
      TRI_ASSERT(false);
      return;
    }

    TRI_ASSERT(it->second.state != nullptr);

    if (isSideUser) {
      // number of side users is atomically decreased under the bucket's read
      // lock. due to us holding the bucket's read lock here, there can be no
      // other threads concurrently aborting/commiting the transaction (these
      // operations acquire the write lock on the transaction's bucket).
      auto previous =
          it->second.sideUsers.fetch_sub(1, std::memory_order_relaxed);
      TRI_ASSERT(previous > 0);
      // note: we are intentionally _not_ releasing the lock on the transaction
      // here, because we have not acquired it before!
    } else {
      // garbageCollection might soft abort used transactions
      isSoftAborted = it->second.expiryTime == 0;
      if (!isSoftAborted) {
        it->second.updateExpiry();
      }

      it->second.rwlock.unlock();
    }
  }

  // it is important that we release the write lock for the bucket here,
  // because abortManagedTrx will call statusChangeWithTimeout, which will
  // call updateTransaction, which then will try to acquire the same
  // write lock

  TRI_IF_FAILURE("returnManagedTrxForceSoftAbort") { isSoftAborted = true; }

  if (isSoftAborted) {
    TRI_ASSERT(!isSideUser);
    abortManagedTrx(tid, "" /* any database */).waitAndGet();
  }
}

/// @brief get the transaction state
transaction::Status Manager::getManagedTrxStatus(TransactionId tid) const {
  size_t bucket = getBucket(tid);
  READ_LOCKER(writeLocker, _transactions[bucket]._lock);

  auto it = _transactions[bucket]._managed.find(tid);
  if (it == _transactions[bucket]._managed.end() || !isAuthorized(it->second)) {
    return transaction::Status::UNDEFINED;
  }

  auto const& mtrx = it->second;

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
  constexpr double maxWaitTime = 9.0;
  Result res;
  while (true) {
    // does not acquire the lock
    CONDITIONAL_READ_LOCKER(guard, _hotbackupCommitLock, false);
    if (status == Status::COMMITTED) {
      // only for commit acquire the lock
      guard.lock();
    }
    res = updateTransaction(tid, status, false, database);
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
    co_await sleepUsingSchedulerIfAvailable("trx-status-change-backup",
                                            std::chrono::milliseconds{10});
  }
  co_return res;
}

futures::Future<Result> Manager::commitManagedTrx(TransactionId tid,
                                                  std::string const& database) {
  co_return co_await statusChangeWithTimeout(tid, database,
                                             transaction::Status::COMMITTED);
}

futures::Future<Result> Manager::abortManagedTrx(TransactionId tid,
                                                 std::string const& database) {
  return statusChangeWithTimeout(tid, database, transaction::Status::ABORTED);
}

Result Manager::updateTransaction(TransactionId tid, transaction::Status status,
                                  bool clearServers,
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
    std::string msg = absl::StrCat("transaction ", tid.id());
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

  std::shared_ptr<TransactionState> state;
  {
    WRITE_LOCKER(writeLocker, _transactions[bucket]._lock);

    auto& buck = _transactions[bucket];
    auto it = buck._managed.find(tid);
    if (it == buck._managed.end()) {
      // insert a tombstone for an aborted transaction that we never saw before
      auto inserted = buck._managed.try_emplace(
          tid, _feature, MetaType::Tombstone,
          ttlForType(_feature, MetaType::Tombstone), nullptr,
          arangodb::cluster::CallbackGuard{});
      inserted.first->second.finalStatus = transaction::Status::ABORTED;
      inserted.first->second.db = database;
      return res.reset(TRI_ERROR_TRANSACTION_NOT_FOUND,
                       buildErrorMessage(tid, status, /*found*/ false));
    }

    ManagedTrx& mtrx = it->second;
    if (!::authorized(mtrx.user) ||
        (!database.empty() && mtrx.db != database)) {
      return res.reset(TRI_ERROR_TRANSACTION_NOT_FOUND,
                       buildErrorMessage(tid, status, /*found*/ true));
    }

    // in order to modify the transaction's status, we need the write lock here,
    // plus we must ensure that the number of sideUsers is 0.
    TRY_WRITE_LOCKER(tryGuard, mtrx.rwlock);
    bool canAccessTrx = tryGuard.isLocked();
    if (canAccessTrx) {
      canAccessTrx &= (mtrx.sideUsers.load(std::memory_order_relaxed) == 0);
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
      std::string msg =
          absl::StrCat("updating", hint, "transaction status on ", operation,
                       " failed. transaction ", tid.id(), " is in use");
      LOG_TOPIC("dfc30", DEBUG, Logger::TRANSACTIONS) << msg;
      return res.reset(TRI_ERROR_LOCKED, std::move(msg));
    }

    TRI_ASSERT(tryGuard.isLocked());

    if (mtrx.type == MetaType::StandaloneAQL) {
      return res.reset(TRI_ERROR_TRANSACTION_DISALLOWED_OPERATION,
                       "not allowed to change an AQL transaction");
    } else if (mtrx.type == MetaType::Tombstone) {
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
          return res.reset(
              TRI_ERROR_CLUSTER_FOLLOWER_TRANSACTION_COMMIT_PERFORMED);
        }
        return res;  // all good
      } else {
        std::string_view operation;
        if (status == transaction::Status::COMMITTED) {
          operation = "commit";
        } else {
          operation = "abort";
        }
        return res.reset(
            TRI_ERROR_TRANSACTION_DISALLOWED_OPERATION,
            absl::StrCat("while trying to ", operation, " transaction ",
                         tid.id(), ": transaction was already ",
                         (mtrx.wasExpired ? "expired"
                                          : statusString(mtrx.finalStatus))));
      }
    }
    TRI_ASSERT(mtrx.type == MetaType::Managed);

    if (mtrx.expired()) {
      // we will update the expire time of the tombstone shortly afterwards,
      // but we need to store the fact that this transaction originally expired
      wasExpired = true;
      status = transaction::Status::ABORTED;
    }

    std::swap(state, mtrx.state);
    TRI_ASSERT(mtrx.state == nullptr);
    // type is changed under the transaction's write lock and the bucket's write
    // lock
    mtrx.type = MetaType::Tombstone;
    // clear context information of commit/aborted/expired transactions so that
    // we can save memory. there will be many tombstones on busy instances.
    mtrx.context = std::string{};
    if (state->numCommits() > 0) {
      // track the fact that we have performed a commit or an intermediate
      // commit. this is necessary for follower transactions, because we cannot
      // abort a transaction for which there have been intermediate commits
      // already.
      mtrx.intermediateCommits = true;
    }
    mtrx.wasExpired = wasExpired;
    mtrx.finalStatus = status;
    mtrx.updateExpiry();
    // it is sufficient to pretend that the operation already succeeded
  }

  TRI_ASSERT(state);
  if (!state) {  // this should never happen
    return res.reset(TRI_ERROR_INTERNAL, "managed trx in an invalid state");
  }

  auto abortTombstone = [&] {  // set tombstone entry to aborted
    WRITE_LOCKER(writeLocker, _transactions[bucket]._lock);
    auto& buck = _transactions[bucket];
    auto it = buck._managed.find(tid);
    if (it != buck._managed.end()) {
      it->second.finalStatus = Status::ABORTED;
    }
  };
  if (!state->isRunning()) {  // this also should not happen
    abortTombstone();
    return res.reset(TRI_ERROR_TRANSACTION_ABORTED,
                     "transaction was not running");
  }

  bool isCoordinator = state->isCoordinator();
  bool intermediateCommits = state->numCommits() > 0;

  MGMethods trx(std::make_shared<ManagedContext>(tid, std::move(state),
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
      abortTombstone();
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

  return res;
}

/// @brief calls the callback function for each managed transaction
void Manager::iterateManagedTrx(
    std::function<void(TransactionId, ManagedTrx const&)> const& callback,
    bool details) const {
  // iterate over all active transactions
  for (size_t bucket = 0; bucket < numBuckets; ++bucket) {
    // acquiring just the bucket lock in read mode is enough here,
    // as modifications to the transaction state lock
    // the transaction's bucket in write mode.
    READ_LOCKER(locker, _transactions[bucket]._lock);

    auto& buck = _transactions[bucket];
    for (auto const& it : buck._managed) {
      // by default, we only care about managed transactions here, unless we
      // want to see details
      if (it.second.type == MetaType::Managed ||
          (details && it.second.type == MetaType::StandaloneAQL)) {
        callback(it.first, it.second);
      }
    }
  }
}

/// @brief collect forgotten transactions
bool Manager::garbageCollect(bool abortAll) {
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
    if (abortAll) {
      _transactions[bucket]._lock.lockWrite();
    } else {
      _transactions[bucket]._lock.lockRead();
    }
    auto scope =
        scopeGuard([&]() noexcept { _transactions[bucket]._lock.unlock(); });

    for (auto& it : _transactions[bucket]._managed) {
      ManagedTrx& mtrx = it.second;

      if (mtrx.type == MetaType::Managed) {
        TRI_ASSERT(mtrx.state != nullptr);
        if (abortAll || mtrx.expired()) {
          ++numAborted;

          TRY_WRITE_LOCKER(tryGuard,
                           mtrx.rwlock);  // needs lock to access state

          if (tryGuard.isLocked()) {
            TRI_ASSERT(mtrx.state->isRunning());
            TRI_ASSERT(it.first == mtrx.state->id());
            toAbort.emplace_back(mtrx.state->id());
            LOG_TOPIC("7ad3f", INFO, Logger::TRANSACTIONS)
                << "aborting expired transaction " << it.first;
          } else if (abortAll) {  // transaction is in use but we want to abort
            LOG_TOPIC("92431", INFO, Logger::TRANSACTIONS)
                << "soft-aborting expired transaction " << it.first;
            mtrx.expiryTime = 0;  // soft-abort transaction
            didWork = true;
            LOG_TOPIC("7ad4f", INFO, Logger::TRANSACTIONS)
                << "soft aborting transaction " << it.first;
          }
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

  for (TransactionId tid : toAbort) {
    LOG_TOPIC("6fbaf", INFO, Logger::TRANSACTIONS) << "garbage collecting "
                                                   << "transaction " << tid;
    try {
      Result res = updateTransaction(tid, Status::ABORTED, /*clearSrvs*/ true);
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
  containers::SmallVector<TransactionId, 8> toAbort;

  for (size_t bucket = 0; bucket < numBuckets; ++bucket) {
    READ_LOCKER(locker, _transactions[bucket]._lock);

    auto it = _transactions[bucket]._managed.begin();
    while (it != _transactions[bucket]._managed.end()) {
      ManagedTrx& mtrx = it->second;
      if (mtrx.type == MetaType::Managed) {
        TRI_ASSERT(mtrx.state != nullptr);
        TRY_READ_LOCKER(tryGuard, mtrx.rwlock);  // needs lock to access state
        if (tryGuard.isLocked() && cb(*mtrx.state, mtrx.user)) {
          toAbort.emplace_back(it->first);
        }
      }

      ++it;  // next
    }
  }

  for (TransactionId tid : toAbort) {
    Result res = updateTransaction(tid, Status::ABORTED, /*clearSrvs*/ true);
    if (res.fail() &&
        !res.is(TRI_ERROR_CLUSTER_FOLLOWER_TRANSACTION_COMMIT_PERFORMED)) {
      LOG_TOPIC("2bf48", INFO, Logger::TRANSACTIONS)
          << "error aborting transaction " << tid << ": " << res.errorMessage();
    }
  }
  return !toAbort.empty();
}

void Manager::toVelocyPack(VPackBuilder& builder, std::string const& database,
                           std::string const& username, bool fanout,
                           bool details) const {
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
    if (details) {
      options.param("details", "true");
    }

    VPackBuffer<uint8_t> body;

    auto servers = ci.getCurrentCoordinators();
    if (details) {
      auto dbs = ci.getCurrentDBServers();
      std::move(dbs.begin(), dbs.end(), std::back_inserter(servers));
    }

    for (auto const& server : servers) {
      if (server == ServerState::instance()->getId()) {
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
          pool, "server:" + server, fuerte::RestVerb::Get, "/_api/transaction",
          body, options, std::move(headers));
      futures.emplace_back(std::move(f));
    }

    if (!futures.empty()) {
      auto responses = futures::collectAll(futures).waitAndGet();
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
  iterateManagedTrx(
      [this, &builder, &database, details](TransactionId tid,
                                           ManagedTrx const& trx) {
        bool authorized = isAuthorized(trx, database);
        if (details && arangodb::ExecContext::current().isSuperuser()) {
          authorized = true;
        }
        if (!authorized) {
          return;
        }

        builder.openObject(true);
        builder.add("id", VPackValue(std::to_string(tid.id())));
        if (trx.state == nullptr) {
          builder.add("state", VPackValue("none"));
        } else {
          builder.add(
              "state",
              VPackValue(transaction::statusString(trx.state->status())));
        }
        if (details) {
          std::string_view idType = "unknown";
          if (tid.isCoordinatorTransactionId()) {
            idType = "coordinator id";
          } else if (tid.isFollowerTransactionId()) {
            idType = "follower id";
          } else if (tid.isLeaderTransactionId()) {
            idType = "leader id";
          } else if (tid.isLegacyTransactionId()) {
            idType = "legacy id";
          }
          builder.add("idType", VPackValue(idType));
          if (!ServerState::instance()->isSingleServer()) {
            builder.add("coordinatorId",
                        VPackValue(tid.asCoordinatorTransactionId().id()));
          }
          builder.add("context", VPackValue(trx.context));
          builder.add("type", VPackValue(typeName(trx.type)));
          if (trx.type != MetaType::Tombstone) {
            builder.add("expired", VPackValue(trx.expired()));
          }
          builder.add("hasPerformedIntermediateCommits",
                      VPackValue(trx.hasPerformedIntermediateCommits()));
          builder.add(
              "sideUsers",
              VPackValue(trx.sideUsers.load(std::memory_order_relaxed)));
          builder.add("finalStatus",
                      VPackValue(transaction::statusString(trx.finalStatus)));
          builder.add("timeToLive", VPackValue(trx.timeToLive));
          // note: expiry timestamp is a system clock value that indicates
          // number of seconds since the OS was started.
          builder.add("expiryTime",
                      VPackValue(TRI_StringTimeStamp(
                          trx.expiryTime, Logger::getUseLocalTime())));
          if (!ServerState::instance()->isDBServer()) {
            // proper user information is only present on single servers
            // and coordinators
            builder.add("user", VPackValue(trx.user));
          }
          builder.add("database", VPackValue(trx.db));
          builder.add("server", VPackValue(ServerState::instance()->getId()));
        }
        builder.close();
      },
      details);
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
      network::Response const& resp = f.waitAndGet();

      if (resp.statusCode() != fuerte::StatusOK) {
        VPackSlice slice = resp.slice();
        res.reset(network::resultFromBody(slice, TRI_ERROR_FAILED));
      }
    }
  }

  return res;
}

bool Manager::transactionIdExists(TransactionId tid) const {
  size_t bucket = getBucket(tid);
  // quick check whether ID exists
  READ_LOCKER(readLocker, _transactions[bucket]._lock);

  auto const& buck = _transactions[bucket];
  auto it = buck._managed.find(tid);
  return it != buck._managed.end();
}

bool Manager::storeManagedState(
    TransactionId tid, std::shared_ptr<arangodb::TransactionState> state,
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
      tid, _feature, MetaType::Managed, ttl, std::move(state),
      std::move(rGuard));
  return it.second;
}

bool Manager::isAuthorized(ManagedTrx const& trx) const {
  auto const& exec = arangodb::ExecContext::current();
  return isAuthorized(trx, exec.database());
}

bool Manager::isAuthorized(ManagedTrx const& trx,
                           std::string_view database) const {
  auto const& exec = arangodb::ExecContext::current();
  if (exec.isSuperuser()) {
    // if we are a superuser, we do not check the database.
    // This is necessary because we have a few code paths where we no longer
    // have the ExecContext with the user who initiated the request (e.g., the
    // RestDocumentHandler has the _activeTrx member, but when that gets
    // destroyed and thereby releases the transaction, the ExecContext is
    // already gone).
    // TODO - this should be improved in a separate step
    return true;
  }
  return trx.user == exec.user() && trx.db == database;
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

std::string Manager::buildContextFromState(TransactionState& state,
                                           MetaType t) {
  std::string result =
      absl::StrCat("trx type: ", typeName(t),
                   state.isSingleOperation() ? ", single op" : ", multi op",
                   state.isReadOnlyTransaction() ? ", read only" : "",
                   state.isFollowerTransaction() ? ", follower" : "",
                   ", lock timeout: ", state.lockTimeout());

  if (state.hasHint(transaction::Hints::Hint::INTERMEDIATE_COMMITS)) {
    absl::StrAppend(&result, ", intermediate commits");
  }
  if (state.hasHint(transaction::Hints::Hint::ALLOW_RANGE_DELETE)) {
    absl::StrAppend(&result, ", allow range delete");
  }
  if (state.hasHint(transaction::Hints::Hint::FROM_TOPLEVEL_AQL)) {
    absl::StrAppend(&result, ", from toplevel aql");
  }
  if (state.hasHint(transaction::Hints::Hint::GLOBAL_MANAGED)) {
    absl::StrAppend(&result, ", global managed");
  }
  if (state.hasHint(transaction::Hints::Hint::INDEX_CREATION)) {
    absl::StrAppend(&result, ", index creation");
  }

  if (state.numCollections() > 0) {
    absl::StrAppend(&result, ", collections: [");
    bool first = true;
    state.allCollections([&result, &first](auto const& c) -> bool {
      if (first) {
        first = false;
      } else {
        absl::StrAppend(&result, ", ");
      }
      absl::StrAppend(
          &result, "{id: ", c.id().id(), ", name: ", c.collectionName(),
          ", access: ", AccessMode::typeString(c.accessType()), "}");
      return true;
    });
    absl::StrAppend(&result, "]");
  }
  return result;
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

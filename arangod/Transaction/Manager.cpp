////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
#include "Basics/ReadLocker.h"
#include "Basics/ScopeGuard.h"
#include "Basics/WriteLocker.h"
#include "Basics/system-functions.h"
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
#include "StorageEngine/TransactionState.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"
#include "Transaction/SmartContext.h"
#include "Transaction/Status.h"
#include "Utils/CollectionNameResolver.h"
#include "Utils/ExecContext.h"

#ifdef USE_ENTERPRISE
#include "Enterprise/VocBase/VirtualCollection.h"
#endif

#include <fuerte/jwt.h>
#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

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

namespace arangodb {
namespace transaction {

size_t constexpr Manager::maxTransactionSize;

namespace {
struct MGMethods final : arangodb::transaction::Methods {
  MGMethods(std::shared_ptr<arangodb::transaction::Context> const& ctx,
            arangodb::transaction::Options const& opts)
      : Methods(ctx, opts) {}
};
}  // namespace

Manager::Manager(ManagerFeature& feature)
    : _feature(feature),
      _nrRunning(0),
      _nrReadLocked(0),
      _disallowInserts(false),
      _writeLockHeld(false),
      _streamingLockTimeout(feature.streamingLockTimeout()) {}

void Manager::registerTransaction(TransactionId transactionId, bool isReadOnlyTransaction,
                                  bool isFollowerTransaction) {
  // If isFollowerTransaction is set then either the transactionId should be
  // an isFollowerTransactionId or it should be a legacy transactionId:
  TRI_ASSERT(!isFollowerTransaction || transactionId.isFollowerTransactionId() ||
             transactionId.isLegacyTransactionId());
  if (!isReadOnlyTransaction && !isFollowerTransaction) {
    LOG_TOPIC("ccdea", TRACE, Logger::TRANSACTIONS)
        << "Acquiring read lock for tid " << transactionId.id();
    _rwLock.lockRead();
    _nrReadLocked.fetch_add(1, std::memory_order_relaxed);
    LOG_TOPIC("ccdeb", TRACE, Logger::TRANSACTIONS)
        << "Got read lock for tid " << transactionId.id()
        << " nrReadLocked: " << _nrReadLocked.load(std::memory_order_relaxed);
  }

  _nrRunning.fetch_add(1, std::memory_order_relaxed);
}

// unregisters a transaction
void Manager::unregisterTransaction(TransactionId transactionId, bool isReadOnlyTransaction,
                                    bool isFollowerTransaction) {
  // always perform an unlock when we leave this function
  auto guard = scopeGuard([this, transactionId, &isReadOnlyTransaction,
                           &isFollowerTransaction]() {
    if (!isReadOnlyTransaction && !isFollowerTransaction) {
      _rwLock.unlockRead();
      _nrReadLocked.fetch_sub(1, std::memory_order_relaxed);
      LOG_TOPIC("ccded", TRACE, Logger::TRANSACTIONS)
          << "Released lock for tid " << transactionId.id()
          << " nrReadLocked: " << _nrReadLocked.load(std::memory_order_relaxed);
    }
  });

  uint64_t r = _nrRunning.fetch_sub(1, std::memory_order_relaxed);
  TRI_ASSERT(r > 0);
}

uint64_t Manager::getActiveTransactionCount() {
  return _nrRunning.load(std::memory_order_relaxed);
}

/*static*/ double Manager::ttlForType(ManagerFeature const& feature, Manager::MetaType type) {
  if (type == Manager::MetaType::Tombstone) {
    return tombstoneTTL;
  }

  auto role = ServerState::instance()->getRole();
  if ((ServerState::isSingleServer(role) || ServerState::isCoordinator(role))) {
    return feature.streamingIdleTimeout();
  }
  return idleTTLDBServer;
}

Manager::ManagedTrx::ManagedTrx(ManagerFeature const& feature, MetaType t, double ttl, 
                                std::shared_ptr<TransactionState> st,
                                arangodb::cluster::CallbackGuard rGuard)
    : type(t),
      intermediateCommits(false),
      wasExpired(false),
      finalStatus(Status::UNDEFINED),
      timeToLive(ttl),
      expiryTime(TRI_microtime() + Manager::ttlForType(feature, t)),
      state(std::move(st)),
      rGuard(std::move(rGuard)),
      user(::currentUser()),
      db(state ? state->vocbase().name() : ""),
      rwlock() {}

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
    auto ctx = std::make_shared<transaction::ManagedContext>(TransactionId{2}, state,
                                                             /*responsibleForCommit*/ true);
    MGMethods trx(ctx, opts);  // own state now
    TRI_ASSERT(trx.state()->status() == transaction::Status::RUNNING);
    TRI_ASSERT(trx.isMainTransaction());
    trx.abort();
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

Result buildOptions(VPackSlice trxOpts, 
                    transaction::Options& options,
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

arangodb::cluster::CallbackGuard Manager::buildCallbackGuard(TransactionState const& state) {
  arangodb::cluster::CallbackGuard rGuard;
  
  if (ServerState::instance()->isDBServer()) {
    auto const& origin = state.options().origin;
    if (!origin.serverId().empty()) {
      auto& clusterFeature = _feature.server().getFeature<ClusterFeature>();
      auto& clusterInfo = clusterFeature.clusterInfo();
      rGuard = clusterInfo.rebootTracker().callMeOnChange(
          cluster::RebootTracker::PeerState(origin.serverId(), origin.rebootId()),
          [this, tid = state.id()]() {
            // abort the transaction once the coordinator goes away
            abortManagedTrx(tid, std::string());
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
  size_t const bucket = getBucket(tid);
  {
    WRITE_LOCKER(writeLocker, _transactions[bucket]._lock);

    auto& buck = _transactions[bucket];

    double ttl = Manager::ttlForType(_feature, MetaType::StandaloneAQL);
    auto it = buck._managed.try_emplace(tid, _feature, MetaType::StandaloneAQL, ttl, state, std::move(rGuard));
    if (!it.second) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_TRANSACTION_INTERNAL,
          std::string("transaction ID ") + std::to_string(tid.id()) +
              "' already used (while registering AQL trx)");
    }
  }
}

void Manager::unregisterAQLTrx(TransactionId tid) noexcept {
  const size_t bucket = getBucket(tid);
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

ResultT<TransactionId> Manager::createManagedTrx(TRI_vocbase_t& vocbase, VPackSlice trxOpts) {
  transaction::Options options;
  std::vector<std::string> reads, writes, exclusives;

  Result res = buildOptions(trxOpts, options, reads, writes, exclusives);
  if (res.fail()) {
    return res;
  }

  return createManagedTrx(vocbase, reads, writes, exclusives, std::move(options));
}

Result Manager::ensureManagedTrx(TRI_vocbase_t& vocbase, TransactionId tid,
                                 VPackSlice trxOpts, bool isFollowerTransaction) {
  transaction::Options options;
  std::vector<std::string> reads, writes, exclusives;

  Result res = buildOptions(trxOpts, options, reads, writes, exclusives);
  if (res.fail()) {
    return res;
  }

  if (isFollowerTransaction) {
    options.isFollowerTransaction = true;
  }

  return ensureManagedTrx(vocbase, tid, reads, writes, exclusives, std::move(options));
}

transaction::Hints Manager::ensureHints(transaction::Options& options) const {
  transaction::Hints hints;
  hints.set(transaction::Hints::Hint::GLOBAL_MANAGED);
  if (isFollowerTransactionOnDBServer(options)) {
    hints.set(transaction::Hints::Hint::IS_FOLLOWER_TRX);
    // turn on intermediate commits on followers as well. otherwise huge leader
    // transactions could make the follower claim all memory and crash.
    hints.set(transaction::Hints::Hint::INTERMEDIATE_COMMITS);
  }
  return hints;
}

Result Manager::beginTransaction(transaction::Hints hints,
                                 std::shared_ptr<TransactionState>& state) {
  Result res;
  try {
    res = state->beginTransaction(hints);  // registers with transaction manager
  } catch (basics::Exception const& ex) {
    res.reset(ex.code(), ex.what());
  }

  TRI_ASSERT(res.ok() || !state->isRunning());
  return res;
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
    // on db server 3 for shard A, and db server 2 may try to do the same for shard B.
    // Both calls will only send data for "their" shards, so effectively we need to
    // add write collections to the transaction at runtime whenever this happens.
    // It is important that all these calls succeed, because otherwise one of the calls
    // would just drop db server 3 as a follower.
    options.allowImplicitCollectionsForWrite = true;

    // we should not have any locking conflicts on followers, generally. shard
    // locking should be performed on leaders first, which will then, eventually
    // replicate changes to followers. replication to followers is only done
    // once the locks have been acquired on the leader(s). so if there are any
    // locking issues, they are supposed to happen first on leaders, and not
    // affect followers. that's why we can hard-code the lock timeout here to a
    // rather low value on followers
    constexpr double followerLockTimeout = 15.0;
    if (options.lockTimeout == 0.0 || options.lockTimeout >= followerLockTimeout) {
      options.lockTimeout = followerLockTimeout;
    }
  } else {
    // for all other transactions, apply a size limitation
    options.maxTransactionSize =
        std::min<size_t>(options.maxTransactionSize, Manager::maxTransactionSize);
  }

  return res;
}

Result Manager::lockCollections(TRI_vocbase_t& vocbase,
                                std::shared_ptr<TransactionState> state,
                                std::vector<std::string> const& exclusiveCollections,
                                std::vector<std::string> const& writeCollections,
                                std::vector<std::string> const& readCollections) {
  Result res;
  CollectionNameResolver resolver(vocbase);

  auto lockCols = [&](std::vector<std::string> const& cols, AccessMode::Type mode) {
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
                  std::string(TRI_errno_string(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND)) +
                      ": " + cname);
      } else {
#ifdef USE_ENTERPRISE
        if (state->isCoordinator()) {
          try {
            std::shared_ptr<LogicalCollection> col = resolver.getCollection(cname);
            if (col->isSmart() && col->type() == TRI_COL_TYPE_EDGE) {
              auto theEdge =
                  dynamic_cast<arangodb::VirtualSmartEdgeCollection*>(col.get());
              if (theEdge == nullptr) {
                THROW_ARANGO_EXCEPTION_MESSAGE(
                    TRI_ERROR_INTERNAL,
                    "cannot cast collection to smart edge collection");
              }
              res.reset(state->addCollection(theEdge->getLocalCid(), "_local_" + cname,
                                             mode, /*lockUsage*/ false));
              if (res.fail()) {
                return false;
              }
              res.reset(state->addCollection(theEdge->getFromCid(), "_from_" + cname,
                                             mode, /*lockUsage*/ false));
              if (res.fail()) {
                return false;
              }
              res.reset(state->addCollection(theEdge->getToCid(), "_to_" + cname,
                                             mode, /*lockUsage*/ false));
              if (res.fail()) {
                return false;
              }
            }
          } catch (basics::Exception const& ex) {
            res.reset(ex.code(), ex.what());
            return false;
          }
        }
#endif
        res.reset(state->addCollection(cid, cname, mode, /*lockUsage*/ false));
      }

      if (res.fail()) {
        return false;
      }
    }
    return true;
  };

  if (!lockCols(exclusiveCollections, AccessMode::Type::EXCLUSIVE) ||
      !lockCols(writeCollections, AccessMode::Type::WRITE) ||
      !lockCols(readCollections, AccessMode::Type::READ)) {
    if (res.fail()) {
      // error already set by callback function
      return res;
    }
    // no error set. so it must be "data source not found"
    return res.reset(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
  }

  return res;
}

bool Manager::isFollowerTransactionOnDBServer(transaction::Options const& options) const {
  return ServerState::instance()->isDBServer() && options.isFollowerTransaction;
}

/// @brief create managed transaction
ResultT<TransactionId> Manager::createManagedTrx(
    TRI_vocbase_t& vocbase, std::vector<std::string> const& readCollections,
    std::vector<std::string> const& writeCollections,
    std::vector<std::string> const& exclusiveCollections,
    transaction::Options options, double ttl) {
  // We cannot run this on FollowerTransactions.
  // They need to get injected the TransactionIds.
  TRI_ASSERT(!isFollowerTransactionOnDBServer(options));
  Result res;
  if (_disallowInserts.load(std::memory_order_acquire)) {
    return res.reset(TRI_ERROR_SHUTTING_DOWN);
  }

  // no transaction with ID exists yet, so start a new transaction
  res = prepareOptions(options);
  if (res.fail()) {
    return res;
  }
  std::shared_ptr<TransactionState> state;

  ServerState::RoleEnum role = ServerState::instance()->getRole();
  TRI_ASSERT(ServerState::isSingleServerOrCoordinator(role));
  TransactionId tid = ServerState::isSingleServer(role)
                          ? TransactionId::createSingleServer()
                          : TransactionId::createCoordinator();
  try {
    // now start our own transaction
    StorageEngine& engine = vocbase.server().getFeature<EngineSelectorFeature>().engine();
    state = engine.createTransactionState(vocbase, tid, options);
  } catch (basics::Exception const& e) {
    return res.reset(e.code(), e.message());
  }
  TRI_ASSERT(state != nullptr);
  TRI_ASSERT(state->id() == tid);

  // lock collections
  res = lockCollections(vocbase, state, exclusiveCollections, writeCollections, readCollections);
  if (res.fail()) {
    return res;
  }

  // start the transaction
  auto hints = ensureHints(options);
  // We allow to do a fast locking round here
  // We can only do this because we KNOW that the tid is not
  // known to any other place yet.
  hints.set(transaction::Hints::Hint::ALLOW_FAST_LOCK_ROUND_CLUSTER);
  res = beginTransaction(hints, state);
  if (res.fail()) {
    return res;
  }
  // Unset the FastLockRound hint, if for some reason we ever end up locking
  // something again for this transaction we cannot recover from a fast lock
  // failure
  hints.unset(transaction::Hints::Hint::ALLOW_FAST_LOCK_ROUND_CLUSTER);

  // During beginTransaction we may reroll the Transaction ID.
  tid = state->id();

  bool stored = storeManagedState(tid, std::move(state), ttl);
  if (!stored) {
    return res.reset(TRI_ERROR_TRANSACTION_INTERNAL,
                     std::string("transaction id ") + std::to_string(tid.id()) +
                         " already used (while creating)");
  }

  LOG_TOPIC("d6807", DEBUG, Logger::TRANSACTIONS) << "created managed trx " << tid;

  return ResultT{tid};
}

/// @brief create managed transaction
Result Manager::ensureManagedTrx(TRI_vocbase_t& vocbase, TransactionId tid,
                                 std::vector<std::string> const& readCollections,
                                 std::vector<std::string> const& writeCollections,
                                 std::vector<std::string> const& exclusiveCollections,
                                 transaction::Options options, double ttl) {
  Result res;
  if (_disallowInserts.load(std::memory_order_acquire)) {
    return res.reset(TRI_ERROR_SHUTTING_DOWN);
  }

  if (tid.isFollowerTransactionId()) {
    options.isFollowerTransaction = true;
  }

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
      return res;
    }

    return res.reset(TRI_ERROR_TRANSACTION_INTERNAL,
                     std::string("transaction id ") + std::to_string(tid.id()) +
                         " already used (before creating)");
  }

  // no transaction with ID exists yet, so start a new transaction
  res = prepareOptions(options);
  if (res.fail()) {
    return res;
  }

  std::shared_ptr<TransactionState> state;
  try {
    // now start our own transaction
    StorageEngine& engine = vocbase.server().getFeature<EngineSelectorFeature>().engine();
    state = engine.createTransactionState(vocbase, tid, options);
  } catch (basics::Exception const& e) {
    return res.reset(e.code(), e.message());
  }
  TRI_ASSERT(state != nullptr);
  TRI_ASSERT(state->id() == tid);

  // lock collections
  res = lockCollections(vocbase, state, exclusiveCollections, writeCollections, readCollections);
  if (res.fail()) {
    return res;
  }

  // start the transaction
  auto hints = ensureHints(options);
  res = beginTransaction(hints, state);
  if (res.fail()) {
    return res;
  }
  // The coordinator in some cases can reroll the Transaction id.
  // This however can not be allowed here, as this transaction ID
  // may be managed outside.
  TRI_ASSERT(state->id() == tid);

  bool stored = storeManagedState(tid, std::move(state), ttl);
  if (!stored) {
    if (isFollowerTransactionOnDBServer(options)) {
      TRI_ASSERT(res.ok());
      return res;
    }

    return res.reset(TRI_ERROR_TRANSACTION_INTERNAL,
                     std::string("transaction id ") + std::to_string(tid.id()) +
                         " already used (while creating)");
  }

  LOG_TOPIC("d6806", DEBUG, Logger::TRANSACTIONS) << "created managed trx " << tid;

  return res;
}

/// @brief lease the transaction, increases nesting
std::shared_ptr<transaction::Context> Manager::leaseManagedTrx(TransactionId tid,
                                                               AccessMode::Type mode) {
  TRI_ASSERT(mode != AccessMode::Type::NONE);
  if (_disallowInserts.load(std::memory_order_acquire)) {
    return nullptr;
  }

  TRI_IF_FAILURE("leaseManagedTrxFail") { return nullptr; }

  auto const role = ServerState::instance()->getRole();
  std::chrono::steady_clock::time_point endTime;
  if (!ServerState::isDBServer(role)) {  // keep end time as small as possible
    endTime = std::chrono::steady_clock::now() +
              std::chrono::milliseconds(int64_t(1000 * _streamingLockTimeout));
  }
  // always serialize access on coordinator,
  // TransactionState::_knownServers is modified even for READ
  if (ServerState::isCoordinator(role)) {
    mode = AccessMode::Type::WRITE;
  }

  size_t const bucket = getBucket(tid);
  int i = 0;
  std::shared_ptr<TransactionState> state;
  do {
    READ_LOCKER(locker, _transactions[bucket]._lock);

    auto it = _transactions[bucket]._managed.find(tid);
    if (it == _transactions[bucket]._managed.end()) {
      return nullptr;
    }

    ManagedTrx& mtrx = it->second;
    if (mtrx.type == MetaType::Tombstone || mtrx.expired() || !::authorized(mtrx.user)) {
      return nullptr;  // no need to return anything
    }

    if (AccessMode::isWriteOrExclusive(mode)) {
      if (mtrx.type == MetaType::StandaloneAQL) {
        THROW_ARANGO_EXCEPTION_MESSAGE(
            TRI_ERROR_TRANSACTION_DISALLOWED_OPERATION,
            "not allowed to write lock an AQL transaction");
      }
      if (mtrx.rwlock.tryLockWrite()) {
        state = mtrx.state;
        break;
      }
    } else {
      if (mtrx.rwlock.tryLockRead()) {
        TRI_ASSERT(mode == AccessMode::Type::READ);
        state = mtrx.state;
        break;
      }
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_LOCKED, std::string("cannot read-lock, transaction ") +
                                std::to_string(tid.id()) + " is already in use");
    }

    // simon: never allow concurrent use of transactions
    // either busy loop until we get the lock or throw an error

    LOG_TOPIC("abd72", TRACE, Logger::TRANSACTIONS)
        << "transaction " << tid << " is already in use (RO)";

    locker.unlock();  // failure;

    // simon: Two allowed scenarios:
    // 1. User sends concurrent write (CRUD) requests, (which was never intended to be possible)
    //    but now we do have to kind of support it otherwise shitty apps break
    // 2. one does a bulk write within a el-cheapo / V8 transaction into multiple shards
    //    on the same DBServer (still bad design).
    TRI_ASSERT(endTime.time_since_epoch().count() == 0 ||
               !ServerState::instance()->isDBServer());

    if (!ServerState::isDBServer(role) && std::chrono::steady_clock::now() > endTime) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_LOCKED, std::string("cannot write-lock, transaction ") +
                                std::to_string(tid.id()) + " is already in use");
    } else if ((i % 32) == 0) {
      LOG_TOPIC("9e972", DEBUG, Logger::TRANSACTIONS)
          << "waiting on trx write-lock " << tid;
      i = 0;
      if (_feature.server().isStopping()) {
        return nullptr;  // shutting down
      }
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(10));

  } while (true);

  if (state) {
    return std::make_shared<ManagedContext>(tid, std::move(state),
                                            /*responsibleForCommit*/ false);
  }
  TRI_ASSERT(false);  // should be unreachable
  return nullptr;
}

void Manager::returnManagedTrx(TransactionId tid) noexcept {
  bool isSoftAborted = false;

  {
    size_t const bucket = getBucket(tid);
    WRITE_LOCKER(writeLocker, _transactions[bucket]._lock);

    auto it = _transactions[bucket]._managed.find(tid);
    if (it == _transactions[bucket]._managed.end() || !::authorized(it->second.user)) {
      LOG_TOPIC("1d5b0", WARN, Logger::TRANSACTIONS)
          << "managed transaction " << tid << " not found";
      TRI_ASSERT(false);
      return;
    }

    TRI_ASSERT(it->second.state != nullptr);

    // garbageCollection might soft abort used transactions
    isSoftAborted = it->second.expiryTime == 0;
    if (!isSoftAborted) {
      it->second.updateExpiry();
    }

    it->second.rwlock.unlock();
  }

  // it is important that we release the write lock for the bucket here,
  // because abortManagedTrx will call statusChangeWithTimeout, which will
  // call updateTransaction, which then will try to acquire the same
  // write lock

  TRI_IF_FAILURE("returnManagedTrxForceSoftAbort") { isSoftAborted = true; }

  if (isSoftAborted) {
    abortManagedTrx(tid, "" /* any database */);
  }
}

/// @brief get the transasction state
transaction::Status Manager::getManagedTrxStatus(TransactionId tid,
                                                 std::string const& database) const {
  size_t bucket = getBucket(tid);
  READ_LOCKER(writeLocker, _transactions[bucket]._lock);

  auto it = _transactions[bucket]._managed.find(tid);
  if (it == _transactions[bucket]._managed.end() ||
      !::authorized(it->second.user) || it->second.db != database) {
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

Result Manager::statusChangeWithTimeout(TransactionId tid, std::string const& database,
                                        transaction::Status status) {
  double startTime = 0.0;
  constexpr double maxWaitTime = 2.0;
  Result res;
  while (true) {
    res = updateTransaction(tid, status, false, database);
    if (res.ok() || !res.is(TRI_ERROR_LOCKED)) {
      break;
    }
    double const now = TRI_microtime();
    if (startTime <= 0.0001) {  // fp tolerance
      startTime = now;
    } else if (now - startTime > maxWaitTime) {
      // timeout
      break;
    }
    std::this_thread::yield();
  }
  return res;
}

Result Manager::commitManagedTrx(TransactionId tid, std::string const& database) {
  return statusChangeWithTimeout(tid, database, transaction::Status::COMMITTED);
}

Result Manager::abortManagedTrx(TransactionId tid, std::string const& database) {
  return statusChangeWithTimeout(tid, database, transaction::Status::ABORTED);
}

Result Manager::updateTransaction(TransactionId tid, transaction::Status status,
                                  bool clearServers, std::string const& database) {
  TRI_ASSERT(status == transaction::Status::COMMITTED ||
             status == transaction::Status::ABORTED);

  LOG_TOPIC("7bd2f", DEBUG, Logger::TRANSACTIONS)
      << "managed trx " << tid << " updating to '"
      << (status == transaction::Status::COMMITTED ? "COMMITED" : "ABORTED") << "'";

  Result res;
  size_t const bucket = getBucket(tid);
  bool wasExpired = false;

  std::shared_ptr<TransactionState> state;
  {
    WRITE_LOCKER(writeLocker, _transactions[bucket]._lock);

    auto& buck = _transactions[bucket];
    auto it = buck._managed.find(tid);
    if (it == buck._managed.end() || !::authorized(it->second.user) ||
        (!database.empty() && it->second.db != database)) {
      std::string msg = "transaction " + std::to_string(tid.id());
      if (it == buck._managed.end()) {
        msg += " not found";
      } else {
        msg += " inaccessible";
      }
      if (status == transaction::Status::COMMITTED) {
        msg += " on commit operation";
      } else {
        msg += " on abort operation";
      }
      return res.reset(TRI_ERROR_TRANSACTION_NOT_FOUND, std::move(msg));
    }

    ManagedTrx& mtrx = it->second;
    TRY_WRITE_LOCKER(tryGuard, mtrx.rwlock);
    if (!tryGuard.isLocked()) {
      LOG_TOPIC("dfc30", DEBUG, Logger::TRANSACTIONS) << "transaction " << tid << " is in use";
      return res.reset(TRI_ERROR_LOCKED,
                       std::string("read lock failed, transaction ") +
                           std::to_string(tid.id()) + " is in use");
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
        if (ServerState::instance()->isDBServer() && tid.isFollowerTransactionId() &&
            mtrx.finalStatus == transaction::Status::ABORTED && mtrx.intermediateCommits) {
          // we are trying to abort a follower transaction (again) that had intermediate
          // commits already. in this case we return a special error code, which makes
          // the leader drop us as a follower for all shards in the transaction.
          return res.reset(TRI_ERROR_CLUSTER_FOLLOWER_TRANSACTION_COMMIT_PERFORMED);
        }
        return res;  // all good
      } else {
        std::string msg("transaction was already ");
        if (mtrx.wasExpired) {
          msg.append("expired");
        } else {
          msg.append(statusString(mtrx.finalStatus));
        }
        return res.reset(TRI_ERROR_TRANSACTION_DISALLOWED_OPERATION, std::move(msg));
      }
    }
    TRI_ASSERT(mtrx.type == MetaType::Managed);

    if (mtrx.expired()) {
      // we will update the expire time of the tombstone shortly afterwards, 
      // so we need to store the fact that this transaction originally expired
      wasExpired = true;
      status = transaction::Status::ABORTED;
    }

    std::swap(state, mtrx.state);
    TRI_ASSERT(mtrx.state == nullptr);
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

  bool const isCoordinator = state->isCoordinator();
  bool const intermediateCommits = state->numCommits() > 0;

  transaction::Options trxOpts;
  MGMethods trx(std::make_shared<ManagedContext>(tid, std::move(state),
                                                 /*responsibleForCommit*/ true),
                trxOpts);
  TRI_ASSERT(trx.state()->isRunning());
  TRI_ASSERT(trx.isMainTransaction());
  if (clearServers && !isCoordinator) {
    trx.state()->clearKnownServers();
  }
  if (status == transaction::Status::COMMITTED) {
    res = trx.commit();
    if (res.fail()) {  // set final status to aborted
      abortTombstone();
    }
  } else {
    res = trx.abort();
    if (intermediateCommits && ServerState::instance()->isDBServer() &&
        tid.isFollowerTransactionId()) {
      // we are trying to abort a follower transaction that had intermediate
      // commits already. in this case we return a special error code, which makes
      // the leader drop us as a follower for all shards in the transaction.
      res.reset(TRI_ERROR_CLUSTER_FOLLOWER_TRANSACTION_COMMIT_PERFORMED);
    } else if (res.ok() && wasExpired) {
      res.reset(TRI_ERROR_TRANSACTION_ABORTED);
    }
  }
  TRI_ASSERT(!trx.state()->isRunning());

  return res;
}

/// @brief calls the callback function for each managed transaction
void Manager::iterateManagedTrx(std::function<void(TransactionId, ManagedTrx const&)> const& callback) const {
  // iterate over all active transactions
  for (size_t bucket = 0; bucket < numBuckets; ++bucket) {
    READ_LOCKER(locker, _transactions[bucket]._lock);

    auto& buck = _transactions[bucket];
    for (auto const& it : buck._managed) {
      if (it.second.type == MetaType::Managed) {
        // we only care about managed transactions here
        callback(it.first, it.second);
      }
    }
  }
}

/// @brief collect forgotten transactions
bool Manager::garbageCollect(bool abortAll) {
  bool didWork = false;
  ::arangodb::containers::SmallVector<TransactionId, 64>::allocator_type::arena_type a1;
  ::arangodb::containers::SmallVector<TransactionId, 64> toAbort{a1};
  ::arangodb::containers::SmallVector<TransactionId, 64>::allocator_type::arena_type a2;
  ::arangodb::containers::SmallVector<TransactionId, 64> toErase{a2};

  uint64_t numAborted = 0;

  for (size_t bucket = 0; bucket < numBuckets; ++bucket) {
    if (abortAll) {
      _transactions[bucket]._lock.lockWrite();
    } else {
      _transactions[bucket]._lock.lockRead();
    }
    auto scope = scopeGuard([&] { _transactions[bucket]._lock.unlock(); });

    for (auto& it : _transactions[bucket]._managed) {
      ManagedTrx& mtrx = it.second;

      if (mtrx.type == MetaType::Managed) {
        TRI_ASSERT(mtrx.state != nullptr);
        if (abortAll || mtrx.expired()) {
          ++numAborted;

          TRY_WRITE_LOCKER(tryGuard, mtrx.rwlock);  // needs lock to access state

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
    LOG_TOPIC("1df7f", DEBUG, Logger::TRANSACTIONS) << "garbage-collecting expired transaction " << tid;
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
        LOG_TOPIC("0a07f", INFO, Logger::TRANSACTIONS) << "error while aborting "
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
    size_t const bucket = getBucket(tid);
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
bool Manager::abortManagedTrx(std::function<bool(TransactionState const&, std::string const&)> cb) {
  ::arangodb::containers::SmallVector<TransactionId, 64>::allocator_type::arena_type arena;
  ::arangodb::containers::SmallVector<TransactionId, 64> toAbort{arena};

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
    if (res.fail() && !res.is(TRI_ERROR_CLUSTER_FOLLOWER_TRANSACTION_COMMIT_PERFORMED)) {
      LOG_TOPIC("2bf48", INFO, Logger::TRANSACTIONS) << "error aborting "
                                                        "transaction: "
                                                     << res.errorMessage();
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
          headers.try_emplace(StaticStrings::Authorization,
                              "bearer " + fuerte::jwt::generateUserToken(
                                              auth->tokenCache().jwtSecret(), username));
        } else {
          headers.try_emplace(StaticStrings::Authorization,
                              "bearer " + auth->tokenCache().jwtToken());
        }
      }

      auto f = network::sendRequest(pool, "server:" + coordinator,
                                    fuerte::RestVerb::Get, "/_api/transaction",
                                    body, options, std::move(headers));
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
  iterateManagedTrx([&builder, &database](TransactionId tid, ManagedTrx const& trx) {
    if (::authorized(trx.user) && trx.db == database) {
      builder.openObject(true);
      builder.add("id", VPackValue(std::to_string(tid.id())));
      builder.add("state", VPackValue(transaction::statusString(trx.state->status())));
      builder.close();
    }
  });
}

Result Manager::abortAllManagedWriteTrx(std::string const& username, bool fanout) {
  LOG_TOPIC("bba16", INFO, Logger::QUERIES)
      << "aborting all " << (fanout ? "" : "local ") << "write transactions";
  Result res;

  DatabaseFeature& databaseFeature = _feature.server().getFeature<DatabaseFeature>();
  databaseFeature.enumerate([](TRI_vocbase_t* vocbase) {
    auto queryList = vocbase->queryList();
    TRI_ASSERT(queryList != nullptr);
    // we are only interested in killed write queries
    queryList->kill([](aql::Query& query) { return query.isModificationQuery(); }, false);
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
          headers.try_emplace(StaticStrings::Authorization,
                              "bearer " + fuerte::jwt::generateUserToken(
                                              auth->tokenCache().jwtSecret(), username));
        } else {
          headers.try_emplace(StaticStrings::Authorization,
                              "bearer " + auth->tokenCache().jwtToken());
        }
      }

      auto f = network::sendRequest(pool, "server:" + coordinator, fuerte::RestVerb::Delete,
                                    "_api/transaction/write", body, reqOpts,
                                    std::move(headers));
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
  size_t const bucket = getBucket(tid);
  // quick check whether ID exists
  READ_LOCKER(readLocker, _transactions[bucket]._lock);

  auto const& buck = _transactions[bucket];
  auto it = buck._managed.find(tid);
  return it != buck._managed.end();
}

bool Manager::storeManagedState(TransactionId const& tid,
                                std::shared_ptr<arangodb::TransactionState> state,
                                double ttl) {
  if (ttl <= 0) {
    ttl = Manager::ttlForType(_feature, MetaType::Managed);
  }

  TRI_ASSERT(state != nullptr);
  arangodb::cluster::CallbackGuard rGuard = buildCallbackGuard(*state);

  // add transaction to bucket
  size_t const bucket = getBucket(tid);
  WRITE_LOCKER(writeLocker, _transactions[bucket]._lock);

  auto it = _transactions[bucket]._managed.try_emplace(tid, _feature, MetaType::Managed, ttl,
                                                       std::move(state), std::move(rGuard));
  return it.second;
}

}  // namespace transaction
}  // namespace arangodb

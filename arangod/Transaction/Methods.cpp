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
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

#include <velocypack/Builder.h>
#include <velocypack/Collection.h>
#include <velocypack/Iterator.h>
#include <velocypack/Options.h>
#include <velocypack/Slice.h>

#include "Methods.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Aql/Ast.h"
#include "Aql/AstNode.h"
#include "Aql/Condition.h"
#include "Basics/Exceptions.h"
#include "Basics/ScopeGuard.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/system-compiler.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ClusterMethods.h"
#include "Cluster/ClusterTrxMethods.h"
#include "Cluster/FollowerInfo.h"
#include "Cluster/ReplicationTimeoutFeature.h"
#include "Cluster/ServerState.h"
#include "Containers/FlatHashSet.h"
#include "Futures/Utilities.h"
#include "Indexes/Index.h"
#include "Logger/Logger.h"
#include "Network/Methods.h"
#include "Network/NetworkFeature.h"
#include "Network/Utils.h"
#include "Random/RandomGenerator.h"
#include "Metrics/Counter.h"
#include "Replication/ReplicationMetricsFeature.h"
#include "Replication2/StateMachines/Document/DocumentLeaderState.h"
#include "RestServer/DatabaseFeature.h"
#include "RocksDBEngine/ReplicatedRocksDBTransactionCollection.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/PhysicalCollection.h"
#include "StorageEngine/StorageEngine.h"
#include "StorageEngine/TransactionCollection.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/BatchOptions.h"
#include "Transaction/Context.h"
#include "Transaction/Helpers.h"
#include "Transaction/Options.h"
#include "Utils/CollectionNameResolver.h"
#include "Utils/Events.h"
#include "Utils/ExecContext.h"
#include "Utils/OperationOptions.h"
#include "VocBase/ComputedValues.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ManagedDocumentResult.h"
#include "VocBase/ticks.h"

#include <sstream>

using namespace arangodb;
using namespace arangodb::transaction;
using namespace arangodb::transaction::helpers;

template<typename T>
using Future = futures::Future<T>;

namespace {

BatchOptions buildBatchOptions(OperationOptions const& options,
                               LogicalCollection& collection,
                               TRI_voc_document_operation_e opType,
                               bool isDBServer) {
  BatchOptions batchOptions;

  if (!options.isRestore && options.isSynchronousReplicationFrom.empty()) {
    if (isDBServer) {
      batchOptions.validateShardKeysOnUpdateReplace =
          opType != TRI_VOC_DOCUMENT_OPERATION_INSERT &&
          (collection.shardKeys().size() > 1 ||
           collection.shardKeys()[0] != StaticStrings::KeyString);
      batchOptions.validateSmartJoinAttribute =
          collection.hasSmartJoinAttribute();
    }

    if (options.validate) {
      batchOptions.schema = collection.schema();
    }

    auto cv = collection.computedValues();
    if (cv != nullptr) {
      bool pick = false;

      if (opType == TRI_VOC_DOCUMENT_OPERATION_INSERT) {
        pick = cv->mustComputeValuesOnInsert();
        if (options.overwriteMode == OperationOptions::OverwriteMode::Replace) {
          pick |= cv->mustComputeValuesOnReplace();
        } else if (options.overwriteMode ==
                   OperationOptions::OverwriteMode::Update) {
          pick |= cv->mustComputeValuesOnUpdate();
        }
      } else if (opType == TRI_VOC_DOCUMENT_OPERATION_UPDATE) {
        pick |= cv->mustComputeValuesOnUpdate();
      } else if (opType == TRI_VOC_DOCUMENT_OPERATION_REPLACE) {
        pick |= cv->mustComputeValuesOnReplace();
      }

      if (pick) {
        batchOptions.computedValues = std::move(cv);
      }
    }
  }

  return batchOptions;
}

/// @brief check if a list of attributes have the same values in two vpack
/// documents
bool shardKeysChanged(LogicalCollection const& collection,
                      velocypack::Slice oldValue, velocypack::Slice newValue,
                      bool isPatch) {
  TRI_ASSERT(oldValue.isObject());
  TRI_ASSERT(newValue.isObject());

  auto const& shardKeys = collection.shardKeys();

  for (auto const& shardKey : shardKeys) {
    if (shardKey == StaticStrings::KeyString) {
      continue;
    }

    VPackSlice n = newValue.get(shardKey);

    if (n.isNone() && isPatch) {
      // attribute not set in patch document. this means no update
      continue;
    }

    VPackSlice o = oldValue.get(shardKey);

    if (o.isNone()) {
      // if attribute is undefined, use "null" instead
      o = velocypack::Slice::nullSlice();
    }

    if (n.isNone()) {
      // if attribute is undefined, use "null" instead
      n = velocypack::Slice::nullSlice();
    }

    if (!basics::VelocyPackHelper::equal(n, o, false)) {
      return true;
    }
  }

  return false;
}

bool smartJoinAttributeChanged(LogicalCollection const& collection,
                               VPackSlice oldValue, VPackSlice newValue,
                               bool isPatch) {
  if (!collection.hasSmartJoinAttribute()) {
    return false;
  }
  if (!oldValue.isObject() || !newValue.isObject()) {
    // expecting two objects. everything else is an error
    return true;
  }

  auto const& s = collection.smartJoinAttribute();

  VPackSlice n = newValue.get(s);
  if (!n.isString()) {
    if (isPatch && n.isNone()) {
      // attribute not set in patch document. this means no update
      return false;
    }

    // no string value... invalid!
    return true;
  }

  VPackSlice o = oldValue.get(s);
  TRI_ASSERT(o.isString());

  return !basics::VelocyPackHelper::equal(n, o, false);
}

/// @brief choose a timeout for synchronous replication, based on the
/// number of documents we ship over
double chooseTimeoutForReplication(ReplicationTimeoutFeature const& feature,
                                   size_t count, size_t totalBytes) {
  // We essentially stop using a meaningful timeout for this operation.
  // This is achieved by setting the default for the minimal timeout to 15m or
  // 900s. The reason behind this is the following: We have to live with RocksDB
  // stalls and write stops, which can happen in overload situations. Then, no
  // meaningful timeout helps and it is almost certainly better to keep trying
  // to not have to drop the follower and make matters worse. In case of an
  // actual failure (or indeed a restart), the follower is marked as failed and
  // its reboot id is increased. As a consequence, the connection is aborted and
  // we run into an error anyway. This is when a follower will be dropped.

  // We leave this code in place for now.

  // We usually assume that a server can process at least 2500 documents
  // per second (this is a low estimate), and use a low limit of 0.5s
  // and a high timeout of 120s
  double timeout = count / 2500.0;

  // Really big documents need additional adjustment. Using total size
  // of all messages to handle worst case scenario of constrained resource
  // processing all
  timeout += (totalBytes / 4096.0) * feature.timeoutPer4k();

  return std::clamp(timeout, feature.lowerLimit(), feature.upperLimit()) *
         feature.timeoutFactor();
}

Result buildRefusalResult(LogicalCollection const& collection,
                          std::string_view operation,
                          OperationOptions const& options,
                          std::string const& leader) {
  std::stringstream msg;
  msg << TRI_errno_string(TRI_ERROR_CLUSTER_SHARD_FOLLOWER_REFUSES_OPERATION)
      << ": shard: " << collection.vocbase().name() << "/" << collection.name()
      << ", operation: " << operation
      << ", from: " << options.isSynchronousReplicationFrom
      << ", current leader: " << leader;
  return Result(TRI_ERROR_CLUSTER_SHARD_FOLLOWER_REFUSES_OPERATION, msg.str());
}

// wrap vector inside a static function to ensure proper initialization order
std::vector<arangodb::transaction::Methods::DataSourceRegistrationCallback>&
getDataSourceRegistrationCallbacks() {
  static std::vector<
      arangodb::transaction::Methods::DataSourceRegistrationCallback>
      callbacks;

  return callbacks;
}

/// @return the status change callbacks stored in state
///         or nullptr if none and !create
std::vector<arangodb::transaction::Methods::StatusChangeCallback const*>*
getStatusChangeCallbacks(arangodb::TransactionState& state,
                         bool create = false) {
  struct CookieType : public arangodb::TransactionState::Cookie {
    std::vector<arangodb::transaction::Methods::StatusChangeCallback const*>
        _callbacks;
  };

  static const int key = 0;  // arbitrary location in memory, common for all

// TODO FIXME find a better way to look up a ViewState
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  auto* cookie = dynamic_cast<CookieType*>(state.cookie(&key));
#else
  auto* cookie = static_cast<CookieType*>(state.cookie(&key));
#endif

  if (!cookie && create) {
    auto ptr = std::make_unique<CookieType>();

    cookie = ptr.get();
    state.cookie(&key, std::move(ptr));
  }

  return cookie ? &(cookie->_callbacks) : nullptr;
}

/// @brief notify callbacks of association of 'cid' with this TransactionState
/// @note done separately from addCollection() to avoid creating a
///       TransactionCollection instance for virtual entities, e.g. View
arangodb::Result applyDataSourceRegistrationCallbacks(
    LogicalDataSource& dataSource, arangodb::transaction::Methods& trx) {
  for (auto& callback : getDataSourceRegistrationCallbacks()) {
    TRI_ASSERT(
        callback);  // addDataSourceRegistrationCallback(...) ensures valid

    try {
      auto res = callback(dataSource, trx);

      if (res.fail()) {
        return res;
      }
    } catch (...) {
      return arangodb::Result(TRI_ERROR_INTERNAL);
    }
  }

  return arangodb::Result();
}

/// @brief notify callbacks of association of 'cid' with this TransactionState
/// @note done separately from addCollection() to avoid creating a
///       TransactionCollection instance for virtual entities, e.g. View
Result applyStatusChangeCallbacks(arangodb::transaction::Methods& trx,
                                  arangodb::transaction::Status status) noexcept
    try {
  TRI_ASSERT(arangodb::transaction::Status::ABORTED == status ||
             arangodb::transaction::Status::COMMITTED == status ||
             arangodb::transaction::Status::RUNNING == status);

  auto* state = trx.state();
  if (!state) {
    return {};  // nothing to apply
  }

  TRI_ASSERT(trx.isMainTransaction());

  auto* callbacks = getStatusChangeCallbacks(*state);

  if (!callbacks) {
    return {};  // no callbacks to apply
  }

  Result res;

  // no need to lock since transactions are single-threaded
  for (auto& callback : *callbacks) {
    TRI_ASSERT(callback);  // addStatusChangeCallback(...) ensures valid

    try {
      (*callback)(trx, status);
    } catch (basics::Exception const& ex) {
      // we must not propagate exceptions from here
      if (res.ok()) {
        // only track the first error
        res = {ex.code(), ex.what()};
      }
    } catch (std::exception const& ex) {
      // we must not propagate exceptions from here
      if (res.ok()) {
        // only track the first error
        res = {TRI_ERROR_INTERNAL, ex.what()};
      }
    } catch (...) {
      if (res.ok()) {
        res = {
            TRI_ERROR_INTERNAL,
            "caught unknown exception while applying status change callbacks"};
      }
    }
  }

  return res;
} catch (...) {
  return Result(TRI_ERROR_OUT_OF_MEMORY);
}

void throwCollectionNotFound(std::string const& name) {
  THROW_ARANGO_EXCEPTION_MESSAGE(
      TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
      std::string(TRI_errno_string(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND)) +
          ": " + name);
}

/// @brief Insert an error reported instead of the new document
void createBabiesError(VPackBuilder* builder,
                       std::unordered_map<ErrorCode, size_t>& countErrorCodes,
                       Result const& error) {
  // on followers, builder will be a nullptr, so we can spare building
  // the error result details in the response body, which the leader
  // will ignore anyway.
  if (builder != nullptr) {
    // only build error detail results if we got a builder passed here.
    builder->openObject(false);
    builder->add(StaticStrings::Error, VPackValue(true));
    builder->add(StaticStrings::ErrorNum, VPackValue(error.errorNumber()));
    builder->add(StaticStrings::ErrorMessage, VPackValue(error.errorMessage()));
    builder->close();
  }

  // always (also on followers) increase error counter for the
  // error code we got.
  auto& value = countErrorCodes[error.errorNumber()];
  ++value;
}

OperationResult emptyResult(OperationOptions const& options) {
  VPackBuilder resultBuilder;
  resultBuilder.add(VPackSlice::emptyArraySlice());
  return OperationResult(Result(), resultBuilder.steal(), options);
}

}  // namespace

/*static*/ void transaction::Methods::addDataSourceRegistrationCallback(
    DataSourceRegistrationCallback const& callback) {
  if (callback) {
    getDataSourceRegistrationCallbacks().emplace_back(callback);
  }
}

bool transaction::Methods::addStatusChangeCallback(
    StatusChangeCallback const* callback) {
  if (!callback || !*callback) {
    return true;  // nothing to call back
  } else if (!_state) {
    return false;  // nothing to add to
  }

  auto* statusChangeCallbacks = getStatusChangeCallbacks(*_state, true);

  TRI_ASSERT(nullptr != statusChangeCallbacks);  // 'create' was specified

  // no need to lock since transactions are single-threaded
  statusChangeCallbacks->emplace_back(callback);

  return true;
}

bool transaction::Methods::removeStatusChangeCallback(
    StatusChangeCallback const* callback) {
  if (!callback || !*callback) {
    return true;  // nothing to call back
  } else if (!_state) {
    return false;  // nothing to add to
  }

  auto* statusChangeCallbacks = getStatusChangeCallbacks(*_state, false);
  if (statusChangeCallbacks) {
    auto it = std::find(statusChangeCallbacks->begin(),
                        statusChangeCallbacks->end(), callback);
    TRI_ASSERT(it != statusChangeCallbacks->end());
    if (ADB_LIKELY(it != statusChangeCallbacks->end())) {
      statusChangeCallbacks->erase(it);
    }
  }
  return true;
}

/*static*/ void transaction::Methods::clearDataSourceRegistrationCallbacks() {
  getDataSourceRegistrationCallbacks().clear();
}

TRI_vocbase_t& transaction::Methods::vocbase() const {
  return _state->vocbase();
}

/// @brief whether or not the transaction consists of a single operation only
bool transaction::Methods::isSingleOperationTransaction() const {
  return _state->isSingleOperation();
}

/// @brief get the status of the transaction
transaction::Status transaction::Methods::status() const {
  return _state->status();
}

velocypack::Options const& transaction::Methods::vpackOptions() const {
  return *transactionContextPtr()->getVPackOptions();
}

/// @brief Find out if any of the given requests has ended in a refusal
/// by a leader.
static bool findRefusal(
    std::vector<futures::Try<network::Response>> const& responses) {
  for (auto const& it : responses) {
    if (it.hasValue() && it.get().ok() &&
        it.get().statusCode() == fuerte::StatusNotAcceptable) {
      auto r = it.get().combinedResult();
      bool followerRefused =
          (r.errorNumber() ==
           TRI_ERROR_CLUSTER_SHARD_LEADER_REFUSES_REPLICATION);
      if (followerRefused) {
        return true;
      }
    }
  }
  return false;
}

transaction::Methods::Methods(std::shared_ptr<transaction::Context> const& ctx,
                              transaction::Options const& options)
    : _state(nullptr), _transactionContext(ctx), _mainTransaction(false) {
  TRI_ASSERT(_transactionContext != nullptr);
  if (ADB_UNLIKELY(_transactionContext == nullptr)) {
    // in production, we must not go on with undefined behavior, so we bail out
    // here with an exception as last resort
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "invalid transaction context pointer");
  }

  // initialize the transaction
  _state = _transactionContext->acquireState(options, _mainTransaction);
  TRI_ASSERT(_state != nullptr);
}

transaction::Methods::Methods(std::shared_ptr<transaction::Context> ctx,
                              std::string const& collectionName,
                              AccessMode::Type type)
    : transaction::Methods(std::move(ctx), transaction::Options{}) {
  TRI_ASSERT(AccessMode::isWriteOrExclusive(type));
  Result res = Methods::addCollection(collectionName, type);
  if (res.fail()) {
    THROW_ARANGO_EXCEPTION(res);
  }
}

/// @brief create the transaction, used to be UserTransaction
transaction::Methods::Methods(
    std::shared_ptr<transaction::Context> const& ctx,
    std::vector<std::string> const& readCollections,
    std::vector<std::string> const& writeCollections,
    std::vector<std::string> const& exclusiveCollections,
    transaction::Options const& options)
    : transaction::Methods(ctx, options) {
  Result res;
  for (auto const& it : exclusiveCollections) {
    res = Methods::addCollection(it, AccessMode::Type::EXCLUSIVE);
    if (res.fail()) {
      THROW_ARANGO_EXCEPTION(res);
    }
  }
  for (auto const& it : writeCollections) {
    res = Methods::addCollection(it, AccessMode::Type::WRITE);
    if (res.fail()) {
      THROW_ARANGO_EXCEPTION(res);
    }
  }
  for (auto const& it : readCollections) {
    res = Methods::addCollection(it, AccessMode::Type::READ);
    if (res.fail()) {
      THROW_ARANGO_EXCEPTION(res);
    }
  }
}

/// @brief destroy the transaction
transaction::Methods::~Methods() {
  if (_mainTransaction) {  // _nestingLevel == 0
    // unregister transaction from context
    _transactionContext->unregisterTransaction();

    // auto abort a non-read-only and still running transaction
    if (_state->status() == transaction::Status::RUNNING) {
      if (_state->isReadOnlyTransaction()) {
        // read-only transactions are never comitted or aborted during their
        // regular life cycle. we want now to properly clean up and count them.
        _state->updateStatus(transaction::Status::COMMITTED);
      } else {
        auto res = this->abort();
        if (res.fail()) {
          LOG_TOPIC("6d20f", ERR, Logger::TRANSACTIONS)
              << "Abort failed while destroying transaction " << tid()
              << " on server " << ServerState::instance()->getId() << " "
              << res;
        }
      }
    }

    // free the state associated with the transaction
    TRI_ASSERT(_state->status() != transaction::Status::RUNNING);

    // store result in context
    _transactionContext->storeTransactionResult(
        _state->id(), _state->wasRegistered(), _state->isReadOnlyTransaction(),
        _state->isFollowerTransaction());

    _state = nullptr;
  }
}

/// @brief return the collection name resolver
CollectionNameResolver const* transaction::Methods::resolver() const {
  return &(_transactionContext->resolver());
}

/// @brief return the transaction collection for a document collection
TransactionCollection* transaction::Methods::trxCollection(
    DataSourceId cid, AccessMode::Type type) const {
  TRI_ASSERT(_state != nullptr);
  TRI_ASSERT(_state->status() == transaction::Status::RUNNING ||
             _state->status() == transaction::Status::CREATED);
  return _state->collection(cid, type);
}

/// @brief return the transaction collection for a document collection
TransactionCollection* transaction::Methods::trxCollection(
    std::string const& name, AccessMode::Type type) const {
  TRI_ASSERT(_state != nullptr);
  TRI_ASSERT(_state->status() == transaction::Status::RUNNING ||
             _state->status() == transaction::Status::CREATED);
  return _state->collection(name, type);
}

/// @brief extract the _id attribute from a slice, and convert it into a
/// string
std::string transaction::Methods::extractIdString(VPackSlice slice) {
  return transaction::helpers::extractIdString(resolver(), slice, VPackSlice());
}

/// @brief build a VPack object with _id, _key and _rev, the result is
/// added to the builder in the argument as a single object.
void transaction::Methods::buildDocumentIdentity(
    LogicalCollection& collection, velocypack::Builder& builder,
    DataSourceId cid, std::string_view key, RevisionId rid, RevisionId oldRid,
    velocypack::Builder const* oldDoc, velocypack::Builder const* newDoc) {
  builder.openObject();

  // _id
  StringLeaser leased(_transactionContext.get());
  std::string& temp(*leased.get());
  temp.reserve(64);

  if (_state->isRunningInCluster()) {
    std::string resolved = resolver()->getCollectionNameCluster(cid);
#ifdef USE_ENTERPRISE
    if (resolved.starts_with(StaticStrings::FullLocalPrefix)) {
      resolved.erase(0, StaticStrings::FullLocalPrefix.size());
    } else if (resolved.starts_with(StaticStrings::FullFromPrefix)) {
      resolved.erase(0, StaticStrings::FullFromPrefix.size());
    } else if (resolved.starts_with(StaticStrings::FullToPrefix)) {
      resolved.erase(0, StaticStrings::FullToPrefix.size());
    }
#endif
    // build collection name
    temp.append(resolved);
  } else {
    // build collection name
    temp.append(collection.name());
  }

  // append / and key part
  temp.push_back('/');
  temp.append(key.data(), key.size());

  builder.add(StaticStrings::IdString, VPackValue(temp));

  // _key
  builder.add(StaticStrings::KeyString, VPackValue(key));

  // _rev
  char ridBuffer[arangodb::basics::maxUInt64StringSize];
  builder.add(StaticStrings::RevString, rid.toValuePair(ridBuffer));

  // _oldRev
  if (oldRid.isSet()) {
    builder.add("_oldRev", VPackValue(oldRid.toString()));
  }

  // old
  if (oldDoc != nullptr) {
    builder.add(StaticStrings::Old, oldDoc->slice());
  }

  // new
  if (newDoc != nullptr) {
    builder.add(StaticStrings::New, newDoc->slice());
  }

  builder.close();
}

/// @brief begin the transaction
Result transaction::Methods::begin() {
  if (_state == nullptr) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "invalid transaction state");
  }

  Result res;

  if (_mainTransaction) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    bool a = _localHints.has(transaction::Hints::Hint::FROM_TOPLEVEL_AQL);
    bool b = _localHints.has(transaction::Hints::Hint::GLOBAL_MANAGED);
    TRI_ASSERT(!(a && b));
#endif

    res = _state->beginTransaction(_localHints);
    if (res.ok()) {
      res = applyStatusChangeCallbacks(*this, Status::RUNNING);
    }
  } else {
    TRI_ASSERT(_state->status() == transaction::Status::RUNNING);
  }

  return res;
}

auto Methods::commit() noexcept -> Result {
  return commitInternal(MethodsApi::Synchronous)  //
      .then(basics::tryToResult)
      .get();
}

/// @brief commit / finish the transaction
auto transaction::Methods::commitAsync() noexcept -> Future<Result> {
  return commitInternal(MethodsApi::Asynchronous)  //
      .then(basics::tryToResult);
}

auto Methods::abort() noexcept -> Result {
  return abortInternal(MethodsApi::Synchronous)  //
      .then(basics::tryToResult)
      .get();
}

/// @brief abort the transaction
auto transaction::Methods::abortAsync() noexcept -> Future<Result> {
  return abortInternal(MethodsApi::Asynchronous)  //
      .then(basics::tryToResult);
}

auto Methods::finish(Result const& res) noexcept -> Result {
  return finishInternal(res, MethodsApi::Synchronous)  //
      .then(basics::tryToResult)
      .get();
}

/// @brief finish a transaction (commit or abort), based on the previous state
auto transaction::Methods::finishAsync(Result const& res) noexcept
    -> Future<Result> {
  return finishInternal(res, MethodsApi::Asynchronous)  //
      .then(basics::tryToResult);
}

/// @brief return the transaction id
TransactionId transaction::Methods::tid() const {
  TRI_ASSERT(_state != nullptr);
  return _state->id();
}

std::string transaction::Methods::name(DataSourceId cid) const {
  auto c = trxCollection(cid);
  if (c == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
  }
  return c->collectionName();
}

/// @brief read all master pointers, using skip and limit.
/// The resualt guarantees that all documents are contained exactly once
/// as long as the collection is not modified.
OperationResult transaction::Methods::any(std::string const& collectionName,
                                          OperationOptions const& options) {
  if (_state->isCoordinator()) {
    return anyCoordinator(collectionName, options);
  }
  return anyLocal(collectionName, options);
}

/// @brief fetches documents in a collection in random order, coordinator
OperationResult transaction::Methods::anyCoordinator(std::string const&,
                                                     OperationOptions const&) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

/// @brief fetches documents in a collection in random order, local
OperationResult transaction::Methods::anyLocal(
    std::string const& collectionName, OperationOptions const& options) {
  DataSourceId cid =
      addCollectionAtRuntime(collectionName, AccessMode::Type::READ);
  TransactionCollection* trxColl = trxCollection(cid);
  if (trxColl == nullptr) {
    throwCollectionNotFound(collectionName);
  }

  VPackBuilder resultBuilder;
  if (_state->isDBServer()) {
    std::shared_ptr<LogicalCollection> const& collection =
        trxColl->collection();
    if (collection == nullptr) {
      return OperationResult(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, options);
    }
    auto const& followerInfo = collection->followers();
    if (!followerInfo->getLeader().empty()) {
      return OperationResult(TRI_ERROR_CLUSTER_SHARD_LEADER_RESIGNED, options);
    }
  }

  resultBuilder.openArray();

  auto iterator = indexScan(
      collectionName, transaction::Methods::CursorType::ANY, ReadOwnWrites::no);

  iterator->nextDocument(
      [&resultBuilder](LocalDocumentId const& /*token*/, VPackSlice slice) {
        resultBuilder.add(slice);
        return true;
      },
      1);

  resultBuilder.close();

  return OperationResult(Result(), resultBuilder.steal(), options);
}

DataSourceId transaction::Methods::addCollectionAtRuntime(
    DataSourceId cid, std::string const& collectionName,
    AccessMode::Type type) {
  auto collection = trxCollection(cid);

  if (collection == nullptr) {
    Result res =
        _state->addCollection(cid, collectionName, type, /*lockUsage*/ true);

    if (res.fail()) {
      THROW_ARANGO_EXCEPTION(res);
    }

    auto dataSource = resolver()->getDataSource(cid);

    if (!dataSource) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
    }

    res = applyDataSourceRegistrationCallbacks(*dataSource, *this);
    if (res.fail()) {
      THROW_ARANGO_EXCEPTION(res);
    }

    collection = trxCollection(cid);
    if (collection == nullptr) {
      throwCollectionNotFound(collectionName);
    }

  } else {
    AccessMode::Type collectionAccessType = collection->accessType();
    if (AccessMode::isRead(collectionAccessType) && !AccessMode::isRead(type)) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_TRANSACTION_UNREGISTERED_COLLECTION,
          std::string(
              TRI_errno_string(TRI_ERROR_TRANSACTION_UNREGISTERED_COLLECTION)) +
              ": " + collectionName + " [" + AccessMode::typeString(type) +
              "]");
    }
  }

  TRI_ASSERT(collection != nullptr);
  return cid;
}

/// @brief add a collection to the transaction for read, at runtime
DataSourceId transaction::Methods::addCollectionAtRuntime(
    std::string const& collectionName, AccessMode::Type type) {
  if (collectionName == _collectionCache.name && !collectionName.empty()) {
    return _collectionCache.cid;
  }

  TRI_ASSERT(!_state->isCoordinator());
  auto cid = resolver()->getCollectionIdLocal(collectionName);

  if (cid.empty()) {
    throwCollectionNotFound(collectionName);
  }
  addCollectionAtRuntime(cid, collectionName, type);
  _collectionCache.cid = cid;
  _collectionCache.name = collectionName;
  return cid;
}

/// @brief return the type of a collection
bool transaction::Methods::isEdgeCollection(
    std::string const& collectionName) const {
  return getCollectionType(collectionName) == TRI_COL_TYPE_EDGE;
}

/// @brief return the type of a collection
TRI_col_type_e transaction::Methods::getCollectionType(
    std::string const& collectionName) const {
  auto collection = resolver()->getCollection(collectionName);

  return collection ? collection->type() : TRI_COL_TYPE_UNKNOWN;
}

/// @brief return one document from a collection, fast path
///        If everything went well the result will contain the found document
///        (as an external on single_server) and this function will return
///        TRI_ERROR_NO_ERROR.
///        If there was an error the code is returned and it is guaranteed
///        that result remains unmodified.
///        Does not care for revision handling!
Result transaction::Methods::documentFastPath(std::string const& collectionName,
                                              VPackSlice value,
                                              OperationOptions const& options,
                                              VPackBuilder& result) {
  TRI_ASSERT(_state->status() == transaction::Status::RUNNING);
  if (!value.isObject() && !value.isString()) {
    // must provide a document object or string
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
  }

  if (_state->isCoordinator()) {
    OperationResult opRes = documentCoordinator(collectionName, value, options,
                                                MethodsApi::Synchronous)
                                .get();
    if (!opRes.fail()) {
      result.add(opRes.slice());
    }
    return opRes.result;
  }

  auto translateName = [this](std::string const& collectionName) {
    if (_state->isDBServer()) {
      auto collection = resolver()->getCollectionStructCluster(collectionName);
      if (collection != nullptr) {
        auto& ci =
            vocbase().server().getFeature<ClusterFeature>().clusterInfo();
        auto shards = ci.getShardList(std::to_string(collection->id().id()));
        if (shards != nullptr && shards->size() == 1) {
          TRI_ASSERT(vocbase().isOneShard());
          return (*shards)[0];
        }
      }
    }
    return collectionName;
  };

  std::string_view key(transaction::helpers::extractKeyPart(value));
  if (key.empty()) {
    return {TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD};
  }

  DataSourceId cid = addCollectionAtRuntime(translateName(collectionName),
                                            AccessMode::Type::READ);

  TransactionCollection* trxColl = trxCollection(cid);
  if (trxColl == nullptr) {
    return {TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND};
  }
  auto const& collection = trxColl->collection();
  if (collection == nullptr) {
    return {TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND};
  }

  return collection->getPhysical()->read(
      this, key,
      [&](LocalDocumentId const&, VPackSlice doc) {
        result.add(doc);
        return true;
      },
      ReadOwnWrites::no);
}

/// @brief return one document from a collection, fast path
///        If everything went well the result will contain the found document
///        (as an external on single_server) and this function will return
///        TRI_ERROR_NO_ERROR.
///        If there was an error the code is returned
///        Does not care for revision handling!
///        Must only be called on a local server, not in cluster case!
Result transaction::Methods::documentFastPathLocal(
    std::string const& collectionName, std::string_view key,
    IndexIterator::DocumentCallback const& cb) {
  TRI_ASSERT(!ServerState::instance()->isCoordinator());
  TRI_ASSERT(_state->status() == transaction::Status::RUNNING);

  DataSourceId cid =
      addCollectionAtRuntime(collectionName, AccessMode::Type::READ);
  TransactionCollection* trxColl = trxCollection(cid);
  if (trxColl == nullptr) {
    return {TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND};
  }
  std::shared_ptr<LogicalCollection> const& collection = trxColl->collection();
  TRI_ASSERT(collection != nullptr);
  if (collection == nullptr) {
    return {TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND};
  }

  if (key.empty()) {
    return {TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD};
  }

  // We never want to see our own writes here, otherwise we could observe
  // documents which have been inserted by a currently running query.
  return collection->getPhysical()->read(this, key, cb, ReadOwnWrites::no);
}

namespace {
template<typename F>
Future<OperationResult> addTracking(Future<OperationResult>&& f, F&& func) {
#ifdef USE_ENTERPRISE
  return std::move(f).thenValue(func);
#else
  return std::move(f);
#endif
}
}  // namespace

OperationResult Methods::document(std::string const& collectionName,
                                  VPackSlice value,
                                  OperationOptions const& options) {
  return documentInternal(collectionName, value, options,
                          MethodsApi::Synchronous)
      .get();
}

/// @brief return one or multiple documents from a collection
Future<OperationResult> transaction::Methods::documentAsync(
    std::string const& collectionName, VPackSlice value,
    OperationOptions const& options) {
  return documentInternal(collectionName, value, options,
                          MethodsApi::Asynchronous);
}

/// @brief read one or multiple documents in a collection, coordinator
#ifndef USE_ENTERPRISE
Future<OperationResult> transaction::Methods::documentCoordinator(
    std::string const& collectionName, VPackSlice value,
    OperationOptions const& options, MethodsApi api) {
  if (!value.isArray()) {
    std::string_view key(transaction::helpers::extractKeyPart(value));
    if (key.empty()) {
      return OperationResult(TRI_ERROR_ARANGO_DOCUMENT_KEY_BAD, options);
    }
  }

  auto colptr = resolver()->getCollectionStructCluster(collectionName);
  if (colptr == nullptr) {
    return futures::makeFuture(
        OperationResult(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, options));
  }

  return arangodb::getDocumentOnCoordinator(*this, *colptr, value, options,
                                            api);
}
#endif

/// @brief read one or multiple documents in a collection, local
Future<OperationResult> transaction::Methods::documentLocal(
    std::string const& collectionName, VPackSlice value,
    OperationOptions const& options) {
  DataSourceId cid =
      addCollectionAtRuntime(collectionName, AccessMode::Type::READ);
  TransactionCollection* trxColl = trxCollection(cid);
  if (trxColl == nullptr) {
    return futures::makeFuture(
        OperationResult(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, options));
  }
  std::shared_ptr<LogicalCollection> const& collection = trxColl->collection();
  if (collection == nullptr) {
    return futures::makeFuture(
        OperationResult(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, options));
  }

  if (_state->isDBServer()) {
    auto const& followerInfo = collection->followers();
    if (!followerInfo->getLeader().empty()) {
      // We believe to be a follower!
      if (!options.allowDirtyReads) {
        return futures::makeFuture(
            OperationResult(TRI_ERROR_CLUSTER_SHARD_LEADER_RESIGNED, options));
      }
    }
  }

  VPackBuilder resultBuilder;
  Result res;

  auto workForOneDocument = [&](VPackSlice value, bool isMultiple) -> Result {
    Result res;

    std::string_view key(transaction::helpers::extractKeyPart(value));
    if (key.empty()) {
      res.reset(TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD);
    } else {
      bool conflict = false;
      res = collection->getPhysical()->read(
          this, key,
          [&](LocalDocumentId const&, VPackSlice doc) {
            if (!options.ignoreRevs && value.isObject()) {
              RevisionId expectedRevision = RevisionId::fromSlice(value);
              if (expectedRevision.isSet()) {
                RevisionId foundRevision =
                    transaction::helpers::extractRevFromDocument(doc);
                if (expectedRevision != foundRevision) {
                  if (!isMultiple) {
                    // still return
                    buildDocumentIdentity(*collection, resultBuilder, cid, key,
                                          foundRevision, RevisionId::none(),
                                          nullptr, nullptr);
                  }
                  conflict = true;
                  return false;
                }
              }
            }

            if (!options.silent) {
              resultBuilder.add(doc);
            } else if (isMultiple) {
              resultBuilder.add(VPackSlice::nullSlice());
            }
            return true;
          },
          ReadOwnWrites::no);

      if (conflict) {
        res.reset(TRI_ERROR_ARANGO_CONFLICT);
      }
    }
    return res;
  };

  std::unordered_map<ErrorCode, size_t> countErrorCodes;
  if (!value.isArray()) {
    res = workForOneDocument(value, false);
  } else {
    VPackArrayBuilder guard(&resultBuilder);
    for (VPackSlice s : VPackArrayIterator(value)) {
      res = workForOneDocument(s, true);
      if (res.fail()) {
        createBabiesError(&resultBuilder, countErrorCodes, res);
      }
    }
    res.reset();  // With babies the reporting is handled somewhere else.
  }

  events::ReadDocument(vocbase().name(), collectionName, value, options,
                       res.errorNumber());

  return futures::makeFuture(OperationResult(
      std::move(res), resultBuilder.steal(), options, countErrorCodes));
}

OperationResult Methods::insert(std::string const& collectionName,
                                VPackSlice value,
                                OperationOptions const& options) {
  return insertInternal(collectionName, value, options, MethodsApi::Synchronous)
      .get();
}

/// @brief create one or multiple documents in a collection
/// the single-document variant of this operation will either succeed or,
/// if it fails, clean up after itself
Future<OperationResult> transaction::Methods::insertAsync(
    std::string const& collectionName, VPackSlice value,
    OperationOptions const& options) {
  return insertInternal(collectionName, value, options,
                        MethodsApi::Asynchronous);
}

/// @brief create one or multiple documents in a collection, coordinator
/// the single-document variant of this operation will either succeed or,
/// if it fails, clean up after itself
#ifndef USE_ENTERPRISE
Future<OperationResult> transaction::Methods::insertCoordinator(
    std::string const& collectionName, VPackSlice value,
    OperationOptions const& options, MethodsApi api) {
  auto colptr = resolver()->getCollectionStructCluster(collectionName);
  if (colptr == nullptr) {
    return futures::makeFuture(
        OperationResult(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, options));
  }
  return arangodb::createDocumentOnCoordinator(*this, *colptr, value, options,
                                               api);
}
#endif

void transaction::Methods::trackWaitForSync(LogicalCollection& collection,
                                            OperationOptions& options) {
  if (collection.waitForSync() && !options.isRestore) {
    options.waitForSync = true;
  }
  if (options.waitForSync) {
    state()->waitForSync(true);
  }
}

/// @brief Determine the replication type and the followers for a transaction.
/// The replication type indicates whether this server is the leader or a
/// follower. The followers are the servers that will be contacted for the
/// actual replication.
///
/// We had to split this function into two parts, because the first one is used
/// by replication1 and the second one is used by replication2.
Result transaction::Methods::determineReplicationTypeAndFollowers(
    LogicalCollection& collection, std::string_view operationName,
    velocypack::Slice value, OperationOptions& options,
    ReplicationType& replicationType,
    std::shared_ptr<std::vector<ServerID> const>& followers) {
  auto replicationVersion = collection.replicationVersion();
  if (replicationVersion == replication::Version::ONE) {
    return determineReplication1TypeAndFollowers(
        collection, operationName, value, options, replicationType, followers);
  }
  TRI_ASSERT(replicationVersion == replication::Version::TWO);
  return determineReplication2TypeAndFollowers(
      collection, operationName, value, options, replicationType, followers);
}

/// @brief The original code for determineReplicationTypeAndFollowers, used for
/// replication1.
Result transaction::Methods::determineReplication1TypeAndFollowers(
    LogicalCollection& collection, std::string_view operationName,
    velocypack::Slice value, OperationOptions& options,
    ReplicationType& replicationType,
    std::shared_ptr<std::vector<ServerID> const>& followers) {
  replicationType = ReplicationType::NONE;
  TRI_ASSERT(followers == nullptr);

  if (_state->isDBServer()) {
    // This failure point is to test the case that a former leader has
    // resigned in the meantime but still gets an insert request from
    // a coordinator who does not know this yet. That is, the test sets
    // the failure point on all servers, including the current leader.
    TRI_IF_FAILURE("documents::insertLeaderRefusal") {
      if (operationName == "insert" && value.isObject() &&
          value.hasKey("ThisIsTheRetryOnLeaderRefusalTest")) {
        return {TRI_ERROR_CLUSTER_SHARD_LEADER_RESIGNED};
      }
    }

    // Block operation early if we are not supposed to perform it:
    auto const& followerInfo = collection.followers();
    std::string theLeader = followerInfo->getLeader();
    if (theLeader.empty()) {
      // This indicates that we believe to be the leader.
      if (!options.isSynchronousReplicationFrom.empty()) {
        return {TRI_ERROR_CLUSTER_SHARD_LEADER_REFUSES_REPLICATION};
      }

      switch (followerInfo->allowedToWrite()) {
        case FollowerInfo::WriteState::FORBIDDEN:
          // We cannot fulfill minimum replication Factor. Reject write.
          return {TRI_ERROR_ARANGO_READ_ONLY};
        case FollowerInfo::WriteState::UNAVAILABLE:
        case FollowerInfo::WriteState::STARTUP:
          return {TRI_ERROR_CLUSTER_BACKEND_UNAVAILABLE};
        default:
          break;
      }

      replicationType = ReplicationType::LEADER;
      followers = followerInfo->get();
      // We cannot be silent if we may have to replicate later.
      // If we need to get the followers under the single document operation's
      // lock, we don't know yet if we will have followers later and thus cannot
      // be silent.
      // Otherwise, if we already know the followers to replicate to, we can
      // just check if they're empty.
      if (!followers->empty()) {
        options.silent = false;
      }
    } else {  // we are a follower following theLeader
      replicationType = ReplicationType::FOLLOWER;
      if (options.isSynchronousReplicationFrom.empty()) {
        return {TRI_ERROR_CLUSTER_SHARD_LEADER_RESIGNED};
      }
      bool sendRefusal = (options.isSynchronousReplicationFrom != theLeader);
      TRI_IF_FAILURE("synchronousReplication::neverRefuseOnFollower") {
        sendRefusal = false;
      }
      TRI_IF_FAILURE("synchronousReplication::refuseOnFollower") {
        sendRefusal = true;
      }
      TRI_IF_FAILURE("synchronousReplication::expectFollowingTerm") {
        // expect a following term id or send a refusal
        if (!options.isRestore) {
          sendRefusal |= (options.isSynchronousReplicationFrom.find('_') ==
                          std::string::npos);
        }
      }
      if (sendRefusal) {
        return ::buildRefusalResult(collection, operationName, options,
                                    theLeader);
      }

      // we are a valid follower. we do not need to send a proper result with
      // _key, _id, _rev back to the leader, because it will ignore all these
      // data anyway. it is sufficient to send headers and the proper error
      // codes back.
      options.silent = true;
    }
  }

  TRI_ASSERT((replicationType == ReplicationType::LEADER) ==
             (followers != nullptr));
  TRI_ASSERT(!options.silent || replicationType != ReplicationType::LEADER ||
             followers->empty());
  // on followers, the silent flag must always be set
  TRI_ASSERT(replicationType != ReplicationType::FOLLOWER || options.silent);

  return {};
}

/// @brief The replication2 version for determineReplicationTypeAndFollowers.
/// The replication type is determined from the replicated state status (could
/// be follower status or leader status). Followers is always an empty vector,
/// because replication2 framework handles followers itself.
Result transaction::Methods::determineReplication2TypeAndFollowers(
    LogicalCollection& collection, std::string_view operationName,
    velocypack::Slice value, OperationOptions& options,
    ReplicationType& replicationType,
    std::shared_ptr<std::vector<ServerID> const>& followers) {
  if (!_state->isDBServer()) {
    replicationType = ReplicationType::NONE;
    return {};
  }

  auto state = collection.getDocumentState();
  TRI_ASSERT(state != nullptr);
  if (state == nullptr) {
    return {TRI_ERROR_REPLICATION_REPLICATED_STATE_NOT_FOUND,
            "Could not get replicated state"};
  }

  auto status = state->getStatus();
  if (status == std::nullopt) {
    return {TRI_ERROR_REPLICATION_REPLICATED_STATE_NOT_AVAILABLE,
            "Could not get replicated state status"};
  }

  using namespace replication2::replicated_state;

  if (auto leaderStatus = status->asLeaderStatus(); leaderStatus != nullptr) {
    if (leaderStatus->managerState.state ==
        LeaderInternalState::kRecoveryInProgress) {
      // Even though we are the leader, we don't want to replicate during
      // recovery.
      options.silent = true;
      replicationType = ReplicationType::FOLLOWER;
      followers = nullptr;
    } else if (leaderStatus->managerState.state ==
               LeaderInternalState::kServiceAvailable) {
      options.silent = false;
      replicationType = ReplicationType::LEADER;
      followers = std::make_shared<std::vector<ServerID>>();
    } else {
      return {TRI_ERROR_REPLICATION_LEADER_ERROR,
              "Unexpected manager state " +
                  std::string(to_string(leaderStatus->managerState.state))};
    }
  } else if (auto followerStatus = status->asFollowerStatus();
             followerStatus != nullptr) {
    options.silent = true;
    replicationType = ReplicationType::FOLLOWER;
    followers = std::make_shared<std::vector<ServerID> const>();
  } else {
    std::stringstream s;
    s << "Status is " << *status;
    return {TRI_ERROR_REPLICATION_REPLICATED_STATE_NOT_AVAILABLE, s.str()};
  }
  return {};
}

/// @brief create one or multiple documents in a collection, local.
/// the single-document variant of this operation will either succeed or,
/// if it fails, clean up after itself
Future<OperationResult> transaction::Methods::insertLocal(
    std::string const& collectionName, VPackSlice value,
    OperationOptions& options) {
  DataSourceId cid =
      addCollectionAtRuntime(collectionName, AccessMode::Type::WRITE);
  TransactionCollection* trxColl = trxCollection(cid);
  if (trxColl == nullptr) {
    return futures::makeFuture(
        OperationResult(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, options));
  }
  std::shared_ptr<LogicalCollection> const& collection = trxColl->collection();
  if (collection == nullptr) {
    return futures::makeFuture(
        OperationResult(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, options));
  }

  ReplicationType replicationType = ReplicationType::NONE;
  std::shared_ptr<std::vector<ServerID> const> followers;
  // this call will populate replicationType and followers
  Result res = determineReplicationTypeAndFollowers(
      *collection, "insert", value, options, replicationType, followers);

  if (res.fail()) {
    return futures::makeFuture(OperationResult(std::move(res), options));
  }

  // set up batch options
  BatchOptions batchOptions = ::buildBatchOptions(
      options, *collection, TRI_VOC_DOCUMENT_OPERATION_INSERT,
      _state->isDBServer());

  bool excludeAllFromReplication =
      replicationType != ReplicationType::LEADER ||
      (followers->empty() &&
       collection->replicationVersion() != replication::Version::TWO);

  // builder for a single document (will be recycled for each document)
  transaction::BuilderLeaser newDocumentBuilder(this);
  // all document data that are going to be replicated, append-only
  transaction::BuilderLeaser replicationData(this);
  // total result that is going to be returned to the caller, append-only
  VPackBuilder resultBuilder;

  auto workForOneDocument = [&](VPackSlice value, bool isArray) -> Result {
    newDocumentBuilder->clear();

    if (!value.isObject()) {
      return {TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID};
    }

    LocalDocumentId oldDocumentId;
    RevisionId oldRevisionId = RevisionId::none();
    VPackSlice key;

    Result res;

    if (options.isOverwriteModeSet() &&
        options.overwriteMode != OperationOptions::OverwriteMode::Conflict) {
      key = value.get(StaticStrings::KeyString);
      if (key.isString()) {
        std::pair<LocalDocumentId, RevisionId> lookupResult;
        // modifications always need to observe all changes in order to validate
        // uniqueness constraints
        res = collection->getPhysical()->lookupKey(
            this, key.stringView(), lookupResult, ReadOwnWrites::yes);
        if (res.ok()) {
          TRI_ASSERT(lookupResult.first.isSet());
          TRI_ASSERT(lookupResult.second.isSet());
          oldDocumentId = lookupResult.first;
          oldRevisionId = lookupResult.second;
        }
      }
    }

    bool const isPrimaryKeyConstraintViolation = oldDocumentId.isSet();
    TRI_ASSERT(!isPrimaryKeyConstraintViolation || !key.isNone());

    // only populated for update/replace
    transaction::BuilderLeaser previousDocumentBuilder(this);

    RevisionId newRevisionId;
    bool didReplace = false;
    bool excludeFromReplication = excludeAllFromReplication;

    if (!isPrimaryKeyConstraintViolation) {
      // regular insert without overwrite option. the insert itself will check
      // if the primary key already exists
      res = insertLocalHelper(*collection, value, newRevisionId,
                              *newDocumentBuilder, options, batchOptions);

      TRI_ASSERT(res.fail() || newDocumentBuilder->slice().isObject());
    } else {
      // RepSert Case - unique_constraint violated ->  try update, replace or
      // ignore!
      TRI_ASSERT(options.isOverwriteModeSet());
      TRI_ASSERT(options.overwriteMode !=
                 OperationOptions::OverwriteMode::Conflict);
      TRI_ASSERT(res.ok());
      TRI_ASSERT(oldDocumentId.isSet());

      if (options.overwriteMode == OperationOptions::OverwriteMode::Ignore) {
        // in case of unique constraint violation: ignore and do nothing (no
        // write!)
        if (replicationType != ReplicationType::FOLLOWER) {
          // intentionally do not fill replicationData here
          TRI_ASSERT(key.isString());
          buildDocumentIdentity(*collection, resultBuilder, cid,
                                key.stringView(), oldRevisionId,
                                RevisionId::none(), nullptr, nullptr);
        }
        return res;
      }

      if (options.overwriteMode == OperationOptions::OverwriteMode::Update ||
          options.overwriteMode == OperationOptions::OverwriteMode::Replace) {
        // in case of unique constraint violation: (partially) update existing
        // document.
        previousDocumentBuilder->clear();
        res = collection->getPhysical()->lookupDocument(
            *this, oldDocumentId, *previousDocumentBuilder,
            /*readCache*/ true, /*fillCache*/ false, ReadOwnWrites::yes);

        if (res.ok()) {
          TRI_ASSERT(previousDocumentBuilder->slice().isObject());

          res = modifyLocalHelper(
              *collection, value, oldDocumentId, oldRevisionId,
              previousDocumentBuilder->slice(), newRevisionId,
              *newDocumentBuilder, options, batchOptions,
              /*isUpdate*/ options.overwriteMode ==
                  OperationOptions::OverwriteMode::Update);
        }

        TRI_ASSERT(res.fail() || newDocumentBuilder->slice().isObject());

        if (res.ok() && oldRevisionId == newRevisionId &&
            options.overwriteMode == OperationOptions::OverwriteMode::Update) {
          // did not actually update - intentionally do not fill replicationData
          excludeFromReplication |= true;
        }
      } else {
        TRI_ASSERT(false);
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                       "internal overwriteMode state");
      }

      TRI_ASSERT(res.fail() || oldRevisionId.isSet());
      didReplace = true;
    }

    if (res.fail()) {
      // Error reporting in the babies case is done outside of here.
      if (res.is(TRI_ERROR_ARANGO_CONFLICT) && !isArray &&
          oldRevisionId.isSet()) {
        TRI_ASSERT(didReplace);

        if (replicationType != ReplicationType::FOLLOWER) {
          TRI_ASSERT(key.isString());
          buildDocumentIdentity(*collection, resultBuilder, cid,
                                key.stringView(), oldRevisionId,
                                RevisionId::none(), nullptr, nullptr);
        }
      }
      // intentionally do not fill replicationData here
      return res;
    }

    TRI_ASSERT(res.ok());

    if (!options.silent) {
      TRI_ASSERT(newDocumentBuilder->slice().isObject());

      bool const showReplaced = (options.returnOld && didReplace);
      TRI_ASSERT(!options.returnNew || !newDocumentBuilder->isEmpty());
      TRI_ASSERT(!showReplaced || oldRevisionId.isSet());
      TRI_ASSERT(!showReplaced || previousDocumentBuilder->slice().isObject());

      key = newDocumentBuilder->slice().get(StaticStrings::KeyString);

      buildDocumentIdentity(
          *collection, resultBuilder, cid, key.stringView(), newRevisionId,
          oldRevisionId, showReplaced ? previousDocumentBuilder.get() : nullptr,
          options.returnNew ? newDocumentBuilder.get() : nullptr);
    }

    if (!excludeFromReplication) {
      TRI_ASSERT(newDocumentBuilder->slice().isObject());
      // _id values are written to the database as VelocyPack Custom values.
      // However, these cannot be transferred as Custom types, because the
      // VelocyPack validator on the receiver side will complain about them.
      // so we need to rewrite the document here to not include any Custom
      // types.
      // replicationData->add(newDocumentBuilder->slice());
      basics::VelocyPackHelper::sanitizeNonClientTypes(
          newDocumentBuilder->slice(), VPackSlice::noneSlice(),
          *replicationData, transactionContextPtr()->getVPackOptions(), true,
          true, false);
    }

    return res;
  };

  std::unordered_map<ErrorCode, size_t> errorCounter;

  replicationData->openArray(true);
  if (value.isArray()) {
    resultBuilder.openArray();

    for (VPackSlice s : VPackArrayIterator(value)) {
      TRI_IF_FAILURE("insertLocal::fakeResult1") {  //
        // Set an error *instead* of calling `workForOneDocument`
        res.reset(TRI_ERROR_DEBUG);
      }
      else {
        res = workForOneDocument(s, true);
      }
      if (res.fail()) {
        createBabiesError(replicationType == ReplicationType::FOLLOWER
                              ? nullptr
                              : &resultBuilder,
                          errorCounter, res);
        res.reset();
      }
    }

    resultBuilder.close();
  } else {
    res = workForOneDocument(value, false);

    // on a follower, our result should always be an empty object
    if (replicationType == ReplicationType::FOLLOWER) {
      TRI_ASSERT(resultBuilder.slice().isNone());
      // add an empty object here so that when sending things back in JSON
      // format, there is no "non-representable type 'none'" issue.
      resultBuilder.add(VPackSlice::emptyObjectSlice());
    }
  }
  replicationData->close();

  // on a follower, our result should always be an empty array or object
  TRI_ASSERT(replicationType != ReplicationType::FOLLOWER ||
             (value.isArray() && resultBuilder.slice().isEmptyArray()) ||
             (value.isObject() && resultBuilder.slice().isEmptyObject()));
  TRI_ASSERT(replicationData->slice().isArray());
  TRI_ASSERT(replicationType != ReplicationType::FOLLOWER ||
             replicationData->slice().isEmptyArray());

  TRI_ASSERT(res.ok() || !value.isArray());

  TRI_IF_FAILURE("insertLocal::fakeResult2") { res.reset(TRI_ERROR_DEBUG); }

  TRI_ASSERT(!value.isArray() || options.silent ||
             resultBuilder.slice().length() == value.length());

  std::shared_ptr<VPackBufferUInt8> resDocs = resultBuilder.steal();
  if (res.ok()) {
#ifdef ARANGODB_USE_GOOGLE_TESTS
    StorageEngine& engine = collection->vocbase()
                                .server()
                                .getFeature<EngineSelectorFeature>()
                                .engine();

    bool isMock = (engine.typeName() == "Mock");
#else
    constexpr bool isMock = false;
#endif

    if (!isMock && replicationType == ReplicationType::LEADER &&
        (!followers->empty() ||
         collection->replicationVersion() == replication::Version::TWO) &&
        !replicationData->slice().isEmptyArray()) {
      TRI_ASSERT(collection != nullptr);

      // In the multi babies case res is always TRI_ERROR_NO_ERROR if we
      // get here, in the single document case, we do not try to replicate
      // in case of an error.

      // Now replicate the good operations on all followers:
      return replicateOperations(*trxColl, followers, options, *replicationData,
                                 TRI_VOC_DOCUMENT_OPERATION_INSERT)
          .thenValue([options, errs = std::move(errorCounter),
                      resultData = std::move(resDocs)](Result res) mutable {
            if (!res.ok()) {
              return OperationResult{std::move(res), options};
            }
            if (options.silent && errs.empty()) {
              // We needed the results, but do not want to report:
              resultData->clear();
            }
            return OperationResult(std::move(res), std::move(resultData),
                                   options, std::move(errs));
          });
    }

    // execute a deferred intermediate commit, if required.
    res = performIntermediateCommitIfRequired(collection->id());
  }

  if (options.silent && errorCounter.empty()) {
    // We needed the results, but do not want to report:
    resDocs->clear();
  }
  return futures::makeFuture(OperationResult(std::move(res), std::move(resDocs),
                                             options, std::move(errorCounter)));
}

Result transaction::Methods::insertLocalHelper(
    LogicalCollection& collection, velocypack::Slice value,
    RevisionId& newRevisionId, velocypack::Builder& newDocumentBuilder,
    OperationOptions& options, BatchOptions& batchOptions) {
  TRI_IF_FAILURE("LogicalCollection::insert") { return {TRI_ERROR_DEBUG}; }

  Result res = newObjectForInsert(*this, collection, value, newRevisionId,
                                  newDocumentBuilder, options, batchOptions);

  if (res.ok()) {
    TRI_ASSERT(newRevisionId.isSet());
    TRI_ASSERT(newDocumentBuilder.slice().isObject());

    if (batchOptions.validateSmartJoinAttribute) {
      auto r = transaction::Methods::validateSmartJoinAttribute(
          collection, newDocumentBuilder.slice());

      if (r != TRI_ERROR_NO_ERROR) {
        return res.reset(r);
      }
    }

#ifdef ARANGODB_USE_GOOGLE_TESTS
    StorageEngine& engine = collection.vocbase()
                                .server()
                                .getFeature<EngineSelectorFeature>()
                                .engine();

    bool isMock = (engine.typeName() == "Mock");
#else
    constexpr bool isMock = false;
#endif
    // note: schema can be a nullptr here, but we need to call validate()
    // anyway. the reason is that validate() does not only perform schema
    // validation, but also some validation for SmartGraph data
    if (!isMock) {
      res = collection.validate(batchOptions.schema, newDocumentBuilder.slice(),
                                transactionContextPtr()->getVPackOptions());
    }
  }

  if (res.ok()) {
    res = collection.getPhysical()->insert(*this, newRevisionId,
                                           newDocumentBuilder.slice(), options);

    if (res.ok()) {
      trackWaitForSync(collection, options);
    }
  }

  // return final result
  return res;
}

OperationResult Methods::update(std::string const& collectionName,
                                VPackSlice updateValue,
                                OperationOptions const& options) {
  return updateInternal(collectionName, updateValue, options,
                        MethodsApi::Synchronous)
      .get();
}

/// @brief update/patch one or multiple documents in a collection
/// the single-document variant of this operation will either succeed or,
/// if it fails, clean up after itself
Future<OperationResult> transaction::Methods::updateAsync(
    std::string const& collectionName, VPackSlice newValue,
    OperationOptions const& options) {
  return updateInternal(collectionName, newValue, options,
                        MethodsApi::Asynchronous);
}

/// @brief update one or multiple documents in a collection, coordinator
/// the single-document variant of this operation will either succeed or,
/// if it fails, clean up after itself
#ifndef USE_ENTERPRISE
Future<OperationResult> transaction::Methods::modifyCoordinator(
    std::string const& collectionName, VPackSlice newValue,
    OperationOptions const& options, TRI_voc_document_operation_e operation,
    MethodsApi api) {
  if (!newValue.isArray()) {
    std::string_view key(transaction::helpers::extractKeyPart(newValue));
    if (key.empty()) {
      return OperationResult(TRI_ERROR_ARANGO_DOCUMENT_KEY_BAD, options);
    }
  }

  auto colptr = resolver()->getCollectionStructCluster(collectionName);
  if (colptr == nullptr) {
    return futures::makeFuture(
        OperationResult(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, options));
  }

  const bool isPatch = (TRI_VOC_DOCUMENT_OPERATION_UPDATE == operation);
  return arangodb::modifyDocumentOnCoordinator(*this, *colptr, newValue,
                                               options, isPatch, api);
}
#endif

OperationResult Methods::replace(std::string const& collectionName,
                                 VPackSlice replaceValue,
                                 OperationOptions const& options) {
  return replaceInternal(collectionName, replaceValue, options,
                         MethodsApi::Synchronous)
      .get();
}

/// @brief replace one or multiple documents in a collection
/// the single-document variant of this operation will either succeed or,
/// if it fails, clean up after itself
Future<OperationResult> transaction::Methods::replaceAsync(
    std::string const& collectionName, VPackSlice newValue,
    OperationOptions const& options) {
  return replaceInternal(collectionName, newValue, options,
                         MethodsApi::Asynchronous);
}

/// @brief replace one or multiple documents in a collection, local
/// the single-document variant of this operation will either succeed or,
/// if it fails, clean up after itself
Future<OperationResult> transaction::Methods::modifyLocal(
    std::string const& collectionName, VPackSlice newValue,
    OperationOptions& options, bool isUpdate) {
  DataSourceId cid =
      addCollectionAtRuntime(collectionName, AccessMode::Type::WRITE);
  TransactionCollection* trxColl = trxCollection(cid);
  if (trxColl == nullptr) {
    return futures::makeFuture(
        OperationResult(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, options));
  }
  std::shared_ptr<LogicalCollection> const& collection = trxColl->collection();
  if (collection == nullptr) {
    return futures::makeFuture(
        OperationResult(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, options));
  }
  TRI_ASSERT(trxColl->isLocked(AccessMode::Type::WRITE));

  // this call will populate replicationType and followers
  ReplicationType replicationType = ReplicationType::NONE;
  std::shared_ptr<std::vector<ServerID> const> followers;

  Result res = determineReplicationTypeAndFollowers(
      *collection, isUpdate ? "update" : "replace", newValue, options,
      replicationType, followers);

  if (res.fail()) {
    return futures::makeFuture(OperationResult(std::move(res), options));
  }

  // set up batch options
  BatchOptions batchOptions =
      ::buildBatchOptions(options, *collection,
                          isUpdate ? TRI_VOC_DOCUMENT_OPERATION_UPDATE
                                   : TRI_VOC_DOCUMENT_OPERATION_REPLACE,
                          _state->isDBServer());

  bool excludeAllFromReplication =
      replicationType != ReplicationType::LEADER ||
      (followers->empty() &&
       collection->replicationVersion() != replication::Version::TWO);

  // builder for a single document (will be recycled for each document)
  transaction::BuilderLeaser newDocumentBuilder(this);
  // builder for a single, old version of document (will be recycled for each
  // document)
  transaction::BuilderLeaser previousDocumentBuilder(this);
  // all document data that are going to be replicated, append-only
  transaction::BuilderLeaser replicationData(this);
  // total result that is going to be returned to the caller, append-only
  VPackBuilder resultBuilder;

  auto workForOneDocument = [&](VPackSlice newValue, bool isArray) -> Result {
    newDocumentBuilder->clear();
    previousDocumentBuilder->clear();

    if (!newValue.isObject()) {
      return {TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID};
    }

    VPackSlice key = newValue.get(StaticStrings::KeyString);
    if (key.isNone()) {
      return {TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD};
    } else if (!key.isString() || key.stringView().empty()) {
      return {TRI_ERROR_ARANGO_DOCUMENT_KEY_BAD};
    }

    // replace and update are two operations each, thus this can and must not be
    // single document operations. We need to have a lock here already.
    TRI_ASSERT(isLocked(collection.get(), AccessMode::Type::WRITE));

    std::pair<LocalDocumentId, RevisionId> lookupResult;
    Result res = collection->getPhysical()->lookupKey(
        this, key.stringView(), lookupResult, ReadOwnWrites::yes);
    if (res.fail()) {
      return {TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND};
    }

    TRI_ASSERT(lookupResult.first.isSet());
    TRI_ASSERT(lookupResult.second.isSet());
    auto [oldDocumentId, oldRevisionId] = lookupResult;

    previousDocumentBuilder->clear();
    res = collection->getPhysical()->lookupDocument(
        *this, oldDocumentId, *previousDocumentBuilder,
        /*readCache*/ true, /*fillCache*/ false, ReadOwnWrites::yes);

    if (res.fail()) {
      return res;
    }

    bool excludeFromReplication = excludeAllFromReplication;
    TRI_ASSERT(previousDocumentBuilder->slice().isObject());
    RevisionId newRevisionId;
    res =
        modifyLocalHelper(*collection, newValue, oldDocumentId, oldRevisionId,
                          previousDocumentBuilder->slice(), newRevisionId,
                          *newDocumentBuilder, options, batchOptions, isUpdate);

    if (res.fail()) {
      if (res.is(TRI_ERROR_ARANGO_CONFLICT) && !isArray) {
        TRI_ASSERT(oldRevisionId.isSet());
        buildDocumentIdentity(
            *collection, resultBuilder, cid, key.stringView(), oldRevisionId,
            RevisionId::none(),
            options.returnOld ? previousDocumentBuilder.get() : nullptr,
            nullptr);
      }
      return res;
    }

    TRI_ASSERT(res.ok());
    TRI_ASSERT(newRevisionId.isSet());
    TRI_ASSERT(newDocumentBuilder->slice().isObject());

    if (!options.silent) {
      TRI_ASSERT(newRevisionId.isSet() && oldRevisionId.isSet());

      buildDocumentIdentity(
          *collection, resultBuilder, cid, key.stringView(), newRevisionId,
          oldRevisionId,
          options.returnOld ? previousDocumentBuilder.get() : nullptr,
          options.returnNew ? newDocumentBuilder.get() : nullptr);
      if (newRevisionId == oldRevisionId && isUpdate) {
        excludeFromReplication |= true;
      }
    }

    if (!excludeFromReplication) {
      // _id values are written to the database as VelocyPack Custom values.
      // However, these cannot be transferred as Custom types, because the
      // VelocyPack validator on the receiver side will complain about them.
      // so we need to rewrite the document here to not include any Custom
      // types.
      // replicationData->add(newDocumentBuilder->slice());
      basics::VelocyPackHelper::sanitizeNonClientTypes(
          newDocumentBuilder->slice(), VPackSlice::noneSlice(),
          *replicationData, transactionContextPtr()->getVPackOptions(), true,
          true, false);
    }

    return res;
  };

  std::unordered_map<ErrorCode, size_t> errorCounter;

  replicationData->openArray(true);
  if (newValue.isArray()) {
    resultBuilder.openArray();

    for (VPackSlice s : VPackArrayIterator(newValue)) {
      res = workForOneDocument(s, true);
      if (res.fail()) {
        createBabiesError(replicationType == ReplicationType::FOLLOWER
                              ? nullptr
                              : &resultBuilder,
                          errorCounter, res);
        res.reset();
      }
    }

    resultBuilder.close();
  } else {
    res = workForOneDocument(newValue, false);

    // on a follower, our result should always be an empty object
    if (replicationType == ReplicationType::FOLLOWER) {
      TRI_ASSERT(resultBuilder.slice().isNone());
      // add an empty object here so that when sending things back in JSON
      // format, there is no "non-representable type 'none'" issue.
      resultBuilder.add(VPackSlice::emptyObjectSlice());
    }
  }
  replicationData->close();

  // on a follower, our result should always be an empty array or object
  TRI_ASSERT(replicationType != ReplicationType::FOLLOWER ||
             (newValue.isArray() && resultBuilder.slice().isEmptyArray()) ||
             (newValue.isObject() && resultBuilder.slice().isEmptyObject()));
  TRI_ASSERT(replicationData->slice().isArray());
  TRI_ASSERT(replicationType != ReplicationType::FOLLOWER ||
             replicationData->slice().isEmptyArray());
  TRI_ASSERT(!newValue.isArray() || options.silent ||
             resultBuilder.slice().length() == newValue.length());

  auto resDocs = resultBuilder.steal();
  if (res.ok()) {
    if (replicationType == ReplicationType::LEADER &&
        (!followers->empty() ||
         collection->replicationVersion() == replication::Version::TWO) &&
        !replicationData->slice().isEmptyArray()) {
      // We still hold a lock here, because this is update/replace and we're
      // therefore not doing single document operations. But if we didn't hold
      // it at the beginning of the method the followers may not be up-to-date.
      TRI_ASSERT(isLocked(collection.get(), AccessMode::Type::WRITE));
      TRI_ASSERT(collection != nullptr);

      // In the multi babies case res is always TRI_ERROR_NO_ERROR if we
      // get here, in the single document case, we do not try to replicate
      // in case of an error.

      // Now replicate the good operations on all followers:
      return replicateOperations(*trxColl, followers, options, *replicationData,
                                 isUpdate ? TRI_VOC_DOCUMENT_OPERATION_UPDATE
                                          : TRI_VOC_DOCUMENT_OPERATION_REPLACE)
          .thenValue([options, errs = std::move(errorCounter),
                      resultData = std::move(resDocs)](Result&& res) mutable {
            if (!res.ok()) {
              return OperationResult{std::move(res), options};
            }
            if (options.silent && errs.empty()) {
              // We needed the results, but do not want to report:
              resultData->clear();
            }
            return OperationResult(std::move(res), std::move(resultData),
                                   std::move(options), std::move(errs));
          });
    }

    // execute a deferred intermediate commit, if required.
    res = performIntermediateCommitIfRequired(collection->id());
  }

  if (options.silent && errorCounter.empty()) {
    // We needed the results, but do not want to report:
    resDocs->clear();
  }

  return OperationResult(std::move(res), std::move(resDocs), std::move(options),
                         std::move(errorCounter));
}

Result transaction::Methods::modifyLocalHelper(
    LogicalCollection& collection, velocypack::Slice value,
    LocalDocumentId previousDocumentId, RevisionId previousRevisionId,
    velocypack::Slice previousDocument, RevisionId& newRevisionId,
    velocypack::Builder& newDocumentBuilder, OperationOptions& options,
    BatchOptions& batchOptions, bool isUpdate) {
  TRI_IF_FAILURE("LogicalCollection::update") {
    if (isUpdate) {
      return {TRI_ERROR_DEBUG};
    }
  }
  TRI_IF_FAILURE("LogicalCollection::replace") {
    if (!isUpdate) {
      return {TRI_ERROR_DEBUG};
    }
  }

  if (!options.ignoreRevs) {
    // Check old revision:
    auto checkRevision = [](RevisionId expected, RevisionId found) noexcept {
      return expected.empty() || found == expected;
    };

    RevisionId expectedRevision = RevisionId::fromSlice(value);
    if (expectedRevision.isSet() &&
        !checkRevision(expectedRevision, previousRevisionId)) {
      return {TRI_ERROR_ARANGO_CONFLICT, "conflict, _rev values do not match"};
    }
  }

  // no-op update: no values in the document are changed. in this case we
  // do not perform any update, but simply return. note: no-op updates are
  // not allowed if there are computed attributes.
  bool isNoOpUpdate = (value.length() <= 1 && isUpdate && !options.isRestore &&
                       options.isSynchronousReplicationFrom.empty() &&
                       batchOptions.computedValues == nullptr);

  // merge old and new values
  Result res;
  if (isUpdate) {
    res = mergeObjectsForUpdate(*this, collection, previousDocument, value,
                                isNoOpUpdate, previousRevisionId, newRevisionId,
                                newDocumentBuilder, options, batchOptions);
  } else {
    res = newObjectForReplace(*this, collection, previousDocument, value,
                              newRevisionId, newDocumentBuilder, options,
                              batchOptions);
  }

  if (res.ok()) {
    if (isNoOpUpdate) {
      // shortcut. no need to do anything
      TRI_ASSERT(batchOptions.computedValues == nullptr);
      TRI_ASSERT(previousRevisionId == newRevisionId);
      trackWaitForSync(collection, options);
      return {};
    }

    TRI_ASSERT(newRevisionId.isSet());

    // Need to check that no sharding keys have changed:
    if (batchOptions.validateShardKeysOnUpdateReplace &&
        shardKeysChanged(collection, previousDocument,
                         newDocumentBuilder.slice(), isUpdate)) {
      return {TRI_ERROR_CLUSTER_MUST_NOT_CHANGE_SHARDING_ATTRIBUTES};
    }

    if (batchOptions.validateSmartJoinAttribute &&
        smartJoinAttributeChanged(collection, previousDocument,
                                  newDocumentBuilder.slice(), isUpdate)) {
      return {TRI_ERROR_CLUSTER_MUST_NOT_CHANGE_SMART_JOIN_ATTRIBUTE};
    }

    // note: schema can be a nullptr here, but we need to call validate()
    // anyway. the reason is that validate() does not only perform schema
    // validation, but also some validation for SmartGraph data
    res = collection.validate(batchOptions.schema, newDocumentBuilder.slice(),
                              previousDocument,
                              transactionContextPtr()->getVPackOptions());

    if (res.ok()) {
      if (isUpdate) {
        res = collection.getPhysical()->update(
            *this, previousDocumentId, previousRevisionId, previousDocument,
            newRevisionId, newDocumentBuilder.slice(), options);
      } else {
        res = collection.getPhysical()->replace(
            *this, previousDocumentId, previousRevisionId, previousDocument,
            newRevisionId, newDocumentBuilder.slice(), options);
      }
    }

    if (res.ok()) {
      trackWaitForSync(collection, options);
    }
  }

  return res;
}

OperationResult Methods::remove(std::string const& collectionName,
                                VPackSlice value,
                                OperationOptions const& options) {
  return removeInternal(collectionName, value, options, MethodsApi::Synchronous)
      .get();
}

/// @brief remove one or multiple documents in a collection
/// the single-document variant of this operation will either succeed or,
/// if it fails, clean up after itself
Future<OperationResult> transaction::Methods::removeAsync(
    std::string const& collectionName, VPackSlice value,
    OperationOptions const& options) {
  return removeInternal(collectionName, value, options,
                        MethodsApi::Asynchronous);
}

/// @brief remove one or multiple documents in a collection, coordinator
/// the single-document variant of this operation will either succeed or,
/// if it fails, clean up after itself
#ifndef USE_ENTERPRISE
Future<OperationResult> transaction::Methods::removeCoordinator(
    std::string const& collectionName, VPackSlice value,
    OperationOptions const& options, MethodsApi api) {
  auto colptr = resolver()->getCollectionStructCluster(collectionName);
  if (colptr == nullptr) {
    return futures::makeFuture(
        OperationResult(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, options));
  }
  return arangodb::removeDocumentOnCoordinator(*this, *colptr, value, options,
                                               api);
}
#endif

/// @brief remove one or multiple documents in a collection, local
/// the single-document variant of this operation will either succeed or,
/// if it fails, clean up after itself
Future<OperationResult> transaction::Methods::removeLocal(
    std::string const& collectionName, VPackSlice value,
    OperationOptions& options) {
  DataSourceId cid =
      addCollectionAtRuntime(collectionName, AccessMode::Type::WRITE);
  TransactionCollection* trxColl = trxCollection(cid);
  if (trxColl == nullptr) {
    return futures::makeFuture(
        OperationResult(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, options));
  }
  std::shared_ptr<LogicalCollection> const& collection = trxColl->collection();
  if (collection == nullptr) {
    return futures::makeFuture(
        OperationResult(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, options));
  }
  TRI_ASSERT(trxColl->isLocked(AccessMode::Type::WRITE));

  ReplicationType replicationType = ReplicationType::NONE;
  std::shared_ptr<std::vector<ServerID> const> followers;
  // this call will populate replicationType and followers
  Result res = determineReplicationTypeAndFollowers(
      *collection, "remove", value, options, replicationType, followers);

  if (res.fail()) {
    return futures::makeFuture(OperationResult(std::move(res), options));
  }

  bool excludeAllFromReplication =
      replicationType != ReplicationType::LEADER ||
      (followers->empty() &&
       collection->replicationVersion() != replication::Version::TWO);

  // total result that is going to be returned to the caller, append-only
  VPackBuilder resultBuilder;
  // all document data that are going to be replicated, append-only
  transaction::BuilderLeaser replicationData(this);
  // builder for a single, old version of document (will be recycled for each
  // document)
  transaction::BuilderLeaser previousDocumentBuilder(this);
  // temporary builder for building keys
  transaction::BuilderLeaser keyBuilder(this);

  auto workForOneDocument = [&](VPackSlice value, bool isArray) -> Result {
    std::string_view key;

    if (value.isString()) {
      key = value.stringView();
      // strip everything before a / (likely an _id value)
      size_t pos = key.find('/');
      if (pos != std::string::npos) {
        key = key.substr(pos + 1);
        keyBuilder->clear();
        keyBuilder->add(VPackValue(key));
        value = keyBuilder->slice();
      }
    } else if (value.isObject()) {
      VPackSlice keySlice = value.get(StaticStrings::KeyString);
      if (keySlice.isString()) {
        key = keySlice.stringView();
      }
    }

    // primary key must not be empty
    if (key.empty()) {
      return {TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD};
    }

    std::pair<LocalDocumentId, RevisionId> lookupResult;
    Result res = collection->getPhysical()->lookupKey(this, key, lookupResult,
                                                      ReadOwnWrites::yes);
    if (res.fail()) {
      return {TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND};
    }

    TRI_ASSERT(lookupResult.first.isSet());
    TRI_ASSERT(lookupResult.second.isSet());
    auto [oldDocumentId, oldRevisionId] = lookupResult;

    previousDocumentBuilder->clear();
    res = collection->getPhysical()->lookupDocument(
        *this, oldDocumentId, *previousDocumentBuilder,
        /*readCache*/ true, /*fillCache*/ false, ReadOwnWrites::yes);

    if (res.fail()) {
      return res;
    }

    bool excludeFromReplication = excludeAllFromReplication;
    TRI_ASSERT(previousDocumentBuilder->slice().isObject());

    res = removeLocalHelper(*collection, value, oldDocumentId, oldRevisionId,
                            previousDocumentBuilder->slice(), options);

    if ((res.is(TRI_ERROR_ARANGO_CONFLICT) && !isArray) ||
        (res.ok() && !options.silent)) {
      TRI_ASSERT(oldRevisionId.isSet());
      buildDocumentIdentity(
          *collection, resultBuilder, cid, key, oldRevisionId,
          RevisionId::none(),
          options.returnOld ? previousDocumentBuilder.get() : nullptr, nullptr);
    }

    if (res.ok() && !excludeFromReplication) {
      replicationData->openObject(/*unindexed*/ true);
      replicationData->add(StaticStrings::KeyString, VPackValue(key));

      char ridBuffer[arangodb::basics::maxUInt64StringSize];
      replicationData->add(StaticStrings::RevString,
                           oldRevisionId.toValuePair(ridBuffer));
      replicationData->close();
    }

    return res;
  };

  replicationData->openArray(true);
  std::unordered_map<ErrorCode, size_t> errorCounter;
  if (value.isArray()) {
    resultBuilder.openArray();

    for (VPackSlice s : VPackArrayIterator(value)) {
      res = workForOneDocument(s, true);
      if (res.fail()) {
        createBabiesError(replicationType == ReplicationType::FOLLOWER
                              ? nullptr
                              : &resultBuilder,
                          errorCounter, res);
        res.reset();
      }
    }

    resultBuilder.close();
  } else {
    res = workForOneDocument(value, false);

    // on a follower, our result should always be an empty object
    if (replicationType == ReplicationType::FOLLOWER) {
      TRI_ASSERT(resultBuilder.slice().isNone());
      // add an empty object here so that when sending things back in JSON
      // format, there is no "non-representable type 'none'" issue.
      resultBuilder.add(VPackSlice::emptyObjectSlice());
    }
  }
  replicationData->close();

  // on a follower, our result should always be an empty array or object
  TRI_ASSERT(replicationType != ReplicationType::FOLLOWER ||
             (value.isArray() && resultBuilder.slice().isEmptyArray()) ||
             (value.isObject() && resultBuilder.slice().isEmptyObject()));
  TRI_ASSERT(replicationData->slice().isArray());
  TRI_ASSERT(replicationType != ReplicationType::FOLLOWER ||
             replicationData->slice().isEmptyArray());
  TRI_ASSERT(!value.isArray() || options.silent ||
             resultBuilder.slice().length() == value.length());

  auto resDocs = resultBuilder.steal();
  if (res.ok()) {
    auto replicationVersion = collection->replicationVersion();
    if (replicationType == ReplicationType::LEADER &&
        (!followers->empty() ||
         replicationVersion == replication::Version::TWO) &&
        !replicationData->slice().isEmptyArray()) {
      TRI_ASSERT(collection != nullptr);
      // Now replicate the same operation on all followers:

      // In the multi babies case res is always TRI_ERROR_NO_ERROR if we
      // get here, in the single document case, we do not try to replicate
      // in case of an error.

      // Now replicate the good operations on all followers:
      return replicateOperations(*trxColl, followers, options, *replicationData,
                                 TRI_VOC_DOCUMENT_OPERATION_REMOVE)
          .thenValue([options, errs = std::move(errorCounter),
                      resultData = std::move(resDocs)](Result res) mutable {
            if (!res.ok()) {
              return OperationResult{std::move(res), options};
            }
            if (options.silent && errs.empty()) {
              // We needed the results, but do not want to report:
              resultData->clear();
            }
            return OperationResult(std::move(res), std::move(resultData),
                                   std::move(options), std::move(errs));
          });
    }

    // execute a deferred intermediate commit, if required.
    res = performIntermediateCommitIfRequired(collection->id());
  }

  if (options.silent && errorCounter.empty()) {
    // We needed the results, but do not want to report:
    resDocs->clear();
  }

  return OperationResult(std::move(res), std::move(resDocs), std::move(options),
                         std::move(errorCounter));
}

Result transaction::Methods::removeLocalHelper(
    LogicalCollection& collection, velocypack::Slice value,
    LocalDocumentId previousDocumentId, RevisionId previousRevisionId,
    velocypack::Slice previousDocument, OperationOptions& options) {
  TRI_IF_FAILURE("LogicalCollection::remove") { return {TRI_ERROR_DEBUG}; }

  // check revisions only if value is a proper object. if value is simply
  // a key, we cannot check the revisions.
  if (!options.ignoreRevs && value.isObject()) {
    // Check old revision:
    auto checkRevision = [](RevisionId expected, RevisionId found) noexcept {
      return expected.empty() || found == expected;
    };

    RevisionId expectedRevision = RevisionId::fromSlice(value);
    if (expectedRevision.isSet() &&
        !checkRevision(expectedRevision, previousRevisionId)) {
      return {TRI_ERROR_ARANGO_CONFLICT, "conflict, _rev values do not match"};
    }
  }

  Result res = collection.getPhysical()->remove(
      *this, previousDocumentId, previousRevisionId, previousDocument, options);

  if (res.ok()) {
    trackWaitForSync(collection, options);
  }

  return res;
}

/// @brief fetches all documents in a collection
OperationResult transaction::Methods::all(std::string const& collectionName,
                                          uint64_t skip, uint64_t limit,
                                          OperationOptions const& options) {
  TRI_ASSERT(_state->status() == transaction::Status::RUNNING);

  OperationOptions optionsCopy = options;

  if (_state->isCoordinator()) {
    return allCoordinator(collectionName, skip, limit, optionsCopy);
  }

  return allLocal(collectionName, skip, limit, optionsCopy);
}

/// @brief fetches all documents in a collection, coordinator
OperationResult transaction::Methods::allCoordinator(
    std::string const& collectionName, uint64_t skip, uint64_t limit,
    OperationOptions& options) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

/// @brief fetches all documents in a collection, local
OperationResult transaction::Methods::allLocal(
    std::string const& collectionName, uint64_t skip, uint64_t limit,
    OperationOptions& options) {
  DataSourceId cid =
      addCollectionAtRuntime(collectionName, AccessMode::Type::READ);
  TransactionCollection* trxColl = trxCollection(cid);
  if (trxColl == nullptr) {
    return OperationResult(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, options);
  }
  TRI_ASSERT(trxColl->isLocked(AccessMode::Type::READ));

  VPackBuilder resultBuilder;

  if (_state->isDBServer()) {
    std::shared_ptr<LogicalCollection> const& collection =
        trxColl->collection();
    if (collection == nullptr) {
      return OperationResult(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, options);
    }
    auto const& followerInfo = collection->followers();
    if (!followerInfo->getLeader().empty()) {
      return OperationResult(TRI_ERROR_CLUSTER_SHARD_LEADER_RESIGNED, options);
    }
  }

  resultBuilder.openArray();

  auto iterator = indexScan(
      collectionName, transaction::Methods::CursorType::ALL, ReadOwnWrites::no);

  iterator->allDocuments(
      [&resultBuilder](LocalDocumentId const& /*token*/, VPackSlice slice) {
        resultBuilder.add(slice);
        return true;
      });

  resultBuilder.close();

  return OperationResult(Result(), resultBuilder.steal(), options);
}

OperationResult Methods::truncate(std::string const& collectionName,
                                  OperationOptions const& options) {
  return truncateInternal(collectionName, options, MethodsApi::Synchronous)
      .get();
}

/// @brief remove all documents in a collection
Future<OperationResult> transaction::Methods::truncateAsync(
    std::string const& collectionName, OperationOptions const& options) {
  return truncateInternal(collectionName, options, MethodsApi::Asynchronous);
}

/// @brief remove all documents in a collection, coordinator
#ifndef USE_ENTERPRISE
Future<OperationResult> transaction::Methods::truncateCoordinator(
    std::string const& collectionName, OperationOptions& options,
    MethodsApi api) {
  return arangodb::truncateCollectionOnCoordinator(*this, collectionName,
                                                   options, api);
}
#endif

/// @brief remove all documents in a collection, local
Future<OperationResult> transaction::Methods::truncateLocal(
    std::string const& collectionName, OperationOptions& options) {
  DataSourceId cid =
      addCollectionAtRuntime(collectionName, AccessMode::Type::WRITE);
  TransactionCollection* trxColl = trxCollection(cid);
  if (trxColl == nullptr) {
    return futures::makeFuture(
        OperationResult(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, options));
  }
  auto const& collection = trxColl->collection();
  if (collection == nullptr) {
    return futures::makeFuture(
        OperationResult(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, options));
  }

  // this call will populate replicationType and followers
  ReplicationType replicationType = ReplicationType::NONE;
  std::shared_ptr<std::vector<ServerID> const> followers;

  Result res = determineReplicationTypeAndFollowers(
      *collection, "truncate", VPackSlice::noneSlice(), options,
      replicationType, followers);

  if (res.fail()) {
    return futures::makeFuture(OperationResult(std::move(res), options));
  }

  res = collection->truncate(*this, options);

  if (res.fail()) {
    return futures::makeFuture(OperationResult(res, options));
  }

  auto replicationVersion = collection->replicationVersion();
  if (replicationType == ReplicationType::LEADER &&
      replicationVersion == replication::Version::TWO) {
    auto& rtc = static_cast<ReplicatedRocksDBTransactionCollection&>(*trxColl);
    auto leaderState = rtc.leaderState();
    auto body = VPackBuilder();
    {
      VPackObjectBuilder ob(&body);
      body.add("collection", collectionName);
    }
    leaderState->replicateOperation(
        body.sharedSlice(),
        replication2::replicated_state::document::OperationType::kTruncate,
        state()->id(),
        replication2::replicated_state::document::ReplicationOptions{});
    return OperationResult{Result{}, options};
  }

  // Now see whether or not we have to do synchronous replication:
  if (replicationType == ReplicationType::LEADER && !followers->empty()) {
    // Now replicate the good operations on all followers:
    NetworkFeature const& nf = vocbase().server().getFeature<NetworkFeature>();
    network::ConnectionPool* pool = nf.pool();
    if (pool != nullptr) {
      // nullptr only happens on controlled shutdown
      std::string path =
          "/_api/collection/" +
          arangodb::basics::StringUtils::urlEncode(collectionName) +
          "/truncate";
      VPackBuffer<uint8_t> body;
      VPackSlice s = VPackSlice::emptyObjectSlice();
      body.append(s.start(), s.byteSize());

      // Now prepare the requests:
      std::vector<network::FutureRes> futures;
      futures.reserve(followers->size());

      network::RequestOptions reqOpts;
      reqOpts.database = vocbase().name();
      reqOpts.timeout = network::Timeout(600);
      reqOpts.param(StaticStrings::Compact,
                    (options.truncateCompact ? "true" : "false"));

      for (auto const& f : *followers) {
        // check following term id for the follower:
        // if it is 0, it means that the follower cannot handle following
        // term ids safely, so we only pass the leader id string to id but
        // no following term. this happens for followers < 3.8.3
        // if the following term id is != 0, we will pass it on along with
        // the leader id string, in format "LEADER_FOLLOWINGTERMID"
        uint64_t followingTermId =
            collection->followers()->getFollowingTermId(f);
        if (followingTermId == 0) {
          reqOpts.param(StaticStrings::IsSynchronousReplicationString,
                        ServerState::instance()->getId());
        } else {
          reqOpts.param(StaticStrings::IsSynchronousReplicationString,
                        ServerState::instance()->getId() + "_" +
                            basics::StringUtils::itoa(followingTermId));
        }
        // reqOpts is copied deep in sendRequestRetry, so we are OK to
        // change it in the loop!
        network::Headers headers;
        ClusterTrxMethods::addTransactionHeader(*this, f, headers);
        auto future = network::sendRequestRetry(
            pool, "server:" + f, fuerte::RestVerb::Put, path, body, reqOpts,
            std::move(headers));
        futures.emplace_back(std::move(future));
      }

      auto responses = futures::collectAll(futures).get();
      // we drop all followers that were not successful:
      for (size_t i = 0; i < followers->size(); ++i) {
        bool replicationWorked =
            responses[i].hasValue() && responses[i].get().ok() &&
            (responses[i].get().statusCode() == fuerte::StatusAccepted ||
             responses[i].get().statusCode() == fuerte::StatusOK);
        if (!replicationWorked) {
          if (!vocbase().server().isStopping()) {
            auto const& followerInfo = collection->followers();
            LOG_TOPIC("0e2e0", WARN, Logger::REPLICATION)
                << "truncateLocal: dropping follower " << (*followers)[i]
                << " for shard " << collection->vocbase().name() << "/"
                << collectionName << ": "
                << responses[i].get().combinedResult().errorMessage();
            res = followerInfo->remove((*followers)[i]);
            // intentionally do NOT remove the follower from the list of
            // known servers here. if we do, we will not be able to
            // send the commit/abort to the follower later. However, we
            // still need to send the commit/abort to the follower at
            // transaction end, because the follower may be responsbile
            // for _other_ shards as well.
            // it does not matter if we later commit the writes of the shard
            // from which we just removed the follower, because the follower
            // is now dropped and will try to get back in sync anyway, so
            // it will run the full shard synchronization process.
            if (res.fail()) {
              LOG_TOPIC("359bc", WARN, Logger::REPLICATION)
                  << "truncateLocal: could not drop follower "
                  << (*followers)[i] << " for shard "
                  << collection->vocbase().name() << "/" << collection->name()
                  << ": " << res.errorMessage();

              // Note: it is safe here to exit the loop early. We are losing the
              // leadership here. No matter what happens next, the Current entry
              // in the agency is rewritten and thus replication is restarted
              // from the new leader. There is no need to keep trying to drop
              // followers at this point.

              if (res.is(TRI_ERROR_CLUSTER_NOT_LEADER)) {
                // In this case, we know that we are not or no longer
                // the leader for this shard. Therefore we need to
                // send a code which let's the coordinator retry.
                THROW_ARANGO_EXCEPTION(TRI_ERROR_CLUSTER_SHARD_LEADER_RESIGNED);
              } else {
                // In this case, some other error occurred and we
                // most likely are still the proper leader, so
                // the error needs to be reported and the local
                // transaction must be rolled back.
                THROW_ARANGO_EXCEPTION(
                    TRI_ERROR_CLUSTER_COULD_NOT_DROP_FOLLOWER);
              }
            }
          } else {
            LOG_TOPIC("cb953", INFO, Logger::REPLICATION)
                << "truncateLocal: shutting down and not replicating "
                << (*followers)[i] << " for shard "
                << collection->vocbase().name() << "/" << collection->name()
                << ": " << res.errorMessage();
            THROW_ARANGO_EXCEPTION(TRI_ERROR_SHUTTING_DOWN);
          }
        }
      }
      // If any would-be-follower refused to follow there must be a
      // new leader in the meantime, in this case we must not allow
      // this operation to succeed, we simply return with a refusal
      // error (note that we use the follower version, since we have
      // lost leadership):
      if (findRefusal(responses)) {
        ++vocbase()
              .server()
              .getFeature<arangodb::ClusterFeature>()
              .followersRefusedCounter();
        return futures::makeFuture(
            OperationResult(TRI_ERROR_CLUSTER_SHARD_LEADER_RESIGNED, options));
      }
    }
  }

  return futures::makeFuture(OperationResult(res, options));
}

OperationResult Methods::count(std::string const& collectionName,
                               CountType type,
                               OperationOptions const& options) {
  return countInternal(collectionName, type, options, MethodsApi::Synchronous)
      .get();
}

/// @brief count the number of documents in a collection
futures::Future<OperationResult> transaction::Methods::countAsync(
    std::string const& collectionName, transaction::CountType type,
    OperationOptions const& options) {
  return countInternal(collectionName, type, options, MethodsApi::Asynchronous);
}

#ifndef USE_ENTERPRISE
/// @brief count the number of documents in a collection
futures::Future<OperationResult> transaction::Methods::countCoordinator(
    std::string const& collectionName, transaction::CountType type,
    OperationOptions const& options, MethodsApi api) {
  // First determine the collection ID from the name:
  auto colptr = resolver()->getCollectionStructCluster(collectionName);
  if (colptr == nullptr) {
    return futures::makeFuture(
        OperationResult(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, options));
  }

  return countCoordinatorHelper(colptr, collectionName, type, options, api);
}

#endif

futures::Future<OperationResult> transaction::Methods::countCoordinatorHelper(
    std::shared_ptr<LogicalCollection> const& collinfo,
    std::string const& collectionName, transaction::CountType type,
    OperationOptions const& options, MethodsApi api) {
  TRI_ASSERT(collinfo != nullptr);
  auto& cache = collinfo->countCache();

  uint64_t documents = CountCache::NotPopulated;
  if (type == transaction::CountType::ForceCache) {
    // always return from the cache, regardless what's in it
    documents = cache.get();
  } else if (type == transaction::CountType::TryCache) {
    documents = cache.getWithTtl();
  }

  if (documents == CountCache::NotPopulated) {
    // no cache hit, or detailed results requested
    return arangodb::countOnCoordinator(*this, collectionName, options, api)
        .thenValue(
            [&cache, type, options](OperationResult&& res) -> OperationResult {
              if (res.fail()) {
                return std::move(res);
              }

              // reassemble counts from vpack
              std::vector<std::pair<std::string, uint64_t>> counts;
              TRI_ASSERT(res.slice().isArray());
              for (VPackSlice count : VPackArrayIterator(res.slice())) {
                TRI_ASSERT(count.isArray());
                TRI_ASSERT(count[0].isString());
                TRI_ASSERT(count[1].isNumber());
                std::string key = count[0].copyString();
                uint64_t value = count[1].getNumericValue<uint64_t>();
                counts.emplace_back(std::move(key), value);
              }

              uint64_t total = 0;
              OperationResult opRes =
                  buildCountResult(options, counts, type, total);
              cache.store(total);
              return opRes;
            });
  }

  // cache hit!
  TRI_ASSERT(documents != CountCache::NotPopulated);
  TRI_ASSERT(type != transaction::CountType::Detailed);

  // return number from cache
  VPackBuilder resultBuilder;
  resultBuilder.add(VPackValue(documents));
  return OperationResult(Result(), resultBuilder.steal(), options);
}

/// @brief count the number of documents in a collection
OperationResult transaction::Methods::countLocal(
    std::string const& collectionName, transaction::CountType type,
    OperationOptions const& options) {
  DataSourceId cid =
      addCollectionAtRuntime(collectionName, AccessMode::Type::READ);
  TransactionCollection* trxColl = trxCollection(cid);
  if (trxColl == nullptr) {
    return OperationResult(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, options);
  }
  auto const& collection = trxColl->collection();
  if (collection == nullptr) {
    return OperationResult(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, options);
  }

  TRI_ASSERT(isLocked(collection.get(), AccessMode::Type::READ));

  uint64_t num = collection->numberDocuments(this, type);

  VPackBuilder resultBuilder;
  resultBuilder.add(VPackValue(num));

  return OperationResult(Result(), resultBuilder.steal(), options);
}

/// @brief factory for IndexIterator objects from AQL
std::unique_ptr<IndexIterator> transaction::Methods::indexScanForCondition(
    IndexHandle const& idx, arangodb::aql::AstNode const* condition,
    arangodb::aql::Variable const* var, IndexIteratorOptions const& opts,
    ReadOwnWrites readOwnWrites, int mutableConditionIdx) {
  if (_state->isCoordinator()) {
    // The index scan is only available on DBServers and Single Server.
    THROW_ARANGO_EXCEPTION(TRI_ERROR_CLUSTER_ONLY_ON_DBSERVER);
  }

  if (nullptr == idx) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "The index id cannot be empty.");
  }

  // TODO: an extra optimizer rule could make this unnecessary
  if (isInaccessibleCollection(idx->collection().name())) {
    return std::make_unique<EmptyIndexIterator>(&idx->collection(), this);
  }

  // Now create the Iterator
  TRI_ASSERT(!idx->inProgress());
  return idx->iteratorForCondition(this, condition, var, opts, readOwnWrites,
                                   mutableConditionIdx);
}

/// @brief factory for IndexIterator objects
/// note: the caller must have read-locked the underlying collection when
/// calling this method
std::unique_ptr<IndexIterator> transaction::Methods::indexScan(
    std::string const& collectionName, CursorType cursorType,
    ReadOwnWrites readOwnWrites) {
  // For now we assume indexId is the iid part of the index.

  if (ADB_UNLIKELY(_state->isCoordinator())) {
    // The index scan is only available on DBServers and Single Server.
    THROW_ARANGO_EXCEPTION(TRI_ERROR_CLUSTER_ONLY_ON_DBSERVER);
  }

  DataSourceId cid =
      addCollectionAtRuntime(collectionName, AccessMode::Type::READ);
  TransactionCollection* trxColl = trxCollection(cid);
  if (trxColl == nullptr) {
    throwCollectionNotFound(collectionName);
  }
  TRI_ASSERT(trxColl->isLocked(AccessMode::Type::READ));

  std::shared_ptr<LogicalCollection> const& logical = trxColl->collection();
  if (logical == nullptr) {
    throwCollectionNotFound(collectionName);
  }
  TRI_ASSERT(logical != nullptr);

  // TODO: an extra optimizer rule could make this unnecessary
  if (isInaccessibleCollection(collectionName)) {
    return std::make_unique<EmptyIndexIterator>(logical.get(), this);
  }

  std::unique_ptr<IndexIterator> iterator;
  switch (cursorType) {
    case CursorType::ANY: {
      iterator = logical->getAnyIterator(this);
      break;
    }
    case CursorType::ALL: {
      iterator = logical->getAllIterator(this, readOwnWrites);
      break;
    }
  }

  // the above methods must always return a valid iterator or throw!
  TRI_ASSERT(iterator != nullptr);
  return iterator;
}

/// @brief return the collection
arangodb::LogicalCollection* transaction::Methods::documentCollection(
    std::string const& name) const {
  TRI_ASSERT(_state != nullptr);
  TRI_ASSERT(_state->status() == transaction::Status::RUNNING);

  auto trxColl = trxCollection(name, AccessMode::Type::READ);
  if (trxColl == nullptr || trxColl->collection() == nullptr) {
    throwCollectionNotFound(name);
  }

  return trxColl->collection().get();
}

/// @brief add a collection by id, with the name supplied
Result transaction::Methods::addCollection(DataSourceId cid,
                                           std::string const& collectionName,
                                           AccessMode::Type type) {
  if (_state == nullptr) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "cannot add collection without state");
  }

  Status const status = _state->status();

  if (status == transaction::Status::COMMITTED ||
      status == transaction::Status::ABORTED) {
    // transaction already finished?
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL,
        "cannot add collection to committed or aborted transaction");
  }

  if (_mainTransaction && status != transaction::Status::CREATED) {
    // transaction already started?
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_TRANSACTION_INTERNAL,
        "cannot add collection to a previously started top-level transaction");
  }

  if (cid.empty()) {
    // invalid cid
    throwCollectionNotFound(collectionName);
  }

  const bool lockUsage = !_mainTransaction;

  auto addCollectionCallback = [this, &collectionName, type,
                                lockUsage](DataSourceId cid) -> void {
    auto res = _state->addCollection(cid, collectionName, type, lockUsage);

    if (res.fail()) {
      THROW_ARANGO_EXCEPTION(res);
    }
  };

  Result res;
  bool visited = false;
  std::function<bool(LogicalCollection&)> visitor(
      [this, &addCollectionCallback, &res, cid,
       &visited](LogicalCollection& col) -> bool {
        addCollectionCallback(col.id());  // will throw on error
        res = applyDataSourceRegistrationCallbacks(col, *this);
        visited |= cid == col.id();

        return res.ok();  // add the remaining collections (or break on error)
      });

  if (!resolver()->visitCollections(visitor, cid) || res.fail()) {
    // trigger exception as per the original behavior (tests depend on this)
    if (res.ok() && !visited) {
      addCollectionCallback(cid);  // will throw on error
    }

    return res.ok() ? Result(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND)
                    : res;  // return first error
  }

  // skip provided 'cid' if it was already done by the visitor
  if (visited) {
    return res;
  }

  auto dataSource = resolver()->getDataSource(cid);

  return dataSource ? applyDataSourceRegistrationCallbacks(*dataSource, *this)
                    : Result(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
}

/// @brief add a collection by name
Result transaction::Methods::addCollection(std::string const& name,
                                           AccessMode::Type type) {
  return addCollection(resolver()->getCollectionId(name), name, type);
}

/// @brief test if a collection is already locked
bool transaction::Methods::isLocked(LogicalCollection* document,
                                    AccessMode::Type type) const {
  if (_state == nullptr || _state->status() != transaction::Status::RUNNING) {
    return false;
  }
  if (_state->hasHint(Hints::Hint::LOCK_NEVER)) {
    // In the lock never case we have made sure that
    // some other process holds this lock.
    // So we can lie here and report that it actually
    // is locked!
    return true;
  }

  TransactionCollection* trxColl = trxCollection(document->id(), type);
  TRI_ASSERT(trxColl != nullptr);
  return trxColl->isLocked(type);
}

Result transaction::Methods::resolveId(
    char const* handle, size_t length,
    std::shared_ptr<LogicalCollection>& collection, char const*& key,
    size_t& outLength) {
  char const* p = static_cast<char const*>(
      memchr(handle, TRI_DOCUMENT_HANDLE_SEPARATOR_CHR, length));

  if (p == nullptr || *p == '\0') {
    return {TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD};
  }

  std::string const name(handle, p - handle);
  collection = resolver()->getCollectionStructCluster(name);

  if (collection == nullptr) {
    return {TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND};
  }

  key = p + 1;
  outLength = length - (key - handle);

  return {};
}

// Unified replication of operations. May be inserts (with or without
// overwrite), removes, or modifies (updates/replaces).
Future<Result> Methods::replicateOperations(
    TransactionCollection& transactionCollection,
    std::shared_ptr<const std::vector<ServerID>> const& followerList,
    OperationOptions const& options, velocypack::Builder const& replicationData,
    TRI_voc_document_operation_e operation) {
  auto const& collection = transactionCollection.collection();
  TRI_ASSERT(followerList != nullptr);

  // It is normal to have an empty followerList when using replication2
  TRI_ASSERT(!followerList->empty() ||
             collection->vocbase().replicationVersion() ==
                 replication::Version::TWO);

  TRI_ASSERT(replicationData.slice().isArray());
  TRI_ASSERT(!replicationData.slice().isEmptyArray());

  // replication2 is handled here
  if (collection->replicationVersion() == replication::Version::TWO) {
    auto& rtc = static_cast<ReplicatedRocksDBTransactionCollection&>(
        transactionCollection);
    auto leaderState = rtc.leaderState();
    leaderState->replicateOperation(
        replicationData.sharedSlice(),
        replication2::replicated_state::document::fromDocumentOperation(
            operation),
        state()->id(),
        replication2::replicated_state::document::ReplicationOptions{});
    return Result{};
  }

  // path and requestType are different for insert/remove/modify.

  network::RequestOptions reqOpts;
  reqOpts.database = vocbase().name();
  reqOpts.param(StaticStrings::IsRestoreString, "true");
  std::string url = "/_api/document/";
  url.append(arangodb::basics::StringUtils::urlEncode(collection->name()));

  char const* opName = "unknown";
  arangodb::fuerte::RestVerb requestType = arangodb::fuerte::RestVerb::Illegal;
  switch (operation) {
    case TRI_VOC_DOCUMENT_OPERATION_INSERT:
      requestType = arangodb::fuerte::RestVerb::Post;

      opName = "insert";
      // handle overwrite modes
      if (options.isOverwriteModeSet()) {
        if (options.overwriteMode != OperationOptions::OverwriteMode::Unknown) {
          reqOpts.param(
              StaticStrings::OverwriteMode,
              OperationOptions::stringifyOverwriteMode(options.overwriteMode));
          if (options.overwriteMode ==
              OperationOptions::OverwriteMode::Update) {
            opName = "insert w/ overwriteMode update";
          } else if (options.overwriteMode ==
                     OperationOptions::OverwriteMode::Replace) {
            opName = "insert w/ overwriteMode replace";
          } else if (options.overwriteMode ==
                     OperationOptions::OverwriteMode::Ignore) {
            opName = "insert w/ overwriteMode ingore";
          }
        }
        if (options.overwriteMode == OperationOptions::OverwriteMode::Update) {
          // extra parameters only required for update
          reqOpts.param(StaticStrings::KeepNullString,
                        options.keepNull ? "true" : "false");
          reqOpts.param(StaticStrings::MergeObjectsString,
                        options.mergeObjects ? "true" : "false");
        }
      }
      break;
    case TRI_VOC_DOCUMENT_OPERATION_UPDATE:
      requestType = arangodb::fuerte::RestVerb::Patch;
      opName = "update";
      break;
    case TRI_VOC_DOCUMENT_OPERATION_REPLACE:
      requestType = arangodb::fuerte::RestVerb::Put;
      opName = "replace";
      break;
    case TRI_VOC_DOCUMENT_OPERATION_REMOVE:
      requestType = arangodb::fuerte::RestVerb::Delete;
      opName = "remove";
      break;
    case TRI_VOC_DOCUMENT_OPERATION_UNKNOWN:
    default:
      TRI_ASSERT(false);
  }

  size_t count = replicationData.slice().length();
  ReplicationTimeoutFeature& timeouts =
      vocbase().server().getFeature<ReplicationTimeoutFeature>();
  reqOpts.timeout = network::Timeout(
      ::chooseTimeoutForReplication(timeouts, count, replicationData.size()));
  TRI_IF_FAILURE("replicateOperations_randomize_timeout") {
    reqOpts.timeout = network::Timeout(
        static_cast<double>(RandomGenerator::interval(uint32_t(60))));
  }

  TRI_IF_FAILURE("replicateOperationsDropFollowerBeforeSending") {
    // drop all our followers, intentionally
    for (auto const& f : *followerList) {
      Result res = collection->followers()->remove(f);
      TRI_ASSERT(res.ok());
    }
  }

  // Now prepare the requests:
  std::vector<Future<network::Response>> futures;
  futures.reserve(followerList->size());

  auto startTimeReplication = std::chrono::steady_clock::now();

  auto* pool = vocbase().server().getFeature<NetworkFeature>().pool();
  for (auto const& f : *followerList) {
    // check following term id for the follower:
    // if it is 0, it means that the follower cannot handle following
    // term ids safely, so we only pass the leader id string to id but
    // no following term. this happens for followers < 3.8.3
    // if the following term id is != 0, we will pass it on along with
    // the leader id string, in format "LEADER_FOLLOWINGTERMID"
    uint64_t followingTermId = collection->followers()->getFollowingTermId(f);
    if (followingTermId == 0) {
      reqOpts.param(StaticStrings::IsSynchronousReplicationString,
                    ServerState::instance()->getId());
    } else {
      reqOpts.param(StaticStrings::IsSynchronousReplicationString,
                    ServerState::instance()->getId() + "_" +
                        basics::StringUtils::itoa(followingTermId));
    }
    // reqOpts is copied deep in sendRequestRetry, so we are OK to
    // change it in the loop!
    network::Headers headers;
    ClusterTrxMethods::addTransactionHeader(*this, f, headers);
    futures.emplace_back(network::sendRequestRetry(
        pool, "server:" + f, requestType, url, *(replicationData.buffer()),
        reqOpts, std::move(headers)));

    LOG_TOPIC("fecaf", TRACE, Logger::REPLICATION)
        << "replicating " << count << " " << opName << " operations for shard "
        << collection->vocbase().name() << "/" << collection->name()
        << ", server:" << f;
  }

  // If any would-be-follower refused to follow there are two possiblities:
  // (1) there is a new leader in the meantime, or
  // (2) the follower was restarted and forgot that it is a follower.
  // Unfortunately, we cannot know which is the case.
  // In case (1) case we must not allow
  // this operation to succeed, since the new leader is now responsible.
  // In case (2) we at least have to drop the follower such that it
  // resyncs and we can be sure that it is in sync again.
  // We have some hint from the error message of the follower. If it is
  // TRI_ERROR_CLUSTER_SHARD_LEADER_REFUSES_REPLICATION, we have reason
  // to believe that the follower is now the new leader and we assume
  // case (1).
  // If the error is TRI_ERROR_CLUSTER_SHARD_FOLLOWER_REFUSES_OPERATION,
  // we continue with the operation, since most likely, the follower was
  // simply dropped in the meantime.
  // In any case, we drop the follower here (just in case).
  auto cb =
      [=, this](
          std::vector<futures::Try<network::Response>>&& responses) -> Result {
    auto duration = std::chrono::steady_clock::now() - startTimeReplication;
    auto& replMetrics =
        vocbase().server().getFeature<ReplicationMetricsFeature>();
    replMetrics.synchronousOpsTotal() += 1;
    replMetrics.synchronousTimeTotal() +=
        std::chrono::nanoseconds(duration).count();

    bool didRefuse = false;
    // We drop all followers that were not successful:
    for (size_t i = 0; i < followerList->size(); ++i) {
      network::Response const& resp = responses[i].get();
      ServerID const& follower = (*followerList)[i];

      std::string replicationFailureReason;
      if (resp.error == fuerte::Error::NoError) {
        if (resp.statusCode() == fuerte::StatusAccepted ||
            resp.statusCode() == fuerte::StatusCreated ||
            resp.statusCode() == fuerte::StatusOK) {
          bool found;
          std::string const& errors = resp.response().header.metaByKey(
              StaticStrings::ErrorCodes, found);
          if (found) {
            replicationFailureReason =
                "got error header from follower: " + errors;
          }

        } else {
          auto r = resp.combinedResult();
          bool followerRefused =
              (r.errorNumber() ==
               TRI_ERROR_CLUSTER_SHARD_LEADER_REFUSES_REPLICATION);
          didRefuse = didRefuse || followerRefused;

          replicationFailureReason =
              "got error from follower: " + std::string(r.errorMessage());

          if (followerRefused) {
            ++vocbase()
                  .server()
                  .getFeature<arangodb::ClusterFeature>()
                  .followersRefusedCounter();

            LOG_TOPIC("3032c", WARN, Logger::REPLICATION)
                << "synchronous replication of " << opName << " operation: "
                << "follower " << follower << " for shard "
                << collection->vocbase().name() << "/" << collection->name()
                << " refused the operation: " << r.errorMessage();
          }
        }
      } else {
        replicationFailureReason = "no response from follower";
      }

      TRI_IF_FAILURE("replicateOperationsDropFollower") {
        replicationFailureReason = "intentional debug error";
      }

      if (!replicationFailureReason.empty()) {
        if (!vocbase().server().isStopping()) {
          LOG_TOPIC("12d8c", WARN, Logger::REPLICATION)
              << "synchronous replication of " << opName << " operation "
              << "(" << count << " doc(s)): "
              << "dropping follower " << follower << " for shard "
              << collection->vocbase().name() << "/" << collection->name()
              << ": failure reason: " << replicationFailureReason
              << ", http response code: " << (int)resp.statusCode()
              << ", error message: " << resp.combinedResult().errorMessage();

          Result res = collection->followers()->remove(follower);
          // intentionally do NOT remove the follower from the list of
          // known servers here. if we do, we will not be able to
          // send the commit/abort to the follower later. However, we
          // still need to send the commit/abort to the follower at
          // transaction end, because the follower may be responsbile
          // for _other_ shards as well.
          // it does not matter if we later commit the writes of the shard
          // from which we just removed the follower, because the follower
          // is now dropped and will try to get back in sync anyway, so
          // it will run the full shard synchronization process.
          if (res.fail()) {
            LOG_TOPIC("db473", ERR, Logger::REPLICATION)
                << "synchronous replication of " << opName << " operation: "
                << "could not drop follower " << follower << " for shard "
                << collection->vocbase().name() << "/" << collection->name()
                << ": " << res.errorMessage();

            // Note: it is safe here to exit the loop early. We are losing the
            // leadership here. No matter what happens next, the Current entry
            // in the agency is rewritten and thus replication is restarted from
            // the new leader. There is no need to keep trying to drop followers
            // at this point.

            if (res.is(TRI_ERROR_CLUSTER_NOT_LEADER)) {
              // In this case, we know that we are not or no longer
              // the leader for this shard. Therefore we need to
              // send a code which let's the coordinator retry.
              THROW_ARANGO_EXCEPTION(TRI_ERROR_CLUSTER_SHARD_LEADER_RESIGNED);
            } else {
              // In this case, some other error occurred and we
              // most likely are still the proper leader, so
              // the error needs to be reported and the local
              // transaction must be rolled back.
              THROW_ARANGO_EXCEPTION(TRI_ERROR_CLUSTER_COULD_NOT_DROP_FOLLOWER);
            }
          }
        } else {
          LOG_TOPIC("8921e", INFO, Logger::REPLICATION)
              << "synchronous replication of " << opName << " operation: "
              << "follower " << follower << " for shard "
              << collection->vocbase().name() << "/" << collection->name()
              << " stopped as we're shutting down";
          THROW_ARANGO_EXCEPTION(TRI_ERROR_SHUTTING_DOWN);
        }
      }
    }

    Result res;
    if (didRefuse) {  // case (1), caller may abort this transaction
      res.reset(TRI_ERROR_CLUSTER_SHARD_LEADER_RESIGNED);
    } else {
      // execute a deferred intermediate commit, if required.
      res = performIntermediateCommitIfRequired(collection->id());
    }
    return res;
  };
  return futures::collectAll(std::move(futures)).thenValue(std::move(cb));
}

Future<Result> Methods::commitInternal(MethodsApi api) noexcept try {
  TRI_IF_FAILURE("TransactionCommitFail") { return Result(TRI_ERROR_DEBUG); }

  if (_state == nullptr || _state->status() != transaction::Status::RUNNING) {
    // transaction not created or not running
    return Result(TRI_ERROR_TRANSACTION_INTERNAL,
                  "transaction not running on commit");
  }

  if (!_state->isReadOnlyTransaction()) {
    auto const& exec = ExecContext::current();
    bool cancelRW = ServerState::readOnly() && !exec.isSuperuser();
    if (exec.isCanceled() || cancelRW) {
      return Result(TRI_ERROR_ARANGO_READ_ONLY, "server is in read-only mode");
    }
  }

  auto f = futures::makeFuture(Result());

  if (!_mainTransaction) {
    return f;
  }

  if (_state->isRunningInCluster() &&
      (_state->vocbase().replicationVersion() != replication::Version::TWO ||
       tid().isCoordinatorTransactionId())) {
    // In case we're using replication 2, let the coordinator notify the db
    // servers
    f = ClusterTrxMethods::commitTransaction(*this, api);
  }

  return std::move(f)
      .thenValue([this](Result res) -> futures::Future<Result> {
        if (res.fail()) {  // do not commit locally
          LOG_TOPIC("5743a", WARN, Logger::TRANSACTIONS)
              << "failed to commit on subordinates: '" << res.errorMessage()
              << "'";
          return res;
        }

        return _state->commitTransaction(this);
      })
      .thenValue([this](Result res) -> Result {
        if (res.ok()) {
          res = applyStatusChangeCallbacks(*this, Status::COMMITTED);
        }
        return res;
      });
} catch (basics::Exception const& ex) {
  return Result(ex.code(), ex.message());
} catch (std::exception const& ex) {
  return Result(TRI_ERROR_INTERNAL, ex.what());
} catch (...) {
  return Result(TRI_ERROR_INTERNAL);
}

Future<Result> Methods::abortInternal(MethodsApi api) noexcept try {
  if (_state == nullptr || _state->status() != transaction::Status::RUNNING) {
    // transaction not created or not running
    return Result(TRI_ERROR_TRANSACTION_INTERNAL,
                  "transaction not running on abort");
  }

  auto f = futures::makeFuture(Result());

  if (!_mainTransaction) {
    return f;
  }

  if (_state->isRunningInCluster() &&
      (_state->vocbase().replicationVersion() != replication::Version::TWO ||
       tid().isCoordinatorTransactionId())) {
    // In case we're using replication 2, let the coordinator notify the db
    // servers
    f = ClusterTrxMethods::abortTransaction(*this, api);
  }

  return std::move(f).thenValue([this](Result res) -> Result {
    if (res.fail()) {  // do not commit locally
      LOG_TOPIC("d89a8", WARN, Logger::TRANSACTIONS)
          << "failed to abort on subordinates: " << res.errorMessage();
    }  // abort locally anyway

    res = _state->abortTransaction(this);
    if (res.ok()) {
      res = applyStatusChangeCallbacks(*this, Status::ABORTED);
    }

    return res;
  });
} catch (basics::Exception const& ex) {
  return Result(ex.code(), ex.message());
} catch (std::exception const& ex) {
  return Result(TRI_ERROR_INTERNAL, ex.what());
} catch (...) {
  return Result(TRI_ERROR_INTERNAL);
}

Future<Result> Methods::finishInternal(Result const& res,
                                       MethodsApi api) noexcept try {
  if (res.ok()) {
    // there was no previous error, so we'll commit
    return this->commitInternal(api);
  }

  // there was a previous error, so we'll abort
  return this->abortInternal(api).thenValue([res](Result const& ignore) {
    return res;  // return original error
  });
} catch (basics::Exception const& ex) {
  return Result(ex.code(), ex.message());
} catch (std::exception const& ex) {
  return Result(TRI_ERROR_INTERNAL, ex.what());
} catch (...) {
  return Result(TRI_ERROR_INTERNAL);
}

Future<OperationResult> Methods::documentInternal(
    std::string const& collectionName, VPackSlice value,
    OperationOptions const& options, MethodsApi api) {
  TRI_ASSERT(_state->status() == transaction::Status::RUNNING);

  if (!value.isObject() && !value.isArray()) {
    // must provide a document object or an array of documents
    events::ReadDocument(vocbase().name(), collectionName, value, options,
                         TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
  }

  if (_state->isCoordinator()) {
    return addTracking(documentCoordinator(collectionName, value, options, api),
                       [=, this](OperationResult&& opRes) {
                         events::ReadDocument(vocbase().name(), collectionName,
                                              value, opRes.options,
                                              opRes.errorNumber());
                         return std::move(opRes);
                       });
  }
  return documentLocal(collectionName, value, options);
}

Future<OperationResult> Methods::insertInternal(
    std::string const& collectionName, VPackSlice value,
    OperationOptions const& options, MethodsApi api) {
  TRI_ASSERT(_state->status() == transaction::Status::RUNNING);

  if (!value.isObject() && !value.isArray()) {
    // must provide a document object or an array of documents
    events::CreateDocument(vocbase().name(), collectionName, value, options,
                           TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
  }
  if (value.isArray() && value.length() == 0) {
    events::CreateDocument(vocbase().name(), collectionName, value, options,
                           TRI_ERROR_NO_ERROR);
    return emptyResult(options);
  }

  auto f = Future<OperationResult>::makeEmpty();
  if (_state->isCoordinator()) {
    f = insertCoordinator(collectionName, value, options, api);
  } else {
    OperationOptions optionsCopy = options;
    f = insertLocal(collectionName, value, optionsCopy);
  }

  return addTracking(std::move(f), [=, this](OperationResult&& opRes) {
    events::CreateDocument(
        vocbase().name(), collectionName,
        (opRes.ok() && opRes.options.returnNew) ? opRes.slice() : value,
        opRes.options, opRes.errorNumber());
    return std::move(opRes);
  });
}

Future<OperationResult> Methods::updateInternal(
    std::string const& collectionName, VPackSlice newValue,
    OperationOptions const& options, MethodsApi api) {
  TRI_ASSERT(_state->status() == transaction::Status::RUNNING);

  if (!newValue.isObject() && !newValue.isArray()) {
    // must provide a document object or an array of documents
    events::ModifyDocument(vocbase().name(), collectionName, newValue, options,
                           TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
  }
  if (newValue.isArray() && newValue.length() == 0) {
    events::ModifyDocument(vocbase().name(), collectionName, newValue, options,
                           TRI_ERROR_NO_ERROR);
    return emptyResult(options);
  }

  auto f = Future<OperationResult>::makeEmpty();
  if (_state->isCoordinator()) {
    f = modifyCoordinator(collectionName, newValue, options,
                          TRI_VOC_DOCUMENT_OPERATION_UPDATE, api);
  } else {
    OperationOptions optionsCopy = options;
    f = modifyLocal(collectionName, newValue, optionsCopy, /*isUpdate*/ true);
  }
  return addTracking(std::move(f), [=, this](OperationResult&& opRes) {
    events::ModifyDocument(vocbase().name(), collectionName, newValue,
                           opRes.options, opRes.errorNumber());
    return std::move(opRes);
  });
}

Future<OperationResult> Methods::replaceInternal(
    std::string const& collectionName, VPackSlice newValue,
    OperationOptions const& options, MethodsApi api) {
  TRI_ASSERT(_state->status() == transaction::Status::RUNNING);

  if (!newValue.isObject() && !newValue.isArray()) {
    // must provide a document object or an array of documents
    events::ReplaceDocument(vocbase().name(), collectionName, newValue, options,
                            TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
  }
  if (newValue.isArray() && newValue.length() == 0) {
    events::ReplaceDocument(vocbase().name(), collectionName, newValue, options,
                            TRI_ERROR_NO_ERROR);
    return futures::makeFuture(emptyResult(options));
  }

  auto f = Future<OperationResult>::makeEmpty();
  if (_state->isCoordinator()) {
    f = modifyCoordinator(collectionName, newValue, options,
                          TRI_VOC_DOCUMENT_OPERATION_REPLACE, api);
  } else {
    OperationOptions optionsCopy = options;
    f = modifyLocal(collectionName, newValue, optionsCopy, /*isUpdate*/ false);
  }
  return addTracking(std::move(f), [=, this](OperationResult&& opRes) {
    events::ReplaceDocument(vocbase().name(), collectionName, newValue,
                            opRes.options, opRes.errorNumber());
    return std::move(opRes);
  });
}

Future<OperationResult> Methods::removeInternal(
    std::string const& collectionName, VPackSlice value,
    OperationOptions const& options, MethodsApi api) {
  TRI_ASSERT(_state->status() == transaction::Status::RUNNING);

  if (!value.isObject() && !value.isArray() && !value.isString()) {
    // must provide a document object or an array of documents
    events::DeleteDocument(vocbase().name(), collectionName, value, options,
                           TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
  }
  if (value.isArray() && value.length() == 0) {
    events::DeleteDocument(vocbase().name(), collectionName, value, options,
                           TRI_ERROR_NO_ERROR);
    return emptyResult(options);
  }

  auto f = Future<OperationResult>::makeEmpty();
  if (_state->isCoordinator()) {
    f = removeCoordinator(collectionName, value, options, api);
  } else {
    OperationOptions optionsCopy = options;
    f = removeLocal(collectionName, value, optionsCopy);
  }
  return addTracking(std::move(f), [=, this](OperationResult&& opRes) {
    events::DeleteDocument(vocbase().name(), collectionName, value,
                           opRes.options, opRes.errorNumber());
    return std::move(opRes);
  });
}

Future<OperationResult> Methods::truncateInternal(
    std::string const& collectionName, OperationOptions const& options,
    MethodsApi api) {
  TRI_ASSERT(_state->status() == transaction::Status::RUNNING);

  OperationOptions optionsCopy = options;
  auto cb = [this, collectionName](OperationResult res) {
    events::TruncateCollection(vocbase().name(), collectionName, res);
    return res;
  };

  if (_state->isCoordinator()) {
    return truncateCoordinator(collectionName, optionsCopy, api).thenValue(cb);
  }
  return truncateLocal(collectionName, optionsCopy).thenValue(cb);
}

futures::Future<OperationResult> Methods::countInternal(
    std::string const& collectionName, CountType type,
    OperationOptions const& options, MethodsApi api) {
  TRI_ASSERT(_state->status() == transaction::Status::RUNNING);

  if (_state->isCoordinator()) {
    return countCoordinator(collectionName, type, options, api);
  }

  if (type == CountType::Detailed) {
    // we are a single-server... we cannot provide detailed per-shard counts,
    // so just downgrade the request to a normal request
    type = CountType::Normal;
  }

  return futures::makeFuture(countLocal(collectionName, type, options));
}

// perform a (deferred) intermediate commit if required
Result Methods::performIntermediateCommitIfRequired(DataSourceId collectionId) {
  return _state->performIntermediateCommitIfRequired(collectionId);
}

#ifndef USE_ENTERPRISE
ErrorCode Methods::validateSmartJoinAttribute(LogicalCollection const&,
                                              velocypack::Slice) {
  return TRI_ERROR_NO_ERROR;
}
#endif

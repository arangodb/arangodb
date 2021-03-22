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
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

#include <velocypack/Builder.h>
#include <velocypack/Collection.h>
#include <velocypack/Options.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

#include "Methods.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Aql/Ast.h"
#include "Aql/AstNode.h"
#include "Aql/Condition.h"
#include "Aql/SortCondition.h"
#include "Basics/AttributeNameParser.h"
#include "Basics/Exceptions.h"
#include "Basics/NumberUtils.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/encoding.h"
#include "Basics/system-compiler.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterMethods.h"
#include "Cluster/ClusterTrxMethods.h"
#include "Cluster/FollowerInfo.h"
#include "Cluster/ReplicationTimeoutFeature.h"
#include "Cluster/ServerState.h"
#include "ClusterEngine/ClusterEngine.h"
#include "Containers/SmallVector.h"
#include "Futures/Utilities.h"
#include "GeneralServer/RestHandler.h"
#include "Indexes/Index.h"
#include "Logger/Logger.h"
#include "Network/Methods.h"
#include "Network/NetworkFeature.h"
#include "Network/Utils.h"
#include "Replication/ReplicationMetricsFeature.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/PhysicalCollection.h"
#include "StorageEngine/StorageEngine.h"
#include "StorageEngine/TransactionCollection.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/Context.h"
#include "Transaction/Helpers.h"
#include "Transaction/Options.h"
#include "Utils/CollectionNameResolver.h"
#include "Utils/Events.h"
#include "Utils/ExecContext.h"
#include "Utils/OperationOptions.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ManagedDocumentResult.h"
#include "VocBase/Methods/Indexes.h"
#include "VocBase/ticks.h"

#include <sstream>

using namespace arangodb;
using namespace arangodb::transaction;
using namespace arangodb::transaction::helpers;

template<typename T>
using Future = futures::Future<T>;

namespace {

enum class ReplicationType { NONE, LEADER, FOLLOWER };

Result buildRefusalResult(LogicalCollection const& collection, char const* operation,
                          OperationOptions const& options, std::string const& leader) {
  std::stringstream msg;
  msg << TRI_errno_string(TRI_ERROR_CLUSTER_SHARD_FOLLOWER_REFUSES_OPERATION)
      << ": shard: " << collection.vocbase().name() << "/" << collection.name()
      << ", operation: " << operation
      << ", from: " << options.isSynchronousReplicationFrom
      << ", current leader: " << leader;
  return Result(TRI_ERROR_CLUSTER_SHARD_FOLLOWER_REFUSES_OPERATION, msg.str());
}

// wrap vector inside a static function to ensure proper initialization order
std::vector<arangodb::transaction::Methods::DataSourceRegistrationCallback>& getDataSourceRegistrationCallbacks() {
  static std::vector<arangodb::transaction::Methods::DataSourceRegistrationCallback> callbacks;

  return callbacks;
}

/// @return the status change callbacks stored in state
///         or nullptr if none and !create
std::vector<arangodb::transaction::Methods::StatusChangeCallback const*>* getStatusChangeCallbacks(
    arangodb::TransactionState& state, bool create = false) {
  struct CookieType : public arangodb::TransactionState::Cookie {
    std::vector<arangodb::transaction::Methods::StatusChangeCallback const*> _callbacks;
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
arangodb::Result applyDataSourceRegistrationCallbacks(LogicalDataSource& dataSource,
                                                      arangodb::transaction::Methods& trx) {
  for (auto& callback : getDataSourceRegistrationCallbacks()) {
    TRI_ASSERT(callback);  // addDataSourceRegistrationCallback(...) ensures valid

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
void applyStatusChangeCallbacks(arangodb::transaction::Methods& trx,
                                arangodb::transaction::Status status) noexcept {
  TRI_ASSERT(arangodb::transaction::Status::ABORTED == status ||
             arangodb::transaction::Status::COMMITTED == status ||
             arangodb::transaction::Status::RUNNING == status);
//  TRI_ASSERT(!trx.state()  // for embeded transactions status is not always updated
//             || (trx.state()->isTopLevelTransaction() && trx.state()->status() == status) ||
//             (!trx.state()->isTopLevelTransaction() &&
//              arangodb::transaction::Status::RUNNING == trx.state()->status()));
  TRI_ASSERT(trx.isMainTransaction());

  auto* state = trx.state();

  if (!state) {
    return;  // nothing to apply
  }

  auto* callbacks = getStatusChangeCallbacks(*state);

  if (!callbacks) {
    return;  // no callbacks to apply
  }

  // no need to lock since transactions are single-threaded
  for (auto& callback : *callbacks) {
    TRI_ASSERT(callback);  // addStatusChangeCallback(...) ensures valid

    try {
      (*callback)(trx, status);
    } catch (...) {
      // we must not propagate exceptions from here
    }
  }
}

static void throwCollectionNotFound(char const* name) {
  if (name == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
  }
  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
                                 std::string(TRI_errno_string(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND)) +
                                     ": " + name);
}

/// @brief Insert an error reported instead of the new document
static void createBabiesError(VPackBuilder& builder,
                              std::unordered_map<ErrorCode, size_t>& countErrorCodes,
                              Result const& error) {
  builder.openObject();
  builder.add(StaticStrings::Error, VPackValue(true));
  builder.add(StaticStrings::ErrorNum, VPackValue(error.errorNumber()));
  builder.add(StaticStrings::ErrorMessage, VPackValue(error.errorMessage()));
  builder.close();

  auto it = countErrorCodes.find(error.errorNumber());
  if (it == countErrorCodes.end()) {
    countErrorCodes.emplace(error.errorNumber(), 1);
  } else {
    it->second++;
  }
}

static OperationResult emptyResult(OperationOptions const& options) {
  VPackBuilder resultBuilder;
  resultBuilder.openArray();
  resultBuilder.close();
  return OperationResult(Result(), resultBuilder.steal(), options);
}
}  // namespace

/*static*/ void transaction::Methods::addDataSourceRegistrationCallback(
    DataSourceRegistrationCallback const& callback) {
  if (callback) {
    getDataSourceRegistrationCallbacks().emplace_back(callback);
  }
}

bool transaction::Methods::addStatusChangeCallback(StatusChangeCallback const* callback) {
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

bool transaction::Methods::removeStatusChangeCallback(StatusChangeCallback const* callback) {
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
static bool findRefusal(std::vector<futures::Try<network::Response>> const& responses) {
  for (auto const& it : responses) {
    if (it.hasValue() && it.get().ok() &&
        it.get().statusCode() == fuerte::StatusNotAcceptable) {
      auto r = it.get().combinedResult();
      bool followerRefused = (r.errorNumber() == TRI_ERROR_CLUSTER_SHARD_LEADER_REFUSES_REPLICATION);
      if (followerRefused) {
        return true;
      }
    }
  }
  return false;
}

transaction::Methods::Methods(std::shared_ptr<transaction::Context> const& transactionContext,
                              transaction::Options const& options)
    : _state(nullptr),
      _transactionContext(transactionContext),
      _mainTransaction(false) {

  TRI_ASSERT(transactionContext != nullptr);
  if (ADB_UNLIKELY(transactionContext == nullptr)) {
    // in production, we must not go on with undefined behavior, so we bail out
    // here with an exception as last resort
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "invalid transaction context pointer");
  }

  // initialize the transaction
  _state = _transactionContext->acquireState(options, _mainTransaction);
  TRI_ASSERT(_state != nullptr);
}

/// @brief create the transaction, used to be UserTransaction
transaction::Methods::Methods(std::shared_ptr<transaction::Context> const& ctx,
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

    if (_state->status() == transaction::Status::RUNNING) {
      // auto abort a running transaction
      try {
        this->abort();
        TRI_ASSERT(_state->status() != transaction::Status::RUNNING);
      } catch (...) {
        // must never throw because we are in a dtor
      }
    }

    // free the state associated with the transaction
    TRI_ASSERT(_state->status() != transaction::Status::RUNNING);

    // store result in context
    _transactionContext->storeTransactionResult(_state->id(),
                                                _state->wasRegistered(),
                                                _state->isReadOnlyTransaction(),
                                                _state->isFollowerTransaction());

    _state = nullptr;
  }
}

/// @brief return the collection name resolver
CollectionNameResolver const* transaction::Methods::resolver() const {
  return &(_transactionContext->resolver());
}

/// @brief return the transaction collection for a document collection
TransactionCollection* transaction::Methods::trxCollection(DataSourceId cid,
                                                           AccessMode::Type type) const {
  TRI_ASSERT(_state != nullptr);
  TRI_ASSERT(_state->status() == transaction::Status::RUNNING ||
             _state->status() == transaction::Status::CREATED);
  return _state->collection(cid, type);
}

/// @brief return the transaction collection for a document collection
TransactionCollection* transaction::Methods::trxCollection(std::string const& name,
                                                           AccessMode::Type type) const {
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
    LogicalCollection* collection, VPackBuilder& builder, DataSourceId cid,
    arangodb::velocypack::StringRef const& key, RevisionId rid, RevisionId oldRid,
    ManagedDocumentResult const* oldDoc, ManagedDocumentResult const* newDoc) {
  StringLeaser leased(_transactionContext.get());
  std::string& temp(*leased.get());
  temp.reserve(64);

  if (_state->isRunningInCluster()) {
    std::string resolved = resolver()->getCollectionNameCluster(cid);
#ifdef USE_ENTERPRISE
    if (resolved.compare(0, 7, "_local_") == 0) {
      resolved.erase(0, 7);
    } else if (resolved.compare(0, 6, "_from_") == 0) {
      resolved.erase(0, 6);
    } else if (resolved.compare(0, 4, "_to_") == 0) {
      resolved.erase(0, 4);
    }
#endif
    // build collection name
    temp.append(resolved);
  } else {
    // build collection name
    temp.append(collection->name());
  }

  // append / and key part
  temp.push_back('/');
  temp.append(key.data(), key.size());

  builder.openObject();
  builder.add(StaticStrings::IdString, VPackValue(temp));

  builder.add(StaticStrings::KeyString,
              VPackValuePair(key.data(), key.length(), VPackValueType::String));

  char ridBuffer[arangodb::basics::maxUInt64StringSize];
  builder.add(StaticStrings::RevString, rid.toValuePair(ridBuffer));

  if (oldRid.isSet()) {
    builder.add("_oldRev", VPackValue(oldRid.toString()));
  }
  if (oldDoc != nullptr) {
    builder.add(VPackValue(StaticStrings::Old));
    oldDoc->addToBuilder(builder);
  }
  if (newDoc != nullptr) {
    builder.add(VPackValue(StaticStrings::New));
    newDoc->addToBuilder(builder);
  }
  builder.close();
}

/// @brief begin the transaction
Result transaction::Methods::begin() {
  if (_state == nullptr) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "invalid transaction state");
  }

  if (_mainTransaction) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    bool a = _localHints.has(transaction::Hints::Hint::FROM_TOPLEVEL_AQL);
    bool b = _localHints.has(transaction::Hints::Hint::GLOBAL_MANAGED);
    TRI_ASSERT(!(a && b));
#endif

    auto res = _state->beginTransaction(_localHints);
    if (res.fail()) {
      return res;
    }

    applyStatusChangeCallbacks(*this, Status::RUNNING);
  } else {
    TRI_ASSERT(_state->status() == transaction::Status::RUNNING);
  }

  return Result();
}

/// @brief commit / finish the transaction
Future<Result> transaction::Methods::commitAsync() {
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

  if (!_mainTransaction) {
    return futures::makeFuture(Result());
  }

  auto f = futures::makeFuture(Result());
  if (_state->isRunningInCluster()) {
    // first commit transaction on subordinate servers
    f = ClusterTrxMethods::commitTransaction(*this);
  }

  return std::move(f).thenValue([this](Result res) -> Result {
    if (res.fail()) {  // do not commit locally
      LOG_TOPIC("5743a", WARN, Logger::TRANSACTIONS)
          << "failed to commit on subordinates: '" << res.errorMessage() << "'";
      return res;
    }

    res = _state->commitTransaction(this);
    if (res.ok()) {
      applyStatusChangeCallbacks(*this, Status::COMMITTED);
    }

    return res;
  });
}

/// @brief abort the transaction
Future<Result> transaction::Methods::abortAsync() {
  if (_state == nullptr || _state->status() != transaction::Status::RUNNING) {
    // transaction not created or not running
    return Result(TRI_ERROR_TRANSACTION_INTERNAL,
                  "transaction not running on abort");
  }

  if (!_mainTransaction) {
    return futures::makeFuture(Result());
  }

  auto f = futures::makeFuture(Result());
  if (_state->isRunningInCluster()) {
    // first commit transaction on subordinate servers
    f = ClusterTrxMethods::abortTransaction(*this);
  }

  return std::move(f).thenValue([this](Result res) -> Result {
    if (res.fail()) {  // do not commit locally
      LOG_TOPIC("d89a8", WARN, Logger::TRANSACTIONS)
          << "failed to abort on subordinates: " << res.errorMessage();
    }  // abort locally anyway

    res = _state->abortTransaction(this);
    if (res.ok()) {
      applyStatusChangeCallbacks(*this, Status::ABORTED);
    }

    return res;
  });
}

/// @brief finish a transaction (commit or abort), based on the previous state
Future<Result> transaction::Methods::finishAsync(Result const& res) {
  if (res.ok()) {
    // there was no previous error, so we'll commit
    return this->commitAsync();
  }

  // there was a previous error, so we'll abort
  return this->abortAsync().thenValue([res](Result ignore) {
    return res;  // return original error
  });
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
OperationResult transaction::Methods::anyLocal(std::string const& collectionName,
                                               OperationOptions const& options) {

  DataSourceId cid = addCollectionAtRuntime(collectionName, AccessMode::Type::READ);
  TransactionCollection* trxColl = trxCollection(cid);
  if (trxColl == nullptr) {
    throwCollectionNotFound(collectionName.c_str());
  }

  VPackBuilder resultBuilder;
  if (_state->isDBServer()) {
    std::shared_ptr<LogicalCollection> const& collection = trxCollection(cid)->collection();
    auto const& followerInfo = collection->followers();
    if (!followerInfo->getLeader().empty()) {
      return OperationResult(TRI_ERROR_CLUSTER_SHARD_FOLLOWER_REFUSES_OPERATION, options);
    }
  }

  resultBuilder.openArray();

  auto iterator = indexScan(collectionName, transaction::Methods::CursorType::ANY);

  iterator->nextDocument(
      [&resultBuilder](LocalDocumentId const& /*token*/, VPackSlice slice) {
        resultBuilder.add(slice);
        return true;
      }, 1);

  resultBuilder.close();

  return OperationResult(Result(), resultBuilder.steal(), options);
}

DataSourceId transaction::Methods::addCollectionAtRuntime(DataSourceId cid,
                                                          std::string const& cname,
                                                          AccessMode::Type type) {
  auto collection = trxCollection(cid);

  if (collection == nullptr) {
    Result res = _state->addCollection(cid, cname, type, /*lockUsage*/true);

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
      throwCollectionNotFound(cname.c_str());
    }

  } else {
    AccessMode::Type collectionAccessType = collection->accessType();
    if (AccessMode::isRead(collectionAccessType) && !AccessMode::isRead(type)) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_TRANSACTION_UNREGISTERED_COLLECTION,
                                     std::string(TRI_errno_string(TRI_ERROR_TRANSACTION_UNREGISTERED_COLLECTION)) + ": " + cname +
                                     " [" + AccessMode::typeString(type) + "]");
    }
  }

  TRI_ASSERT(collection != nullptr);
  return cid;
}

/// @brief add a collection to the transaction for read, at runtime
DataSourceId transaction::Methods::addCollectionAtRuntime(std::string const& collectionName,
                                                          AccessMode::Type type) {
  if (collectionName == _collectionCache.name && !collectionName.empty()) {
    return _collectionCache.cid;
  }

  TRI_ASSERT(!_state->isCoordinator());
  auto cid = resolver()->getCollectionIdLocal(collectionName);

  if (cid.empty()) {
    throwCollectionNotFound(collectionName.c_str());
  }
  addCollectionAtRuntime(cid, collectionName, type);
  _collectionCache.cid = cid;
  _collectionCache.name = collectionName;
  return cid;
}

/// @brief return the type of a collection
bool transaction::Methods::isEdgeCollection(std::string const& collectionName) const {
  return getCollectionType(collectionName) == TRI_COL_TYPE_EDGE;
}

/// @brief return the type of a collection
bool transaction::Methods::isDocumentCollection(std::string const& collectionName) const {
  return getCollectionType(collectionName) == TRI_COL_TYPE_DOCUMENT;
}

/// @brief return the type of a collection
TRI_col_type_e transaction::Methods::getCollectionType(std::string const& collectionName) const {
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
                                              VPackBuilder& result) {
  TRI_ASSERT(_state->status() == transaction::Status::RUNNING);
  if (!value.isObject() && !value.isString()) {
    // must provide a document object or string
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
  }

  if (_state->isCoordinator()) {
    OperationOptions options;
    OperationResult opRes = documentCoordinator(collectionName, value, options).get();
    if (!opRes.fail()) {
      result.add(opRes.slice());
    }
    return opRes.result;
  }

  auto translateName = [this](std::string const& collectionName) { 
    if (_state->isDBServer()) {
      auto collection = resolver()->getCollectionStructCluster(collectionName);
      if (collection != nullptr) {
        auto& ci = vocbase().server().getFeature<ClusterFeature>().clusterInfo();
        auto shards = ci.getShardList(std::to_string(collection->id().id()));
        if (shards != nullptr && shards->size() == 1) {
          TRI_ASSERT(vocbase().isOneShard());
          return (*shards)[0];
        }
      }
    }
    return collectionName;
  };

  DataSourceId cid = addCollectionAtRuntime(translateName(collectionName), AccessMode::Type::READ);
  auto const& collection = trxCollection(cid)->collection();

  arangodb::velocypack::StringRef key(transaction::helpers::extractKeyPart(value));
  if (key.empty()) {
    return Result(TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD);
  }

  return collection->getPhysical()->read(this, key, [&](LocalDocumentId const&, VPackSlice const& doc) {
    result.add(doc);
    return true;
  });
}

/// @brief return one document from a collection, fast path
///        If everything went well the result will contain the found document
///        (as an external on single_server) and this function will return
///        TRI_ERROR_NO_ERROR.
///        If there was an error the code is returned
///        Does not care for revision handling!
///        Must only be called on a local server, not in cluster case!
Result transaction::Methods::documentFastPathLocal(std::string const& collectionName,
                                                   arangodb::velocypack::StringRef const& key,
                                                   IndexIterator::DocumentCallback const& cb) {
  TRI_ASSERT(!ServerState::instance()->isCoordinator());
  TRI_ASSERT(_state->status() == transaction::Status::RUNNING);

  DataSourceId cid = addCollectionAtRuntime(collectionName, AccessMode::Type::READ);
  TransactionCollection* trxColl = trxCollection(cid);
  TRI_ASSERT(trxColl != nullptr);
  std::shared_ptr<LogicalCollection> const& collection = trxColl->collection();
  TRI_ASSERT(collection != nullptr);

  if (key.empty()) {
    return TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD;
  }

  return collection->getPhysical()->read(this, key, cb);
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
}

/// @brief return one or multiple documents from a collection
Future<OperationResult> transaction::Methods::documentAsync(std::string const& cname,
                                                            VPackSlice value,
                                                            OperationOptions& options) {
  TRI_ASSERT(_state->status() == transaction::Status::RUNNING);

  if (!value.isObject() && !value.isArray()) {
    // must provide a document object or an array of documents
    events::ReadDocument(vocbase().name(), cname, value, options,
                         TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
  }

  if (_state->isCoordinator()) {
    return addTracking(documentCoordinator(cname, value, options),
                       [=](OperationResult&& opRes) {
      events::ReadDocument(vocbase().name(), cname, value, opRes.options, opRes.errorNumber());
      return std::move(opRes);
    });
  }
  return documentLocal(cname, value, options);
}

/// @brief read one or multiple documents in a collection, coordinator
#ifndef USE_ENTERPRISE
Future<OperationResult> transaction::Methods::documentCoordinator(
    std::string const& collectionName, VPackSlice value, OperationOptions& options) {
  if (!value.isArray()) {
    arangodb::velocypack::StringRef key(transaction::helpers::extractKeyPart(value));
    if (key.empty()) {
      return OperationResult(TRI_ERROR_ARANGO_DOCUMENT_KEY_BAD, options);
    }
  }

  auto colptr = resolver()->getCollectionStructCluster(collectionName);
  if (colptr == nullptr) {
    return futures::makeFuture(OperationResult(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, options));
  }

  return arangodb::getDocumentOnCoordinator(*this, *colptr, value, options);
}
#endif

/// @brief read one or multiple documents in a collection, local
Future<OperationResult> transaction::Methods::documentLocal(std::string const& collectionName,
                                                            VPackSlice value,
                                                            OperationOptions& options) {
  DataSourceId cid = addCollectionAtRuntime(collectionName, AccessMode::Type::READ);
  std::shared_ptr<LogicalCollection> const& collection = trxCollection(cid)->collection();

  VPackBuilder resultBuilder;
  Result res;

  if (_state->isDBServer()) {
    auto const& followerInfo = collection->followers();
    if (!followerInfo->getLeader().empty()) {
      return futures::makeFuture(
        OperationResult(TRI_ERROR_CLUSTER_SHARD_FOLLOWER_REFUSES_OPERATION, options));
    }
  }

  auto workForOneDocument = [&](VPackSlice value, bool isMultiple) -> Result {
    Result res;

    arangodb::velocypack::StringRef key(transaction::helpers::extractKeyPart(value));
    if (key.empty()) {
      res.reset(TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD);
    } else {
      bool conflict = false;
      res = collection->getPhysical()->read(this, key, [&](LocalDocumentId const&, VPackSlice const& doc) {
        if (!options.ignoreRevs && value.isObject()) {
          RevisionId expectedRevision = RevisionId::fromSlice(value);
          if (expectedRevision.isSet()) {
            RevisionId foundRevision =
                transaction::helpers::extractRevFromDocument(doc);
            if (expectedRevision != foundRevision) {
              if (!isMultiple) {
                // still return
                buildDocumentIdentity(collection.get(), resultBuilder, cid, key,
                                      foundRevision, RevisionId::none(), nullptr, nullptr);
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
      });

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
        createBabiesError(resultBuilder, countErrorCodes, res);
      }
    }
    res.reset(); // With babies the reporting is handled somewhere else.
  }

  events::ReadDocument(vocbase().name(), collectionName, value, options,
                       res.errorNumber());

  return futures::makeFuture(OperationResult(std::move(res), resultBuilder.steal(),
                                             options, countErrorCodes));
}

/// @brief create one or multiple documents in a collection
/// the single-document variant of this operation will either succeed or,
/// if it fails, clean up after itself
Future<OperationResult> transaction::Methods::insertAsync(std::string const& cname,
                                                          VPackSlice value,
                                                          OperationOptions const& options) {
  TRI_ASSERT(_state->status() == transaction::Status::RUNNING);

  if (!value.isObject() && !value.isArray()) {
    // must provide a document object or an array of documents
    events::CreateDocument(vocbase().name(), cname, value, options,
                           TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
  }
  if (value.isArray() && value.length() == 0) {
    events::CreateDocument(vocbase().name(), cname, value, options, TRI_ERROR_NO_ERROR);
    return emptyResult(options);
  }

  auto f = Future<OperationResult>::makeEmpty();
  if (_state->isCoordinator()) {
    f = insertCoordinator(cname, value, options);
  } else {
    OperationOptions optionsCopy = options;
    f = insertLocal(cname, value, optionsCopy);
  }

  return addTracking(std::move(f), [=](OperationResult&& opRes) {
    events::CreateDocument(vocbase().name(), cname,
                           (opRes.ok() && opRes.options.returnNew) ? opRes.slice() : value,
                           opRes.options, opRes.errorNumber());
    return std::move(opRes);
  });
}

/// @brief create one or multiple documents in a collection, coordinator
/// the single-document variant of this operation will either succeed or,
/// if it fails, clean up after itself
#ifndef USE_ENTERPRISE
Future<OperationResult> transaction::Methods::insertCoordinator(std::string const& collectionName,
                                                                VPackSlice value,
                                                                OperationOptions const& options) {
  auto colptr = resolver()->getCollectionStructCluster(collectionName);
  if (colptr == nullptr) {
    return futures::makeFuture(OperationResult(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, options));
  }
  return arangodb::createDocumentOnCoordinator(*this, *colptr, value, options);
}
#endif

/// @brief choose a timeout for synchronous replication, based on the
/// number of documents we ship over
static double chooseTimeoutForReplication(size_t count, size_t totalBytes) {
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
  timeout += (totalBytes / 4096.0) * ReplicationTimeoutFeature::timeoutPer4k;

  return std::clamp(timeout, ReplicationTimeoutFeature::lowerLimit,
                    ReplicationTimeoutFeature::upperLimit) *
         ReplicationTimeoutFeature::timeoutFactor;
}

/// @brief create one or multiple documents in a collection, local
/// the single-document variant of this operation will either succeed or,
/// if it fails, clean up after itself
Future<OperationResult> transaction::Methods::insertLocal(std::string const& cname,
                                                          VPackSlice value,
                                                          OperationOptions& options) {
  DataSourceId cid = addCollectionAtRuntime(cname, AccessMode::Type::WRITE);
  std::shared_ptr<LogicalCollection> const& collection = trxCollection(cid)->collection();

  std::shared_ptr<std::vector<ServerID> const> followers;

  ReplicationType replicationType = ReplicationType::NONE;
  if (_state->isDBServer()) {
    TRI_ASSERT(followers == nullptr);
    followers = collection->followers()->get();

    // Block operation early if we are not supposed to perform it:
    auto const& followerInfo = collection->followers();
    std::string theLeader = followerInfo->getLeader();
    if (theLeader.empty()) {
      // This indicates that we believe to be the leader.
      if (!options.isSynchronousReplicationFrom.empty()) {
        return OperationResult(TRI_ERROR_CLUSTER_SHARD_LEADER_REFUSES_REPLICATION, options);
      }
      if (!followerInfo->allowedToWrite()) {
        // We cannot fulfill minimum replication Factor.
        // Reject write.
        return OperationResult(TRI_ERROR_ARANGO_READ_ONLY, options);
      }

      replicationType = ReplicationType::LEADER;
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
        return OperationResult(TRI_ERROR_CLUSTER_SHARD_LEADER_RESIGNED, options);
      }
      if (options.isSynchronousReplicationFrom != theLeader) {
        return OperationResult(
            ::buildRefusalResult(*collection, "insert", options, theLeader),
            options);
      }
    }
  }  // isDBServer - early block

  VPackBuilder resultBuilder;
  ManagedDocumentResult docResult;
  ManagedDocumentResult prevDocResult;  // return OLD (with override option)

  auto workForOneDocument = [&](VPackSlice const value, bool isBabies) -> Result {
    if (!value.isObject()) {
      return Result(TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
    }

    docResult.clear();
    prevDocResult.clear();

    LocalDocumentId oldDocumentId;
    RevisionId oldRevisionId = RevisionId::none();
    VPackSlice key;

    Result res;

    if (options.isOverwriteModeSet() &&
        options.overwriteMode != OperationOptions::OverwriteMode::Conflict) {
      key = value.get(StaticStrings::KeyString);
      if (key.isString()) {
        std::pair<LocalDocumentId, RevisionId> lookupResult;
        res = collection->getPhysical()->lookupKey(this, key.stringRef(), lookupResult);
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

    bool didReplace = false;
    if (!isPrimaryKeyConstraintViolation) {
      // regular insert without overwrite option. the insert itself will check if the
      // primary key already exists
      res = collection->insert(this, value, docResult, options);
    } else {
      // RepSert Case - unique_constraint violated ->  try update, replace or ignore!
      TRI_ASSERT(options.isOverwriteModeSet());
      TRI_ASSERT(options.overwriteMode != OperationOptions::OverwriteMode::Conflict);

      if (options.overwriteMode == OperationOptions::OverwriteMode::Ignore) {
        // in case of unique constraint violation: ignore and do nothing (no write!)
        buildDocumentIdentity(collection.get(), resultBuilder, cid, key.stringRef(),
                              oldRevisionId, RevisionId::none(), nullptr, nullptr);
        return res;
      }
      if (options.overwriteMode == OperationOptions::OverwriteMode::Update) {
        // in case of unique constraint violation: (partially) update existing document
        res = collection->update(this, value, docResult, options, prevDocResult);
      } else if (options.overwriteMode == OperationOptions::OverwriteMode::Replace) {
        // in case of unique constraint violation: replace existing document
        // this is also the default behavior
        res = collection->replace(this, value, docResult, options, prevDocResult);
      } else {
        TRI_ASSERT(false);
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "internal overwriteMode state");
      }

      TRI_ASSERT(res.fail() || prevDocResult.revisionId().isSet());
      didReplace = true;
    }

    if (res.fail()) {
      // Error reporting in the babies case is done outside of here,
      if (res.is(TRI_ERROR_ARANGO_CONFLICT) && !isBabies &&
          prevDocResult.revisionId().isSet()) {
        TRI_ASSERT(didReplace);

        buildDocumentIdentity(collection.get(), resultBuilder, cid,
                              value.get(StaticStrings::KeyString).stringRef(),
                              prevDocResult.revisionId(), RevisionId::none(),
                              nullptr, nullptr);
      }
      return res;
    }

    if (!options.silent) {
      bool const showReplaced = (options.returnOld && didReplace);
      TRI_ASSERT(!options.returnNew || !docResult.empty());
      TRI_ASSERT(!showReplaced || !prevDocResult.empty());

      arangodb::velocypack::StringRef keyString;
      if (didReplace) {  // docResult may be empty, but replace requires '_key' in value
        keyString = value.get(StaticStrings::KeyString);
        TRI_ASSERT(!keyString.empty());
      } else {
        keyString =
            transaction::helpers::extractKeyFromDocument(VPackSlice(docResult.vpack()));
      }

      buildDocumentIdentity(collection.get(), resultBuilder, cid, keyString,
                            docResult.revisionId(), prevDocResult.revisionId(),
                            showReplaced ? &prevDocResult : nullptr,
                            options.returnNew ? &docResult : nullptr);
    }
    return Result();
  };

  Result res;
  std::unordered_map<ErrorCode, size_t> errorCounter;
  if (value.isArray()) {
    VPackArrayBuilder b(&resultBuilder);
    for (VPackSlice s : VPackArrayIterator(value)) {
      res = workForOneDocument(s, true);
      if (res.fail()) {
        createBabiesError(resultBuilder, errorCounter, res);
      }
    }
    res.reset(); // With babies reporting is handled in the result body
  } else {
    res = workForOneDocument(value, false);
  }

  std::shared_ptr<VPackBufferUInt8> resDocs = resultBuilder.steal();
  if (res.ok() && replicationType == ReplicationType::LEADER) {
    TRI_ASSERT(collection != nullptr);
    TRI_ASSERT(followers != nullptr);

    // In the multi babies case res is always TRI_ERROR_NO_ERROR if we
    // get here, in the single document case, we do not try to replicate
    // in case of an error.

    // Now replicate the good operations on all followers:
    return replicateOperations(collection.get(), followers, options, value,
                               TRI_VOC_DOCUMENT_OPERATION_INSERT, resDocs)
    .thenValue([options, errs = std::move(errorCounter), resDocs](Result res) mutable {
      if (!res.ok()) {
        return OperationResult{std::move(res), options};
      }
      if (options.silent && errs.empty()) {
        // We needed the results, but do not want to report:
        resDocs->clear();
      }
      return OperationResult(std::move(res), std::move(resDocs), options, std::move(errs));
    });
  }
  if (options.silent && errorCounter.empty()) {
    // We needed the results, but do not want to report:
    resDocs->clear();
  }
  return futures::makeFuture(OperationResult(std::move(res), std::move(resDocs),
                                             options, std::move(errorCounter)));
}

/// @brief update/patch one or multiple documents in a collection
/// the single-document variant of this operation will either succeed or,
/// if it fails, clean up after itself
Future<OperationResult> transaction::Methods::updateAsync(std::string const& cname,
                                                          VPackSlice newValue,
                                                          OperationOptions const& options) {
  TRI_ASSERT(_state->status() == transaction::Status::RUNNING);

  if (!newValue.isObject() && !newValue.isArray()) {
    // must provide a document object or an array of documents
    events::ModifyDocument(vocbase().name(), cname, newValue, options,
                           TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
  }
  if (newValue.isArray() && newValue.length() == 0) {
    events::ModifyDocument(vocbase().name(), cname, newValue, options,
                           TRI_ERROR_NO_ERROR);
    return emptyResult(options);
  }

  auto f = Future<OperationResult>::makeEmpty();
  if (_state->isCoordinator()) {
    f = modifyCoordinator(cname, newValue, options, TRI_VOC_DOCUMENT_OPERATION_UPDATE);
  } else {
    OperationOptions optionsCopy = options;
    f = modifyLocal(cname, newValue, optionsCopy, TRI_VOC_DOCUMENT_OPERATION_UPDATE);
  }
  return addTracking(std::move(f), [=](OperationResult&& opRes) {
    events::ModifyDocument(vocbase().name(), cname, newValue, opRes.options,
                           opRes.errorNumber());
    return std::move(opRes);
  });
}

/// @brief update one or multiple documents in a collection, coordinator
/// the single-document variant of this operation will either succeed or,
/// if it fails, clean up after itself
#ifndef USE_ENTERPRISE
Future<OperationResult> transaction::Methods::modifyCoordinator(
    std::string const& cname, VPackSlice newValue,
    OperationOptions const& options, TRI_voc_document_operation_e operation) {
  if (!newValue.isArray()) {
    arangodb::velocypack::StringRef key(transaction::helpers::extractKeyPart(newValue));
    if (key.empty()) {
      return OperationResult(TRI_ERROR_ARANGO_DOCUMENT_KEY_BAD, options);
    }
  }

  auto colptr = resolver()->getCollectionStructCluster(cname);
  if (colptr == nullptr) {
    return futures::makeFuture(OperationResult(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, options));
  }

  const bool isPatch = (TRI_VOC_DOCUMENT_OPERATION_UPDATE == operation);
  return arangodb::modifyDocumentOnCoordinator(*this, *colptr, newValue, options, isPatch);
}
#endif

/// @brief replace one or multiple documents in a collection
/// the single-document variant of this operation will either succeed or,
/// if it fails, clean up after itself
Future<OperationResult> transaction::Methods::replaceAsync(std::string const& cname,
                                              VPackSlice newValue,
                                              OperationOptions const& options) {
  TRI_ASSERT(_state->status() == transaction::Status::RUNNING);

  if (!newValue.isObject() && !newValue.isArray()) {
    // must provide a document object or an array of documents
    events::ReplaceDocument(vocbase().name(), cname, newValue, options,
                            TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
  }
  if (newValue.isArray() && newValue.length() == 0) {
    events::ReplaceDocument(vocbase().name(), cname, newValue, options,
                            TRI_ERROR_NO_ERROR);
    return futures::makeFuture(emptyResult(options));
  }

  auto f = Future<OperationResult>::makeEmpty();
  if (_state->isCoordinator()) {
    f = modifyCoordinator(cname, newValue, options, TRI_VOC_DOCUMENT_OPERATION_REPLACE);
  } else {
    OperationOptions optionsCopy = options;
    f = modifyLocal(cname, newValue, optionsCopy, TRI_VOC_DOCUMENT_OPERATION_REPLACE);
  }
  return addTracking(std::move(f), [=](OperationResult&& opRes) {
    events::ReplaceDocument(vocbase().name(), cname, newValue, opRes.options,
                            opRes.errorNumber());
    return std::move(opRes);
  });
}

/// @brief replace one or multiple documents in a collection, local
/// the single-document variant of this operation will either succeed or,
/// if it fails, clean up after itself
Future<OperationResult> transaction::Methods::modifyLocal(std::string const& collectionName,
                                                          VPackSlice newValue,
                                                          OperationOptions& options,
                                                          TRI_voc_document_operation_e operation) {
  DataSourceId cid = addCollectionAtRuntime(collectionName, AccessMode::Type::WRITE);
  auto* trxColl = trxCollection(cid);
  TRI_ASSERT(trxColl->isLocked(AccessMode::Type::WRITE));
  auto const& collection = trxColl->collection();

  // Assert my assumption that we don't have a lock only with mmfiles single
  // document operations.

  std::shared_ptr<std::vector<ServerID> const> followers;

  ReplicationType replicationType = ReplicationType::NONE;
  if (_state->isDBServer()) {
    TRI_ASSERT(followers == nullptr);
    followers = collection->followers()->get();

    // Block operation early if we are not supposed to perform it:
    auto const& followerInfo = collection->followers();
    std::string theLeader = followerInfo->getLeader();
    if (theLeader.empty()) {
      if (!options.isSynchronousReplicationFrom.empty()) {
        return OperationResult(TRI_ERROR_CLUSTER_SHARD_LEADER_REFUSES_REPLICATION, options);
      }
      if (!followerInfo->allowedToWrite()) {
        // We cannot fulfill minimum replication Factor.
        // Reject write.
        return OperationResult(TRI_ERROR_ARANGO_READ_ONLY, options);
      }

      replicationType = ReplicationType::LEADER;
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
        return OperationResult(TRI_ERROR_CLUSTER_SHARD_LEADER_RESIGNED, options);
      }
      if (options.isSynchronousReplicationFrom != theLeader) {
        return OperationResult(
            ::buildRefusalResult(*collection, (operation == TRI_VOC_DOCUMENT_OPERATION_REPLACE ? "replace" : "update"), options, theLeader),
            options);
      }
    }
  }  // isDBServer - early block

  // Update/replace are a read and a write, let's get the write lock already
  // for the read operation:
  //  Result lockResult = lockRecursive(cid, AccessMode::Type::WRITE);
  //
  //  if (!lockResult.ok() && !lockResult.is(TRI_ERROR_LOCKED)) {
  //    return OperationResult(lockResult);
  //  }

  VPackBuilder resultBuilder;  // building the complete result
  ManagedDocumentResult previous;
  ManagedDocumentResult result;

  // lambda //////////////
  auto workForOneDocument = [this, &operation, &options, &collection,
                             &resultBuilder, &cid, &previous,
                             &result](VPackSlice const newVal, bool isBabies) -> Result {
    Result res;
    if (!newVal.isObject()) {
      res.reset(TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
      return res;
    }

    result.clear();
    previous.clear();

    // replace and update are two operations each, thus this can and must not be
    // single document operations. We need to have a lock here already.
    TRI_ASSERT(isLocked(collection.get(), AccessMode::Type::WRITE));

    if (operation == TRI_VOC_DOCUMENT_OPERATION_REPLACE) {
      res = collection->replace(this, newVal, result, options, previous);
    } else {
      res = collection->update(this, newVal, result, options, previous);
    }

    if (res.fail()) {
      if (res.is(TRI_ERROR_ARANGO_CONFLICT) && !isBabies) {
        TRI_ASSERT(previous.revisionId().isSet());
        arangodb::velocypack::StringRef key(newVal.get(StaticStrings::KeyString));
        buildDocumentIdentity(collection.get(), resultBuilder, cid, key,
                              previous.revisionId(), RevisionId::none(),
                              options.returnOld ? &previous : nullptr, nullptr);
      }
      return res;
    }

    if (!options.silent) {
      TRI_ASSERT(!options.returnOld || !previous.empty());
      TRI_ASSERT(!options.returnNew || !result.empty());
      TRI_ASSERT(result.revisionId().isSet() && previous.revisionId().isSet());

      arangodb::velocypack::StringRef key(newVal.get(StaticStrings::KeyString));
      buildDocumentIdentity(collection.get(), resultBuilder, cid, key,
                            result.revisionId(), previous.revisionId(),
                            options.returnOld ? &previous : nullptr,
                            options.returnNew ? &result : nullptr);
    }

    return res;  // must be ok!
  };             // workForOneDocument
  ///////////////////////

  bool multiCase = newValue.isArray();
  std::unordered_map<ErrorCode, size_t> errorCounter;
  Result res;
  if (multiCase) {
    {
      VPackArrayBuilder guard(&resultBuilder);
      VPackArrayIterator it(newValue);
      while (it.valid()) {
        res = workForOneDocument(it.value(), true);
        if (res.fail()) {
          createBabiesError(resultBuilder, errorCounter, res);
        }
        ++it;
      }
    }
    res.reset(); // With babies reporting is handled in the result body
  } else {
    res = workForOneDocument(newValue, false);
  }

  auto resDocs = resultBuilder.steal();
  if (res.ok() && replicationType == ReplicationType::LEADER) {
    // We still hold a lock here, because this is update/replace and we're
    // therefore not doing single document operations. But if we didn't hold it
    // at the beginning of the method the followers may not be up-to-date.
    TRI_ASSERT(isLocked(collection.get(), AccessMode::Type::WRITE));
    TRI_ASSERT(collection != nullptr);
    TRI_ASSERT(followers != nullptr);

    // In the multi babies case res is always TRI_ERROR_NO_ERROR if we
    // get here, in the single document case, we do not try to replicate
    // in case of an error.

    // Now replicate the good operations on all followers:
    return replicateOperations(collection.get(), followers, options, newValue,
                               operation, resDocs)
    .thenValue([options, errs = std::move(errorCounter), resDocs](Result&& res) mutable {
      if (!res.ok()) {
        return OperationResult{std::move(res), options};
      }
      if (options.silent && errs.empty()) {
        // We needed the results, but do not want to report:
        resDocs->clear();
      }
      return OperationResult(std::move(res), std::move(resDocs),
                             std::move(options), std::move(errs));
    });
  }

  if (options.silent && errorCounter.empty()) {
    // We needed the results, but do not want to report:
    resDocs->clear();
  }

  return OperationResult(std::move(res), std::move(resDocs),
                         std::move(options), std::move(errorCounter));
}

/// @brief remove one or multiple documents in a collection
/// the single-document variant of this operation will either succeed or,
/// if it fails, clean up after itself
Future<OperationResult> transaction::Methods::removeAsync(std::string const& cname,
                                                          VPackSlice value,
                                                          OperationOptions const& options) {
  TRI_ASSERT(_state->status() == transaction::Status::RUNNING);

  if (!value.isObject() && !value.isArray() && !value.isString()) {
    // must provide a document object or an array of documents
    events::DeleteDocument(vocbase().name(), cname, value, options,
                           TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
  }
  if (value.isArray() && value.length() == 0) {
    events::DeleteDocument(vocbase().name(), cname, value, options, TRI_ERROR_NO_ERROR);
    return emptyResult(options);
  }

  auto f = Future<OperationResult>::makeEmpty();
  if (_state->isCoordinator()) {
    f = removeCoordinator(cname, value, options);
  } else {
    OperationOptions optionsCopy = options;
    f = removeLocal(cname, value, optionsCopy);
  }
  return addTracking(std::move(f), [=](OperationResult&& opRes) {
    events::DeleteDocument(vocbase().name(), cname, value, opRes.options,
                           opRes.errorNumber());
    return std::move(opRes);
  });
}

/// @brief remove one or multiple documents in a collection, coordinator
/// the single-document variant of this operation will either succeed or,
/// if it fails, clean up after itself
#ifndef USE_ENTERPRISE
Future<OperationResult> transaction::Methods::removeCoordinator(std::string const& cname,
                                                                VPackSlice value,
                                                                OperationOptions const& options) {
  auto colptr = resolver()->getCollectionStructCluster(cname);
  if (colptr == nullptr) {
    return futures::makeFuture(OperationResult(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, options));
  }
  return arangodb::removeDocumentOnCoordinator(*this, *colptr, value, options);
}
#endif

/// @brief remove one or multiple documents in a collection, local
/// the single-document variant of this operation will either succeed or,
/// if it fails, clean up after itself
Future<OperationResult> transaction::Methods::removeLocal(std::string const& collectionName,
                                                          VPackSlice value,
                                                          OperationOptions& options) {
  DataSourceId cid = addCollectionAtRuntime(collectionName, AccessMode::Type::WRITE);
  auto* trxColl = trxCollection(cid);
  TRI_ASSERT(trxColl->isLocked(AccessMode::Type::WRITE));
  auto const& collection = trxColl->collection();

  std::shared_ptr<std::vector<ServerID> const> followers;

  ReplicationType replicationType = ReplicationType::NONE;
  if (_state->isDBServer()) {
    TRI_ASSERT(followers == nullptr);
    followers = collection->followers()->get();

    // Block operation early if we are not supposed to perform it:
    auto const& followerInfo = collection->followers();
    std::string theLeader = followerInfo->getLeader();
    if (theLeader.empty()) {
      if (!options.isSynchronousReplicationFrom.empty()) {
        return OperationResult(TRI_ERROR_CLUSTER_SHARD_LEADER_REFUSES_REPLICATION, options);
      }
      if (!followerInfo->allowedToWrite()) {
        // We cannot fulfill minimum replication Factor.
        // Reject write.
        return OperationResult(TRI_ERROR_ARANGO_READ_ONLY, options);
      }

      replicationType = ReplicationType::LEADER;
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
        return OperationResult(TRI_ERROR_CLUSTER_SHARD_LEADER_RESIGNED, options);
      }
      if (options.isSynchronousReplicationFrom != theLeader) {
        return OperationResult(
            ::buildRefusalResult(*collection, "remove", options, theLeader),
            options);
      }
    }
  }  // isDBServer - early block

  VPackBuilder resultBuilder;
  ManagedDocumentResult previous;

  auto workForOneDocument = [&](VPackSlice value, bool isBabies) -> Result {
    transaction::BuilderLeaser builder(this);
    arangodb::velocypack::StringRef key;
    if (value.isString()) {
      key = value;
      size_t pos = key.find('/');
      if (pos != std::string::npos) {
        key = key.substr(pos + 1);
        builder->add(VPackValuePair(key.data(), key.length(), VPackValueType::String));
        value = builder->slice();
      }
    } else if (value.isObject()) {
      VPackSlice keySlice = value.get(StaticStrings::KeyString);
      if (!keySlice.isString()) {
        return Result(TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD);
      }
      key = keySlice;
    } else {
      return Result(TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD);
    }

    // Primary keys must not be empty
    if (key.empty()) {
      return Result(TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD);
    }

    previous.clear();

    auto res = collection->remove(*this, value, options, previous);

    if (res.fail()) {
      if (res.is(TRI_ERROR_ARANGO_CONFLICT) && !isBabies) {
        TRI_ASSERT(previous.revisionId().isSet());
        buildDocumentIdentity(collection.get(), resultBuilder, cid, key,
                              previous.revisionId(), RevisionId::none(),
                              options.returnOld ? &previous : nullptr, nullptr);
      }
      return res;
    }

    if (!options.silent) {
      TRI_ASSERT(!options.returnOld || !previous.empty());
      TRI_ASSERT(previous.revisionId().isSet());
      buildDocumentIdentity(collection.get(), resultBuilder, cid, key,
                            previous.revisionId(), RevisionId::none(),
                            options.returnOld ? &previous : nullptr, nullptr);
    }

    return res;
  };

  Result res;
  std::unordered_map<ErrorCode, size_t> errorCounter;
  if (value.isArray()) {
    VPackArrayBuilder guard(&resultBuilder);
    for (VPackSlice s : VPackArrayIterator(value)) {
      res = workForOneDocument(s, true);
      if (res.fail()) {
        createBabiesError(resultBuilder, errorCounter, res);
      }
    }
    res.reset(); // With babies reporting is handled in the result body
  } else {
    res = workForOneDocument(value, false);
  }

  auto resDocs = resultBuilder.steal();
  if (res.ok() && replicationType == ReplicationType::LEADER) {
    TRI_ASSERT(collection != nullptr);
    TRI_ASSERT(followers != nullptr);
    // Now replicate the same operation on all followers:

    // In the multi babies case res is always TRI_ERROR_NO_ERROR if we
    // get here, in the single document case, we do not try to replicate
    // in case of an error.

    // Now replicate the good operations on all followers:
    return replicateOperations(collection.get(), followers, options, value,
                               TRI_VOC_DOCUMENT_OPERATION_REMOVE, resDocs)
    .thenValue([options, errs = std::move(errorCounter), resDocs](Result res) mutable {
      if (!res.ok()) {
        return OperationResult{std::move(res), options};
      }
      if (options.silent && errs.empty()) {
        // We needed the results, but do not want to report:
        resDocs->clear();
      }
      return OperationResult(std::move(res), std::move(resDocs),
                             std::move(options), std::move(errs));
    });
  }

  if (options.silent && errorCounter.empty()) {
    // We needed the results, but do not want to report:
    resDocs->clear();
  }

  return OperationResult(std::move(res), std::move(resDocs),
                         std::move(options), std::move(errorCounter));
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
OperationResult transaction::Methods::allCoordinator(std::string const& collectionName,
                                                     uint64_t skip, uint64_t limit,
                                                     OperationOptions& options) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

/// @brief fetches all documents in a collection, local
OperationResult transaction::Methods::allLocal(std::string const& collectionName,
                                               uint64_t skip, uint64_t limit,
                                               OperationOptions& options) {
  DataSourceId cid = addCollectionAtRuntime(collectionName, AccessMode::Type::READ);
  TRI_ASSERT(trxCollection(cid)->isLocked(AccessMode::Type::READ));

  VPackBuilder resultBuilder;

  if (_state->isDBServer()) {
    std::shared_ptr<LogicalCollection> const& collection = trxCollection(cid)->collection();
    auto const& followerInfo = collection->followers();
    if (!followerInfo->getLeader().empty()) {
      return OperationResult(TRI_ERROR_CLUSTER_SHARD_FOLLOWER_REFUSES_OPERATION, options);
    }
  }

  resultBuilder.openArray();
  
  auto iterator = indexScan(collectionName, transaction::Methods::CursorType::ALL);

  iterator->allDocuments(
      [&resultBuilder](LocalDocumentId const& /*token*/, VPackSlice slice) {
        resultBuilder.add(slice);
        return true;
      }, 1000);

  resultBuilder.close();

  return OperationResult(Result(), resultBuilder.steal(), options);
}

/// @brief remove all documents in a collection
Future<OperationResult> transaction::Methods::truncateAsync(std::string const& collectionName,
                                                            OperationOptions const& options) {
  TRI_ASSERT(_state->status() == transaction::Status::RUNNING);

  OperationOptions optionsCopy = options;
  auto cb = [this, collectionName](OperationResult res) {
    events::TruncateCollection(vocbase().name(), collectionName, res);
    return res;
  };

  if (_state->isCoordinator()) {
    return truncateCoordinator(collectionName, optionsCopy).thenValue(cb);
  }
  return truncateLocal(collectionName, optionsCopy).thenValue(cb);
}

/// @brief remove all documents in a collection, coordinator
#ifndef USE_ENTERPRISE
Future<OperationResult> transaction::Methods::truncateCoordinator(std::string const& collectionName,
                                                                  OperationOptions& options) {
  return arangodb::truncateCollectionOnCoordinator(*this, collectionName, options);
}
#endif

/// @brief remove all documents in a collection, local
Future<OperationResult> transaction::Methods::truncateLocal(std::string const& collectionName,
                                                            OperationOptions& options) {
  DataSourceId cid = addCollectionAtRuntime(collectionName, AccessMode::Type::WRITE);
  auto const& collection = trxCollection(cid)->collection();

  std::shared_ptr<std::vector<ServerID> const> followers;

  ReplicationType replicationType = ReplicationType::NONE;
  if (_state->isDBServer()) {
    TRI_ASSERT(followers == nullptr);
    followers = collection->followers()->get();

    // Block operation early if we are not supposed to perform it:
    auto const& followerInfo = collection->followers();
    std::string theLeader = followerInfo->getLeader();
    if (theLeader.empty()) {
      if (!options.isSynchronousReplicationFrom.empty()) {
        return futures::makeFuture(
            OperationResult(TRI_ERROR_CLUSTER_SHARD_LEADER_REFUSES_REPLICATION, options));
      }
      if (!followerInfo->allowedToWrite()) {
        // We cannot fulfill minimum replication Factor.
        // Reject write.
        return futures::makeFuture(OperationResult(TRI_ERROR_ARANGO_READ_ONLY, options));
      }

      // fetch followers
      followers = followerInfo->get();
      if (followers->size() > 0) {
        replicationType = ReplicationType::LEADER;
        options.silent = false;
      }
    } else {  // we are a follower following theLeader
      replicationType = ReplicationType::FOLLOWER;
      if (options.isSynchronousReplicationFrom.empty()) {
        return futures::makeFuture(
            OperationResult(TRI_ERROR_CLUSTER_SHARD_LEADER_RESIGNED, options));
      }
      if (options.isSynchronousReplicationFrom != theLeader) {
        return futures::makeFuture(
            OperationResult(
                ::buildRefusalResult(*collection, "truncate", options, theLeader),
                options));
      }
    }
  }  // isDBServer - early block

  TRI_ASSERT(isLocked(collection.get(), AccessMode::Type::WRITE));

  Result res = collection->truncate(*this, options);

  if (res.fail()) {
    return futures::makeFuture(OperationResult(res, options));
  }

  // Now see whether or not we have to do synchronous replication:
  if (replicationType == ReplicationType::LEADER) {
    TRI_ASSERT(followers != nullptr);

    TRI_ASSERT(!_state->hasHint(Hints::Hint::FROM_TOPLEVEL_AQL));

    // Now replicate the good operations on all followers:
    NetworkFeature const& nf = vocbase().server().getFeature<NetworkFeature>();
    network::ConnectionPool* pool = nf.pool();
    if (pool != nullptr) {
      // nullptr only happens on controlled shutdown
      std::string path =
          "/_api/collection/" + arangodb::basics::StringUtils::urlEncode(collectionName) +
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
      reqOpts.param(StaticStrings::IsSynchronousReplicationString, ServerState::instance()->getId());
      reqOpts.param(StaticStrings::Compact, (options.truncateCompact ? "true" : "false"));

      for (auto const& f : *followers) {
        network::Headers headers;
        ClusterTrxMethods::addTransactionHeader(*this, f, headers);
        auto future = network::sendRequest(pool, "server:" + f, fuerte::RestVerb::Put,
                                           path, body, reqOpts, std::move(headers));
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
          auto const& followerInfo = collection->followers();
          LOG_TOPIC("0e2e0", WARN, Logger::REPLICATION)
              << "truncateLocal: dropping follower " << (*followers)[i]
              << " for shard " << collection->vocbase().name() << "/" << collectionName
              << ": " << responses[i].get().combinedResult().errorMessage();
          res = followerInfo->remove((*followers)[i]);
          if (res.ok()) {
            _state->removeKnownServer((*followers)[i]);
          } else {
            LOG_TOPIC("359bc", WARN, Logger::REPLICATION)
                << "truncateLocal: could not drop follower " << (*followers)[i]
                << " for shard " << collection->vocbase().name() << "/" << collection->name()
                << ": " << res.errorMessage();
            THROW_ARANGO_EXCEPTION(TRI_ERROR_CLUSTER_COULD_NOT_DROP_FOLLOWER);
          }
        }
      }
      // If any would-be-follower refused to follow there must be a
      // new leader in the meantime, in this case we must not allow
      // this operation to succeed, we simply return with a refusal
      // error (note that we use the follower version, since we have
      // lost leadership):
      if (findRefusal(responses)) {
        vocbase().server().getFeature<arangodb::ClusterFeature>().followersRefusedCounter()++;
        return futures::makeFuture(
            OperationResult(TRI_ERROR_CLUSTER_SHARD_LEADER_RESIGNED, options));
      }
    }
  }

  return futures::makeFuture(OperationResult(res, options));
}

/// @brief count the number of documents in a collection
futures::Future<OperationResult> transaction::Methods::countAsync(
    std::string const& collectionName, transaction::CountType type,
    OperationOptions const& options) {
  TRI_ASSERT(_state->status() == transaction::Status::RUNNING);

  if (_state->isCoordinator()) {
    return countCoordinator(collectionName, type, options);
  }

  if (type == CountType::Detailed) {
    // we are a single-server... we cannot provide detailed per-shard counts,
    // so just downgrade the request to a normal request
    type = CountType::Normal;
  }

  return futures::makeFuture(countLocal(collectionName, type, options));
}

#ifndef USE_ENTERPRISE
/// @brief count the number of documents in a collection
futures::Future<OperationResult> transaction::Methods::countCoordinator(
    std::string const& collectionName, transaction::CountType type,
    OperationOptions const& options) {
  // First determine the collection ID from the name:
  auto colptr = resolver()->getCollectionStructCluster(collectionName);
  if (colptr == nullptr) {
    return futures::makeFuture(OperationResult(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, options));
  }

  return countCoordinatorHelper(colptr, collectionName, type, options);
}

#endif

futures::Future<OperationResult> transaction::Methods::countCoordinatorHelper(
    std::shared_ptr<LogicalCollection> const& collinfo, std::string const& collectionName,
    transaction::CountType type, OperationOptions const& options) {
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
    return arangodb::countOnCoordinator(*this, collectionName, options)
        .thenValue([&cache, type, options](OperationResult&& res) -> OperationResult {
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
          OperationResult opRes = buildCountResult(options, counts, type, total);
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
OperationResult transaction::Methods::countLocal(std::string const& collectionName,
                                                 transaction::CountType type,
                                                 OperationOptions const& options) {
  DataSourceId cid = addCollectionAtRuntime(collectionName, AccessMode::Type::READ);
  auto const& collection = trxCollection(cid)->collection();

//  Result lockResult = lockRecursive(cid, AccessMode::Type::READ);
//
//  if (!lockResult.ok() && !lockResult.is(TRI_ERROR_LOCKED)) {
//    return OperationResult(lockResult);
//  }

  TRI_ASSERT(isLocked(collection.get(), AccessMode::Type::READ));

  uint64_t num = collection->numberDocuments(this, type);

//  if (lockResult.is(TRI_ERROR_LOCKED)) {
//    Result res = unlockRecursive(cid, AccessMode::Type::READ);
//
//    if (res.fail()) {
//      return OperationResult(res);
//    }
//  }

  VPackBuilder resultBuilder;
  resultBuilder.add(VPackValue(num));

  return OperationResult(Result(), resultBuilder.steal(), options);
}

/// @brief factory for IndexIterator objects from AQL
/// note: the caller must have read-locked the underlying collection when
/// calling this method
std::unique_ptr<IndexIterator> transaction::Methods::indexScanForCondition(
    IndexHandle const& idx, arangodb::aql::AstNode const* condition,
    arangodb::aql::Variable const* var, IndexIteratorOptions const& opts) {
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
  return idx->iteratorForCondition(this, condition, var, opts);
}

/// @brief factory for IndexIterator objects
/// note: the caller must have read-locked the underlying collection when
/// calling this method
std::unique_ptr<IndexIterator> transaction::Methods::indexScan(std::string const& collectionName,
                                                               CursorType cursorType) {
  // For now we assume indexId is the iid part of the index.

  if (ADB_UNLIKELY(_state->isCoordinator())) {
    // The index scan is only available on DBServers and Single Server.
    THROW_ARANGO_EXCEPTION(TRI_ERROR_CLUSTER_ONLY_ON_DBSERVER);
  }

  DataSourceId cid = addCollectionAtRuntime(collectionName, AccessMode::Type::READ);
  TransactionCollection* trxColl = trxCollection(cid);
  if (trxColl == nullptr) {
    throwCollectionNotFound(collectionName.c_str());
  }
  TRI_ASSERT(trxColl->isLocked(AccessMode::Type::READ));

  std::shared_ptr<LogicalCollection> const& logical = trxColl->collection();
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
      iterator = logical->getAllIterator(this);
      break;
    }
  }

  // the above methods must always return a valid iterator or throw!
  TRI_ASSERT(iterator != nullptr);
  return iterator;
}

/// @brief return the collection
arangodb::LogicalCollection* transaction::Methods::documentCollection(DataSourceId cid) const {
  TRI_ASSERT(_state != nullptr);
  TRI_ASSERT(_state->status() == transaction::Status::RUNNING);

  auto trxColl = trxCollection(cid, AccessMode::Type::READ);
  if (trxColl == nullptr) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "could not find collection");
  }

  TRI_ASSERT(trxColl != nullptr);
  TRI_ASSERT(trxColl->collection() != nullptr);
  return trxColl->collection().get();
}

/// @brief return the collection
arangodb::LogicalCollection* transaction::Methods::documentCollection(std::string const& name) const {
  TRI_ASSERT(_state != nullptr);
  TRI_ASSERT(_state->status() == transaction::Status::RUNNING);

  auto trxColl = trxCollection(name, AccessMode::Type::READ);
  if (trxColl == nullptr) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   std::string("could not find collection '") + name + "'");
  }

  TRI_ASSERT(trxColl != nullptr);
  TRI_ASSERT(trxColl->collection() != nullptr);
  return trxColl->collection().get();
}

/// @brief add a collection by id, with the name supplied
Result transaction::Methods::addCollection(DataSourceId cid, std::string const& cname,
                                           AccessMode::Type type) {
  if (_state == nullptr) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "cannot add collection without state");
  }

  Status const status = _state->status();

  if (status == transaction::Status::COMMITTED || status == transaction::Status::ABORTED) {
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
    throwCollectionNotFound(cname.c_str());
  }

  const bool lockUsage = !_mainTransaction;

  auto addCollectionCallback = [this, &cname, type, lockUsage](DataSourceId cid) -> void {
    auto res = _state->addCollection(cid, cname, type, lockUsage);

    if (res.fail()) {
      THROW_ARANGO_EXCEPTION(res);
    }
  };

  Result res;
  bool visited = false;
  std::function<bool(LogicalCollection&)> visitor(
      [this, &addCollectionCallback, &res, cid, &visited](LogicalCollection& col) -> bool {
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

    return res.ok() ? Result(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND) : res;  // return first error
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
Result transaction::Methods::addCollection(std::string const& name, AccessMode::Type type) {
  return addCollection(resolver()->getCollectionId(name), name, type);
}

/// @brief test if a collection is already locked
bool transaction::Methods::isLocked(LogicalCollection* document, AccessMode::Type type) const {
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

Result transaction::Methods::resolveId(char const* handle, size_t length,
                                       std::shared_ptr<LogicalCollection>& collection,
                                       char const*& key, size_t& outLength) {
  char const* p =
      static_cast<char const*>(memchr(handle, TRI_DOCUMENT_HANDLE_SEPARATOR_CHR, length));

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
    LogicalCollection* collection,
    std::shared_ptr<const std::vector<ServerID>> const& followerList,
    OperationOptions const& options, VPackSlice const value,
    TRI_voc_document_operation_e const operation,
    std::shared_ptr<VPackBuffer<uint8_t>> const& ops) {
  TRI_ASSERT(followerList != nullptr);

  if (followerList->empty()) {
    return Result();
  }

  // path and requestType are different for insert/remove/modify.

  network::RequestOptions reqOpts;
  reqOpts.database = vocbase().name();
  reqOpts.param(StaticStrings::IsRestoreString, "true");
  reqOpts.param(StaticStrings::IsSynchronousReplicationString, ServerState::instance()->getId());

  std::string url = "/_api/document/";
  url.append(arangodb::basics::StringUtils::urlEncode(collection->name()));
  if (operation != TRI_VOC_DOCUMENT_OPERATION_INSERT && !value.isArray()) {
    TRI_ASSERT(value.isObject());
    TRI_ASSERT(value.hasKey(StaticStrings::KeyString));
    url.push_back('/');
    VPackValueLength len;
    char const* ptr = value.get(StaticStrings::KeyString).getString(len);
    basics::StringUtils::encodeURIComponent(url, ptr, len);
  }

  char const* opName = "unknown";
  arangodb::fuerte::RestVerb requestType = arangodb::fuerte::RestVerb::Illegal;
  switch (operation) {
    case TRI_VOC_DOCUMENT_OPERATION_INSERT:
      requestType = arangodb::fuerte::RestVerb::Post;
      if (options.isOverwriteModeSet()) {
        if (options.overwriteMode != OperationOptions::OverwriteMode::Unknown) {
          reqOpts.param(StaticStrings::OverwriteMode, OperationOptions::stringifyOverwriteMode(options.overwriteMode));
        }
        if (options.overwriteMode == OperationOptions::OverwriteMode::Update) {
          // extra parameters only required for update
          reqOpts.param(StaticStrings::KeepNullString, options.keepNull ? "true" : "false");
          reqOpts.param(StaticStrings::MergeObjectsString, options.mergeObjects ? "true" : "false");
        }
      }
      opName = "insert";
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

  transaction::BuilderLeaser payload(this);
  auto doOneDoc = [&](VPackSlice const& doc, VPackSlice result) {
    VPackObjectBuilder guard(payload.get());
    VPackSlice s = result.get(StaticStrings::KeyString);
    payload->add(StaticStrings::KeyString, s);
    s = result.get(StaticStrings::RevString);
    payload->add(StaticStrings::RevString, s);
    if (operation != TRI_VOC_DOCUMENT_OPERATION_REMOVE) {
      TRI_SanitizeObject(doc, *payload.get());
    }
  };

  VPackSlice ourResult(ops->data());
  size_t count = 0;
  if (value.isArray()) {
    VPackArrayBuilder guard(payload.get());
    VPackArrayIterator itValue(value);
    VPackArrayIterator itResult(ourResult);
    while (itValue.valid() && itResult.valid()) {
      TRI_ASSERT((*itResult).isObject());
      if (!(*itResult).hasKey(StaticStrings::Error)) {
        doOneDoc(itValue.value(), itResult.value());
        count++;
      }
      itValue.next();
      itResult.next();
    }
  } else {
    doOneDoc(value, ourResult);
    count++;
  }

  if (count == 0) {
    // nothing to do
    return Result();
  }

  reqOpts.timeout = network::Timeout(chooseTimeoutForReplication(count, payload->size()));

  // Now prepare the requests:
  std::vector<Future<network::Response>> futures;
  futures.reserve(followerList->size());

  auto startTimeReplication = std::chrono::steady_clock::now();

  auto* pool = vocbase().server().getFeature<NetworkFeature>().pool();
  for (auto const& f : *followerList) {
    network::Headers headers;
    ClusterTrxMethods::addTransactionHeader(*this, f, headers);
    futures.emplace_back(network::sendRequestRetry(pool, "server:" + f, requestType,
                                                   url, *(payload->buffer()), reqOpts,
                                                   std::move(headers)));
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
  auto cb = [=](std::vector<futures::Try<network::Response>>&& responses) -> Result {

    auto duration = std::chrono::steady_clock::now() - startTimeReplication;
    auto& replMetrics = vocbase().server().getFeature<ReplicationMetricsFeature>();
    replMetrics.synchronousOpsTotal() += 1;
    replMetrics.synchronousTimeTotal() += std::chrono::nanoseconds(duration).count();

    bool didRefuse = false;
    // We drop all followers that were not successful:
    for (size_t i = 0; i < followerList->size(); ++i) {
      network::Response const& resp = responses[i].get();
      ServerID const& follower = (*followerList)[i];

      bool replicationWorked = false;
      if (resp.error == fuerte::Error::NoError) {
        replicationWorked = resp.statusCode() == fuerte::StatusAccepted ||
                            resp.statusCode() == fuerte::StatusCreated ||
                            resp.statusCode() == fuerte::StatusOK;
        if (replicationWorked) {
          bool found;
          resp.response().header.metaByKey(StaticStrings::ErrorCodes, found);
          replicationWorked = !found;
        } else {
          auto r = resp.combinedResult();
          bool followerRefused = (r.errorNumber() == TRI_ERROR_CLUSTER_SHARD_LEADER_REFUSES_REPLICATION);
          didRefuse = didRefuse || followerRefused;

          if (followerRefused) {
            vocbase().server().getFeature<arangodb::ClusterFeature>().followersRefusedCounter()++;

            LOG_TOPIC("3032c", WARN, Logger::REPLICATION)
                << "synchronous replication of " << opName << " operation: "
                << "follower " << follower << " for shard "
                << collection->vocbase().name() << "/" << collection->name()
                << " refused the operation: " << r.errorMessage();
          }
        }
      }

      TRI_IF_FAILURE("replicateOperationsDropFollower") {
        replicationWorked = false;
      }

      if (!replicationWorked) {
        LOG_TOPIC("12d8c", WARN, Logger::REPLICATION)
            << "synchronous replication of " << opName << " operation: "
            << ": dropping follower " << follower << " for shard "
            << collection->vocbase().name() << "/" << collection->name()
            << ": " << resp.combinedResult().errorMessage();

        Result res = collection->followers()->remove(follower);
        if (res.ok()) {
          // simon: follower cannot be re-added without lock on collection
          _state->removeKnownServer(follower);
        } else {
          LOG_TOPIC("db473", ERR, Logger::REPLICATION)
              << "synchronous replication of " << opName << " operation: "
              << "could not drop follower " << follower << " for shard "
              << collection->vocbase().name() << "/" << collection->name()
              << ": " << res.errorMessage();
          THROW_ARANGO_EXCEPTION(TRI_ERROR_CLUSTER_COULD_NOT_DROP_FOLLOWER);
        }
      }
    }

    if (didRefuse) {  // case (1), caller may abort this transaction
      return Result(TRI_ERROR_CLUSTER_SHARD_LEADER_RESIGNED);
    }
    return Result();
  };
  return futures::collectAll(std::move(futures)).thenValue(std::move(cb));
}

#ifndef USE_ENTERPRISE
ErrorCode Methods::validateSmartJoinAttribute(LogicalCollection const&,
                                              arangodb::velocypack::Slice) {
  return TRI_ERROR_NO_ERROR;
}
#endif

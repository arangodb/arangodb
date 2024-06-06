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
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

#include "Methods.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Aql/Ast.h"
#include "Aql/AstNode.h"
#include "Basics/Exceptions.h"
#include "Basics/DownCast.h"
#include "Basics/GlobalResourceMonitor.h"
#include "Basics/ResourceUsage.h"
#include "Basics/ScopeGuard.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/encoding.h"
#include "Basics/system-compiler.h"
#include "Basics/voc-errors.h"
#include "Cluster/AgencyCache.h"
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
#include "Metrics/Counter.h"
#include "Network/Methods.h"
#include "Network/NetworkFeature.h"
#include "Network/Utils.h"
#include "Random/RandomGenerator.h"
#include "Replication/ReplicationMetricsFeature.h"
#include "Replication2/StateMachines/Document/DocumentFollowerState.h"
#include "Replication2/StateMachines/Document/DocumentLeaderState.h"
#include "RestServer/DatabaseFeature.h"
#include "RocksDBEngine/ReplicatedRocksDBTransactionCollection.h"
#include "Scheduler/SchedulerFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/PhysicalCollection.h"
#include "StorageEngine/StorageEngine.h"
#include "StorageEngine/TransactionCollection.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/BatchOptions.h"
#include "Transaction/Context.h"
#include "Transaction/Helpers.h"
#include "Transaction/IndexesSnapshot.h"
#include "Transaction/Manager.h"
#include "Transaction/ManagerFeature.h"
#include "Transaction/OperationOrigin.h"
#include "Transaction/Options.h"
#include "Transaction/ReplicatedContext.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/CollectionNameResolver.h"
#include "Utils/Events.h"
#include "Utils/ExecContext.h"
#include "Utils/OperationOptions.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/ComputedValues.h"
#include "VocBase/Identifiers/RevisionId.h"
#include "VocBase/KeyGenerator.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ticks.h"

#include <absl/strings/str_cat.h>
#include <velocypack/Builder.h>
#include <velocypack/Collection.h>
#include <velocypack/Iterator.h>
#include <velocypack/Options.h>
#include <velocypack/Slice.h>

using namespace arangodb;
using namespace arangodb::transaction;
using namespace arangodb::transaction::helpers;

template<typename T>
using Future = futures::Future<T>;

namespace {

template<Methods::CallbacksTag tag>
struct ToType;

template<>
struct ToType<Methods::CallbacksTag::StatusChange> {
  using Type = Methods::StatusChangeCallback;
};

// builds a document object with just _key and _rev
void buildDocumentStub(velocypack::Builder& builder, std::string_view key,
                       RevisionId revisionId) {
  builder.openObject(/*unindexed*/ true);
  // _key
  builder.add(StaticStrings::KeyString, VPackValue(key));

  // _rev
  char ridBuffer[basics::maxUInt64StringSize];
  builder.add(StaticStrings::RevString, revisionId.toValuePair(ridBuffer));
  builder.close();
}

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
  return {TRI_ERROR_CLUSTER_SHARD_FOLLOWER_REFUSES_OPERATION,
          absl::StrCat(TRI_errno_string(
                           TRI_ERROR_CLUSTER_SHARD_FOLLOWER_REFUSES_OPERATION),
                       ": shard: ", collection.vocbase().name(), "/",
                       collection.name(), ", operation: ", operation,
                       ", from: ", options.isSynchronousReplicationFrom,
                       ", current leader: ", leader)};
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
template<Methods::CallbacksTag tag>
auto* getCallbacks(TransactionState& state, bool create = false) {
  using Callback = typename ToType<tag>::Type;
  struct CookieType : public TransactionState::Cookie {
    std::vector<Callback const*> _callbacks;
  };

  // arbitrary location in memory, common for all
  static const auto key = tag;

  // TODO FIXME find a better way to look up a ViewState
  auto* cookie = basics::downCast<CookieType>(state.cookie(&key));

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
    // addDataSourceRegistrationCallback(...) ensures valid
    TRI_ASSERT(callback);

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
Result applyStatusChangeCallbacks(Methods& trx, Status status) noexcept try {
  TRI_ASSERT(Status::ABORTED == status || Status::COMMITTED == status ||
             Status::RUNNING == status);
  auto* state = trx.state();
  if (!state) {
    return {};  // nothing to apply
  }

  TRI_ASSERT(trx.isMainTransaction());
  auto* callbacks = getCallbacks<Methods::CallbacksTag::StatusChange>(*state);
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

void throwCollectionNotFound(std::string_view name) {
  THROW_ARANGO_EXCEPTION_MESSAGE(
      TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
      absl::StrCat(TRI_errno_string(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND),
                   ": ", name));
}

/// @brief Insert an error reported instead of the new document
void createBabiesError(VPackBuilder* builder,
                       std::unordered_map<ErrorCode, size_t>& countErrorCodes,
                       Result const& error) {
  // on replication1 followers, builder will be a nullptr, so we can spare
  // building the error result details in the response body, which the leader
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

template<class Derived, class OpOptions = OperationOptions>
struct GenericProcessor {
  GenericProcessor(Methods& methods, TransactionCollection& trxColl,
                   LogicalCollection& collection, VPackSlice value,
                   OpOptions& options)
      : _methods(methods),
        _trxColl(trxColl),
        _collection(collection),
        _value(value),
        _options(options) {}

  template<class... Args>
  static futures::Future<ResultT<Derived>> create(
      Methods& methods, std::string const& collectionName, VPackSlice value,
      OpOptions& options, Args&&... args) {
    DataSourceId cid = co_await methods.addCollectionAtRuntime(
        collectionName, Derived::accessMode());
    TransactionCollection* trxColl = methods.trxCollection(cid);
    if (trxColl == nullptr) {
      co_return Result(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
    }
    auto const& collection = trxColl->collection();
    if (collection == nullptr) {
      co_return Result(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
    }

    try {
      co_return Derived(methods, *trxColl, *collection, value, options,
                        std::forward<Args>(args)...);
    } catch (arangodb::basics::Exception const& e) {
      co_return Result(e.code(), e.message());
    }
  }

 protected:
  Derived& self() { return static_cast<Derived&>(*this); }

  /// @brief build a VPack object with _id, _key and _rev, the result is
  /// added to the builder in the argument as a single object.
  void buildDocumentIdentity(std::string_view key, RevisionId rid,
                             RevisionId oldRid,
                             velocypack::Builder const* oldDoc,
                             velocypack::Builder const* newDoc) {
    _resultBuilder.openObject();

    // _id
    StringLeaser leased(_methods.transactionContext().get());
    std::string& temp(*leased.get());
    temp.reserve(64);

    if (_methods.state()->isRunningInCluster()) {
      std::string resolved =
          _methods.resolver()->getCollectionNameCluster(_trxColl.id());
#ifdef USE_ENTERPRISE
      ClusterMethods::realNameFromSmartName(resolved);
#endif
      // build collection name
      temp.append(resolved);
    } else {
      // build collection name
      temp.append(_collection.name());
    }

    // append / and key part
    temp.push_back('/');
    temp.append(key.data(), key.size());

    _resultBuilder.add(StaticStrings::IdString, VPackValue(temp));

    // _key
    _resultBuilder.add(StaticStrings::KeyString, VPackValue(key));

    // _rev
    if (rid.isSet()) {
      char ridBuffer[arangodb::basics::maxUInt64StringSize];
      _resultBuilder.add(StaticStrings::RevString, rid.toValuePair(ridBuffer));
    } else {
      _resultBuilder.add(StaticStrings::RevString, std::string_view{});
    }

    // _oldRev
    if (oldRid.isSet()) {
      _resultBuilder.add("_oldRev", VPackValue(oldRid.toString()));
    }

    // old
    if (oldDoc != nullptr) {
      _resultBuilder.add(StaticStrings::Old, oldDoc->slice());
    }

    // new
    if (newDoc != nullptr) {
      _resultBuilder.add(StaticStrings::New, newDoc->slice());
    }

    _resultBuilder.close();
  }

  Methods& _methods;
  TransactionCollection& _trxColl;
  LogicalCollection& _collection;
  VPackSlice _value;
  OpOptions& _options;
  // total result that is going to be returned to the caller, append-only
  VPackBuilder _resultBuilder;
};

struct GetDocumentProcessor
    : GenericProcessor<GetDocumentProcessor, OperationOptions const> {
  GetDocumentProcessor(Methods& methods, TransactionCollection& trxColl,
                       LogicalCollection& collection, VPackSlice value,
                       OperationOptions const& options)
      : GenericProcessor(methods, trxColl, collection, value, options) {
    if (_methods.state()->isDBServer()) {
      if (!_collection.isLeadingShard()) {
        // We believe to be a follower!
        if (!_options.allowDirtyReads) {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_CLUSTER_SHARD_LEADER_RESIGNED);
        }
      }
    }
  }

  static constexpr AccessMode::Type accessMode() {
    return AccessMode::Type::READ;
  }

  /// @brief read one or multiple documents in a collection, local
  Future<OperationResult> execute() {
    Result res;

    std::unordered_map<ErrorCode, size_t> countErrorCodes;
    if (!_value.isArray()) {
      res = processValue(_value, false);
    } else {
      VPackArrayBuilder guard(&_resultBuilder);
      for (VPackSlice s : VPackArrayIterator(_value)) {
        res = processValue(s, true);
        if (res.fail()) {
          createBabiesError(&_resultBuilder, countErrorCodes, res);
        }
      }
      res.reset();  // With babies the reporting is handled somewhere else.
    }

    events::ReadDocument(_methods.vocbase().name(), _trxColl.collectionName(),
                         _value, _options, res.errorNumber());

    return futures::makeFuture(OperationResult(
        std::move(res), _resultBuilder.steal(), _options, countErrorCodes));
  }

 private:
  auto processValue(VPackSlice value, bool isMultiple) -> Result {
    Result res;

    std::string_view key(transaction::helpers::extractKeyPart(value));
    if (key.empty()) {
      res.reset(TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD);
    } else {
      bool conflict = false;
      auto cb = [&](LocalDocumentId, aql::DocumentData&&, VPackSlice doc) {
        if (!_options.ignoreRevs && value.isObject()) {
          RevisionId expectedRevision = RevisionId::fromSlice(value);
          if (expectedRevision.isSet()) {
            RevisionId foundRevision =
                transaction::helpers::extractRevFromDocument(doc);
            if (expectedRevision != foundRevision) {
              if (!isMultiple) {
                // still return
                buildDocumentIdentity(key, foundRevision, RevisionId::none(),
                                      nullptr, nullptr);
              }
              conflict = true;
              return false;
            }
          }
        }

        if (!_options.silent) {
          _resultBuilder.add(doc);
        } else if (isMultiple) {
          _resultBuilder.add(VPackSlice::nullSlice());
        }
        return true;
      };
      res = _collection.getPhysical()->lookup(&_methods, key, cb,
                                              {.countBytes = true});

      if (conflict) {
        res.reset(TRI_ERROR_ARANGO_CONFLICT);
      }
    }
    return res;
  };
};

template<class Derived>
struct ReplicatedProcessorBase : GenericProcessor<Derived> {
  ReplicatedProcessorBase(Methods& methods, TransactionCollection& trxColl,
                          LogicalCollection& collection, VPackSlice value,
                          OperationOptions& options,
                          std::string_view operationName,
                          TRI_voc_document_operation_e operationType)
      : GenericProcessor<Derived>(methods, trxColl, collection, value, options),
        _indexesSnapshot(collection.getPhysical()->getIndexesSnapshot()),
        _replicationData(&methods),
        _operationType(operationType),
        _needToFetchOldDocument(
            operationType == TRI_VOC_DOCUMENT_OPERATION_UPDATE ||
            _indexesSnapshot.hasSecondaryIndex() || options.returnOld ||
            !options.versionAttribute.empty()),
        _replicationVersion(collection.replicationVersion()) {
    // this call will populate replicationType and followers
    Result res = this->_methods.determineReplicationTypeAndFollowers(
        this->_collection, operationName, this->_value, this->_options,
        _replicationType, _followers);

    if (res.fail()) {
      THROW_ARANGO_EXCEPTION(res);
    }

    _excludeAllFromReplication =
        _replicationType != Methods::ReplicationType::LEADER ||
        (_followers->empty() &&
         this->_collection.replicationVersion() != replication::Version::TWO);
  }

  static constexpr AccessMode::Type accessMode() {
    return AccessMode::Type::WRITE;
  }

  Future<OperationResult> execute() {
    _replicationData->openArray(true);
    std::unordered_map<ErrorCode, size_t> errorCounter;
    Result res;

    // On replication2 we want to report all errors, even in the case of
    // followers, for debugging purposes. On replication1, we avoid doing so,
    // because it costs us extra network traffic, which is ignored by the leader
    // anyway.
    bool reportAllBabiesErrors =
        _replicationVersion == replication::Version::TWO ||
        _replicationType != Methods::ReplicationType::FOLLOWER;

    if (this->_value.isArray()) {
      // we are processing an array of input values here.
      this->_resultBuilder.openArray();

      // this result will initially be ok(), but it can be changed
      // to TRI_ERROR_RESOURCE_LIMIT. once it is set to
      // TRI_ERROR_RESOURCE_LIMIT, it won't change.
      // the rationale for this is to abort all following operations
      // in the batch quickly once we hit a resource limit (e.g.
      // we exceed the maximum transaction size).
      // this is safe as executing even more operations in a
      // transaction that has already reached its resource limits
      // won't make it better.
      // on the plus side, quickly making all following operations
      // fail will save us from repeatedly rolling back the following
      // operations, which can be painfully slow for large write
      // batches in RocksDB.
      Result resourceRes;

      for (VPackSlice s : VPackArrayIterator(this->_value)) {
        if (resourceRes.fail()) {
          res.reset(resourceRes);
        } else {
          res = this->self().processValue(s, true);
        }
        if (res.fail()) {
          if (res.is(TRI_ERROR_RESOURCE_LIMIT) && resourceRes.ok()) {
            // change resourceRes from ok() to TRI_ERROR_RESOURCE_LIMIT
            resourceRes.reset(res);
          }
          createBabiesError(
              reportAllBabiesErrors ? &this->_resultBuilder : nullptr,
              errorCounter, res);
          // unconditionally reset to global res(), as the API for
          // batch operations always returns success even when individual
          // operations fail
          res.reset();
        }
      }

      this->_resultBuilder.close();
    } else {
      res = this->self().processValue(this->_value, false);

      // on a follower, our result should always be an empty object
      if (_replicationType == Methods::ReplicationType::FOLLOWER) {
        TRI_ASSERT(this->_resultBuilder.slice().isNone());
        // add an empty object here so that when sending things back in JSON
        // format, there is no "non-representable type 'none'" issue.
        this->_resultBuilder.add(VPackSlice::emptyObjectSlice());
      }
    }
    _replicationData->close();

    // we are done with indexes. release the lock on the list of indexes as
    // early as possible
    _indexesSnapshot.release();

    // on a replication1 follower, our result should always be an empty array or
    // object
    TRI_ASSERT(_replicationVersion == replication::Version::TWO ||
               _replicationType != Methods::ReplicationType::FOLLOWER ||
               (this->_value.isArray() &&
                this->_resultBuilder.slice().isEmptyArray()) ||
               (this->_value.isObject() &&
                this->_resultBuilder.slice().isEmptyObject()));
    TRI_ASSERT(_replicationData->slice().isArray());
    TRI_ASSERT(_replicationType != Methods::ReplicationType::FOLLOWER ||
               _replicationData->slice().isEmptyArray());
    // On replication2, followers are expected to report babies errors
    TRI_ASSERT((!this->_value.isArray() ||
                _replicationVersion == replication::Version::TWO) ||
               this->_options.silent ||
               this->_resultBuilder.slice().length() == this->_value.length())
        << "operation: " << _operationType
        << ", silent: " << this->_options.silent
        << ", replicationType: " << static_cast<int>(this->_replicationType)
        << ", isArray: " << this->_value.isArray();

    TRI_ASSERT(res.ok() || !this->_value.isArray());

    TRI_IF_FAILURE("insertLocal::fakeResult2") { res.reset(TRI_ERROR_DEBUG); }

    auto resDocs = this->_resultBuilder.steal();
    auto intermediateCommit = futures::makeFuture(res);
    if (res.ok()) {
#ifdef ARANGODB_USE_GOOGLE_TESTS
      StorageEngine& engine = this->_collection.vocbase().engine();

      bool isMock = (engine.typeName() == "Mock");
#else
      constexpr bool isMock = false;
#endif

      if (!isMock && _replicationType == Methods::ReplicationType::LEADER &&
          (!_followers->empty() ||
           _replicationVersion == replication::Version::TWO) &&
          !_replicationData->slice().isEmptyArray()) {
        // In the multi babies case res is always TRI_ERROR_NO_ERROR if we
        // get here, in the single document case, we do not try to replicate
        // in case of an error.

        // Now replicate the good operations on all followers:
        return this->_methods
            .replicateOperations(this->_trxColl, _followers, this->_options,
                                 *this->_replicationData, _operationType,
                                 this->_methods.username())
            .thenValue([options = this->_options,
                        errs = std::move(errorCounter),
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
      intermediateCommit =
          this->_methods.state()->performIntermediateCommitIfRequired(
              this->_collection.id());
    }

    if (this->_options.silent && errorCounter.empty()) {
      // We needed the results, but do not want to report:
      resDocs->clear();
    }

    return std::move(intermediateCommit)
        .thenValue([options = this->_options,
                    errorCounter = std::move(errorCounter),
                    resDocs = std::move(resDocs)](auto&& res) mutable {
          return OperationResult(res, std::move(resDocs), options,
                                 std::move(errorCounter));
        });
  }

 protected:
  void trackWaitForSync() {
    if (this->_collection.waitForSync() && !this->_options.isRestore) {
      this->_options.waitForSync = true;
    }
    if (this->_options.waitForSync) {
      this->_methods.state()->waitForSync(true);
    }
  }

  std::shared_ptr<std::vector<ServerID> const> _followers;
  // indexes snapshot held for operation. note that this will not only contain
  // the list of indexes for the collection, but also protect the list of
  // indexes of the collection from being modified. this is to ensure that while
  // a data modification operation is ongoing, the collection's list of indexes
  // is static and cannot differ between performing the modification and a
  // potential rollback of the modification.
  IndexesSnapshot _indexesSnapshot;

  // all document data that are going to be replicated, append-only
  transaction::BuilderLeaser _replicationData;
  Methods::ReplicationType _replicationType = Methods::ReplicationType::NONE;
  TRI_voc_document_operation_e _operationType;
  bool _excludeAllFromReplication;
  // whether or not we need to read the previous document version
  bool _needToFetchOldDocument;
  // whether we use replication 1 or 2
  replication::Version _replicationVersion;
};

struct RemoveProcessor : ReplicatedProcessorBase<RemoveProcessor> {
  RemoveProcessor(Methods& methods, TransactionCollection& trxColl,
                  LogicalCollection& collection, VPackSlice value,
                  OperationOptions& options)
      : ReplicatedProcessorBase(methods, trxColl, collection, value, options,
                                "remove", TRI_VOC_DOCUMENT_OPERATION_REMOVE),
        _keyBuilder(&_methods),
        _previousDocumentBuilder(&_methods) {}

  auto processValue(VPackSlice value, bool isArray) -> Result {
#ifdef ARANGODB_ENABLE_FAILURE_TESTS
    TRI_IF_FAILURE("failOnCRUDAction" + _collection.name()) {
      return {TRI_ERROR_DEBUG, "Intentional test error"};
    }
#endif
    std::string_view key;

    if (value.isString()) {
      key = value.stringView();
      // strip everything before a / (likely an _id value)
      size_t pos = key.find('/');
      if (pos != std::string::npos) {
        key = key.substr(pos + 1);
        _keyBuilder->clear();
        _keyBuilder->add(VPackValue(key));
        value = _keyBuilder->slice();
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
    Result res = _collection.getPhysical()->lookupKeyForUpdate(&_methods, key,
                                                               lookupResult);
    if (res.fail()) {
      // Error reporting in the babies case is done outside of here.
      if (res.is(TRI_ERROR_ARANGO_CONFLICT) && !isArray) {
        TRI_ASSERT(_replicationType != Methods::ReplicationType::FOLLOWER);
        // if possible we want to provide information about the conflicting
        // document, so we need another lookup. However, theoretically it is
        // possible that the document has been deleted by now.
        auto lookupRes = _collection.getPhysical()->lookupKey(
            &_methods, key, lookupResult, ReadOwnWrites::yes);
        TRI_ASSERT(!lookupRes.ok() || lookupResult.second.isSet());
        buildDocumentIdentity(key, lookupResult.second, RevisionId::none(),
                              nullptr, nullptr);
        auto index = _collection.getPhysical()->primaryIndex();
        if (index) {
          return index->addErrorMsg(res, key);
        }
      }
      return res;
    }

    std::ignore = _methods.state()->ensureSnapshot();

    TRI_ASSERT(lookupResult.first.isSet());
    TRI_ASSERT(lookupResult.second.isSet());
    auto [oldDocumentId, oldRevisionId] = lookupResult;

    _previousDocumentBuilder->clear();
    if (_needToFetchOldDocument) {
      // need to read the full document, so that we can remove all
      // secondary index entries for it
      auto cb = IndexIterator::makeDocumentCallback(*_previousDocumentBuilder);
      res = _collection.getPhysical()->lookup(
          &_methods, oldDocumentId, cb,
          {.fillCache = false, .readOwnWrites = true, .countBytes = false});

      if (res.fail()) {
        return res;
      }
    } else {
      TRI_ASSERT(!_options.returnOld);
      // no need to read the full document, as we don't have secondary
      // index entries to remove. now build an artificial document with
      // just _key for our removal operation
      buildDocumentStub(*_previousDocumentBuilder, key, oldRevisionId);
    }

    TRI_ASSERT(_previousDocumentBuilder->slice().isObject());

    res = removeLocalHelper(value, oldDocumentId, oldRevisionId);

    // we should never get a write-write conflict here since the key is
    // already locked earlier, and in case of a remove there cannot be any
    // conflicts on unique index entries. However, we can still have a
    // conflict error due to a violated precondition when a specific _rev is
    // provided.
    if ((res.is(TRI_ERROR_ARANGO_CONFLICT) && !isArray) ||
        (res.ok() && !_options.silent)) {
      TRI_ASSERT(oldRevisionId.isSet());
      buildDocumentIdentity(
          key, oldRevisionId, RevisionId::none(),
          _options.returnOld ? _previousDocumentBuilder.get() : nullptr,
          nullptr);
    }

    if (res.ok() && !_excludeAllFromReplication) {
      _replicationData->openObject(/*unindexed*/ true);
      _replicationData->add(StaticStrings::KeyString, VPackValue(key));

      char ridBuffer[arangodb::basics::maxUInt64StringSize];
      _replicationData->add(StaticStrings::RevString,
                            oldRevisionId.toValuePair(ridBuffer));
      _replicationData->close();
    }

    return res;
  }

 private:
  Result removeLocalHelper(velocypack::Slice value,
                           LocalDocumentId previousDocumentId,
                           RevisionId previousRevisionId) {
    TRI_IF_FAILURE("LogicalCollection::remove") { return {TRI_ERROR_DEBUG}; }

    // check revisions only if value is a proper object. if value is simply
    // a key, we cannot check the revisions.
    if (!_options.ignoreRevs && value.isObject()) {
      // Check old revision:
      auto checkRevision = [](RevisionId expected, RevisionId found) noexcept {
        return expected.empty() || found == expected;
      };

      RevisionId expectedRevision = RevisionId::fromSlice(value);
      if (expectedRevision.isSet() &&
          !checkRevision(expectedRevision, previousRevisionId)) {
        return {TRI_ERROR_ARANGO_CONFLICT,
                "conflict, _rev values do not match"};
      }
    }

    Result res = _collection.getPhysical()->remove(
        _methods, _indexesSnapshot, previousDocumentId, previousRevisionId,
        _previousDocumentBuilder->slice(), _options);

    if (res.ok()) {
      trackWaitForSync();
    }

    return res;
  }

  // temporary builder for building keys
  transaction::BuilderLeaser _keyBuilder;
  // builder for a single, old version of document (will be recycled for each
  // document)
  transaction::BuilderLeaser _previousDocumentBuilder;
};

template<class Derived>
struct ModifyingProcessorBase : ReplicatedProcessorBase<Derived> {
  ModifyingProcessorBase(Methods& methods, TransactionCollection& trxColl,
                         LogicalCollection& collection, VPackSlice value,
                         OperationOptions& options,
                         std::string_view operationName,
                         TRI_voc_document_operation_e operationType)
      : ReplicatedProcessorBase<Derived>(methods, trxColl, collection, value,
                                         options, operationName, operationType),
        _batchOptions(::buildBatchOptions(options, collection, operationType,
                                          methods.state()->isDBServer())) {
    // in UPDATE operations we always have to fetch the previous version
    // of the document. in REPLACE operations we don't need to fetch it under
    // some circumstances. but if we do have a schema, we need to fetch the
    // old version anyway if the schema validation is configured to accept
    // invalid documents if the old document didn't pass as well ("level"
    // attribute of a schema).
    // in addition, on a REPLACE operation, if a collection uses non-default
    // shard-keys, we also need to fetch the old document to check if shard
    // keys changed.
    this->_needToFetchOldDocument |=
        operationType == TRI_VOC_DOCUMENT_OPERATION_REPLACE &&
        (_batchOptions.schema != nullptr ||
         !collection.usesDefaultShardKeys() ||
         collection.hasSmartJoinAttribute());
  }

 protected:
  Result modifyLocalHelper(velocypack::Slice value,
                           LocalDocumentId previousDocumentId,
                           RevisionId previousRevisionId,
                           velocypack::Slice previousDocument,
                           RevisionId& newRevisionId,
                           velocypack::Builder& newDocumentBuilder,
                           bool isUpdate) {
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

    if (!this->_options.ignoreRevs) {
      // Check old revision:
      auto checkRevision = [](RevisionId expected, RevisionId found) noexcept {
        return expected.empty() || found == expected;
      };

      RevisionId expectedRevision = RevisionId::fromSlice(value);
      if (expectedRevision.isSet() &&
          !checkRevision(expectedRevision, previousRevisionId)) {
        return {TRI_ERROR_ARANGO_CONFLICT,
                "conflict, _rev values do not match"};
      }
    }

    // no-op update: no values in the document are changed. in this case we
    // do not perform any update, but simply return. note: no-op updates are
    // not allowed if there are computed attributes.
    bool isNoOp =
        (value.length() <= 1 && isUpdate && !this->_options.isRestore &&
         this->_options.isSynchronousReplicationFrom.empty() &&
         _batchOptions.computedValues == nullptr);

    if (this->_replicationType != Methods::ReplicationType::FOLLOWER &&
        !this->_options.versionAttribute.empty()) {
      // check document versions
      std::optional<uint64_t> previousVersion;
      std::optional<uint64_t> currentVersion;
      if (VPackSlice previous =
              previousDocument.get(this->_options.versionAttribute);
          previous.isNumber()) {
        try {
          previousVersion = previous.getNumericValue<uint64_t>();
        } catch (...) {
          // no error reported from here
        }
      }
      if (VPackSlice current = value.get(this->_options.versionAttribute);
          current.isNumber()) {
        try {
          currentVersion = current.getNumericValue<uint64_t>();
        } catch (...) {
          // no error reported from here
        }
      }
      if (previousVersion.has_value() && currentVersion.has_value() &&
          *currentVersion <= *previousVersion) {
        // attempt to update a document with an older version
        isNoOp = true;
        value = previousDocument;
      }
    }

    // merge old and new values
    Result res;
    if (isUpdate) {
      res = mergeObjectsForUpdate(
          this->_methods, this->_collection, previousDocument, value, isNoOp,
          previousRevisionId, newRevisionId, newDocumentBuilder, this->_options,
          _batchOptions);
    } else {
      res = newObjectForReplace(
          this->_methods, this->_collection, previousDocument, value, isNoOp,
          previousRevisionId, newRevisionId, newDocumentBuilder, this->_options,
          _batchOptions);
    }

    if (res.ok()) {
      if (isNoOp) {
        // shortcut. no need to do anything
        TRI_ASSERT(_batchOptions.computedValues == nullptr);
        TRI_ASSERT(previousRevisionId == newRevisionId);
        this->trackWaitForSync();
        return {};
      }

      TRI_ASSERT(newRevisionId.isSet());

      // Need to check that no sharding keys have changed:
      if (_batchOptions.validateShardKeysOnUpdateReplace &&
          shardKeysChanged(this->_collection, previousDocument,
                           newDocumentBuilder.slice(), isUpdate)) {
        return {TRI_ERROR_CLUSTER_MUST_NOT_CHANGE_SHARDING_ATTRIBUTES};
      }

      if (_batchOptions.validateSmartJoinAttribute &&
          smartJoinAttributeChanged(this->_collection, previousDocument,
                                    newDocumentBuilder.slice(), isUpdate)) {
        return {TRI_ERROR_CLUSTER_MUST_NOT_CHANGE_SMART_JOIN_ATTRIBUTE};
      }

      // note: schema can be a nullptr here, but we need to call validate()
      // anyway. the reason is that validate() does not only perform schema
      // validation, but also some validation for SmartGraph data
      res = this->_collection.validate(
          _batchOptions.schema, newDocumentBuilder.slice(), previousDocument,
          this->_methods.transactionContextPtr()->getVPackOptions());

      if (res.ok()) {
        if (isUpdate) {
          res = this->_collection.getPhysical()->update(
              this->_methods, this->_indexesSnapshot, previousDocumentId,
              previousRevisionId, previousDocument, newRevisionId,
              newDocumentBuilder.slice(), this->_options);
        } else {
          res = this->_collection.getPhysical()->replace(
              this->_methods, this->_indexesSnapshot, previousDocumentId,
              previousRevisionId, previousDocument, newRevisionId,
              newDocumentBuilder.slice(), this->_options);
        }
      }

      if (res.ok()) {
        this->trackWaitForSync();
      }
    }

    return res;
  }

  BatchOptions _batchOptions;
};

struct InsertProcessor : ModifyingProcessorBase<InsertProcessor> {
  InsertProcessor(Methods& methods, TransactionCollection& trxColl,
                  LogicalCollection& collection, VPackSlice value,
                  OperationOptions& options)
      : ModifyingProcessorBase(methods, trxColl, collection, value, options,
                               "insert", TRI_VOC_DOCUMENT_OPERATION_INSERT),
        _newDocumentBuilder(&_methods),
        // if no overwriteMode is specified we default to Conflict
        _overwriteMode(options.isOverwriteModeSet()
                           ? options.overwriteMode
                           : OperationOptions::OverwriteMode::Conflict) {
    if (_replicationType == Methods::ReplicationType::FOLLOWER &&
        _overwriteMode == OperationOptions::OverwriteMode::Update) {
      // we must turn any INSERT with overwriteMode UPDATE to overwriteMode
      // REPLACE on followers. the reason is that the replication sends the full
      // document as inserted on the leader, but it does not send any null
      // attributes that were removed from the document during the UPDATE.
      _overwriteMode = OperationOptions::OverwriteMode::Replace;
    }
  }

  auto processValue(VPackSlice value, bool isArray) -> Result {
    TRI_IF_FAILURE("insertLocal::fakeResult1") {  //
      // return an error *instead* of actually processing the value
      return TRI_ERROR_DEBUG;
    }
#ifdef ARANGODB_ENABLE_FAILURE_TESTS
    TRI_IF_FAILURE("failOnCRUDAction" + _collection.name()) {
      return {TRI_ERROR_DEBUG, "Intentional test error"};
    }
#endif

    _newDocumentBuilder->clear();

    if (!value.isObject()) {
      return {TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID};
    }

    std::string keyHolder;
    LocalDocumentId oldDocumentId;
    RevisionId oldRevisionId = RevisionId::none();
    std::string_view key;
    {
      VPackSlice keySlice = value.get(StaticStrings::KeyString);
      if (keySlice.isNone()) {
        TRI_ASSERT(!_options.isRestore);  // need key in case of restore
        keyHolder = _collection.keyGenerator().generate(value);

        if (keyHolder.empty()) {
          return Result(TRI_ERROR_ARANGO_OUT_OF_KEYS);
        }

        key = keyHolder;
      } else if (keySlice.isString()) {
        key = keySlice.stringView();

        // we have to validate the key here to prevent an error during the
        // lookup
        auto err =
            _collection.keyGenerator().validate(key, value, _options.isRestore);
        if (err != TRI_ERROR_NO_ERROR) {
          return {err};
        }
      } else {
        return Result(TRI_ERROR_ARANGO_DOCUMENT_KEY_BAD);
      }
    }

    std::pair<LocalDocumentId, RevisionId> lookupResult;
    auto res = _collection.getPhysical()->lookupKeyForUpdate(&_methods, key,
                                                             lookupResult);
    if (res.ok()) {
      // primary key already exists!
      if (_overwriteMode == OperationOptions::OverwriteMode::Conflict) {
        IndexOperationMode mode = _options.indexOperationMode;
        if (mode == IndexOperationMode::internal) {
          // in this error mode, we return the conflicting document's key
          // inside the error message string (and nothing else)!
          return Result{TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED, key};
        }
        // otherwise build a proper error message
        Result result{TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED};
        auto index = _collection.getPhysical()->primaryIndex();
        if (index) {
          return index->addErrorMsg(result, key);
        }
        return result;
      } else {
        TRI_ASSERT(lookupResult.first.isSet());
        TRI_ASSERT(lookupResult.second.isSet());
        std::tie(oldDocumentId, oldRevisionId) = lookupResult;
      }
    } else if (res.isNot(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND)) {
      // Error reporting in the babies case is done outside of here.
      if (res.is(TRI_ERROR_ARANGO_CONFLICT) && !isArray) {
        TRI_ASSERT(_replicationType != Methods::ReplicationType::FOLLOWER);
        // if possible we want to provide information about the conflicting
        // document, so we need another lookup. However, it is possible that
        // the other transaction has not been committed yet, or that the
        // document has been deleted by now.
        auto lookupRes = _collection.getPhysical()->lookupKey(
            &_methods, key, lookupResult, ReadOwnWrites::yes);
        TRI_ASSERT(!lookupRes.ok() || lookupResult.second.isSet());
        buildDocumentIdentity(key, lookupResult.second, RevisionId::none(),
                              nullptr, nullptr);
        auto index = _collection.getPhysical()->primaryIndex();
        if (index) {
          return index->addErrorMsg(res, key);
        }
      }
      return res;
    }

    std::ignore = _methods.state()->ensureSnapshot();

    // only populated for update/replace
    transaction::BuilderLeaser previousDocumentBuilder(&_methods);

    RevisionId newRevisionId;
    bool didReplace = false;
    bool excludeFromReplication = _excludeAllFromReplication;

    if (!oldDocumentId.isSet()) {
      // regular insert without overwrite option. the insert itself will check
      // if the primary key already exists
      res = insertLocalHelper(key, value, newRevisionId);

      TRI_ASSERT(res.fail() || _newDocumentBuilder->slice().isObject());
    } else {
      // RepSert Case - unique_constraint violated ->  try update, replace or
      // ignore!
      TRI_ASSERT(res.ok());
      TRI_ASSERT(oldDocumentId.isSet());
      TRI_ASSERT(_overwriteMode != OperationOptions::OverwriteMode::Conflict);

      if (_overwriteMode == OperationOptions::OverwriteMode::Ignore) {
        // in case of unique constraint violation: ignore and do nothing (no
        // write!)
        if (_replicationType != Methods::ReplicationType::FOLLOWER) {
          // intentionally do not fill replicationData here
          buildDocumentIdentity(key, oldRevisionId, RevisionId::none(), nullptr,
                                nullptr);
        }
        return res;
      }

      if (_overwriteMode == OperationOptions::OverwriteMode::Update ||
          _overwriteMode == OperationOptions::OverwriteMode::Replace) {
        // in case of unique constraint violation: (partially) update existing
        // document.
        previousDocumentBuilder->clear();
        auto cb = IndexIterator::makeDocumentCallback(*previousDocumentBuilder);
        res = _collection.getPhysical()->lookup(
            &_methods, oldDocumentId, cb,
            {.fillCache = false, .readOwnWrites = true, .countBytes = false});

        if (res.ok()) {
          TRI_ASSERT(previousDocumentBuilder->slice().isObject());

          res = modifyLocalHelper(value, oldDocumentId, oldRevisionId,
                                  previousDocumentBuilder->slice(),
                                  newRevisionId, *_newDocumentBuilder,
                                  /*isUpdate*/ _overwriteMode ==
                                      OperationOptions::OverwriteMode::Update);
        }

        TRI_ASSERT(res.fail() || _newDocumentBuilder->slice().isObject());

        if (res.ok() && oldRevisionId == newRevisionId &&
            (_overwriteMode == OperationOptions::OverwriteMode::Update ||
             _overwriteMode == OperationOptions::OverwriteMode::Replace)) {
          // did not actually update - intentionally do not fill
          // replicationData
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
      if (res.is(TRI_ERROR_ARANGO_CONFLICT) && !isArray &&
          _replicationType != Methods::ReplicationType::FOLLOWER) {
        // this indicates a write-write conflict while updating some unique
        // index
        buildDocumentIdentity(key, oldRevisionId, RevisionId::none(), nullptr,
                              nullptr);
      }
      // intentionally do not fill replicationData here
      return res;
    }

    TRI_ASSERT(res.ok());

    if (!_options.silent) {
      TRI_ASSERT(_newDocumentBuilder->slice().isObject());

      bool const showReplaced = (_options.returnOld && didReplace);
      TRI_ASSERT(!_options.returnNew || !_newDocumentBuilder->isEmpty());
      TRI_ASSERT(!showReplaced || oldRevisionId.isSet());
      TRI_ASSERT(!showReplaced || previousDocumentBuilder->slice().isObject());

      buildDocumentIdentity(
          key, newRevisionId, oldRevisionId,
          showReplaced ? previousDocumentBuilder.get() : nullptr,
          _options.returnNew ? _newDocumentBuilder.get() : nullptr);
    }

    if (!excludeFromReplication) {
      TRI_ASSERT(_newDocumentBuilder->slice().isObject());
      // _id values are written to the database as VelocyPack Custom values.
      // However, these cannot be transferred as Custom types, because the
      // VelocyPack validator on the receiver side will complain about them.
      // so we need to rewrite the document here to not include any Custom
      // types.
      // replicationData->add(newDocumentBuilder->slice());
      basics::VelocyPackHelper::sanitizeNonClientTypes(
          _newDocumentBuilder->slice(), VPackSlice::noneSlice(),
          *_replicationData,
          *_methods.transactionContextPtr()->getVPackOptions(),
          /*allowUnindexed*/ false);
    }

    return res;
  }

 private:
  Result insertLocalHelper(std::string_view key, velocypack::Slice value,
                           RevisionId& newRevisionId) {
    TRI_IF_FAILURE("LogicalCollection::insert") { return {TRI_ERROR_DEBUG}; }

    Result res = transaction::helpers::newObjectForInsert(
        _methods, _collection, key, value, newRevisionId, *_newDocumentBuilder,
        _options, _batchOptions);

    if (res.ok()) {
      TRI_ASSERT(newRevisionId.isSet());
      TRI_ASSERT(_newDocumentBuilder->slice().isObject());

      if (_batchOptions.validateSmartJoinAttribute) {
        auto r = transaction::Methods::validateSmartJoinAttribute(
            _collection, _newDocumentBuilder->slice());

        if (r != TRI_ERROR_NO_ERROR) {
          return res.reset(r);
        }
      }

#ifdef ARANGODB_USE_GOOGLE_TESTS
      StorageEngine& engine = _collection.vocbase().engine();

      bool isMock = (engine.typeName() == "Mock");
#else
      constexpr bool isMock = false;
#endif
      // note: schema can be a nullptr here, but we need to call validate()
      // anyway. the reason is that validate() does not only perform schema
      // validation, but also some validation for SmartGraph data
      if (!isMock) {
        res = _collection.validate(
            _batchOptions.schema, _newDocumentBuilder->slice(),
            _methods.transactionContextPtr()->getVPackOptions());
      }
    }

    if (res.ok()) {
      res = _collection.getPhysical()->insert(
          _methods, _indexesSnapshot, newRevisionId,
          _newDocumentBuilder->slice(), _options);

      if (res.ok()) {
        trackWaitForSync();
      }
    }

    // return final result
    return res;
  }

  // builder for a single document (will be recycled for each document)
  transaction::BuilderLeaser _newDocumentBuilder;

  OperationOptions::OverwriteMode _overwriteMode;
};

struct ModifyProcessor : ModifyingProcessorBase<ModifyProcessor> {
  ModifyProcessor(Methods& methods, TransactionCollection& trxColl,
                  LogicalCollection& collection, VPackSlice value,
                  OperationOptions& options, bool isUpdate)
      : ModifyingProcessorBase(methods, trxColl, collection, value, options,
                               isUpdate ? "update" : "replace",
                               isUpdate ? TRI_VOC_DOCUMENT_OPERATION_UPDATE
                                        : TRI_VOC_DOCUMENT_OPERATION_REPLACE),
        _newDocumentBuilder(&_methods),
        _previousDocumentBuilder(&_methods),
        _isUpdate(isUpdate) {}

  auto processValue(VPackSlice newValue, bool isArray) -> Result {
#ifdef ARANGODB_ENABLE_FAILURE_TESTS
    TRI_IF_FAILURE("failOnCRUDAction" + _collection.name()) {
      return {TRI_ERROR_DEBUG, "Intentional test error"};
    }
#endif
    _newDocumentBuilder->clear();
    _previousDocumentBuilder->clear();

    if (!newValue.isObject()) {
      return {TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID};
    }

    VPackSlice key = newValue.get(StaticStrings::KeyString);
    if (key.isNone()) {
      return {TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD};
    } else if (!key.isString() || key.stringView().empty()) {
      return {TRI_ERROR_ARANGO_DOCUMENT_KEY_BAD};
    }

    // replace and update are two operations each, thus this can and must not
    // be single document operations. We need to have a lock here already.
    TRI_ASSERT(_methods.isLocked(&_collection, AccessMode::Type::WRITE));

    std::pair<LocalDocumentId, RevisionId> lookupResult;
    Result res = _collection.getPhysical()->lookupKeyForUpdate(
        &_methods, key.stringView(), lookupResult);
    if (res.fail()) {
      if (res.is(TRI_ERROR_ARANGO_CONFLICT) && !isArray) {
        TRI_ASSERT(_replicationType != Methods::ReplicationType::FOLLOWER);
        // if possible we want to provide information about the conflicting
        // document, so we need another lookup. However, it is possible that
        // the other transaction has not been committed yet, or that the
        // document has been deleted by now.
        auto lookupRes = _collection.getPhysical()->lookupKey(
            &_methods, key.stringView(), lookupResult, ReadOwnWrites::yes);
        TRI_ASSERT(!lookupRes.ok() || lookupResult.second.isSet());
        buildDocumentIdentity(key.stringView(), lookupResult.second,
                              RevisionId::none(), nullptr, nullptr);
        auto index = _collection.getPhysical()->primaryIndex();
        if (index) {
          return index->addErrorMsg(res, key.stringView());
        }
      }
      return res;
    }

    std::ignore = _methods.state()->ensureSnapshot();

    TRI_ASSERT(lookupResult.first.isSet());
    TRI_ASSERT(lookupResult.second.isSet());
    auto [oldDocumentId, oldRevisionId] = lookupResult;

    _previousDocumentBuilder->clear();
    if (_needToFetchOldDocument) {
      // need to read the full document, so that we can remove all
      // secondary index entries for it
      auto cb = IndexIterator::makeDocumentCallback(*_previousDocumentBuilder);
      res = _collection.getPhysical()->lookup(
          &_methods, oldDocumentId, cb,
          {.fillCache = false, .readOwnWrites = true, .countBytes = false});

      if (res.fail()) {
        return res;
      }
    } else {
      TRI_ASSERT(!_options.returnOld);
      TRI_ASSERT(!_isUpdate);
      // no need to read the full document, as we don't have secondary
      // index entries to remove. now build an artificial document with
      // just _key for our removal operation
      buildDocumentIdentityCustom(key.stringView(), oldRevisionId);
    }

    TRI_ASSERT(_previousDocumentBuilder->slice().isObject());
    RevisionId newRevisionId;
    res = modifyLocalHelper(newValue, oldDocumentId, oldRevisionId,
                            _previousDocumentBuilder->slice(), newRevisionId,
                            *_newDocumentBuilder, _isUpdate);

    if (res.fail()) {
      if (res.is(TRI_ERROR_ARANGO_CONFLICT) && !isArray) {
        // this indicates a write-write conflict while updating some unique
        // index
        TRI_ASSERT(oldRevisionId.isSet());
        buildDocumentIdentity(
            key.stringView(), oldRevisionId, RevisionId::none(),
            _options.returnOld ? _previousDocumentBuilder.get() : nullptr,
            nullptr);
      }
      return res;
    }

    TRI_ASSERT(res.ok());
    TRI_ASSERT(newRevisionId.isSet());
    TRI_ASSERT(_newDocumentBuilder->slice().isObject());

    bool excludeFromReplication = _excludeAllFromReplication;

    if (!_options.silent) {
      TRI_ASSERT(newRevisionId.isSet() && oldRevisionId.isSet());

      buildDocumentIdentity(
          key.stringView(), newRevisionId, oldRevisionId,
          _options.returnOld ? _previousDocumentBuilder.get() : nullptr,
          _options.returnNew ? _newDocumentBuilder.get() : nullptr);
      if (newRevisionId == oldRevisionId) {
        excludeFromReplication = true;
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
          _newDocumentBuilder->slice(), VPackSlice::noneSlice(),
          *_replicationData,
          *_methods.transactionContextPtr()->getVPackOptions(),
          /*allowUnindexed*/ false);
    }

    return res;
  }

 private:
  // builds a document stub with _id, _key and _rev. _id is of velocypack
  // type Custom here.
  void buildDocumentIdentityCustom(std::string_view key, RevisionId rid) {
    _previousDocumentBuilder->openObject();

    // _id
    uint8_t* p = _previousDocumentBuilder->add(
        StaticStrings::IdString, VPackValuePair(9ULL, VPackValueType::Custom));

    *p++ = 0xf3;  // custom type for _id

    if (_methods.state()->isDBServer() && !_collection.system()) {
      // db server in cluster, note: the local collections _statistics,
      // _statisticsRaw and _statistics15 (which are the only system
      // collections)
      // must not be treated as shards but as local collections
      encoding::storeNumber<uint64_t>(p, _collection.planId().id(),
                                      sizeof(uint64_t));
    } else {
      // local server
      encoding::storeNumber<uint64_t>(p, _collection.id().id(),
                                      sizeof(uint64_t));
    }

    // _key
    _previousDocumentBuilder->add(StaticStrings::KeyString, VPackValue(key));

    // _rev
    if (rid.isSet()) {
      char ridBuffer[arangodb::basics::maxUInt64StringSize];
      _previousDocumentBuilder->add(StaticStrings::RevString,
                                    rid.toValuePair(ridBuffer));
    } else {
      _previousDocumentBuilder->add(StaticStrings::RevString,
                                    std::string_view{});
    }

    _previousDocumentBuilder->close();
  }

  // builder for a single document (will be recycled for each document)
  transaction::BuilderLeaser _newDocumentBuilder;
  // builder for a single, old version of document (will be recycled for each
  // document)
  transaction::BuilderLeaser _previousDocumentBuilder;

  bool const _isUpdate;
};

}  // namespace

/*static*/ void transaction::Methods::addDataSourceRegistrationCallback(
    DataSourceRegistrationCallback const& callback) {
  if (callback) {
    getDataSourceRegistrationCallbacks().emplace_back(callback);
  }
}

template<transaction::Methods::CallbacksTag tag, typename Callback>
bool transaction::Methods::addCallbackImpl(Callback const* callback) {
  if (!callback || !*callback) {
    // nothing to call back
    return true;
  } else if (!_state) {
    // nothing to add to
    return false;
  }

  auto* statusChangeCallbacks = getCallbacks<tag>(*_state, true);

  TRI_ASSERT(nullptr != statusChangeCallbacks);  // 'create' was specified

  // no need to lock since transactions are single-threaded
  statusChangeCallbacks->emplace_back(callback);

  return true;
}

bool transaction::Methods::addStatusChangeCallback(
    StatusChangeCallback const* callback) {
  return addCallbackImpl<CallbacksTag::StatusChange>(callback);
}

bool transaction::Methods::removeStatusChangeCallback(
    StatusChangeCallback const* callback) {
  if (!callback || !*callback) {
    // nothing to call back
    return true;
  } else if (!_state) {
    // nothing to add to
    return false;
  }

  auto* statusChangeCallbacks =
      getCallbacks<CallbacksTag::StatusChange>(*_state, false);
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
  TRI_ASSERT(_state);
  return _state->vocbase();
}

void transaction::Methods::setUsername(std::string const& name) {
  TRI_ASSERT(_state);
  _state->setUsername(name);
}

std::string_view transaction::Methods::username() const noexcept {
  TRI_ASSERT(_state);
  return _state->username();
}

// is this instance responsible for commit / abort
bool transaction::Methods::isMainTransaction() const noexcept {
  return _mainTransaction;
}

/// @brief add a transaction hint
void transaction::Methods::addHint(transaction::Hints::Hint hint) noexcept {
  _localHints.set(hint);
}

/// @brief whether or not the transaction consists of a single operation only
bool transaction::Methods::isSingleOperationTransaction() const noexcept {
  TRI_ASSERT(_state);
  return _state->isSingleOperation();
}

/// @brief get the status of the transaction
transaction::Status transaction::Methods::status() const noexcept {
  TRI_ASSERT(_state);
  return _state->status();
}

std::string_view transaction::Methods::statusString() const noexcept {
  return transaction::statusString(status());
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
          r.is(TRI_ERROR_CLUSTER_SHARD_LEADER_REFUSES_REPLICATION);
      if (followerRefused) {
        return true;
      }
    }
  }
  return false;
}

transaction::Methods::Methods(std::shared_ptr<transaction::Context> ctx,
                              transaction::Options const& options)
    : _transactionContext(std::move(ctx)), _mainTransaction(false) {
  TRI_ASSERT(_transactionContext != nullptr);
  if (ADB_UNLIKELY(_transactionContext == nullptr)) {
    // in production, we must not go on with undefined behavior, so we bail out
    // here with an exception as last resort
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "invalid transaction context pointer");
  }

  // initialize the transaction. this can update _mainTransaction!
  _state = _transactionContext->acquireState(options, _mainTransaction);
  TRI_ASSERT(_state != nullptr);

  setUsername(ExecContext::current().user());
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
    std::shared_ptr<transaction::Context> ctx,
    std::vector<std::string> const& readCollections,
    std::vector<std::string> const& writeCollections,
    std::vector<std::string> const& exclusiveCollections,
    transaction::Options const& options)
    : transaction::Methods(std::move(ctx), options) {
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

    _state.reset();
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
    std::string_view name, AccessMode::Type type) const {
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

/// @brief begin the transaction
futures::Future<Result> transaction::Methods::beginAsync() {
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

    res = co_await _state->beginTransaction(_localHints);
    if (res.ok()) {
      _transactionContext->setCounterGuard(_state->counterGuard());

      res = applyStatusChangeCallbacks(*this, Status::RUNNING);
    }
  } else {
    TRI_ASSERT(_state->status() == transaction::Status::RUNNING);
  }

  co_return res;
}

auto Methods::commit() noexcept -> Result {
  return commitInternal(MethodsApi::Synchronous)
      .then(basics::tryToResult)
      .get();
}

/// @brief commit / finish the transaction
auto transaction::Methods::commitAsync() noexcept -> Future<Result> {
  return commitInternal(MethodsApi::Asynchronous).then(basics::tryToResult);
}

auto Methods::abort() noexcept -> Result {
  return abortInternal(MethodsApi::Synchronous).then(basics::tryToResult).get();
}

/// @brief abort the transaction
auto transaction::Methods::abortAsync() noexcept -> Future<Result> {
  return abortInternal(MethodsApi::Asynchronous).then(basics::tryToResult);
}

auto Methods::finish(Result const& res) noexcept -> Result {
  return finishInternal(res, MethodsApi::Synchronous)
      .then(basics::tryToResult)
      .get();
}

/// @brief finish a transaction (commit or abort), based on the previous state
auto transaction::Methods::finishAsync(Result const& res) noexcept
    -> Future<Result> {
  return finishInternal(res, MethodsApi::Asynchronous)
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
futures::Future<OperationResult> transaction::Methods::any(
    std::string const& collectionName, OperationOptions const& options) {
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
futures::Future<OperationResult> transaction::Methods::anyLocal(
    std::string const& collectionName, OperationOptions const& options) {
  DataSourceId cid =
      co_await addCollectionAtRuntime(collectionName, AccessMode::Type::READ);
  TransactionCollection* trxColl = trxCollection(cid);
  if (trxColl == nullptr) {
    throwCollectionNotFound(collectionName);
  }

  VPackBuilder resultBuilder;
  if (_state->isDBServer()) {
    std::shared_ptr<LogicalCollection> const& collection =
        trxColl->collection();
    if (collection == nullptr) {
      co_return OperationResult(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
                                options);
    }
    auto const& followerInfo = collection->followers();
    if (!followerInfo->getLeader().empty()) {
      co_return OperationResult(TRI_ERROR_CLUSTER_SHARD_LEADER_RESIGNED,
                                options);
    }
  }

  ResourceMonitor monitor(GlobalResourceMonitor::instance());

  resultBuilder.openArray();

  auto iterator =
      indexScan(monitor, collectionName, transaction::Methods::CursorType::ANY,
                ReadOwnWrites::no);

  auto cb = IndexIterator::makeDocumentCallback(resultBuilder);
  iterator->nextDocument(cb, 1);

  resultBuilder.close();

  co_return OperationResult(Result(), resultBuilder.steal(), options);
}

futures::Future<DataSourceId> transaction::Methods::addCollectionAtRuntime(
    DataSourceId cid, std::string_view collectionName, AccessMode::Type type) {
  auto collection = trxCollection(cid);

  if (collection == nullptr) {
    Result res = co_await _state->addCollection(cid, collectionName, type,
                                                /*lockUsage*/ true);

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
      auto message = absl::StrCat(
          TRI_errno_string(TRI_ERROR_TRANSACTION_UNREGISTERED_COLLECTION), ": ",
          collectionName, " [", AccessMode::typeString(type), "]");
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_TRANSACTION_UNREGISTERED_COLLECTION, std::move(message));
    }
  }

  TRI_ASSERT(collection != nullptr);
  co_return cid;
}

/// @brief add a collection to the transaction for read, at runtime
futures::Future<DataSourceId> transaction::Methods::addCollectionAtRuntime(
    std::string_view collectionName, AccessMode::Type type) {
  if (collectionName == _collectionCache.name && !collectionName.empty()) {
    co_return _collectionCache.cid;
  }

  TRI_ASSERT(!_state->isCoordinator());
  auto cid = resolver()->getCollectionIdLocal(collectionName);

  if (cid.empty()) {
    throwCollectionNotFound(collectionName);
  }
  co_await addCollectionAtRuntime(cid, collectionName, type);
  _collectionCache.cid = cid;
  _collectionCache.name = collectionName;
  co_return cid;
}

/// @brief return the type of a collection
TRI_col_type_e transaction::Methods::getCollectionType(
    std::string_view collectionName) const {
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
futures::Future<Result> transaction::Methods::documentFastPath(
    std::string const& collectionName, VPackSlice value,
    OperationOptions const& options, VPackBuilder& result) {
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
    co_return opRes.result;
  }

  auto translateName = [this](std::string const& collectionName) {
    if (_state->isDBServer()) {
      auto collection = resolver()->getCollectionStructCluster(collectionName);
      if (collection != nullptr) {
        auto& ci =
            vocbase().server().getFeature<ClusterFeature>().clusterInfo();
        auto shards = ci.getShardList(std::to_string(collection->id().id()));
        if (shards != nullptr && shards->size() == 1) {
          // Unfortunately we cannot do this assertion
          // on the _systemDatabase. The DBServer does
          // never set the oneShard flag there.
          TRI_ASSERT(vocbase().isSystem() || vocbase().isOneShard());
          return std::string{(*shards)[0]};
        }
      }
    }
    return collectionName;
  };

  std::string_view key(transaction::helpers::extractKeyPart(value));
  if (key.empty()) {
    co_return {TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD};
  }

  DataSourceId cid = co_await addCollectionAtRuntime(
      translateName(collectionName), AccessMode::Type::READ);

  TransactionCollection* trxColl = trxCollection(cid);
  if (trxColl == nullptr) {
    co_return {TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND};
  }
  auto const& collection = trxColl->collection();
  if (collection == nullptr) {
    co_return {TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND};
  }
  auto cb = IndexIterator::makeDocumentCallback(result);
  co_return collection->getPhysical()->lookup(this, key, cb,
                                              {.countBytes = true});
}

/// @brief return one document from a collection, fast path
///        If everything went well the result will contain the found document
///        (as an external on single_server) and this function will return
///        TRI_ERROR_NO_ERROR.
///        If there was an error the code is returned
///        Does not care for revision handling!
///        Must only be called on a local server, not in cluster case!
futures::Future<Result> transaction::Methods::documentFastPathLocal(
    std::string_view collectionName, std::string_view key,
    IndexIterator::DocumentCallback const& cb) {
  TRI_ASSERT(!ServerState::instance()->isCoordinator());
  TRI_ASSERT(_state->status() == transaction::Status::RUNNING);

  DataSourceId cid =
      co_await addCollectionAtRuntime(collectionName, AccessMode::Type::READ);
  TransactionCollection* trxColl = trxCollection(cid);
  if (trxColl == nullptr) {
    co_return {TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND};
  }
  std::shared_ptr<LogicalCollection> const& collection = trxColl->collection();
  TRI_ASSERT(collection != nullptr);
  if (collection == nullptr) {
    co_return {TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND};
  }

  if (key.empty()) {
    co_return {TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD};
  }

  // We never want to see our own writes here, otherwise we could observe
  // documents which have been inserted by a currently running query.
  co_return collection->getPhysical()->lookup(this, key, cb,
                                              {.countBytes = true});
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
  auto res = co_await GetDocumentProcessor::create(*this, collectionName, value,
                                                   options);
  if (res.fail()) {
    co_return OperationResult(std::move(res).result(), options);
  }
  co_return co_await res.get().execute();
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
  return insertDocumentOnCoordinator(*this, *colptr, value, options, api);
}
#endif

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
  }
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
    // Block operation early if we are not supposed to perform it:
    auto const& followerInfo = collection.followers();
    std::string theLeader = followerInfo->getLeader();
    if (theLeader.empty()) {
      // This indicates that we believe to be the leader.
      if (!options.isSynchronousReplicationFrom.empty()) {
        return {TRI_ERROR_CLUSTER_SHARD_LEADER_REFUSES_REPLICATION};
      }

      switch (followerInfo->allowedToWrite()) {
        case FollowerInfo::WriteState::FORBIDDEN: {
          // We cannot fulfill minimum replication Factor. Reject write.
          auto& clusterFeature =
              vocbase().server().getFeature<ClusterFeature>();
          if (clusterFeature.statusCodeFailedWriteConcern() == 403) {
            return {TRI_ERROR_ARANGO_READ_ONLY};
          }
          return {TRI_ERROR_REPLICATION_WRITE_CONCERN_NOT_FULFILLED};
        }
        case FollowerInfo::WriteState::UNAVAILABLE:
        case FollowerInfo::WriteState::STARTUP:
          return {TRI_ERROR_CLUSTER_BACKEND_UNAVAILABLE};
        default:
          break;
      }

      replicationType = ReplicationType::LEADER;
      TRI_IF_FAILURE("forceDropFollowersInTrxMethods") {
        for (auto followers = followerInfo->get();
             auto const& follower : *followers) {
          followerInfo->remove(follower);
        }
      }
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

      TRI_IF_FAILURE("synchronousReplication::blockReplication") {
        // Block here until the second failure point is switched on, too:
        bool leave = false;
        while (true) {
          TRI_IF_FAILURE("synchronousReplication::unblockReplication") {
            leave = true;
          }
          if (leave) {
            break;
          }
          std::this_thread::sleep_for(std::chrono::milliseconds(300));
        }
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
  // API compatibility: Let us always refuse a Replication request
  if (!options.isSynchronousReplicationFrom.empty()) {
    return {TRI_ERROR_CLUSTER_SHARD_LEADER_REFUSES_REPLICATION};
  }

  // The context type is a good indicator of the replication type.
  // A transaction created on a follower always has a ReplicatedContext, whereas
  // one created on the leader does not. The only exception is while doing
  // recovery, where we have a ReplicatedContext, but we are still the
  // leader. In that case, the leader behaves very similar to a follower.
  if (auto* context = dynamic_cast<transaction::ReplicatedContext*>(
          _transactionContext.get());
      context == nullptr) {
    // We believe to be the leader
    options.silent = false;
    replicationType = ReplicationType::LEADER;
    followers = std::make_shared<std::vector<ServerID>>();

    // This is a stunt to make sure the write concern can be fulfilled.
    auto& vocbase = this->vocbase();
    auto& clusterFeature = vocbase.server().getFeature<ClusterFeature>();
    auto canReachWriteConcern = basics::catchToResultT([&]() {
      auto stateId = collection.replicatedStateId();
      auto& agencyCache = clusterFeature.agencyCache();

      // Fetch replicated log participants
      auto [participantsConfigQuery, raftIndex] = agencyCache.get(
          fmt::format("Plan/ReplicatedLogs/{}/{}/participantsConfig",
                      vocbase.name(), stateId));
      if (participantsConfigQuery->isEmpty()) {
        LOG_TOPIC("90e43", DEBUG, Logger::REPLICATED_STATE)
            << "ParticipantsConfig of log " << stateId
            << " is non-existent according to raftIndex " << raftIndex
            << ", during transaction " << tid() << ", concerning shard "
            << collection.name();
        return false;
      }
      auto participantsConfig =
          velocypack::deserialize<replication2::agency::ParticipantsConfig>(
              participantsConfigQuery->slice());
      auto wc = participantsConfig.config.effectiveWriteConcern;

      // Fetch health information
      auto [healthQuery, healthRaftIndex] =
          agencyCache.get("Supervision/Health");
      if (healthQuery->isEmpty() || !healthQuery->slice().isObject()) {
        LOG_TOPIC("86e21", DEBUG, Logger::REPLICATED_STATE)
            << "Health of participants is invalid according to raftIndex "
            << healthRaftIndex << ", during transaction " << tid()
            << ", concerning shard " << collection.name();
        return false;
      }

      // The number of non-failed participants must satisfy the write concern.
      std::size_t nonFailedParticipants{0};
      for (auto& [pid, _] : participantsConfig.participants) {
        auto health = healthQuery->slice().get(pid);
        // Server can be removed from Health (health is none), as permanently
        // gone In this case it has to be counted as failed participant
        if (health.isObject() &&
            health.get("Status").copyString() != "FAILED") {
          ++nonFailedParticipants;
        }
      }

      return nonFailedParticipants >= wc;
    });

    if (canReachWriteConcern.fail()) {
      LOG_TOPIC("bdce1", DEBUG, Logger::REPLICATED_STATE)
          << "Cannot reach write concern for transaction " << tid()
          << ", concerning shard " << collection.name() << ": "
          << canReachWriteConcern.result();
    } else if (*canReachWriteConcern) {
      return {};
    }

    if (clusterFeature.statusCodeFailedWriteConcern() == 403) {
      return {TRI_ERROR_ARANGO_READ_ONLY};
    }
    return {TRI_ERROR_REPLICATION_WRITE_CONCERN_NOT_FULFILLED};
  } else {
    options.silent = true;
    replicationType = ReplicationType::FOLLOWER;
    followers = std::make_shared<std::vector<ServerID> const>();
  }
  return {};
}

/// @brief create one or multiple documents in a collection, local.
/// the single-document variant of this operation will either succeed or,
/// if it fails, clean up after itself
Future<OperationResult> transaction::Methods::insertLocal(
    std::string const& collectionName, VPackSlice value,
    OperationOptions& options) {
  auto res =
      co_await InsertProcessor::create(*this, collectionName, value, options);
  if (res.fail()) {
    co_return OperationResult(std::move(res).result(), options);
  }
  co_return co_await res.get().execute();
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
  auto res = co_await ModifyProcessor::create(*this, collectionName, newValue,
                                              options, isUpdate);
  if (res.fail()) {
    co_return OperationResult(std::move(res).result(), options);
  }
  co_return co_await res.get().execute();
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
  auto res =
      co_await RemoveProcessor::create(*this, collectionName, value, options);
  if (res.fail()) {
    co_return OperationResult(std::move(res).result(), options);
  }
  co_return co_await res.get().execute();
}

/// @brief fetches all documents in a collection
futures::Future<OperationResult> transaction::Methods::all(
    std::string const& collectionName, uint64_t skip, uint64_t limit,
    OperationOptions const& options) {
  TRI_ASSERT(_state->status() == transaction::Status::RUNNING);

  OperationOptions optionsCopy = options;

  if (_state->isCoordinator()) {
    co_return co_await allCoordinator(collectionName, skip, limit, optionsCopy);
  }

  co_return co_await allLocal(collectionName, skip, limit, optionsCopy);
}

/// @brief fetches all documents in a collection, coordinator
futures::Future<OperationResult> transaction::Methods::allCoordinator(
    std::string const& collectionName, uint64_t skip, uint64_t limit,
    OperationOptions& options) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

/// @brief fetches all documents in a collection, local
futures::Future<OperationResult> transaction::Methods::allLocal(
    std::string const& collectionName, uint64_t skip, uint64_t limit,
    OperationOptions& options) {
  DataSourceId cid =
      co_await addCollectionAtRuntime(collectionName, AccessMode::Type::READ);
  TransactionCollection* trxColl = trxCollection(cid);
  if (trxColl == nullptr) {
    co_return OperationResult(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, options);
  }
  TRI_ASSERT(trxColl->isLocked(AccessMode::Type::READ));

  VPackBuilder resultBuilder;

  if (_state->isDBServer()) {
    std::shared_ptr<LogicalCollection> const& collection =
        trxColl->collection();
    if (collection == nullptr) {
      co_return OperationResult(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
                                options);
    }
    auto const& followerInfo = collection->followers();
    if (!followerInfo->getLeader().empty()) {
      co_return OperationResult(TRI_ERROR_CLUSTER_SHARD_LEADER_RESIGNED,
                                options);
    }
  }

  ResourceMonitor monitor(GlobalResourceMonitor::instance());

  resultBuilder.openArray();

  auto iterator =
      indexScan(monitor, collectionName, transaction::Methods::CursorType::ALL,
                ReadOwnWrites::no);

  auto cb = IndexIterator::makeDocumentCallback(resultBuilder);
  iterator->allDocuments(cb);

  resultBuilder.close();

  co_return OperationResult(Result(), resultBuilder.steal(), options);
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
    std::string collectionName, OperationOptions options) {
  DataSourceId cid =
      co_await addCollectionAtRuntime(collectionName, AccessMode::Type::WRITE);
  TransactionCollection* trxColl = trxCollection(cid);
  if (trxColl == nullptr) {
    co_return OperationResult(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, options);
  }
  auto const& collection = trxColl->collection();
  if (collection == nullptr) {
    co_return OperationResult(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, options);
  }

  // this call will populate replicationType and followers
  ReplicationType replicationType = ReplicationType::NONE;
  std::shared_ptr<std::vector<ServerID> const> followers;

  Result res = determineReplicationTypeAndFollowers(
      *collection, "truncate", VPackSlice::noneSlice(), options,
      replicationType, followers);

  // will be populated by the call to truncate()
  bool usedRangeDelete = false;

  if (res.ok()) {
    res = collection->truncate(*this, options, usedRangeDelete);
  }

  if (res.fail() || !usedRangeDelete) {
    // we must exit here if we didn't perform a range delete.
    // this is because the non-range delete version of truncate
    // removes documents one by one, and also _replicates_ these
    // removal operations.
    co_return OperationResult(std::move(res), options);
  }

  // range delete version of truncate. we are responsible for the
  // replication ourselves
  TRI_ASSERT(usedRangeDelete);

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
    auto maybeShardID = ShardID::shardIdFromString(trxColl->collectionName());
    ADB_PROD_ASSERT(maybeShardID.ok())
        << "Tried to replicate an operation for a collection that is not a "
           "shard."
        << trxColl->collectionName() << " in collection: " << collectionName;
    auto operation =
        replication2::replicated_state::document::ReplicatedOperation::
            buildTruncateOperation(state()->id().asFollowerTransactionId(),
                                   maybeShardID.get(), username());
    // Should finish immediately, because we are not waiting the operation to be
    // committed in the replicated log
    auto replicationFut = leaderState->replicateOperation(
        std::move(operation),
        replication2::replicated_state::document::ReplicationOptions{
            .waitForSync = options.waitForSync});
    TRI_ASSERT(replicationFut.isReady());
    auto replicationRes = co_await std::move(replicationFut);
    co_return OperationResult{replicationRes.result(), options};
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
      network::addUserParameter(reqOpts, username());

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
                        absl::StrCat(ServerState::instance()->getId(), "_",
                                     followingTermId));
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

      auto allResponsesFut = futures::collectAll(futures);
      auto responses = co_await std::move(allResponsesFut);
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
        co_return OperationResult(TRI_ERROR_CLUSTER_SHARD_LEADER_RESIGNED,
                                  options);
      }
    }
  }

  co_return OperationResult(res, options);
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

  if (type == transaction::CountType::kTryCache) {
    // fetch current cache value
    uint64_t documents = cache.get();

    if (documents != CountCache::kNotPopulated) {
      // cache was previously set to some value, but cache value may
      // have been expired.

      // bump the cache expiry value if required. this will only
      // modify the cache's expiry timestamp if the cache value is
      // already expired. when called concurrently, only one thread
      // will succeed and return true from bumpExpiry.
      bool bumped = cache.bumpExpiry();
      if (bumped && SchedulerFeature::SCHEDULER != nullptr) {
        // our thread bumped the expiry date. we need to schedule the refresh
        // ourselves.
        DatabaseFeature& databaseFeature =
            vocbase().server().getFeature<DatabaseFeature>();
        // post a refresh operation onto the scheduler, with LOW priority
        SchedulerFeature::SCHEDULER->queue(
            RequestLane::CLIENT_SLOW,
            [&databaseFeature, databaseName = collinfo->vocbase().name(),
             collectionName]() {
              // it is possible that the target database does not exist anymore
              // when the callback is scheduled for execution
              auto vocbase = databaseFeature.useDatabase(databaseName);
              if (vocbase == nullptr || vocbase->server().isStopping()) {
                return;
              }

              auto coro = [](VocbasePtr vocbase, std::string collectionName)
                  -> futures::Future<futures::Unit> {
                LOG_TOPIC("01d9a", TRACE, Logger::FIXME)
                    << "refreshing count cache value for collection '"
                    << vocbase->name() << "/" << collectionName << "'";

                // start a new transaction
                auto origin = transaction::OperationOriginInternal{
                    "refreshing document count cache"};
                auto trx = std::make_shared<SingleCollectionTransaction>(
                    transaction::StandaloneContext::create(*vocbase, origin),
                    collectionName, AccessMode::Type::READ);

                // the following is all best effort only. if something fails, we
                // can ignore it, because the cache will eventually be refreshed
                auto res = co_await trx->beginAsync();
                if (res.fail()) {
                  co_return;
                }
                OperationOptions options(ExecContext::current());
                auto opResult = co_await trx->countAsync(
                    collectionName, transaction::CountType::kNormal, options);
                if (opResult.result.fail()) {
                  co_return;
                }
                VPackSlice s = opResult.slice();
                TRI_ASSERT(s.isNumber());
                if (trx->documentCollection() != nullptr) {
                  trx->documentCollection()->countCache().store(
                      s.getNumber<uint64_t>());
                }
              };

              coro(std::move(vocbase), std::move(collectionName))
                  .thenFinal([](auto&&) {
                    // weglächeln (ignore possible errors)
                  });
            });
      }

      // if bumped is false here, it means that either the cache value
      // was not expired, or that is was expired, but another concurrent
      // thread updated the expiry value concurrently. in this case we
      // will return the stale cache value if the cache value was ever
      // populated. otherwise we need to update the cache ourselves.
      VPackBuilder resultBuilder;
      resultBuilder.add(VPackValue(documents));
      return OperationResult(Result(), resultBuilder.steal(), options);
    }
    // fallthrough intentional
  }

  // need to query count values from DB server(s), from within the currently
  // running transaction.
  return arangodb::countOnCoordinator(*this, collectionName, options, api)
      .thenValue([&cache, type,
                  options](OperationResult&& res) -> OperationResult {
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

/// @brief count the number of documents in a collection
OperationResult transaction::Methods::countLocal(
    std::string const& collectionName, transaction::CountType /*type*/,
    OperationOptions const& options) {
  DataSourceId cid =
      addCollectionAtRuntime(collectionName, AccessMode::Type::READ).get();
  TransactionCollection* trxColl = trxCollection(cid);
  if (trxColl == nullptr) {
    return OperationResult(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, options);
  }
  auto const& collection = trxColl->collection();
  if (collection == nullptr) {
    return OperationResult(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, options);
  }

  TRI_ASSERT(isLocked(collection.get(), AccessMode::Type::READ));

  uint64_t num = collection->getPhysical()->numberDocuments(this);

  VPackBuilder resultBuilder;
  resultBuilder.add(VPackValue(num));

  return OperationResult(Result(), resultBuilder.steal(), options);
}

/// @brief factory for IndexIterator objects from AQL
std::unique_ptr<IndexIterator> transaction::Methods::indexScanForCondition(
    ResourceMonitor& monitor, IndexHandle const& idx,
    arangodb::aql::AstNode const* condition, arangodb::aql::Variable const* var,
    IndexIteratorOptions const& opts, ReadOwnWrites readOwnWrites,
    int mutableConditionIdx) {
  if (_state->isCoordinator()) {
    // The index scan is only available on DBServers and Single Server.
    THROW_ARANGO_EXCEPTION(TRI_ERROR_CLUSTER_ONLY_ON_DBSERVER);
  }

  if (nullptr == idx) {
    // should never happen
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "the index id cannot be empty");
  }

  // TODO: an extra optimizer rule could make this unnecessary
  if (isInaccessibleCollection(idx->collection().name())) {
    return std::make_unique<EmptyIndexIterator>(&idx->collection(), this);
  }

  TRI_ASSERT(!idx->inProgress());
  if (idx->inProgress()) {
    // should never happen
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL,
        "cannot use an index for querying that is currently being built");
  }

  // Now create the Iterator
  return idx->iteratorForCondition(monitor, this, condition, var, opts,
                                   readOwnWrites, mutableConditionIdx);
}

/// @brief factory for IndexIterator objects
/// note: the caller must have read-locked the underlying collection when
/// calling this method
std::unique_ptr<IndexIterator> transaction::Methods::indexScan(
    ResourceMonitor& monitor, std::string const& collectionName,
    CursorType cursorType, ReadOwnWrites readOwnWrites) {
  // For now we assume indexId is the iid part of the index.

  if (ADB_UNLIKELY(_state->isCoordinator())) {
    // The index scan is only available on DBServers and Single Server.
    THROW_ARANGO_EXCEPTION(TRI_ERROR_CLUSTER_ONLY_ON_DBSERVER);
  }

  DataSourceId cid =
      addCollectionAtRuntime(collectionName, AccessMode::Type::READ).get();
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
    std::string_view name) const {
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
                                           std::string_view collectionName,
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
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_TRANSACTION_INTERNAL,
                                   "cannot add collection to a previously "
                                   "started top-level transaction");
  }

  if (cid.empty()) {
    // invalid cid
    throwCollectionNotFound(collectionName);
  }

  bool lockUsage = !_mainTransaction;

  auto addCollectionCallback = [this, &collectionName, type,
                                lockUsage](DataSourceId cid) -> void {
    auto res =
        _state->addCollection(cid, collectionName, type, lockUsage).get();

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
Result transaction::Methods::addCollection(std::string_view name,
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
    TRI_voc_document_operation_e operation, std::string_view userName) {
  auto const& collection = transactionCollection.collection();
  TRI_ASSERT(followerList != nullptr);

  // It is normal to have an empty followerList when using replication2
  TRI_ASSERT(!followerList->empty() ||
             collection->vocbase().replicationVersion() ==
                 replication::Version::TWO);

  TRI_ASSERT(replicationData.slice().isArray());
  TRI_ASSERT(!replicationData.slice().isEmptyArray());

  TRI_IF_FAILURE("replicateOperations::skip") { return Result(); }

  // index cache refilling...
  bool const refill = std::invoke([&]() {
    if (options.refillIndexCaches == RefillIndexCaches::kDontRefill) {
      // index cache refilling opt-out
      return false;
    }

    // this attribute can have 3 values: default, true and false. only
    // expose it when it is not set to "default"
    auto& engine = vocbase().engine();

    return engine.autoRefillIndexCachesOnFollowers() &&
           ((options.refillIndexCaches == RefillIndexCaches::kRefill) ||
            (options.refillIndexCaches == RefillIndexCaches::kDefault &&
             engine.autoRefillIndexCaches()));
  });

  TRI_IF_FAILURE("RefillIndexCacheOnFollowers::failIfTrue") {
    if (refill) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }
  }
  TRI_IF_FAILURE("RefillIndexCacheOnFollowers::failIfFalse") {
    if (!refill) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }
  }

  // replication2 is handled here
  if (collection->replicationVersion() == replication::Version::TWO) {
    using namespace replication2::replicated_state::document;

    auto& rtc = static_cast<ReplicatedRocksDBTransactionCollection&>(
        transactionCollection);
    auto leaderState = rtc.leaderState();
    auto maybeShardID = ShardID::shardIdFromString(rtc.collectionName());
    ADB_PROD_ASSERT(maybeShardID.ok())
        << "Tried to replicate an operation for a collection that is not a "
           "shard."
        << rtc.collectionName();
    auto operationOptions = ReplicatedOperation::DocumentOperation::Options{
        .refillIndexCaches = refill};
    auto replicatedOp = ReplicatedOperation::buildDocumentOperation(
        operation, state()->id().asFollowerTransactionId(), maybeShardID.get(),
        replicationData.sharedSlice(), userName, operationOptions);
    // Should finish immediately
    auto replicationFut = leaderState->replicateOperation(
        std::move(replicatedOp),
        replication2::replicated_state::document::ReplicationOptions{
            .waitForSync = options.waitForSync});

    // Should finish immediately, because we are not waiting the operation to
    // be committed in the replicated log
    TRI_ASSERT(replicationFut.isReady());

    auto replicationRes = replicationFut.get();
    if (replicationRes.fail()) {
      return replicationRes.result();
    }
    return performIntermediateCommitIfRequired(collection->id());
  }

  // path and requestType are different for insert/remove/modify.

  network::RequestOptions reqOpts;
  reqOpts.database = vocbase().name();
  // use a HIGH priority lane to execute the callback when the
  // response arrives. we must not skip the scheduler entirely here
  // because the callback may execute an intermediate commit in
  // RocksDB or carry out agency communication in case a follower
  // needs to be dropped. the HIGH priority is justified here
  // because the callback must make progress: the callback can
  // unblock other block threads on the leader that synchronously
  // wait for this future to be resolved.
  reqOpts.continuationLane = RequestLane::CLUSTER_INTERNAL;

  reqOpts.param(StaticStrings::IsRestoreString, "true");
  reqOpts.param(StaticStrings::RefillIndexCachesString,
                refill ? "true" : "false");

  network::addUserParameter(reqOpts, username());

  std::string url = absl::StrCat(
      "/_api/document/", basics::StringUtils::urlEncode(collection->name()));

  std::string_view opName = "unknown";
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
      }
      break;
    case TRI_VOC_DOCUMENT_OPERATION_UPDATE:
      // intentionally turn UPDATE operations on the leader into REPLACE
      // operations on the follower.
      // we need to do this because we are replicating the (full) document
      // that we have written on the leader, but this does not include any
      // removed attributes anymore. However, an UPDATE operation on the
      // follower would merge what we have written on the leader with the
      // existing document on the follower, which could silently cause
      // data drift.
      // by replicating the document as it was written on the leader and
      // replicating the operation as a REPLACE, we ensure that we write
      // the same document on the follower as we did on the leader.
      requestType =
          arangodb::fuerte::RestVerb::Put;  // this will make it a REPLACE
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
      reqOpts.param(
          StaticStrings::IsSynchronousReplicationString,
          absl::StrCat(ServerState::instance()->getId(), "_", followingTermId));
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
  auto cb = [=, this](std::vector<futures::Try<network::Response>>&& responses)
      -> futures::Future<Result> {
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
                absl::StrCat("got error header from follower: ", errors);
          }

        } else {
          auto r = resp.combinedResult();

          // Special case if the follower has already aborted the transaction.
          // This can happen if a query fails and causes the leaders to abort
          // the transaction on the followers. However, if the followers have
          // transactions that are shared with leaders of other shards and one
          // of those leaders has not yet seen the error, then it will happily
          // continue to replicate to that follower. But if the follower has
          // already aborted the transaction, then it will reject the
          // replication request. In this case we do not want to drop the
          // follower, but simply return the error and abort our local
          // transaction.
          if (r.is(TRI_ERROR_TRANSACTION_ABORTED) &&
              this->state()->hasHint(
                  transaction::Hints::Hint::FROM_TOPLEVEL_AQL)) {
            return r;
          }

          bool followerRefused =
              r.is(TRI_ERROR_CLUSTER_SHARD_LEADER_REFUSES_REPLICATION);
          didRefuse = didRefuse || followerRefused;

          replicationFailureReason =
              absl::StrCat("got error from follower: ", r.errorMessage());

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

    if (didRefuse) {  // case (1), caller may abort this transaction
      return Result{TRI_ERROR_CLUSTER_SHARD_LEADER_RESIGNED};
    } else {
      // execute a deferred intermediate commit, if required.
      return performIntermediateCommitIfRequired(collection->id());
    }
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
    f = modifyLocal(collectionName, newValue, optionsCopy,
                    /*isUpdate*/ false);
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

  if (type == CountType::kDetailed) {
    // we are a single-server... we cannot provide detailed per-shard counts,
    // so just downgrade the request to a normal request
    type = CountType::kNormal;
  }

  return futures::makeFuture(countLocal(collectionName, type, options));
}

// perform a (deferred) intermediate commit if required
futures::Future<Result> Methods::performIntermediateCommitIfRequired(
    DataSourceId collectionId) {
  return _state->performIntermediateCommitIfRequired(collectionId);
}

Result Methods::triggerIntermediateCommit() {
  return _state->triggerIntermediateCommit();
}

Result Methods::begin() { return beginAsync().get(); }

#ifndef USE_ENTERPRISE
ErrorCode Methods::validateSmartJoinAttribute(LogicalCollection const&,
                                              velocypack::Slice) {
  return TRI_ERROR_NO_ERROR;
}
#endif

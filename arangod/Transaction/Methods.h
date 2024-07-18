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

#pragma once

#include "Indexes/IndexIterator.h"
#include "Rest/CommonDefines.h"
#include "Transaction/CountCache.h"
#include "Transaction/Hints.h"
#include "Transaction/MethodsApi.h"
#include "Transaction/Options.h"
#include "Transaction/Status.h"
#include "VocBase/AccessMode.h"
#include "VocBase/LogicalDataSource.h"
#include "VocBase/voc-types.h"

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#ifdef USE_ENTERPRISE
#define ENTERPRISE_VIRT virtual
#else
#define ENTERPRISE_VIRT TEST_VIRTUAL
#endif

struct TRI_vocbase_t;

namespace arangodb {

namespace futures {
template<class T>
class Future;
}
namespace basics {
struct AttributeName;
}  // namespace basics

namespace velocypack {
class Builder;
class Slice;
}  // namespace velocypack

namespace aql {
class Ast;
struct AstNode;
struct Variable;
}  // namespace aql

class CollectionNameResolver;
class DataSourceId;
class Index;
class IndexIterator;
class LocalDocumentId;
class LogicalDataSource;
struct IndexIteratorOptions;
struct OperationOptions;
struct OperationResult;
struct ResourceMonitor;
class Result;
class RevisionId;
class TransactionId;
class TransactionState;
class TransactionCollection;

namespace transaction {
struct BatchOptions;
class Context;
struct Options;

class Methods {
 public:
  template<typename T>
  using Future = futures::Future<T>;
  using IndexHandle = std::shared_ptr<arangodb::Index>;  // legacy
  using VPackSlice = arangodb::velocypack::Slice;

  static constexpr int kNoMutableConditionIdx{-1};

  Methods() = delete;
  Methods(Methods const&) = delete;
  Methods& operator=(Methods const&) = delete;

  /// @brief create the transaction
  explicit Methods(std::shared_ptr<Context> ctx,
                   Options const& options = Options());

  /// @brief create the transaction, and add a collection to it.
  /// use on followers only!
  Methods(std::shared_ptr<Context> ctx, std::string const& collectionName,
          AccessMode::Type type);

  /// @brief create the transaction, used to be UserTransaction
  Methods(std::shared_ptr<Context> ctx,
          std::vector<std::string> const& readCollections,
          std::vector<std::string> const& writeCollections,
          std::vector<std::string> const& exclusiveCollections,
          Options const& options);

  /// @brief destroy the transaction
  virtual ~Methods();

  Methods(Methods&&) = default;
  Methods& operator=(Methods&&) = default;

  typedef Result (*DataSourceRegistrationCallback)(
      LogicalDataSource& dataSource, Methods& trx);

  enum class CallbacksTag {
    StatusChange = 0,
  };

  /// @brief definition from TransactionState::StatusChangeCallback
  /// @param status the new status of the transaction
  ///               will match trx.state()->status() for top-level transactions
  ///               may not match trx.state()->status() for embeded transactions
  ///               since their staus is not updated from RUNNING
  using StatusChangeCallback = std::function<void(Methods& trx, Status status)>;

  /// @brief add a callback to be called for LogicalDataSource instance
  ///        association events, e.g. addCollection(...)
  /// @note not thread-safe on the assumption of static factory registration
  static void addDataSourceRegistrationCallback(
      DataSourceRegistrationCallback const& callback);

  /// @brief add a callback to be called for state change events
  /// @param callback nullptr and empty functors are ignored, treated as success
  /// @return success
  bool addStatusChangeCallback(StatusChangeCallback const* callback);
  bool removeStatusChangeCallback(StatusChangeCallback const* callback);

  /// @brief clear all called for LogicalDataSource instance association events
  /// @note not thread-safe on the assumption of static factory registration
  /// @note FOR USE IN TESTS ONLY to reset test state
  /// FIXME TODO StateRegistrationCallback logic should be moved into its own
  /// feature
  static void clearDataSourceRegistrationCallbacks();

  /// @brief Type of cursor
  enum class CursorType { ALL = 0, ANY };

  /// @brief return database of transaction
  TRI_vocbase_t& vocbase() const;

  /// @brief return internals of transaction
  TransactionState* state() const noexcept { return _state.get(); }
  std::shared_ptr<TransactionState> stateShrdPtr() const { return _state; }

  Result resolveId(char const* handle, size_t length,
                   std::shared_ptr<LogicalCollection>& collection,
                   char const*& key, size_t& outLength);

  /// @brief return a pointer to the transaction context
  std::shared_ptr<Context> const& transactionContext() const {
    return _transactionContext;
  }

  TEST_VIRTUAL Context* transactionContextPtr() const {
    TRI_ASSERT(_transactionContext != nullptr);
    return _transactionContext.get();
  }

  /// @brief set name of user who originated the transaction. will
  /// only be set if no user has been registered with the transaction yet.
  /// this user name is informational only and can be used for logging,
  /// metrics etc. it should not be used for permission checks.
  void setUsername(std::string const& name);

  /// @brief return name of user who originated the transaction. may be
  /// empty. this user name is informational only and can be used for logging,
  /// metrics etc. it should not be used for permission checks.
  std::string_view username() const noexcept;

  // is this instance responsible for commit / abort
  bool isMainTransaction() const noexcept;

  /// @brief add a transaction hint
  void addHint(Hints::Hint hint) noexcept;

  /// @brief whether or not the transaction consists of a single operation only
  bool isSingleOperationTransaction() const noexcept;

  /// @brief get the status of the transaction
  Status status() const noexcept;

  /// @brief get the status of the transaction, as a string_view
  std::string_view statusString() const noexcept;

  /// @brief options used, not dump options
  TEST_VIRTUAL velocypack::Options const& vpackOptions() const;

  /// @brief begin the transaction
  [[nodiscard, deprecated("use async variant")]] Result begin();
  [[nodiscard]] futures::Future<Result> beginAsync();

  /// @deprecated use async variant
  [[nodiscard, deprecated("use async variant")]] auto commit() noexcept
      -> Result;
  /// @brief commit / finish the transaction
  [[nodiscard]] auto commitAsync() noexcept -> Future<Result>;

  /// @deprecated use async variant
  [[nodiscard, deprecated("use async variant")]] auto abort() noexcept
      -> Result;
  /// @brief abort the transaction
  [[nodiscard]] auto abortAsync() noexcept -> Future<Result>;

  /// @deprecated use async variant
  [[nodiscard, deprecated("use async variant")]] auto finish(
      Result const& res) noexcept -> Result;

  /// @brief finish a transaction (commit or abort), based on the previous state
  [[nodiscard]] auto finishAsync(Result const& res) noexcept -> Future<Result>;

  /// @brief return the transaction id
  TransactionId tid() const;

  /// @brief return a collection name
  std::string name(DataSourceId cid) const;

  /// @brief extract the _id attribute from a slice,
  /// and convert it into a string
  std::string extractIdString(VPackSlice);

  /// @brief read many documents, using skip and limit in arbitrary order
  /// The result guarantees that all documents are contained exactly once
  /// as long as the collection is not modified.
  ENTERPRISE_VIRT futures::Future<OperationResult> any(
      std::string const& collectionName, OperationOptions const& options);

  /// @brief add a collection to the transaction for read, at runtime
  futures::Future<DataSourceId> addCollectionAtRuntime(
      DataSourceId cid, std::string_view collectionName, AccessMode::Type type);

  /// @brief add a collection to the transaction for read, at runtime
  virtual futures::Future<DataSourceId> addCollectionAtRuntime(
      std::string_view collectionName, AccessMode::Type type);

  /// @brief return the type of a collection
  TRI_col_type_e getCollectionType(std::string_view collectionName) const;

  /// @brief return one  document from a collection, fast path
  ///        If everything went well the result will contain the found document
  ///        (as an external on single_server)  and this function will return
  ///        TRI_ERROR_NO_ERROR. If there was an error the code is returned and
  ///        it is guaranteed that result remains unmodified. Does not care for
  ///        revision handling! shouldLock indicates if the transaction should
  ///        lock the collection if set to false it will not lock it (make sure
  ///        it is already locked!)
  ENTERPRISE_VIRT futures::Future<Result> documentFastPath(
      std::string const& collectionName, arangodb::velocypack::Slice value,
      OperationOptions const& options, arangodb::velocypack::Builder& result);

  /// @brief return one  document from a collection, fast path
  ///        If everything went well the result will contain the found document
  ///        (as an external on single_server)  and this function will return
  ///        TRI_ERROR_NO_ERROR. If there was an error the code is returned Does
  ///        not care for revision handling! Must only be called on a local
  ///        server, not in cluster case!
  ENTERPRISE_VIRT futures::Future<Result> documentFastPathLocal(
      std::string_view collectionName, std::string_view key,
      IndexIterator::DocumentCallback const& cb);

  /// @brief return one or multiple documents from a collection
  /// @deprecated use async variant
  [[deprecated]] OperationResult document(std::string const& collectionName,
                                          VPackSlice value,
                                          OperationOptions const& options);

  /// @brief return one or multiple documents from a collection
  Future<OperationResult> documentAsync(std::string const& collectionName,
                                        VPackSlice value,
                                        OperationOptions const& options);

  /// @deprecated use async variant
  [[deprecated]] OperationResult insert(std::string const& collectionName,
                                        VPackSlice value,
                                        OperationOptions const& options);

  /// @brief create one or multiple documents in a collection
  /// The single-document variant of this operation will either succeed or,
  /// if it fails, clean up after itself
  Future<OperationResult> insertAsync(std::string const& collectionName,
                                      VPackSlice value,
                                      OperationOptions const& options);

  /// @deprecated use async variant
  [[deprecated]] OperationResult update(std::string const& collectionName,
                                        VPackSlice updateValue,
                                        OperationOptions const& options);

  /// @brief update/patch one or multiple documents in a collection.
  /// The single-document variant of this operation will either succeed or,
  /// if it fails, clean up after itself
  Future<OperationResult> updateAsync(std::string const& collectionName,
                                      VPackSlice updateValue,
                                      OperationOptions const& options);

  /// @deprecated use async variant
  [[deprecated]] OperationResult replace(std::string const& collectionName,
                                         VPackSlice replaceValue,
                                         OperationOptions const& options);

  /// @brief replace one or multiple documents in a collection.
  /// The single-document variant of this operation will either succeed or,
  /// if it fails, clean up after itself
  Future<OperationResult> replaceAsync(std::string const& collectionName,
                                       VPackSlice replaceValue,
                                       OperationOptions const& options);

  /// @deprecated use async variant
  [[deprecated]] OperationResult remove(std::string const& collectionName,
                                        VPackSlice value,
                                        OperationOptions const& options);

  /// @brief remove one or multiple documents in a collection
  /// the single-document variant of this operation will either succeed or,
  /// if it fails, clean up after itself
  Future<OperationResult> removeAsync(std::string const& collectionName,
                                      VPackSlice value,
                                      OperationOptions const& options);

  /// @brief fetches all documents in a collection
  ENTERPRISE_VIRT futures::Future<OperationResult> all(
      std::string const& collectionName, uint64_t skip, uint64_t limit,
      OperationOptions const& options);

  /// @brief deprecated use async variant
  [[deprecated]] OperationResult truncate(std::string const& collectionName,
                                          OperationOptions const& options);

  /// @brief remove all documents in a collection
  Future<OperationResult> truncateAsync(std::string const& collectionName,
                                        OperationOptions const& options);

  /// deprecated, use async variant
  [[deprecated]] OperationResult count(std::string const& collectionName,
                                       CountType type,
                                       OperationOptions const& options);

  /// @brief count the number of documents in a collection
  futures::Future<OperationResult> countAsync(std::string const& collectionName,
                                              CountType type,
                                              OperationOptions const& options);

  /// @brief factory for IndexIterator objects from AQL
  /// note: the caller must have read-locked the underlying collection when
  /// calling this method
  std::unique_ptr<IndexIterator> indexScanForCondition(
      ResourceMonitor& monitor, IndexHandle const&,
      arangodb::aql::AstNode const*, arangodb::aql::Variable const*,
      IndexIteratorOptions const&, ReadOwnWrites readOwnWrites,
      int mutableConditionIdx);

  /// @brief factory for IndexIterator objects
  /// note: the caller must have read-locked the underlying collection when
  /// calling this method
  futures::Future<std::unique_ptr<IndexIterator>> indexScan(
      std::string const& collectionName, CursorType cursorType,
      ReadOwnWrites readOwnWrites);

  /// @brief test if a collection is already locked
  ENTERPRISE_VIRT bool isLocked(arangodb::LogicalCollection*,
                                AccessMode::Type) const;

  /// @brief fetch the LogicalCollection by name
  arangodb::LogicalCollection* documentCollection(std::string_view name) const;

  /// @brief return the collection name resolver
  CollectionNameResolver const* resolver() const;

#ifndef USE_ENTERPRISE
  [[nodiscard]] bool skipInaccessible() const { return false; }
  [[nodiscard]] bool isInaccessibleCollection(DataSourceId /*cid*/) const {
    return false;
  }
  [[nodiscard]] bool isInaccessibleCollection(
      std::string_view /*collectionName*/) const {
    return false;
  }
#else
  [[nodiscard]] bool skipInaccessible() const;
  [[nodiscard]] bool isInaccessibleCollection(DataSourceId /*cid*/) const;
  [[nodiscard]] bool isInaccessibleCollection(
      std::string_view /*collectionName*/) const;
#endif

  static ErrorCode validateSmartJoinAttribute(LogicalCollection const& collinfo,
                                              velocypack::Slice value);

  Result triggerIntermediateCommit();

  enum class ReplicationType { NONE, LEADER, FOLLOWER };

  Result determineReplicationTypeAndFollowers(
      LogicalCollection& collection, std::string_view operationName,
      velocypack::Slice value, OperationOptions& options,
      ReplicationType& replicationType,
      std::shared_ptr<std::vector<ServerID> const>& followers);

  /// @brief return the transaction collection for a document collection
  TransactionCollection* trxCollection(
      DataSourceId cid, AccessMode::Type type = AccessMode::Type::READ) const;

  Future<Result> replicateOperations(
      TransactionCollection& collection,
      std::shared_ptr<const std::vector<std::string>> const& followers,
      OperationOptions const& options,
      velocypack::Builder const& replicationData,
      TRI_voc_document_operation_e operation, std::string_view userName);

 private:
  // perform a (deferred) intermediate commit if required
  futures::Future<Result> performIntermediateCommitIfRequired(
      DataSourceId collectionId);

  futures::Future<OperationResult> documentCoordinator(
      std::string const& collectionName, VPackSlice value,
      OperationOptions options, MethodsApi api);

  Future<OperationResult> documentLocal(std::string const& collectionName,
                                        VPackSlice value,
                                        OperationOptions options);

  Future<OperationResult> insertCoordinator(std::string const& collectionName,
                                            VPackSlice value,
                                            OperationOptions options,
                                            MethodsApi api);

  Future<OperationResult> insertLocal(std::string const& collectionName,
                                      VPackSlice value,
                                      OperationOptions options);

  Result determineReplication1TypeAndFollowers(
      LogicalCollection& collection, std::string_view operationName,
      velocypack::Slice value, OperationOptions& options,
      ReplicationType& replicationType,
      std::shared_ptr<std::vector<ServerID> const>& followers);

  Result determineReplication2TypeAndFollowers(
      LogicalCollection& collection, std::string_view operationName,
      velocypack::Slice value, OperationOptions& options,
      ReplicationType& replicationType,
      std::shared_ptr<std::vector<ServerID> const>& followers);

  Future<OperationResult> modifyCoordinator(
      std::string const& collectionName, VPackSlice newValue,
      OperationOptions options, TRI_voc_document_operation_e operation,
      MethodsApi api);

  Future<OperationResult> modifyLocal(std::string const& collectionName,
                                      VPackSlice newValue,
                                      OperationOptions options, bool isUpdate);

  Future<OperationResult> removeCoordinator(std::string const& collectionName,
                                            VPackSlice value,
                                            OperationOptions options,
                                            MethodsApi api);

  Future<OperationResult> removeLocal(std::string const& collectionName,
                                      VPackSlice value,
                                      OperationOptions options);

  futures::Future<OperationResult> allCoordinator(
      std::string const& collectionName, uint64_t skip, uint64_t limit,
      OperationOptions const& options);

  futures::Future<OperationResult> allLocal(std::string const& collectionName,
                                            uint64_t skip, uint64_t limit,
                                            OperationOptions options);

  OperationResult anyCoordinator(std::string const& collectionName,
                                 OperationOptions const& options);

  futures::Future<OperationResult> anyLocal(std::string const& collectionName,
                                            OperationOptions options);

  Future<OperationResult> truncateCoordinator(std::string const& collectionName,
                                              OperationOptions options,
                                              MethodsApi api);

  Future<OperationResult> truncateLocal(std::string collectionName,
                                        OperationOptions options);

 protected:
  // The internal methods distinguish between the synchronous and asynchronous
  // APIs via an additional parameter, so `skipScheduler` can be set for network
  // requests.
  [[nodiscard]] auto commitInternal(MethodsApi api) noexcept -> Future<Result>;
  [[nodiscard]] auto abortInternal(MethodsApi api) noexcept -> Future<Result>;
  [[nodiscard]] auto finishInternal(Result const& res, MethodsApi api) noexcept
      -> Future<Result>;
  // is virtual for IgnoreNoAccessMethods
  ENTERPRISE_VIRT auto documentInternal(std::string const& collectionName,
                                        VPackSlice value,
                                        OperationOptions const& options,
                                        MethodsApi api)
      -> Future<OperationResult>;
  auto insertInternal(std::string const& collectionName, VPackSlice value,
                      OperationOptions const& options, MethodsApi api)
      -> Future<OperationResult>;
  auto updateInternal(std::string const& collectionName, VPackSlice updateValue,
                      OperationOptions const& options, MethodsApi api)
      -> Future<OperationResult>;
  auto replaceInternal(std::string const& collectionName,
                       VPackSlice replaceValue, OperationOptions const& options,
                       MethodsApi api) -> Future<OperationResult>;
  auto removeInternal(std::string const& collectionName, VPackSlice value,
                      OperationOptions const& options, MethodsApi api)
      -> Future<OperationResult>;
  auto truncateInternal(std::string const& collectionName,
                        OperationOptions const& options, MethodsApi api)
      -> Future<OperationResult>;
  // is virtual for IgnoreNoAccessMethods
  ENTERPRISE_VIRT auto countInternal(std::string const& collectionName,
                                     CountType type,
                                     OperationOptions const& options,
                                     MethodsApi api)
      -> futures::Future<OperationResult>;

  TransactionCollection* trxCollection(
      std::string_view name,
      AccessMode::Type type = AccessMode::Type::READ) const;

  futures::Future<OperationResult> countCoordinator(
      std::string const& collectionName, CountType type,
      OperationOptions options, MethodsApi api);

  futures::Future<OperationResult> countCoordinatorHelper(
      std::shared_ptr<LogicalCollection> const& collinfo,
      std::string const& collectionName, CountType type,
      OperationOptions const& options, MethodsApi api);

  futures::Future<OperationResult> countLocal(std::string const& collectionName,
                                              CountType type,
                                              OperationOptions options);

  /// @brief add a collection by id, with the name supplied
  Result addCollection(DataSourceId, std::string_view, AccessMode::Type);

  /// @brief add a collection by name
  Result addCollection(std::string_view, AccessMode::Type);

 private:
  template<CallbacksTag tag, typename Callback>
  [[nodiscard]] bool addCallbackImpl(Callback const* callback);

  /// @brief the state
  std::shared_ptr<TransactionState> _state;

  /// @brief the transaction context
  std::shared_ptr<Context> _transactionContext;

  /// @brief transaction hints
  Hints _localHints;

  bool _mainTransaction;

  /// @brief name-to-cid lookup cache for last collection seen
  struct {
    DataSourceId cid = DataSourceId::none();
    std::string name;
  } _collectionCache;
};

}  // namespace transaction
}  // namespace arangodb

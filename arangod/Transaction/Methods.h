////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2017 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_TRANSACTION_METHODS_H
#define ARANGOD_TRANSACTION_METHODS_H 1

#include "Aql/IndexHint.h"
#include "Basics/Common.h"
#include "Basics/Exceptions.h"
#include "Basics/Result.h"
#include "Futures/Future.h"
#include "Rest/CommonDefines.h"
#include "Transaction/CountCache.h"
#include "Transaction/Hints.h"
#include "Transaction/Options.h"
#include "Transaction/Status.h"
#include "Utils/OperationResult.h"
#include "VocBase/AccessMode.h"
#include "VocBase/voc-types.h"
#include "VocBase/vocbase.h"

#include <velocypack/Slice.h>
#include <velocypack/StringRef.h>

#ifdef USE_ENTERPRISE
#define ENTERPRISE_VIRT virtual
#else
#define ENTERPRISE_VIRT TEST_VIRTUAL
#endif

namespace arangodb {

namespace basics {
struct AttributeName;
}  // namespace basics

namespace velocypack {
class Builder;
}

namespace aql {
class Ast;
struct AstNode;
class SortCondition;
struct Variable;
}  // namespace aql

namespace rest {
enum class ResponseCode;
}

namespace traverser {
class BaseEngine;
}

namespace transaction {
class Context;
struct Options;
}  // namespace transaction

/// @brief forward declarations
class ClusterFeature;
class CollectionNameResolver;
class Index;
class LocalDocumentId;
class ManagedDocumentResult;
struct IndexIteratorOptions;
struct OperationOptions;
class TransactionState;
class TransactionCollection;

namespace transaction {

class Methods {
 public:

  template<typename T>
  using Future = futures::Future<T>;
  using IndexHandle = std::shared_ptr<arangodb::Index>; // legacy
  using VPackSlice = arangodb::velocypack::Slice;

  /// @brief transaction::Methods
 private:
  Methods() = delete;
  Methods(Methods const&) = delete;
  Methods& operator=(Methods const&) = delete;

 public:
  
  /// @brief create the transaction
  explicit Methods(std::shared_ptr<transaction::Context> const& transactionContext,
                   transaction::Options const& options = transaction::Options());

  /// @brief create the transaction, used to be UserTransaction
  Methods(std::shared_ptr<transaction::Context> const& ctx,
          std::vector<std::string> const& readCollections,
          std::vector<std::string> const& writeCollections,
          std::vector<std::string> const& exclusiveCollections,
          transaction::Options const& options);

  /// @brief destroy the transaction
  virtual ~Methods();

  typedef Result (*DataSourceRegistrationCallback)(LogicalDataSource& dataSource,
                                                   Methods& trx);

  /// @brief definition from TransactionState::StatusChangeCallback
  /// @param status the new status of the transaction
  ///               will match trx.state()->status() for top-level transactions
  ///               may not match trx.state()->status() for embeded transactions
  ///               since their staus is not updated from RUNNING
  typedef std::function<void(transaction::Methods& trx, transaction::Status status)> StatusChangeCallback;

  /// @brief add a callback to be called for LogicalDataSource instance
  ///        association events, e.g. addCollection(...)
  /// @note not thread-safe on the assumption of static factory registration
  static void addDataSourceRegistrationCallback(DataSourceRegistrationCallback const& callback);

  /// @brief add a callback to be called for state change events
  /// @param callback nullptr and empty functers are ignored, treated as success
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
  inline TransactionState* state() const { return _state.get(); }
  inline std::shared_ptr<TransactionState> const& stateShrdPtr() const { return _state; }

  Result resolveId(char const* handle, size_t length,
                   std::shared_ptr<LogicalCollection>& collection,
                   char const*& key, size_t& outLength);

  /// @brief return a pointer to the transaction context
  std::shared_ptr<transaction::Context> const& transactionContext() const {
    return _transactionContext;
  }

  TEST_VIRTUAL inline transaction::Context* transactionContextPtr() const {
    TRI_ASSERT(_transactionContext != nullptr);
    return _transactionContext.get();
  }
  
  // is this instance responsible for commit / abort
  bool isMainTransaction() const {
    return _mainTransaction;
  }
  
  /// @brief add a transaction hint
  void addHint(transaction::Hints::Hint hint) { _localHints.set(hint); }

  /// @brief whether or not the transaction consists of a single operation only
  bool isSingleOperationTransaction() const;

  /// @brief get the status of the transaction
  Status status() const;

  /// @brief get the status of the transaction, as a string
  char const* statusString() const {
    return transaction::statusString(status());
  }
  
  /// @brief options used, not dump options
  velocypack::Options const& vpackOptions() const;

  /// @brief begin the transaction
  Result begin();

  /// @deprecated use async variant
  Result commit() { return commitAsync().get(); }
  /// @brief commit / finish the transaction
  Future<Result> commitAsync();

  /// @deprecated use async variant
  Result abort() { return abortAsync().get(); }
  /// @brief abort the transaction
  Future<Result> abortAsync();

  /// @deprecated use async variant
  Result finish(Result const& res) { return finishAsync(res).get(); }

  /// @brief finish a transaction (commit or abort), based on the previous state
  Future<Result> finishAsync(Result const& res);

  /// @brief return the transaction id
  TRI_voc_tid_t tid() const;

  /// @brief return a collection name
  std::string name(TRI_voc_cid_t cid) const;

  /// @brief extract the _id attribute from a slice,
  /// and convert it into a string
  std::string extractIdString(VPackSlice);

  /// @brief read many documents, using skip and limit in arbitrary order
  /// The result guarantees that all documents are contained exactly once
  /// as long as the collection is not modified.
  ENTERPRISE_VIRT OperationResult any(std::string const& collectionName);

  /// @brief add a collection to the transaction for read, at runtime
  TRI_voc_cid_t addCollectionAtRuntime(TRI_voc_cid_t cid, std::string const& collectionName,
                                       AccessMode::Type type);

  /// @brief add a collection to the transaction for read, at runtime
  virtual TRI_voc_cid_t addCollectionAtRuntime(std::string const& collectionName,
                                               AccessMode::Type type);

  /// @brief return the type of a collection
  bool isEdgeCollection(std::string const& collectionName) const;
  bool isDocumentCollection(std::string const& collectionName) const;
  TRI_col_type_e getCollectionType(std::string const& collectionName) const;

  /// @brief return one  document from a collection, fast path
  ///        If everything went well the result will contain the found document
  ///        (as an external on single_server)  and this function will return
  ///        TRI_ERROR_NO_ERROR. If there was an error the code is returned and
  ///        it is guaranteed that result remains unmodified. Does not care for
  ///        revision handling! shouldLock indicates if the transaction should
  ///        lock the collection if set to false it will not lock it (make sure
  ///        it is already locked!)
  ENTERPRISE_VIRT Result documentFastPath(std::string const& collectionName,
                                          ManagedDocumentResult* mmdr,
                                          arangodb::velocypack::Slice const value,
                                          arangodb::velocypack::Builder& result);

  /// @brief return one  document from a collection, fast path
  ///        If everything went well the result will contain the found document
  ///        (as an external on single_server)  and this function will return
  ///        TRI_ERROR_NO_ERROR. If there was an error the code is returned Does
  ///        not care for revision handling! Must only be called on a local
  ///        server, not in cluster case!
  ENTERPRISE_VIRT Result documentFastPathLocal(std::string const& collectionName,
                                               arangodb::velocypack::StringRef const& key,
                                               ManagedDocumentResult& result);

  /// @brief return one or multiple documents from a collection
  /// @deprecated use async variant
  ENTERPRISE_VIRT OperationResult document(std::string const& collectionName,
                                           VPackSlice const value,
                                           OperationOptions& options) {
    return documentAsync(collectionName, value, options).get();
  }

  /// @brief return one or multiple documents from a collection
  Future<OperationResult> documentAsync(std::string const& collectionName,
                                        VPackSlice const value, OperationOptions& options);

  /// @deprecated use async variant
  OperationResult insert(std::string const& cname,
                         VPackSlice const value,
                         OperationOptions const& options) {
    return this->insertAsync(cname, value, options).get();
  }

  /// @brief create one or multiple documents in a collection
  /// The single-document variant of this operation will either succeed or,
  /// if it fails, clean up after itself
  Future<OperationResult> insertAsync(std::string const& collectionName,
                                      VPackSlice const value,
                                      OperationOptions const& options);
  
  /// @deprecated use async variant
  OperationResult update(std::string const& cname, VPackSlice const updateValue,
                         OperationOptions const& options) {
    return this->updateAsync(cname, updateValue, options).get();
  }

  /// @brief update/patch one or multiple documents in a collection.
  /// The single-document variant of this operation will either succeed or,
  /// if it fails, clean up after itself
  Future<OperationResult> updateAsync(std::string const& collectionName, VPackSlice const updateValue,
                                      OperationOptions const& options);
  
  /// @deprecated use async variant
  OperationResult replace(std::string const& cname, VPackSlice const replaceValue,
                         OperationOptions const& options) {
    return this->replaceAsync(cname, replaceValue, options).get();
  }

  /// @brief replace one or multiple documents in a collection.
  /// The single-document variant of this operation will either succeed or,
  /// if it fails, clean up after itself
  Future<OperationResult> replaceAsync(std::string const& collectionName, VPackSlice const replaceValue,
                                       OperationOptions const& options);

  /// @deprecated use async variant
  OperationResult remove(std::string const& collectionName,
                         VPackSlice const value, OperationOptions const& options) {
    return removeAsync(collectionName, value, options).get();
  }

  /// @brief remove one or multiple documents in a collection
  /// the single-document variant of this operation will either succeed or,
  /// if it fails, clean up after itself
  Future<OperationResult> removeAsync(std::string const& collectionName,
                                      VPackSlice const value, OperationOptions const& options);

  /// @brief fetches all documents in a collection
  ENTERPRISE_VIRT OperationResult all(std::string const& collectionName, uint64_t skip,
                                      uint64_t limit, OperationOptions const& options);

  /// @brief deprecated use async variant
  OperationResult truncate(std::string const& collectionName, OperationOptions const& options) {
    return this->truncateAsync(collectionName, options).get();
  }

  /// @brief remove all documents in a collection
  Future<OperationResult> truncateAsync(std::string const& collectionName,
                                        OperationOptions const& options);

  /// deprecated, use async variant
  virtual OperationResult count(std::string const& collectionName, CountType type) {
    return countAsync(collectionName, type).get();
  }

  /// @brief count the number of documents in a collection
  virtual futures::Future<OperationResult> countAsync(std::string const& collectionName,
                                                      CountType type);

  /// @brief factory for IndexIterator objects from AQL
  /// note: the caller must have read-locked the underlying collection when
  /// calling this method
  std::unique_ptr<IndexIterator> indexScanForCondition(IndexHandle const&,
                                                       arangodb::aql::AstNode const*,
                                                       arangodb::aql::Variable const*,
                                                       IndexIteratorOptions const&);

  /// @brief factory for IndexIterator objects
  /// note: the caller must have read-locked the underlying collection when
  /// calling this method
  std::unique_ptr<IndexIterator> indexScan(std::string const& collectionName,
                                           CursorType cursorType);

  /// @brief test if a collection is already locked
  ENTERPRISE_VIRT bool isLocked(arangodb::LogicalCollection*, AccessMode::Type) const;
  
  /// @brief fetch the LogicalCollection by CID
  arangodb::LogicalCollection* documentCollection(TRI_voc_cid_t cid) const;
  
  /// @brief fetch the LogicalCollection by name
  arangodb::LogicalCollection* documentCollection(std::string const& name) const;
  
  /// @brief return the collection name resolver
  CollectionNameResolver const* resolver() const;
    
#ifndef USE_ENTERPRISE
  bool skipInaccessible() const {
    return false;
  }
  bool isInaccessibleCollection(TRI_voc_cid_t /*cid*/) const {
    return false;
  }
  bool isInaccessibleCollection(std::string const& /*cname*/) const {
    return false;
  }
#else
  bool skipInaccessible() const;
  bool isInaccessibleCollection(TRI_voc_cid_t /*cid*/) const;
  bool isInaccessibleCollection(std::string const& /*cname*/) const;
#endif

  static int validateSmartJoinAttribute(LogicalCollection const& collinfo,
                                        arangodb::velocypack::Slice value);

 private:
  /// @brief build a VPack object with _id, _key and _rev and possibly
  /// oldRef (if given), the result is added to the builder in the
  /// argument as a single object.

  // SHOULD THE OPTIONS BE CONST?
  void buildDocumentIdentity(arangodb::LogicalCollection* collection,
                             velocypack::Builder& builder, TRI_voc_cid_t cid,
                             arangodb::velocypack::StringRef const& key, TRI_voc_rid_t rid,
                             TRI_voc_rid_t oldRid, ManagedDocumentResult const* oldDoc,
                             ManagedDocumentResult const* newDoc);

  Future<OperationResult> documentCoordinator(std::string const& collectionName,
                                              VPackSlice const value,
                                              OperationOptions& options);

  Future<OperationResult> documentLocal(std::string const& collectionName,
                                        VPackSlice const value, OperationOptions& options);

  Future<OperationResult> insertCoordinator(std::string const& collectionName,
                                            VPackSlice const value,
                                            OperationOptions const& options);

  Future<OperationResult> insertLocal(std::string const& collectionName,
                                      VPackSlice const value, OperationOptions& options);

  Future<OperationResult> modifyCoordinator(std::string const& collectionName,
                                            VPackSlice const newValue,
                                            OperationOptions const& options,
                                            TRI_voc_document_operation_e operation);

  Future<OperationResult> modifyLocal(std::string const& collectionName,
                                      VPackSlice const newValue,
                                      OperationOptions& options,
                                      TRI_voc_document_operation_e operation);

  Future<OperationResult> removeCoordinator(std::string const& collectionName,
                                            VPackSlice const value,
                                            OperationOptions const& options);

  Future<OperationResult> removeLocal(std::string const& collectionName,
                                      VPackSlice const value,
                                      OperationOptions& options);

  OperationResult allCoordinator(std::string const& collectionName, uint64_t skip,
                                 uint64_t limit, OperationOptions& options);

  OperationResult allLocal(std::string const& collectionName, uint64_t skip,
                           uint64_t limit, OperationOptions& options);

  OperationResult anyCoordinator(std::string const& collectionName);

  OperationResult anyLocal(std::string const& collectionName);

  Future<OperationResult> truncateCoordinator(std::string const& collectionName,
                                              OperationOptions& options);

  Future<OperationResult> truncateLocal(std::string const& collectionName,
                                        OperationOptions& options);

 protected:
  /// @brief return the transaction collection for a document collection
  TransactionCollection* trxCollection(
      TRI_voc_cid_t cid, AccessMode::Type type = AccessMode::Type::READ) const;
  
  TransactionCollection* trxCollection(
      std::string const& name, AccessMode::Type type = AccessMode::Type::READ) const;

  futures::Future<OperationResult> countCoordinator(std::string const& collectionName,
                                                    CountType type);

  futures::Future<OperationResult> countCoordinatorHelper(
      std::shared_ptr<LogicalCollection> const& collinfo,
      std::string const& collectionName, CountType type);

  OperationResult countLocal(std::string const& collectionName, CountType type);

  /// @brief add a collection by id, with the name supplied
  Result addCollection(TRI_voc_cid_t, std::string const&, AccessMode::Type);

  /// @brief add a collection by name
  Result addCollection(std::string const&, AccessMode::Type);
  
 protected:
  /// @brief the state
  std::shared_ptr<TransactionState> _state;

  /// @brief the transaction context
  std::shared_ptr<transaction::Context> _transactionContext;
  
  bool _mainTransaction;
  
 private:
  
  Future<Result> replicateOperations(
      LogicalCollection* collection,
      std::shared_ptr<const std::vector<std::string>> const& followers,
      OperationOptions const& options, VPackSlice value, TRI_voc_document_operation_e operation,
      std::shared_ptr<velocypack::Buffer<uint8_t>> const& ops);

 private:
  /// @brief transaction hints
  transaction::Hints _localHints;

  /// @brief name-to-cid lookup cache for last collection seen
  struct {
    TRI_voc_cid_t cid = 0;
    std::string name;
  } _collectionCache;
};

}  // namespace transaction
}  // namespace arangodb

#endif

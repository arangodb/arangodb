////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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

#include "Basics/Common.h"
#include "Basics/Exceptions.h"
#include "Basics/StringRef.h"
#include "Basics/Result.h"
#include "Utils/OperationResult.h"
#include "Transaction/Hints.h"
#include "Transaction/Options.h"
#include "Transaction/Status.h"
#include "VocBase/AccessMode.h"
#include "VocBase/vocbase.h"
#include "VocBase/voc-types.h"

#include <velocypack/Slice.h>

namespace arangodb {

namespace basics {
struct AttributeName;
class StringBuffer;
}

namespace velocypack {
class Builder;
}

namespace aql {
class Ast;
struct AstNode;
class SortCondition;
struct Variable;
}

namespace rest {
enum class ResponseCode;
}

namespace traverser {
class BaseEngine;
}

namespace transaction {
class CallbackInvoker;
class Context;
}

/// @brief forward declarations
class CollectionNameResolver;
struct DocumentIdentifierToken;
class Index;
class ManagedDocumentResult;
struct OperationCursor;
struct OperationOptions;
class TransactionState;
class TransactionCollection;

namespace transaction {
struct Options;
  
#ifdef USE_ENTERPRISE
  #define ENTERPRISE_VIRT virtual
#else
  #define ENTERPRISE_VIRT
#endif

class Methods {
  friend class traverser::BaseEngine;
  friend class CallbackInvoker;

 public:
  class IndexHandle {
    friend class transaction::Methods;

    std::shared_ptr<arangodb::Index> _index;
   public:
    IndexHandle() = default;
    void toVelocyPack(arangodb::velocypack::Builder& builder,
                      bool withFigures) const;
    bool operator==(IndexHandle const& other) const {
      return other._index.get() == _index.get();
    }
    bool operator!=(IndexHandle const& other) const {
      return other._index.get() != _index.get();
    }
    explicit IndexHandle(std::shared_ptr<arangodb::Index> const& idx) : _index(idx) {}
    std::vector<std::vector<std::string>> fieldNames() const;

   public:
    std::shared_ptr<arangodb::Index> getIndex() const;
  };

  using VPackBuilder = arangodb::velocypack::Builder;
  using VPackSlice = arangodb::velocypack::Slice;

  double const TRX_FOLLOWER_TIMEOUT = 3.0;

  /// @brief transaction::Methods
 private:
  Methods() = delete;
  Methods(Methods const&) = delete;
  Methods& operator=(Methods const&) = delete;

 protected:

  /// @brief create the transaction
  Methods(std::shared_ptr<transaction::Context> const& transactionContext, transaction::Options const& options = transaction::Options());

 public:

  /// @brief destroy the transaction
  virtual ~Methods();

 public:

  /// @brief default batch size for index and other operations
  static constexpr uint64_t defaultBatchSize() { return 1000; }

  /// @brief Type of cursor
  enum class CursorType {
    ALL = 0,
    ANY
  };

  /// @brief register a callback for transaction commit or abort
  void registerCallback(std::function<void(arangodb::transaction::Methods* trx)> const& onFinish) { _onFinish = onFinish; }

  /// @brief return database of transaction
  TRI_vocbase_t* vocbase() const;
  inline std::string const& databaseName() const { return vocbase()->name(); }

  /// @brief return internals of transaction
  inline TransactionState* state() const { return _state; }

  Result resolveId(char const* handle, size_t length, TRI_voc_cid_t& cid, char const*& key, size_t& outLength);

  /// @brief return a pointer to the transaction context
  std::shared_ptr<transaction::Context> transactionContext() const {
    return _transactionContext;
  }

  inline transaction::Context* transactionContextPtr() const {
    TRI_ASSERT(_transactionContextPtr != nullptr);
    return _transactionContextPtr;
  }

  /// @brief add a transaction hint
  void addHint(transaction::Hints::Hint hint) { _localHints.set(hint); }

  /// @brief whether or not the transaction consists of a single operation only
  bool isSingleOperationTransaction() const;

  /// @brief get the status of the transaction
  Status status() const;

  /// @brief get the status of the transaction, as a string
  char const* statusString() const { return transaction::statusString(status()); }

  /// @brief begin the transaction
  Result begin();

  /// @brief commit / finish the transaction
  Result commit();

  /// @brief abort the transaction
  Result abort();

  /// @brief finish a transaction (commit or abort), based on the previous state
  Result finish(int errorNum);
  Result finish(Result const& res);

  /// @brief return a collection name
  std::string name(TRI_voc_cid_t cid) const;

  /// @brief order a ditch for a collection
  void pinData(TRI_voc_cid_t);

  /// @brief whether or not a ditch has been created for the collection
  bool isPinned(TRI_voc_cid_t cid) const;

  /// @brief extract the _id attribute from a slice, and convert it into a
  /// string
  std::string extractIdString(VPackSlice);

  /// @brief read many documents, using skip and limit in arbitrary order
  /// The result guarantees that all documents are contained exactly once
  /// as long as the collection is not modified.
  ENTERPRISE_VIRT OperationResult any(std::string const&,
                                      uint64_t skip = 0,
                                      uint64_t limit = 1);

  /// @brief add a collection to the transaction for read, at runtime
  ENTERPRISE_VIRT TRI_voc_cid_t addCollectionAtRuntime(TRI_voc_cid_t cid,
                                       std::string const& collectionName,
                                       AccessMode::Type type = AccessMode::Type::READ);

  /// @brief add a collection to the transaction for read, at runtime
  virtual TRI_voc_cid_t addCollectionAtRuntime(std::string const& collectionName);

  /// @brief return the type of a collection
  bool isEdgeCollection(std::string const& collectionName);
  bool isDocumentCollection(std::string const& collectionName);
  TRI_col_type_e getCollectionType(std::string const& collectionName);

  /// @brief return the name of a collection
  std::string collectionName(TRI_voc_cid_t cid);

  /// @brief Iterate over all elements of the collection.
  ENTERPRISE_VIRT void invokeOnAllElements(std::string const& collectionName,
                           std::function<bool(arangodb::DocumentIdentifierToken const&)>);

  /// @brief return one  document from a collection, fast path
  ///        If everything went well the result will contain the found document
  ///        (as an external on single_server)  and this function will return TRI_ERROR_NO_ERROR.
  ///        If there was an error the code is returned and it is guaranteed
  ///        that result remains unmodified.
  ///        Does not care for revision handling!
  ///        shouldLock indicates if the transaction should lock the collection
  ///        if set to false it will not lock it (make sure it is already locked!)
  ENTERPRISE_VIRT Result documentFastPath(std::string const& collectionName,
                       ManagedDocumentResult* mmdr,
                       arangodb::velocypack::Slice const value,
                       arangodb::velocypack::Builder& result,
                       bool shouldLock);

  /// @brief return one  document from a collection, fast path
  ///        If everything went well the result will contain the found document
  ///        (as an external on single_server)  and this function will return TRI_ERROR_NO_ERROR.
  ///        If there was an error the code is returned
  ///        Does not care for revision handling!
  ///        Must only be called on a local server, not in cluster case!
  Result documentFastPathLocal(std::string const& collectionName,
                               StringRef const& key,
                               ManagedDocumentResult& result,
                               bool shouldLock);

  /// @brief return one or multiple documents from a collection
  OperationResult document(std::string const& collectionName,
                           VPackSlice const value,
                           OperationOptions& options);

  /// @brief create one or multiple documents in a collection
  /// the single-document variant of this operation will either succeed or,
  /// if it fails, clean up after itself
  OperationResult insert(std::string const& collectionName,
                         VPackSlice const value,
                         OperationOptions const& options);

  /// @brief update/patch one or multiple documents in a collecti  Result
  /// the single-document variant of this operation will either succeed or,
  /// if it fails, clean up after itself
  OperationResult update(std::string const& collectionName,
                         VPackSlice const updateValue,
                         OperationOptions const& options);

  /// @brief replace one or multiple documents in a collection
  /// the single-document variant of this operation will either succeed or,
  /// if it fails, clean up after itself
  OperationResult replace(std::string const& collectionName,
                          VPackSlice const updateValue,
                          OperationOptions const& options);

  /// @brief remove one or multiple documents in a collection
  /// the single-document variant of this operation will either succeed or,
  /// if it fails, clean up after itself
  OperationResult remove(std::string const& collectionName,
                         VPackSlice const value,
                         OperationOptions const& options);

  /// @brief fetches all documents in a collection
  ENTERPRISE_VIRT OperationResult all(std::string const& collectionName,
                      uint64_t skip, uint64_t limit,
                      OperationOptions const& options);

  /// @brief remove all documents in a collection
  OperationResult truncate(std::string const& collectionName,
                           OperationOptions const& options);

  /// @brief count the number of documents in a collection
  ENTERPRISE_VIRT OperationResult count(std::string const& collectionName, bool aggregate);

  /// @brief Gets the best fitting index for an AQL condition.
  /// note: the caller must have read-locked the underlying collection when
  /// calling this method
  std::pair<bool, bool> getBestIndexHandlesForFilterCondition(
      std::string const&, arangodb::aql::Ast*, arangodb::aql::AstNode*,
      arangodb::aql::Variable const*, arangodb::aql::SortCondition const*,
      size_t, std::vector<IndexHandle>&, bool&);

  /// @brief Gets the best fitting index for one specific condition.
  ///        Difference to IndexHandles: Condition is only one NARY_AND
  ///        and the Condition stays unmodified. Also does not care for sorting.
  ///        Returns false if no index could be found.
  ///        If it returned true, the AstNode contains the specialized condition

  ENTERPRISE_VIRT bool getBestIndexHandleForFilterCondition(std::string const&,
                                            arangodb::aql::AstNode*&,
                                            arangodb::aql::Variable const*,
                                            size_t, IndexHandle&);

  /// @brief Checks if the index supports the filter condition.
  /// note: the caller must have read-locked the underlying collection when
  /// calling this method
  bool supportsFilterCondition(IndexHandle const&,
                               arangodb::aql::AstNode const*,
                               arangodb::aql::Variable const*, size_t, size_t&,
                               double&);

  /// @brief Get the index features:
  ///        Returns the covered attributes, and sets the first bool value
  ///        to isSorted and the second bool value to isSparse
  std::vector<std::vector<arangodb::basics::AttributeName>> getIndexFeatures(
      IndexHandle const&, bool&, bool&);

  /// @brief Gets the best fitting index for an AQL sort condition
  /// note: the caller must have read-locked the underlying collection when
  /// calling this method
  ENTERPRISE_VIRT std::pair<bool, bool> getIndexForSortCondition(
      std::string const&, arangodb::aql::SortCondition const*,
      arangodb::aql::Variable const*, size_t,
      std::vector<IndexHandle>&,
      size_t& coveredAttributes);

  /// @brief factory for OperationCursor objects from AQL
  /// note: the caller must have read-locked the underlying collection when
  /// calling this method
  OperationCursor* indexScanForCondition(IndexHandle const&,
                                         arangodb::aql::AstNode const*,
                                         arangodb::aql::Variable const*,
                                         ManagedDocumentResult*,
                                         uint64_t, uint64_t, bool);

  /// @brief factory for OperationCursor objects
  /// note: the caller must have read-locked the underlying collection when
  /// calling this method
  std::unique_ptr<OperationCursor> indexScan(std::string const& collectionName,
                                             CursorType cursorType,
                                             ManagedDocumentResult*,
                                             uint64_t skip, uint64_t limit,
                                             uint64_t batchSize, bool reverse);

  /// @brief test if a collection is already locked
  bool isLocked(arangodb::LogicalCollection*, AccessMode::Type) const;

  arangodb::LogicalCollection* documentCollection(TRI_voc_cid_t) const;

/// @brief get the index by it's identifier. Will either throw or
///        return a valid index. nullptr is impossible.
  IndexHandle getIndexByIdentifier(
    std::string const& collectionName, std::string const& indexHandle);

/// @brief get all indexes for a collection name
  std::vector<std::shared_ptr<arangodb::Index>> indexesForCollection(
      std::string const&);

  /// @brief Lock all collections. Only works for selected sub-classes
  virtual int lockCollections();

  /// @brief Clone this transaction. Only works for selected sub-classes
  virtual transaction::Methods* clone(transaction::Options const&) const;

  /// @brief return the collection name resolver
  CollectionNameResolver const* resolver() const;
  
#ifdef USE_ENTERPRISE
  virtual bool isInaccessibleCollection(TRI_voc_cid_t cid) { return false; }
#endif

 private:

  /// @brief build a VPack object with _id, _key and _rev and possibly
  /// oldRef (if given), the result is added to the builder in the
  /// argument as a single object.
  void buildDocumentIdentity(arangodb::LogicalCollection* collection,
                             VPackBuilder& builder, TRI_voc_cid_t cid,
                             StringRef const& key, TRI_voc_rid_t rid,
                             TRI_voc_rid_t oldRid,
                             ManagedDocumentResult const* oldDoc,
                             ManagedDocumentResult const* newDoc);

  OperationResult documentCoordinator(std::string const& collectionName,
                                      VPackSlice const value,
                                      OperationOptions& options);

  OperationResult documentLocal(std::string const& collectionName,
                                VPackSlice const value,
                                OperationOptions& options);

  OperationResult insertCoordinator(std::string const& collectionName,
                                    VPackSlice const value,
                                    OperationOptions& options);

  OperationResult insertLocal(std::string const& collectionName,
                              VPackSlice const value,
                              OperationOptions& options);

  OperationResult updateCoordinator(std::string const& collectionName,
                                    VPackSlice const newValue,
                                    OperationOptions& options);

  OperationResult replaceCoordinator(std::string const& collectionName,
                                     VPackSlice const newValue,
                                     OperationOptions& options);

  OperationResult modifyLocal(std::string const& collectionName,
                              VPackSlice const newValue,
                              OperationOptions& options,
                              TRI_voc_document_operation_e operation);

  OperationResult removeCoordinator(std::string const& collectionName,
                                    VPackSlice const value,
                                    OperationOptions& options);

  OperationResult removeLocal(std::string const& collectionName,
                              VPackSlice const value,
                              OperationOptions& options);

  OperationResult allCoordinator(std::string const& collectionName,
                                 uint64_t skip, uint64_t limit,
                                 OperationOptions& options);

  OperationResult allLocal(std::string const& collectionName,
                           uint64_t skip, uint64_t limit,
                           OperationOptions& options);

  OperationResult anyCoordinator(std::string const& collectionName,
                                 uint64_t skip, uint64_t limit);

  OperationResult anyLocal(std::string const& collectionName, uint64_t skip,
                           uint64_t limit);

  OperationResult truncateCoordinator(std::string const& collectionName,
                                      OperationOptions& options);

  OperationResult truncateLocal(std::string const& collectionName,
                                OperationOptions& options);

  OperationResult countCoordinator(std::string const& collectionName, bool aggregate);
  OperationResult countLocal(std::string const& collectionName);

 protected:
  /// @brief return the transaction collection for a document collection
  TransactionCollection* trxCollection(TRI_voc_cid_t cid) const;

  /// @brief return the collection
  arangodb::LogicalCollection* documentCollection(
      TransactionCollection const*) const;

  /// @brief add a collection by id, with the name supplied
  ENTERPRISE_VIRT Result addCollection(TRI_voc_cid_t, char const*, AccessMode::Type);

  /// @brief add a collection by id, with the name supplied
  Result addCollection(TRI_voc_cid_t, std::string const&, AccessMode::Type);

  /// @brief add a collection by id
  Result addCollection(TRI_voc_cid_t, AccessMode::Type);

  /// @brief add a collection by name
  Result addCollection(std::string const&, AccessMode::Type);

  /// @brief read- or write-lock a collection
  Result lock(TransactionCollection*, AccessMode::Type);

  /// @brief read- or write-unlock a collection
  Result unlock(TransactionCollection*, AccessMode::Type);

 private:

/// @brief Helper create a Cluster Communication document
  OperationResult clusterResultDocument(
      rest::ResponseCode const& responseCode,
      std::shared_ptr<arangodb::velocypack::Builder> const& resultBody,
      std::unordered_map<int, size_t> const& errorCounter) const;

/// @brief Helper create a Cluster Communication insert
  OperationResult clusterResultInsert(
      rest::ResponseCode const& responseCode,
      std::shared_ptr<arangodb::velocypack::Builder> const& resultBody,
      std::unordered_map<int, size_t> const& errorCounter) const;

/// @brief Helper create a Cluster Communication modify result
  OperationResult clusterResultModify(
      rest::ResponseCode const& responseCode,
      std::shared_ptr<arangodb::velocypack::Builder> const& resultBody,
      std::unordered_map<int, size_t> const& errorCounter) const;

/// @brief Helper create a Cluster Communication remove result
  OperationResult clusterResultRemove(
      rest::ResponseCode const& responseCode,
      std::shared_ptr<arangodb::velocypack::Builder> const& resultBody,
      std::unordered_map<int, size_t> const& errorCounter) const;

  /// @brief sort ORs for the same attribute so they are in ascending value
  /// order. this will only work if the condition is for a single attribute
  /// the usedIndexes vector may also be re-sorted
  bool sortOrs(arangodb::aql::Ast* ast,
               arangodb::aql::AstNode* root,
               arangodb::aql::Variable const* variable,
               std::vector<transaction::Methods::IndexHandle>& usedIndexes);

  /// @brief findIndexHandleForAndNode
  std::pair<bool, bool> findIndexHandleForAndNode(
      std::vector<std::shared_ptr<Index>> indexes, arangodb::aql::AstNode* node,
      arangodb::aql::Variable const* reference,
      arangodb::aql::SortCondition const* sortCondition,
      size_t itemsInCollection,
      std::vector<transaction::Methods::IndexHandle>& usedIndexes,
      arangodb::aql::AstNode*& specializedCondition,
      bool& isSparse) const;

  /// @brief findIndexHandleForAndNode, Shorthand which does not support Sort
  bool findIndexHandleForAndNode(std::vector<std::shared_ptr<Index>> indexes,
                                 arangodb::aql::AstNode*& node,
                                 arangodb::aql::Variable const* reference,
                                 size_t itemsInCollection,
                                 transaction::Methods::IndexHandle& usedIndex) const;

  /// @brief Get one index by id for a collection name, coordinator case
  std::shared_ptr<arangodb::Index> indexForCollectionCoordinator(
      std::string const&, std::string const&) const;

  /// @brief Get all indexes for a collection name, coordinator case
  std::vector<std::shared_ptr<arangodb::Index>> indexesForCollectionCoordinator(
      std::string const&) const;

  /// @brief add a collection to an embedded transaction
  Result addCollectionEmbedded(TRI_voc_cid_t, char const* name, AccessMode::Type);

  /// @brief add a collection to a top-level transaction
  Result addCollectionToplevel(TRI_voc_cid_t, char const* name, AccessMode::Type);

  /// @brief set up an embedded transaction
  void setupEmbedded(TRI_vocbase_t*);

  /// @brief set up a top-level transaction
  void setupToplevel(TRI_vocbase_t*, transaction::Options const&);

 protected:
  /// @brief the state
  TransactionState* _state;

  /// @brief the transaction context
  std::shared_ptr<transaction::Context> _transactionContext;

  /// @brief pointer to transaction context (faster than shared ptr)
  transaction::Context* const _transactionContextPtr;

 private:
  /// @brief transaction hints
  transaction::Hints _localHints;

  /// @brief name-to-cid lookup cache for last collection seen
  struct {
    TRI_voc_cid_t cid = 0;
    std::string name;
  }
  _collectionCache;

  /// @brief optional callback function that will be called on transaction
  /// commit or abort
  std::function<void(arangodb::transaction::Methods* trx)> _onFinish;
};

class CallbackInvoker {
 public:
  explicit CallbackInvoker(transaction::Methods* trx) : _trx(trx) {}
  ~CallbackInvoker() {
    invoke();
  }

  void invoke() noexcept;

 private:
  transaction::Methods* _trx;
};

}
}

#endif

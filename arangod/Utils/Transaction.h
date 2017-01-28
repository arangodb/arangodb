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

#ifndef ARANGOD_UTILS_TRANSACTION_H
#define ARANGOD_UTILS_TRANSACTION_H 1

#include "Basics/Common.h"
#include "Basics/Exceptions.h"
#include "Basics/StringRef.h"
#include "Cluster/ServerState.h"
#include "Utils/OperationResult.h"
#include "Utils/TransactionHints.h"
#include "VocBase/AccessMode.h"
#include "VocBase/vocbase.h"
#include "VocBase/voc-types.h"

#include <velocypack/Slice.h>

namespace rocksdb {
class Transaction;
}

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
class BaseTraverserEngine;
}

/// @brief forward declarations
class CollectionNameResolver;
class DocumentDitch;
struct DocumentIdentifierToken;
class Index;
class ManagedDocumentResult;
struct OperationCursor;
struct OperationOptions;
class TransactionContext;
struct TransactionState;
struct TransactionCollection;

class Transaction {
  friend class traverser::BaseTraverserEngine;

 public:

  /// @brief transaction statuses
  enum class Status : uint32_t {
    UNDEFINED = 0,
    CREATED = 1,
    RUNNING = 2,
    COMMITTED = 3,
    ABORTED = 4
  };

  /// @brief time (in seconds) that is spent waiting for a lock
  static constexpr double DefaultLockTimeout = 30.0; 

  class IndexHandle {
    friend class Transaction;
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
    explicit IndexHandle(std::shared_ptr<arangodb::Index> idx) : _index(idx) {
    }
    std::vector<std::vector<std::string>> fieldNames() const;

    bool isMMFilesEdgeIndex() const;

   public:
    std::shared_ptr<arangodb::Index> getIndex() const;
  };

  using VPackBuilder = arangodb::velocypack::Builder;
  using VPackSlice = arangodb::velocypack::Slice;
  
  double const TRX_FOLLOWER_TIMEOUT = 3.0;

  /// @brief Transaction
 private:
  Transaction() = delete;
  Transaction(Transaction const&) = delete;
  Transaction& operator=(Transaction const&) = delete;

 protected:

  /// @brief create the transaction
  explicit Transaction(std::shared_ptr<TransactionContext> transactionContext);

 public:

  /// @brief destroy the transaction
  virtual ~Transaction();

 public:

  /// @brief default batch size for index and other operations
  static constexpr uint64_t defaultBatchSize() { return 1000; }

  /// @brief Type of cursor
  enum class CursorType {
    ALL = 0,
    ANY,
    INDEX
  };

  /// @brief return database of transaction
  inline TRI_vocbase_t* vocbase() const { return _vocbase; }

  /// @brief return internals of transaction
  inline TransactionState* getInternals() const { return _trx; }
  
  /// @brief return role of server in cluster
  inline ServerState::RoleEnum serverRole() const { return _serverRole; }
  
  bool isCluster();

  int resolveId(char const* handle, size_t length, TRI_voc_cid_t& cid, char const*& key, size_t& outLength); 
  
  /// @brief return a pointer to the transaction context
  std::shared_ptr<TransactionContext> transactionContext() const {
    return _transactionContext;
  }
  
  inline TransactionContext* transactionContextPtr() const {
    return _transactionContextPtr;
  }
  
  /// @brief get (or create) a rocksdb WriteTransaction
  rocksdb::Transaction* rocksTransaction();

  /// @brief add a transaction hint
  void addHint(TransactionHints::Hint hint, bool passthrough);

  /// @brief remove a transaction hint
  void removeHint(TransactionHints::Hint hint, bool passthrough);

  /// @brief return the registered error data
  std::string const getErrorData() const { return _errorData; }

  /// @brief return the names of all collections used in the transaction
  std::vector<std::string> collectionNames() const;

  /// @brief return the collection name resolver
  CollectionNameResolver const* resolver();

  /// @brief whether or not the transaction is embedded
  inline bool isEmbeddedTransaction() const { return (_nestingLevel > 0); }
  
  /// @brief whether or not the transaction consists of a single operation only
  bool isSingleOperationTransaction() const;

  /// @brief get the status of the transaction
  Status getStatus() const;

  int nestingLevel() const { return _nestingLevel; }

  /// @brief begin the transaction
  int begin();

  /// @brief commit / finish the transaction
  int commit();

  /// @brief abort the transaction
  int abort();

  /// @brief finish a transaction (commit or abort), based on the previous state
  int finish(int errorNum);

  /// @brief return a collection name
  std::string name(TRI_voc_cid_t cid) const;

  /// @brief order a ditch for a collection
  arangodb::DocumentDitch* orderDitch(TRI_voc_cid_t);
  
  /// @brief whether or not a ditch has been created for the collection
  bool hasDitch(TRI_voc_cid_t cid) const;

  /// @brief extract the _key attribute from a slice
  static StringRef extractKeyPart(VPackSlice const);

  /// @brief extract the _id attribute from a slice, and convert it into a 
  /// string
  std::string extractIdString(VPackSlice);

  static std::string extractIdString(CollectionNameResolver const*, 
                                     VPackSlice, VPackSlice const&);

  /// @brief quick access to the _key attribute in a database document
  /// the document must have at least two attributes, and _key is supposed to
  /// be the first one
  static VPackSlice extractKeyFromDocument(VPackSlice);
  
  /// @brief quick access to the _id attribute in a database document
  /// the document must have at least two attributes, and _id is supposed to
  /// be the second one
  /// note that this may return a Slice of type Custom!
  static VPackSlice extractIdFromDocument(VPackSlice);

  /// @brief quick access to the _from attribute in a database document
  /// the document must have at least five attributes: _key, _id, _from, _to
  /// and _rev (in this order)
  static VPackSlice extractFromFromDocument(VPackSlice);

  /// @brief quick access to the _to attribute in a database document
  /// the document must have at least five attributes: _key, _id, _from, _to
  /// and _rev (in this order)
  static VPackSlice extractToFromDocument(VPackSlice);
  
  /// @brief extract _key and _rev from a document, in one go
  /// this is an optimized version used when loading collections, WAL 
  /// collection and compaction
  static void extractKeyAndRevFromDocument(VPackSlice slice,
                                           VPackSlice& keySlice,
                                           TRI_voc_rid_t& revisionId);
  
  /// @brief extract _rev from a database document
  static TRI_voc_rid_t extractRevFromDocument(VPackSlice slice);
  static VPackSlice extractRevSliceFromDocument(VPackSlice slice);

  /// @brief read any (random) document
  OperationResult any(std::string const&);

  /// @brief read many documents, using skip and limit in arbitrary order
  /// The result guarantees that all documents are contained exactly once
  /// as long as the collection is not modified.
  OperationResult any(std::string const&, uint64_t, uint64_t);

  /// @brief add a collection to the transaction for read, at runtime
  TRI_voc_cid_t addCollectionAtRuntime(TRI_voc_cid_t cid, 
                                       std::string const& collectionName,
                                       AccessMode::Type type = AccessMode::Type::READ);
  
  /// @brief add a collection to the transaction for read, at runtime
  TRI_voc_cid_t addCollectionAtRuntime(std::string const& collectionName);

  /// @brief return the type of a collection
  bool isEdgeCollection(std::string const& collectionName);
  bool isDocumentCollection(std::string const& collectionName);
  TRI_col_type_e getCollectionType(std::string const& collectionName);

  /// @brief return the name of a collection
  std::string collectionName(TRI_voc_cid_t cid); 
  
  /// @brief return the edge index handle of collection
  IndexHandle edgeIndexHandle(std::string const&); 
  
  /// @brief Iterate over all elements of the collection.
  void invokeOnAllElements(std::string const& collectionName,
                           std::function<bool(arangodb::DocumentIdentifierToken const&)>);

  /// @brief return one  document from a collection, fast path
  ///        If everything went well the result will contain the found document
  ///        (as an external on single_server)  and this function will return TRI_ERROR_NO_ERROR.
  ///        If there was an error the code is returned and it is guaranteed
  ///        that result remains unmodified.
  ///        Does not care for revision handling!
  ///        shouldLock indicates if the transaction should lock the collection
  ///        if set to false it will not lock it (make sure it is already locked!)
  int documentFastPath(std::string const& collectionName,
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
  int documentFastPathLocal(std::string const& collectionName,
                            std::string const& key,
                            ManagedDocumentResult& result);
 
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
  
  /// @brief update/patch one or multiple documents in a collection
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
  OperationResult all(std::string const& collectionName,
                      uint64_t skip, uint64_t limit,
                      OperationOptions const& options);
  
  /// @brief remove all documents in a collection
  OperationResult truncate(std::string const& collectionName,
                           OperationOptions const& options);
  
  /// @brief count the number of documents in a collection
  OperationResult count(std::string const& collectionName, bool aggregate);

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

  bool getBestIndexHandleForFilterCondition(std::string const&,
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
  std::pair<bool, bool> getIndexForSortCondition(
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
                                             IndexHandle const& indexId,
                                             VPackSlice const search,
                                             ManagedDocumentResult*,
                                             uint64_t skip, uint64_t limit,
                                             uint64_t batchSize, bool reverse);

  /// @brief test if a collection is already locked
  bool isLocked(arangodb::LogicalCollection*, AccessMode::Type);

  /// @brief return the setup state
  int setupState() { return _setupState; }
  
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
  virtual Transaction* clone() const;
  
 private:
  
  /// @brief creates an id string from a custom _id value and the _key string
  static std::string makeIdFromCustom(CollectionNameResolver const* resolver,
                                      VPackSlice const& idPart, 
                                      VPackSlice const& keyPart);

  /// @brief build a VPack object with _id, _key and _rev and possibly
  /// oldRef (if given), the result is added to the builder in the
  /// argument as a single object.
  void buildDocumentIdentity(arangodb::LogicalCollection* collection,
                             VPackBuilder& builder, TRI_voc_cid_t cid,
                             StringRef const& key, TRI_voc_rid_t rid,
                             TRI_voc_rid_t oldRid,
                             uint8_t const* oldVPack,
                             uint8_t const* newVPack);

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

  static OperationResult buildCountResult(std::vector<std::pair<std::string, uint64_t>> const& count, bool aggregate);

  /// @brief return the transaction collection for a document collection
  TransactionCollection* trxCollection(TRI_voc_cid_t cid) const;

  /// @brief return the collection
  arangodb::LogicalCollection* documentCollection(
      TransactionCollection const*) const;
  
  /// @brief add a collection by id, with the name supplied
  int addCollection(TRI_voc_cid_t, char const*, AccessMode::Type);

  /// @brief add a collection by id, with the name supplied
  int addCollection(TRI_voc_cid_t, std::string const&, AccessMode::Type);

  /// @brief add a collection by id
  int addCollection(TRI_voc_cid_t, AccessMode::Type);
  
  /// @brief add a collection by name
  int addCollection(std::string const&, AccessMode::Type);

  /// @brief set the lock acquisition timeout
  void setTimeout(double timeout) { _timeout = timeout; }

  /// @brief set the waitForSync property
  void setWaitForSync() { _waitForSync = true; }

  /// @brief set the allowImplicitCollections property
  void setAllowImplicitCollections(bool value);

  /// @brief read- or write-lock a collection
  int lock(TransactionCollection*, AccessMode::Type);

  /// @brief read- or write-unlock a collection
  int unlock(TransactionCollection*, AccessMode::Type);

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
               std::vector<arangodb::Transaction::IndexHandle>& usedIndexes);

  /// @brief findIndexHandleForAndNode
  std::pair<bool, bool> findIndexHandleForAndNode(
      std::vector<std::shared_ptr<Index>> indexes, arangodb::aql::AstNode* node,
      arangodb::aql::Variable const* reference,
      arangodb::aql::SortCondition const* sortCondition,
      size_t itemsInCollection,
      std::vector<Transaction::IndexHandle>& usedIndexes,
      arangodb::aql::AstNode*& specializedCondition,
      bool& isSparse) const;

  /// @brief findIndexHandleForAndNode, Shorthand which does not support Sort
  bool findIndexHandleForAndNode(std::vector<std::shared_ptr<Index>> indexes,
                                 arangodb::aql::AstNode*& node,
                                 arangodb::aql::Variable const* reference,
                                 size_t itemsInCollection,
                                 Transaction::IndexHandle& usedIndex) const;

  /// @brief Get one index by id for a collection name, coordinator case
  std::shared_ptr<arangodb::Index> indexForCollectionCoordinator(
      std::string const&, std::string const&) const;

  /// @brief Get all indexes for a collection name, coordinator case
  std::vector<std::shared_ptr<arangodb::Index>> indexesForCollectionCoordinator(
      std::string const&) const;

  /// @brief register an error for the transaction
  int registerError(int errorNum) {
    TRI_ASSERT(errorNum != TRI_ERROR_NO_ERROR);

    if (_setupState == TRI_ERROR_NO_ERROR) {
      _setupState = errorNum;
    }

    TRI_ASSERT(_setupState != TRI_ERROR_NO_ERROR);

    return errorNum;
  }

  /// @brief add a collection to an embedded transaction
  int addCollectionEmbedded(TRI_voc_cid_t, AccessMode::Type);

  /// @brief add a collection to a top-level transaction
  int addCollectionToplevel(TRI_voc_cid_t, AccessMode::Type);

  /// @brief initialize the transaction
  /// this will first check if the transaction is embedded in a parent
  /// transaction. if not, it will create a transaction of its own
  int setupTransaction();

  /// @brief set up an embedded transaction
  int setupEmbedded();

  /// @brief set up a top-level transaction
  int setupToplevel();

  /// @brief free transaction
  void freeTransaction();

 private:
  /// @brief role of server in cluster
  ServerState::RoleEnum _serverRole;

  /// @brief error that occurred on transaction initialization (before begin())
  int _setupState;

  /// @brief how deep the transaction is down in a nested transaction structure
  int _nestingLevel;

  /// @brief additional error data
  std::string _errorData;

  /// @brief transaction hints
  TransactionHints _hints;

  /// @brief timeout for lock acquisition
  double _timeout;

  /// @brief wait for sync property for transaction
  bool _waitForSync;

  /// @brief allow implicit collections for transaction
  bool _allowImplicitCollections;

  /// @brief whether or not this is a "real" transaction
  bool _isReal;

 protected:
  /// @brief the C transaction struct
  TransactionState* _trx;

  /// @brief the vocbase
  TRI_vocbase_t* const _vocbase;
  
  /// @brief collection name resolver (cached)
  CollectionNameResolver const* _resolver;

  /// @brief the transaction context
  std::shared_ptr<TransactionContext> _transactionContext;

  /// @brief cache for last handed out DocumentDitch
  struct {
    TRI_voc_cid_t cid = 0;
    DocumentDitch* ditch = nullptr;
  }
  _ditchCache;

  struct {
    TRI_voc_cid_t cid = 0;
    std::string name;
  }
  _collectionCache;
  
  /// @brief pointer to transaction context (faster than shared ptr)
  TransactionContext* const _transactionContextPtr;

 public:
  /// @brief makeNolockHeaders
  static thread_local std::unordered_set<std::string>* _makeNolockHeaders;
};

class StringBufferLeaser {
 public:
  explicit StringBufferLeaser(arangodb::Transaction*); 
  explicit StringBufferLeaser(arangodb::TransactionContext*); 
  ~StringBufferLeaser();
  arangodb::basics::StringBuffer* stringBuffer() const { return _stringBuffer; }
  arangodb::basics::StringBuffer* operator->() const { return _stringBuffer; }
  arangodb::basics::StringBuffer* get() const { return _stringBuffer; }
 private:
  arangodb::TransactionContext* _transactionContext;
  arangodb::basics::StringBuffer* _stringBuffer;
};

class TransactionBuilderLeaser {
 public:
  explicit TransactionBuilderLeaser(arangodb::Transaction*); 
  explicit TransactionBuilderLeaser(arangodb::TransactionContext*); 
  ~TransactionBuilderLeaser();
  inline arangodb::velocypack::Builder* builder() const { return _builder; }
  inline arangodb::velocypack::Builder* operator->() const { return _builder; }
  inline arangodb::velocypack::Builder* get() const { return _builder; }
  inline arangodb::velocypack::Builder* steal() { 
    arangodb::velocypack::Builder* res = _builder;
    _builder = nullptr;
    return res;
  }
 private:
  arangodb::TransactionContext* _transactionContext;
  arangodb::velocypack::Builder* _builder;
};

}

#endif

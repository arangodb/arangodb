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

#ifndef ARANGOD_VOCBASE_PHYSICAL_COLLECTION_H
#define ARANGOD_VOCBASE_PHYSICAL_COLLECTION_H 1

#include <set>

#include <velocypack/Builder.h>

#include "Basics/Common.h"
#include "Basics/ReadWriteLock.h"
#include "Containers/MerkleTree.h"
#include "Futures/Future.h"
#include "Indexes/Index.h"
#include "Indexes/IndexIterator.h"
#include "StorageEngine/ReplicationIterator.h"
#include "Utils/OperationResult.h"
#include "VocBase/Identifiers/IndexId.h"
#include "VocBase/voc-types.h"

namespace arangodb {

class LocalDocumentId;
class Index;
class IndexIterator;
class LogicalCollection;
class ManagedDocumentResult;
struct OperationOptions;
class Result;

class PhysicalCollection {
 public:
  constexpr static double defaultLockTimeout = 10.0 * 60.0;
  
  virtual ~PhysicalCollection() = default;

  virtual PhysicalCollection* clone(LogicalCollection& logical) const = 0;

  // path to logical collection
  virtual std::string const& path() const = 0;
  // creation happens atm in engine->createCollection
  virtual arangodb::Result updateProperties(arangodb::velocypack::Slice const& slice,
                                            bool doSync) = 0;
  virtual TRI_voc_rid_t revision(arangodb::transaction::Methods* trx) const = 0;

  /// @brief export properties
  virtual void getPropertiesVPack(velocypack::Builder&) const = 0;

  virtual int close() = 0;
  virtual void load() = 0;
  virtual void unload() = 0;

  // @brief Return the number of documents in this collection
  virtual uint64_t numberDocuments(transaction::Methods* trx) const = 0;

  /// @brief report extra memory used by indexes etc.
  virtual size_t memory() const = 0;

  void drop();

  /// recalculate counts for collection in case of failure, blocking
  virtual uint64_t recalculateCounts();

  /// @brief whether or not the collection contains any documents. this
  /// function is allowed to return true even if there are no documents
  virtual bool hasDocuments();

  ////////////////////////////////////
  // -- SECTION Indexes --
  ///////////////////////////////////

  /// @brief fetches current index selectivity estimates
  /// if allowUpdate is true, will potentially make a cluster-internal roundtrip
  /// to fetch current values!
  virtual IndexEstMap clusterIndexEstimates(bool allowUpdating, TRI_voc_tick_t tid);

  /// @brief flushes the current index selectivity estimates
  virtual void flushClusterIndexEstimates();

  virtual void prepareIndexes(arangodb::velocypack::Slice indexesSlice) = 0;

  bool hasIndexOfType(arangodb::Index::IndexType type) const;

  /// @brief determines order of index execution on collection
  struct IndexOrder {
    bool operator()(const std::shared_ptr<Index>& left,
                              const std::shared_ptr<Index>& right) const;
  };

  using IndexContainerType = std::set<std::shared_ptr<Index>, IndexOrder> ;
  /// @brief find index by definition
  static std::shared_ptr<Index> findIndex(velocypack::Slice const&,
                                          IndexContainerType const&);
  /// @brief Find index by definition
  std::shared_ptr<Index> lookupIndex(velocypack::Slice const&) const;

  /// @brief Find index by iid
  std::shared_ptr<Index> lookupIndex(IndexId) const;

  /// @brief Find index by name
  std::shared_ptr<Index> lookupIndex(std::string const&) const;

  /// @brief get list of all indices
  std::vector<std::shared_ptr<Index>> getIndexes() const;

  void getIndexesVPack(velocypack::Builder&,
                       std::function<bool(arangodb::Index const*, std::underlying_type<Index::Serialize>::type&)> const& filter) const;

  /// @brief return the figures for a collection
  virtual futures::Future<OperationResult> figures(bool details);

  /// @brief create or restore an index
  /// @param restore utilize specified ID, assume index has to be created
  virtual std::shared_ptr<Index> createIndex(arangodb::velocypack::Slice const& info,
                                             bool restore, bool& created) = 0;

  virtual bool dropIndex(IndexId iid) = 0;

  virtual std::unique_ptr<IndexIterator> getAllIterator(transaction::Methods* trx) const = 0;
  virtual std::unique_ptr<IndexIterator> getAnyIterator(transaction::Methods* trx) const = 0;

  /// @brief Get an iterator associated with the specified replication batch
  virtual std::unique_ptr<ReplicationIterator> getReplicationIterator(ReplicationIterator::Ordering,
                                                                      uint64_t batchId);

  /// @brief Get an iterator associated with the specified transaction
  virtual std::unique_ptr<ReplicationIterator> getReplicationIterator(
      ReplicationIterator::Ordering, transaction::Methods&);

  virtual void adjustNumberDocuments(transaction::Methods&, int64_t);

  ////////////////////////////////////
  // -- SECTION DML Operations --
  ///////////////////////////////////

  virtual Result truncate(transaction::Methods& trx, OperationOptions& options) = 0;

  /// @brief compact-data operation
  virtual Result compact() = 0;

  /// @brief Defer a callback to be executed when the collection
  ///        can be dropped. The callback is supposed to drop
  ///        the collection and it is guaranteed that no one is using
  ///        it at that moment.
  virtual void deferDropCollection(std::function<bool(LogicalCollection&)> const& callback) = 0;
  
  virtual Result lookupKey(transaction::Methods*,
                           arangodb::velocypack::StringRef,
                           std::pair<LocalDocumentId, TRI_voc_rid_t>&) const = 0;

  virtual Result read(transaction::Methods*, arangodb::velocypack::StringRef const& key,
                      ManagedDocumentResult& result) = 0;

  /// @brief read a documument referenced by token (internal method)
  virtual bool readDocument(transaction::Methods* trx, LocalDocumentId const& token,
                            ManagedDocumentResult& result) const = 0;

  /// @brief read a documument referenced by token (internal method)
  virtual bool readDocumentWithCallback(transaction::Methods* trx,
                                        LocalDocumentId const& token,
                                        IndexIterator::DocumentCallback const& cb) const = 0;
  /**
   * @brief Perform document insert, may generate a '_key' value
   * If (options.returnNew == false && !options.silent) result might
   * just contain an object with the '_key' field
   */
  virtual Result insert(arangodb::transaction::Methods* trx,
                        arangodb::velocypack::Slice newSlice,
                        arangodb::ManagedDocumentResult& result,
                        OperationOptions& options) = 0;

  virtual Result update(arangodb::transaction::Methods* trx,
                        arangodb::velocypack::Slice newSlice,
                        ManagedDocumentResult& result, OperationOptions& options,
                        ManagedDocumentResult& previous) = 0;

  virtual Result replace(arangodb::transaction::Methods* trx,
                         arangodb::velocypack::Slice newSlice,
                         ManagedDocumentResult& result, OperationOptions& options,
                         ManagedDocumentResult& previous) = 0;

  virtual Result remove(transaction::Methods& trx, velocypack::Slice slice,
                        ManagedDocumentResult& previous, OperationOptions& options) = 0;

  virtual Result remove(transaction::Methods& trx, LocalDocumentId documentId,
                        ManagedDocumentResult& previous, OperationOptions& options);

  /// @brief new object for insert, value must have _key set correctly.
  Result newObjectForInsert(transaction::Methods* trx, velocypack::Slice const& value,
                            bool isEdgeCollection, velocypack::Builder& builder,
                            bool isRestore, TRI_voc_rid_t& revisionId) const;

  virtual std::unique_ptr<containers::RevisionTree> revisionTree(
      transaction::Methods& trx);
  virtual std::unique_ptr<containers::RevisionTree> revisionTree(uint64_t batchId);

  virtual Result rebuildRevisionTree();

  virtual void placeRevisionTreeBlocker(TRI_voc_tid_t transactionId);
  virtual void removeRevisionTreeBlocker(TRI_voc_tid_t transactionId);

  TRI_voc_rid_t newRevisionId() const;

  virtual Result upgrade();
  virtual bool didPartialUpgrade();
  virtual Result cleanupAfterUpgrade();

 protected:
  PhysicalCollection(LogicalCollection& collection, arangodb::velocypack::Slice const& info);

  /// @brief Inject figures that are specific to StorageEngine
  virtual void figuresSpecific(bool details, arangodb::velocypack::Builder&) = 0;

  // SECTION: Document pre commit preperation

  bool isValidEdgeAttribute(velocypack::Slice const& slice) const;

  /// @brief new object for remove, must have _key set
  void newObjectForRemove(transaction::Methods* trx, velocypack::Slice const& oldValue,
                          velocypack::Builder& builder, bool isRestore,
                          TRI_voc_rid_t& revisionId) const;

  /// @brief merge two objects for update
  Result mergeObjectsForUpdate(transaction::Methods* trx, velocypack::Slice const& oldValue,
                               velocypack::Slice const& newValue,
                               bool isEdgeCollection, bool mergeObjects,
                               bool keepNull, velocypack::Builder& builder,
                               bool isRestore, TRI_voc_rid_t& revisionId) const;

  /// @brief new object for replace
  Result newObjectForReplace(transaction::Methods* trx, velocypack::Slice const& oldValue,
                             velocypack::Slice const& newValue,
                             bool isEdgeCollection, velocypack::Builder& builder,
                             bool isRestore, TRI_voc_rid_t& revisionId) const;

  int checkRevision(transaction::Methods* trx, TRI_voc_rid_t expected,
                    TRI_voc_rid_t found) const;

  LogicalCollection& _logicalCollection;
  bool const _isDBServer;

  mutable basics::ReadWriteLock _indexesLock;
  IndexContainerType _indexes;
};

}  // namespace arangodb

#endif

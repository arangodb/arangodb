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

#include "Basics/Common.h"
#include "Basics/ReadWriteLock.h"
#include "Indexes/Index.h"
#include "Indexes/IndexIterator.h"
#include "VocBase/voc-types.h"

#include <velocypack/Builder.h>

namespace arangodb {
namespace transaction {
class Methods;
}

class LocalDocumentId;
class Index;
class IndexIterator;
class LogicalCollection;
class ManagedDocumentResult;
struct OperationOptions;
class Result;

class PhysicalCollection {
 protected:
  PhysicalCollection(LogicalCollection* collection,
                     arangodb::velocypack::Slice const& info);

 public:
  virtual ~PhysicalCollection() = default;

  // path to logical collection
  virtual std::string const& path() const = 0;
  virtual void setPath(
      std::string const&) = 0;  // should be set during collection creation
  // creation happens atm in engine->createCollection
  virtual arangodb::Result updateProperties(
      arangodb::velocypack::Slice const& slice, bool doSync) = 0;
  virtual arangodb::Result persistProperties() = 0;

  virtual PhysicalCollection* clone(LogicalCollection*) const = 0;

  virtual TRI_voc_rid_t revision(arangodb::transaction::Methods* trx) const = 0;

  /// @brief export properties
  virtual void getPropertiesVPack(velocypack::Builder&) const = 0;
  /// @brief used for updating properties
  virtual void getPropertiesVPackCoordinator(velocypack::Builder&) const = 0;

  /// @brief return the figures for a collection
  std::shared_ptr<velocypack::Builder> figures();

  void figures(std::shared_ptr<arangodb::velocypack::Builder>& builder);

  virtual int close() = 0;
  virtual void load() = 0;
  virtual void unload() = 0;

  /// @brief rotate the active journal - will do nothing if there is no journal
  virtual int rotateActiveJournal() { return TRI_ERROR_NO_ERROR; }

  // @brief Return the number of documents in this collection
  virtual uint64_t numberDocuments(transaction::Methods* trx) const = 0;

  /// @brief report extra memory used by indexes etc.
  virtual size_t memory() const = 0;

  /// @brief opens an existing collection
  virtual void open(bool ignoreErrors) = 0;

  void drop();

  ////////////////////////////////////
  // -- SECTION Indexes --
  ///////////////////////////////////

  virtual void prepareIndexes(arangodb::velocypack::Slice indexesSlice) = 0;

  bool hasIndexOfType(arangodb::Index::IndexType type) const;

  /// @brief Find index by definition
  virtual std::shared_ptr<Index> lookupIndex(
      velocypack::Slice const&) const = 0;

  /// @brief Find index by iid
  std::shared_ptr<Index> lookupIndex(TRI_idx_iid_t) const;

  std::vector<std::shared_ptr<Index>> getIndexes() const;

  void getIndexesVPack(velocypack::Builder&, bool withFigures, bool forPersistence,
                       std::function<bool(arangodb::Index const*)> const& filter) const;

  virtual std::shared_ptr<Index> createIndex(
      transaction::Methods* trx, arangodb::velocypack::Slice const& info,
      bool& created) = 0;

  /// @brief Restores an index from VelocyPack.
  virtual int restoreIndex(transaction::Methods*, velocypack::Slice const&,
                           std::shared_ptr<Index>&) = 0;

  virtual bool dropIndex(TRI_idx_iid_t iid) = 0;

  virtual std::unique_ptr<IndexIterator> getAllIterator(
      transaction::Methods* trx, ManagedDocumentResult* mdr, bool reverse) const = 0;
  virtual std::unique_ptr<IndexIterator> getAnyIterator(
      transaction::Methods* trx, ManagedDocumentResult* mdr) const = 0;
  virtual void invokeOnAllElements(
      transaction::Methods* trx,
      std::function<bool(LocalDocumentId const&)> callback) = 0;

  ////////////////////////////////////
  // -- SECTION DML Operations --
  ///////////////////////////////////

  virtual Result truncate(transaction::Methods* trx,
                          OperationOptions& options) = 0;

  virtual LocalDocumentId lookupKey(
      transaction::Methods*, arangodb::velocypack::Slice const&) = 0;

  virtual Result read(transaction::Methods*,
                      arangodb::StringRef const& key,
                      ManagedDocumentResult& result, bool) = 0;

  virtual Result read(transaction::Methods*,
                      arangodb::velocypack::Slice const& key,
                      ManagedDocumentResult& result, bool) = 0;

  virtual bool readDocument(transaction::Methods* trx,
                            LocalDocumentId const& token,
                            ManagedDocumentResult& result) const = 0;

  virtual bool readDocumentWithCallback(transaction::Methods* trx,
                                        LocalDocumentId const& token,
                                        IndexIterator::DocumentCallback const& cb) const = 0;

  virtual Result insert(arangodb::transaction::Methods* trx,
                        arangodb::velocypack::Slice const newSlice,
                        arangodb::ManagedDocumentResult& result,
                        OperationOptions& options,
                        TRI_voc_tick_t& resultMarkerTick, bool lock,
                        TRI_voc_tick_t& revisionId) = 0;

  virtual Result update(arangodb::transaction::Methods* trx,
                        arangodb::velocypack::Slice const newSlice,
                        ManagedDocumentResult& result,
                        OperationOptions& options,
                        TRI_voc_tick_t& resultMarkerTick, bool lock,
                        TRI_voc_rid_t& prevRev, ManagedDocumentResult& previous,
                        arangodb::velocypack::Slice const key) = 0;

  virtual Result replace(transaction::Methods* trx,
                         arangodb::velocypack::Slice const newSlice,
                         ManagedDocumentResult& result,
                         OperationOptions& options,
                         TRI_voc_tick_t& resultMarkerTick, bool lock,
                         TRI_voc_rid_t& prevRev,
                         ManagedDocumentResult& previous) = 0;

  virtual Result remove(arangodb::transaction::Methods* trx,
                        arangodb::velocypack::Slice const slice,
                        arangodb::ManagedDocumentResult& previous,
                        OperationOptions& options,
                        TRI_voc_tick_t& resultMarkerTick, bool lock,
                        TRI_voc_rid_t& prevRev,
                        TRI_voc_rid_t& revisionId) = 0;

  /// @brief Defer a callback to be executed when the collection
  ///        can be dropped. The callback is supposed to drop
  ///        the collection and it is guaranteed that no one is using
  ///        it at that moment.
  virtual void deferDropCollection(
      std::function<bool(LogicalCollection*)> callback) = 0;

 protected:
  /// @brief Inject figures that are specific to StorageEngine
  virtual void figuresSpecific(
      std::shared_ptr<arangodb::velocypack::Builder>&) = 0;

  bool isValidEdgeAttribute(velocypack::Slice const& slice) const;

  // SECTION: Document pre commit preperation

  /// @brief new object for insert, value must have _key set correctly.
  Result newObjectForInsert(transaction::Methods* trx,
                            velocypack::Slice const& value,
                            LocalDocumentId const& documentId,
                            bool isEdgeCollection, velocypack::Builder& builder,
                            bool isRestore,
                            TRI_voc_rid_t& revisionId) const;

  /// @brief new object for remove, must have _key set
  void newObjectForRemove(transaction::Methods* trx,
                          velocypack::Slice const& oldValue,
                          LocalDocumentId const& documentId,
                          velocypack::Builder& builder,
                          bool isRestore, TRI_voc_rid_t& revisionId) const;

  /// @brief merge two objects for update
  Result mergeObjectsForUpdate(transaction::Methods* trx,
                               velocypack::Slice const& oldValue,
                               velocypack::Slice const& newValue,
                               bool isEdgeCollection, LocalDocumentId const& documentId,
                               bool mergeObjects, bool keepNull,
                               velocypack::Builder& builder,
                               bool isRestore, TRI_voc_rid_t& revisionId) const;

  /// @brief new object for replace
  Result newObjectForReplace(transaction::Methods* trx,
                             velocypack::Slice const& oldValue,
                             velocypack::Slice const& newValue,
                             bool isEdgeCollection, LocalDocumentId const& documentId,
                             velocypack::Builder& builder,
                             bool isRestore, TRI_voc_rid_t& revisionId) const;

  int checkRevision(transaction::Methods* trx, TRI_voc_rid_t expected,
                    TRI_voc_rid_t found) const;

 protected:
  LogicalCollection* _logicalCollection;
  bool const _isDBServer;

  mutable basics::ReadWriteLock _indexesLock;
  std::vector<std::shared_ptr<Index>> _indexes;
};

}  // namespace arangodb

#endif

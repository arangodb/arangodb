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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_VOC_BASE_DOCUMENT_COLLECTION_H
#define ARANGOD_VOC_BASE_DOCUMENT_COLLECTION_H 1

#include "Basics/Common.h"
#include "Basics/ReadWriteLock.h"
#include "Basics/StringRef.h"
#include "Cluster/ClusterInfo.h"
#include "VocBase/collection.h"
#include "VocBase/DatafileHelper.h"
#include "VocBase/DatafileStatistics.h"
#include "VocBase/Ditch.h"
#include "VocBase/MasterPointer.h"
#include "VocBase/MasterPointers.h"
#include "VocBase/voc-types.h"
#include "Wal/Marker.h"

struct TRI_vocbase_t;

namespace arangodb {
class Index;
struct OperationOptions;
class Transaction;
namespace velocypack {
class Builder;
class Slice;
}
namespace wal {
struct DocumentOperation;
}
}

////////////////////////////////////////////////////////////////////////////////
/// @brief document collection with global read-write lock
///
/// A document collection is a collection with a single read-write lock. This
/// lock is used to coordinate the read and write transactions.
////////////////////////////////////////////////////////////////////////////////

struct TRI_document_collection_t : public TRI_collection_t {
  explicit TRI_document_collection_t(TRI_vocbase_t* vocbase);

 public:
  // function that is called to garbage-collect the collection's indexes
  int (*cleanupIndexes)(struct TRI_document_collection_t*);

  int read(arangodb::Transaction*, std::string const&, TRI_doc_mptr_t*, bool);
  int read(arangodb::Transaction*, arangodb::StringRef const&, TRI_doc_mptr_t*, bool);
  int insert(arangodb::Transaction*, arangodb::velocypack::Slice const,
             TRI_doc_mptr_t*, arangodb::OperationOptions&, TRI_voc_tick_t&, bool);
  int update(arangodb::Transaction*, arangodb::velocypack::Slice const,
             TRI_doc_mptr_t*, arangodb::OperationOptions&, TRI_voc_tick_t&, bool,
             VPackSlice&, TRI_doc_mptr_t&);
  int replace(arangodb::Transaction*, arangodb::velocypack::Slice const,
             TRI_doc_mptr_t*, arangodb::OperationOptions&, TRI_voc_tick_t&, bool,
             VPackSlice&, TRI_doc_mptr_t&);
  int remove(arangodb::Transaction*, arangodb::velocypack::Slice const,
             arangodb::OperationOptions&, TRI_voc_tick_t&, bool, 
             VPackSlice&, TRI_doc_mptr_t&);

  int rollbackOperation(arangodb::Transaction*, TRI_voc_document_operation_e, 
                        TRI_doc_mptr_t*, TRI_doc_mptr_t const*);
  
 private:

  int lookupDocument(arangodb::Transaction*, arangodb::velocypack::Slice const,
                     TRI_doc_mptr_t*&);
  int checkRevision(arangodb::Transaction*, arangodb::velocypack::Slice const,
                    arangodb::velocypack::Slice const);
  int updateDocument(arangodb::Transaction*, TRI_voc_rid_t, TRI_doc_mptr_t*,
                     arangodb::wal::DocumentOperation&,
                     TRI_doc_mptr_t*, bool&);
  int insertDocument(arangodb::Transaction*, TRI_doc_mptr_t*,
                     arangodb::wal::DocumentOperation&, TRI_doc_mptr_t*,
                     bool&);
  int insertPrimaryIndex(arangodb::Transaction*, TRI_doc_mptr_t*);
  int insertSecondaryIndexes(arangodb::Transaction*, TRI_doc_mptr_t const*,
                             bool);
 public:
  int deletePrimaryIndex(arangodb::Transaction*, TRI_doc_mptr_t const*);
 private:
  int deleteSecondaryIndexes(arangodb::Transaction*, TRI_doc_mptr_t const*,
                             bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief new object for insert, value must have _key set correctly.
////////////////////////////////////////////////////////////////////////////////
    
  int newObjectForInsert(
      arangodb::Transaction* trx,
      arangodb::velocypack::Slice const& value,
      arangodb::velocypack::Slice const& fromSlice,
      arangodb::velocypack::Slice const& toSlice,
      bool isEdgeCollection,
      uint64_t& hash,
      arangodb::velocypack::Builder& builder,
      bool isRestore);

////////////////////////////////////////////////////////////////////////////////
/// @brief new object for Replace
////////////////////////////////////////////////////////////////////////////////
    
  void newObjectForReplace(
      arangodb::Transaction* trx,
      arangodb::velocypack::Slice const& oldValue,
      arangodb::velocypack::Slice const& newValue,
      arangodb::velocypack::Slice const& fromSlice,
      arangodb::velocypack::Slice const& toSlice,
      bool isEdgeCollection,
      std::string const& rev,
      arangodb::velocypack::Builder& builder);

////////////////////////////////////////////////////////////////////////////////
/// @brief merge two objects for update
////////////////////////////////////////////////////////////////////////////////
    
  void mergeObjectsForUpdate(
      arangodb::Transaction* trx,
      arangodb::velocypack::Slice const& oldValue,
      arangodb::velocypack::Slice const& newValue,
      bool isEdgeCollection,
      std::string const& rev,
      bool mergeObjects, bool keepNull,
      arangodb::velocypack::Builder& b);

////////////////////////////////////////////////////////////////////////////////
/// @brief new object for remove, must have _key set
////////////////////////////////////////////////////////////////////////////////
    
  void newObjectForRemove(
      arangodb::Transaction* trx,
      arangodb::velocypack::Slice const& oldValue,
      std::string const& rev,
      arangodb::velocypack::Builder& builder);
};

/// @brief creates a new collection
TRI_document_collection_t* TRI_CreateDocumentCollection(
    TRI_vocbase_t*, arangodb::VocbaseCollectionInfo&,
    TRI_voc_cid_t);

/// @brief create an index, based on a VelocyPack description
int TRI_FromVelocyPackIndexDocumentCollection(
    arangodb::Transaction*, TRI_document_collection_t*,
    arangodb::velocypack::Slice const&, arangodb::Index**);

/// @brief fill the additional (non-primary) indexes
int TRI_FillIndexesDocumentCollection(arangodb::Transaction*,
                                      TRI_vocbase_col_t*,
                                      TRI_document_collection_t*);

/// @brief opens an existing collection
TRI_document_collection_t* TRI_OpenDocumentCollection(TRI_vocbase_t*,
                                                      TRI_vocbase_col_t*, bool);

/// @brief closes an open collection
int TRI_CloseDocumentCollection(TRI_document_collection_t*, bool);

/// @brief saves an index
int TRI_SaveIndex(TRI_document_collection_t*, arangodb::Index*,
                  bool writeMarker);

/// @brief returns a description of all indexes
/// the caller must have read-locked the underyling collection!
std::vector<std::shared_ptr<arangodb::velocypack::Builder>>
TRI_IndexesDocumentCollection(TRI_document_collection_t*, bool);

/// @brief drops an index, including index file removal and replication
bool TRI_DropIndexDocumentCollection(TRI_document_collection_t*, TRI_idx_iid_t,
                                     bool);

/// @brief finds a geo index, list style
/// Note that the caller must hold at least a read-lock.
arangodb::Index* TRI_LookupGeoIndex1DocumentCollection(
    TRI_document_collection_t*, std::vector<std::string> const&, bool);

/// @brief finds a geo index, attribute style
/// Note that the caller must hold at least a read-lock.
arangodb::Index* TRI_LookupGeoIndex2DocumentCollection(
    TRI_document_collection_t*, std::vector<std::string> const&,
    std::vector<std::string> const&);

/// @brief ensures that a geo index exists, list style
arangodb::Index* TRI_EnsureGeoIndex1DocumentCollection(
    arangodb::Transaction*, TRI_document_collection_t*, TRI_idx_iid_t,
    std::string const&, bool, bool&);

/// @brief ensures that a geo index exists, attribute style
arangodb::Index* TRI_EnsureGeoIndex2DocumentCollection(
    arangodb::Transaction*, TRI_document_collection_t*, TRI_idx_iid_t,
    std::string const&, std::string const&, bool&);

/// @brief finds a hash index
/// @note The caller must hold at least a read-lock.
/// @note The @FA{paths} must be sorted.
arangodb::Index* TRI_LookupHashIndexDocumentCollection(
    TRI_document_collection_t*, std::vector<std::string> const&, int, bool);

/// @brief ensures that a hash index exists
arangodb::Index* TRI_EnsureHashIndexDocumentCollection(
    arangodb::Transaction* trx, TRI_document_collection_t*, TRI_idx_iid_t,
    std::vector<std::string> const&, bool, bool, bool&);

/// @brief finds a skiplist index
/// Note that the caller must hold at least a read-lock.
arangodb::Index* TRI_LookupSkiplistIndexDocumentCollection(
    TRI_document_collection_t*, std::vector<std::string> const&, int, bool);

/// @brief ensures that a skiplist index exists
arangodb::Index* TRI_EnsureSkiplistIndexDocumentCollection(
    arangodb::Transaction* trx, TRI_document_collection_t*, TRI_idx_iid_t,
    std::vector<std::string> const&, bool, bool, bool&);

/// @brief finds a RocksDB index
/// Note that the caller must hold at least a read-lock.
arangodb::Index* TRI_LookupRocksDBIndexDocumentCollection(
    TRI_document_collection_t*, std::vector<std::string> const&, int, bool);

/// @brief ensures that a RocksDB index exists
arangodb::Index* TRI_EnsureRocksDBIndexDocumentCollection(
    arangodb::Transaction* trx, TRI_document_collection_t*, TRI_idx_iid_t,
    std::vector<std::string> const&, bool, bool, bool&);

/// @brief finds a fulltext index
/// Note that the caller must hold at least a read-lock.
arangodb::Index* TRI_LookupFulltextIndexDocumentCollection(
    TRI_document_collection_t*, std::string const&, int);

/// @brief ensures that a fulltext index exists
arangodb::Index* TRI_EnsureFulltextIndexDocumentCollection(
    arangodb::Transaction* trx, TRI_document_collection_t*, TRI_idx_iid_t,
    std::string const&, int, bool&);

#endif

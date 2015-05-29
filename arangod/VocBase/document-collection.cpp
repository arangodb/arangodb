////////////////////////////////////////////////////////////////////////////////
/// @brief document collection with global read-write lock
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "document-collection.h"

#include "Basics/Barrier.h"
#include "Basics/conversions.h"
#include "Basics/Exceptions.h"
#include "Basics/files.h"
#include "Basics/logging.h"
#include "Basics/tri-strings.h"
#include "Basics/ThreadPool.h"
#include "FulltextIndex/fulltext-index.h"
#include "Indexes/CapConstraint.h"
#include "Indexes/EdgeIndex.h"
#include "Indexes/FulltextIndex.h"
#include "Indexes/GeoIndex2.h"
#include "Indexes/HashIndex.h"
#include "Indexes/PrimaryIndex.h"
#include "Indexes/SkiplistIndex2.h"
#include "RestServer/ArangoServer.h"
#include "ShapedJson/shape-accessor.h"
#include "Utils/transactions.h"
#include "Utils/CollectionReadLocker.h"
#include "Utils/CollectionWriteLocker.h"
#include "VocBase/Ditch.h"
#include "VocBase/edge-collection.h"
#include "VocBase/ExampleMatcher.h"
#include "VocBase/key-generator.h"
#include "VocBase/server.h"
#include "VocBase/update-policy.h"
#include "VocBase/voc-shaper.h"
#include "Wal/DocumentOperation.h"
#include "Wal/LogfileManager.h"
#include "Wal/Marker.h"
#include "Wal/Slots.h"

using namespace triagens::arango;

////////////////////////////////////////////////////////////////////////////////
/// @brief return a pointer to the beginning of the marker
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_MAINTAINER_MODE
void const* TRI_doc_mptr_t::getDataPtr () const {
  TransactionBase::assertCurrentTrxActive();
  return _dataptr;
}
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief set the pointer to the beginning of the memory for the marker
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_MAINTAINER_MODE
void TRI_doc_mptr_t::setDataPtr (void const* d) {
  TransactionBase::assertCurrentTrxActive();
  _dataptr = d;
}
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief return a pointer to the beginning of the marker, copy object
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_MAINTAINER_MODE
void const* TRI_doc_mptr_copy_t::getDataPtr () const {
  TransactionBase::assertSomeTrxInScope();
  return _dataptr;
}
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief set the pointer to the beginning of the memory for the marker,
/// copy object
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_MAINTAINER_MODE
void TRI_doc_mptr_copy_t::setDataPtr (void const* d) {
  TransactionBase::assertSomeTrxInScope();
  _dataptr = d;
}
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief create a document collection
////////////////////////////////////////////////////////////////////////////////

TRI_document_collection_t::TRI_document_collection_t () 
  : _useSecondaryIndexes(true),
    _capConstraint(nullptr),
    _ditches(this),
    _headersPtr(nullptr),
    _keyGenerator(nullptr),
    _uncollectedLogfileEntries(0),
    _cleanupIndexes(0) {

  _tickMax = 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy a document collection
////////////////////////////////////////////////////////////////////////////////

TRI_document_collection_t::~TRI_document_collection_t () {
  delete _keyGenerator;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add an index to the collection
/// note: this may throw. it's the caller's responsibility to catch and clean up
////////////////////////////////////////////////////////////////////////////////
  
void TRI_document_collection_t::addIndex (triagens::arango::Index* idx) {
  _indexes.emplace_back(idx);
    
  if (idx->type() == triagens::arango::Index::TRI_IDX_TYPE_CAP_CONSTRAINT) {
    // register cap constraint
    _capConstraint = static_cast<triagens::arango::CapConstraint*>(idx);
  }
  else if (idx->type() == triagens::arango::Index::TRI_IDX_TYPE_FULLTEXT_INDEX) {
    ++_cleanupIndexes;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get an index by id
////////////////////////////////////////////////////////////////////////////////
  
triagens::arango::Index* TRI_document_collection_t::removeIndex (TRI_idx_iid_t iid) {
  size_t const n = _indexes.size();

  for (size_t i = 0; i < n; ++i) {
    auto idx = _indexes[i];

    if (idx->type() == triagens::arango::Index::TRI_IDX_TYPE_PRIMARY_INDEX ||
        idx->type() == triagens::arango::Index::TRI_IDX_TYPE_EDGE_INDEX) {
      continue;
    }

    if (idx->id() == iid) {
      // found!
      _indexes.erase(_indexes.begin() + i);

      if (idx->type() == triagens::arango::Index::TRI_IDX_TYPE_CAP_CONSTRAINT) {
        // unregister cap constraint
        _capConstraint = nullptr;
      }
      else if (idx->type() == triagens::arango::Index::TRI_IDX_TYPE_FULLTEXT_INDEX) {
        --_cleanupIndexes;
      }

      return idx;
    }
  }

  // not found
  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get all indexes of the collection
////////////////////////////////////////////////////////////////////////////////
  
std::vector<triagens::arango::Index*> TRI_document_collection_t::allIndexes () const {
  return _indexes;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the primary index
////////////////////////////////////////////////////////////////////////////////
  
triagens::arango::PrimaryIndex* TRI_document_collection_t::primaryIndex () {
  TRI_ASSERT(! _indexes.empty());
  // the primary index must be the index at position #0
  return static_cast<triagens::arango::PrimaryIndex*>(_indexes[0]);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the collection's edge index, if it exists
////////////////////////////////////////////////////////////////////////////////
  
triagens::arango::EdgeIndex* TRI_document_collection_t::edgeIndex () {
  if (_indexes.size() >= 2 &&
      _indexes[1]->type() == triagens::arango::Index::TRI_IDX_TYPE_EDGE_INDEX) {
    // edge index must be the index at position #1
    return static_cast<triagens::arango::EdgeIndex*>(_indexes[1]);
  }

  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the cap constraint index, if it exists
////////////////////////////////////////////////////////////////////////////////
  
triagens::arango::CapConstraint* TRI_document_collection_t::capConstraint () {
  for (auto const& idx : _indexes) {
    if (idx->type() == triagens::arango::Index::TRI_IDX_TYPE_CAP_CONSTRAINT) {
      return static_cast<triagens::arango::CapConstraint*>(idx);
    }
  }

  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get an index by id
////////////////////////////////////////////////////////////////////////////////
  
triagens::arango::Index* TRI_document_collection_t::lookupIndex (TRI_idx_iid_t iid) const {
  for (auto const& it : _indexes) {
    if (it->id() == iid) {
      return it;
    }
  }
  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return a pointer to the shaper
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_MAINTAINER_MODE
TRI_shaper_t* TRI_document_collection_t::getShaper () const {
  if (! _ditches.contains(triagens::arango::Ditch::TRI_DITCH_DOCUMENT)) {
    TransactionBase::assertSomeTrxInScope();
  }
  return _shaper;
}
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief add a WAL operation for a transaction collection
////////////////////////////////////////////////////////////////////////////////

int TRI_AddOperationTransaction (triagens::wal::DocumentOperation&, bool&);

// -----------------------------------------------------------------------------
// --SECTION--                                              forward declarations
// -----------------------------------------------------------------------------

static int FillIndex (TRI_document_collection_t*,
                      triagens::arango::Index*);

static int CapConstraintFromJson (TRI_document_collection_t*,
                                  TRI_json_t const*,
                                  TRI_idx_iid_t,
                                  triagens::arango::Index**);

static int GeoIndexFromJson (TRI_document_collection_t*,
                             TRI_json_t const*,
                             TRI_idx_iid_t,
                             triagens::arango::Index**);

static int HashIndexFromJson (TRI_document_collection_t*,
                              TRI_json_t const*,
                              TRI_idx_iid_t,
                              triagens::arango::Index**);

static int SkiplistIndexFromJson (TRI_document_collection_t*,
                                  TRI_json_t const*,
                                  TRI_idx_iid_t,
                                  triagens::arango::Index**);

static int FulltextIndexFromJson (TRI_document_collection_t*,
                                  TRI_json_t const*,
                                  TRI_idx_iid_t,
                                  triagens::arango::Index**);

// -----------------------------------------------------------------------------
// --SECTION--                                                  HELPER FUNCTIONS
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief hashes a datafile identifier
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashKeyDatafile (TRI_associative_pointer_t* array, void const* key) {
  TRI_voc_fid_t const* k = static_cast<TRI_voc_fid_t const*>(key);

  return *k;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief hashes a datafile identifier
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashElementDatafile (TRI_associative_pointer_t* array, void const* element) {
  TRI_doc_datafile_info_t const* e = static_cast<TRI_doc_datafile_info_t const*>(element);

  return e->_fid;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compares a datafile identifier and a datafile info
////////////////////////////////////////////////////////////////////////////////

static bool IsEqualKeyElementDatafile (TRI_associative_pointer_t* array, void const* key, void const* element) {
  TRI_voc_fid_t const* k = static_cast<TRI_voc_fid_t const*>(key);
  TRI_doc_datafile_info_t const* e = static_cast<TRI_doc_datafile_info_t const*>(element);

  return *k == e->_fid;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief free an assoc array of datafile infos
////////////////////////////////////////////////////////////////////////////////

static void FreeDatafileInfo (TRI_doc_datafile_info_t* dfi) {
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, dfi);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief size of a primary collection
///
/// the caller must have read-locked the collection!
////////////////////////////////////////////////////////////////////////////////

static TRI_voc_size_t Count (TRI_document_collection_t* document) {
  return (TRI_voc_size_t) document->_numberDocuments;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief set the collection tick with the marker's tick value
////////////////////////////////////////////////////////////////////////////////

static inline void SetRevision (TRI_document_collection_t* document,
                                TRI_voc_rid_t rid,
                                bool force) {
  TRI_col_info_t* info = &document->_info;

  if (force || rid > info->_revision) {
    info->_revision = rid;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures that an error code is set in all required places
////////////////////////////////////////////////////////////////////////////////

static void EnsureErrorCode (int code) {
  if (code == TRI_ERROR_NO_ERROR) {
    // must have an error code
    code = TRI_ERROR_INTERNAL;
  }

  TRI_set_errno(code);
  errno = code;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new entry in the primary index
////////////////////////////////////////////////////////////////////////////////

static int InsertPrimaryIndex (TRI_document_collection_t* document,
                               TRI_doc_mptr_t const* header,
                               bool isRollback) {
  TRI_IF_FAILURE("InsertPrimaryIndex") {
    return TRI_ERROR_DEBUG;
  }

  TRI_doc_mptr_t* found;

  TRI_ASSERT(document != nullptr);
  TRI_ASSERT(header != nullptr);
  TRI_ASSERT(header->getDataPtr() != nullptr);  // ONLY IN INDEX, PROTECTED by RUNTIME

  // insert into primary index
  auto primaryIndex = document->primaryIndex();
  int res = primaryIndex->insertKey(header, (void const**) &found);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  if (found == nullptr) {
    // success
    return TRI_ERROR_NO_ERROR;
  }

  // we found a previous revision in the index
  // the found revision is still alive
  LOG_TRACE("document '%s' already existed with revision %llu while creating revision %llu",
            TRI_EXTRACT_MARKER_KEY(header),  // ONLY IN INDEX, PROTECTED by RUNTIME
            (unsigned long long) found->_rid,
            (unsigned long long) header->_rid);

  return TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new entry in the secondary indexes
////////////////////////////////////////////////////////////////////////////////

static int InsertSecondaryIndexes (TRI_document_collection_t* document,
                                   TRI_doc_mptr_t const* header,
                                   bool isRollback) {
  TRI_IF_FAILURE("InsertSecondaryIndexes") {
    return TRI_ERROR_DEBUG;
  }

  if (! document->useSecondaryIndexes()) {
    return TRI_ERROR_NO_ERROR;
  }

  int result = TRI_ERROR_NO_ERROR;

  auto const& indexes = document->allIndexes();
  size_t const n = indexes.size();

  for (size_t i = 1; i < n; ++i) {
    auto idx = indexes[i];
    int res = idx->insert(header, isRollback);

    // in case of no-memory, return immediately
    if (res == TRI_ERROR_OUT_OF_MEMORY) {
      return res;
    }
    else if (res != TRI_ERROR_NO_ERROR) {
      if (res == TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED ||
          result == TRI_ERROR_NO_ERROR) {
        // "prefer" unique constraint violated
        result = res;
      }
    }
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes an entry from the primary index
////////////////////////////////////////////////////////////////////////////////

static int DeletePrimaryIndex (TRI_document_collection_t* document,
                               TRI_doc_mptr_t const* header,
                               bool isRollback) {
  TRI_IF_FAILURE("DeletePrimaryIndex") {
    return TRI_ERROR_DEBUG;
  }

  auto primaryIndex = document->primaryIndex();
  auto found = static_cast<TRI_doc_mptr_t*>(primaryIndex->removeKey(TRI_EXTRACT_MARKER_KEY(header))); // ONLY IN INDEX, PROTECTED by RUNTIME

  if (found == nullptr) {
    return TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND;
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes an entry from the secondary indexes
////////////////////////////////////////////////////////////////////////////////

static int DeleteSecondaryIndexes (TRI_document_collection_t* document,
                                   TRI_doc_mptr_t const* header,
                                   bool isRollback) {
  if (! document->useSecondaryIndexes()) {
    return TRI_ERROR_NO_ERROR;
  }

  TRI_IF_FAILURE("DeleteSecondaryIndexes") {
    return TRI_ERROR_DEBUG;
  }

  int result = TRI_ERROR_NO_ERROR;

  auto const& indexes = document->allIndexes();
  size_t const n = indexes.size();

  for (size_t i = 1; i < n; ++i) {
    auto idx = indexes[i];
    int res = idx->remove(header, isRollback);

    if (res != TRI_ERROR_NO_ERROR) {
      // an error occurred
      result = res;
    }
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates and initially populates a document master pointer
////////////////////////////////////////////////////////////////////////////////

static int CreateHeader (TRI_document_collection_t* document,
                         TRI_doc_document_key_marker_t const* marker,
                         TRI_voc_fid_t fid,
                         TRI_doc_mptr_t** result) {
  size_t markerSize = (size_t) marker->base._size;
  TRI_ASSERT(markerSize > 0);

  // get a new header pointer
  TRI_doc_mptr_t* header = document->_headersPtr->request(markerSize);  // ONLY IN OPENITERATOR

  if (header == nullptr) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  auto primaryIndex = document->primaryIndex();

  header->_rid     = marker->_rid;
  header->_fid     = fid;
  header->setDataPtr(marker);  // ONLY IN OPENITERATOR
  header->_hash    = primaryIndex->calculateHash(TRI_EXTRACT_MARKER_KEY(header));  // ONLY IN OPENITERATOR, PROTECTED by RUNTIME
  *result = header;

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes an index file
////////////////////////////////////////////////////////////////////////////////

static bool RemoveIndexFile (TRI_document_collection_t* collection, 
                             TRI_idx_iid_t id) {
  // construct filename
  char* number = TRI_StringUInt64(id);

  if (number == nullptr) {
    TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
    LOG_ERROR("out of memory when creating index number");
    return false;
  }

  char* name = TRI_Concatenate3String("index-", number, ".json");

  if (name == nullptr) {
    TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);

    TRI_FreeString(TRI_CORE_MEM_ZONE, number);
    LOG_ERROR("out of memory when creating index name");
    return false;
  }

  char* filename = TRI_Concatenate2File(collection->_directory, name);

  if (filename == nullptr) {
    TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);

    TRI_FreeString(TRI_CORE_MEM_ZONE, number);
    TRI_FreeString(TRI_CORE_MEM_ZONE, name);
    LOG_ERROR("out of memory when creating index filename");
    return false;
  }

  TRI_FreeString(TRI_CORE_MEM_ZONE, name);
  TRI_FreeString(TRI_CORE_MEM_ZONE, number);

  int res = TRI_UnlinkFile(filename);
  TRI_FreeString(TRI_CORE_MEM_ZONE, filename);

  if (res != TRI_ERROR_NO_ERROR) {
    LOG_ERROR("cannot remove index definition: %s", TRI_last_error());
    return false;
  }

  return true;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                     DOCUMENT CRUD
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief updates an existing header
////////////////////////////////////////////////////////////////////////////////

static void UpdateHeader (TRI_voc_fid_t fid,
                          TRI_df_marker_t const* m,
                          TRI_doc_mptr_t* newHeader,
                          TRI_doc_mptr_t const* oldHeader) {
  TRI_doc_document_key_marker_t const* marker;

  marker = (TRI_doc_document_key_marker_t const*) m;

  TRI_ASSERT(marker != nullptr);
  TRI_ASSERT(m->_size > 0);

  newHeader->_rid     = marker->_rid;
  newHeader->_fid     = fid;
  newHeader->setDataPtr(marker);  // ONLY IN OPENITERATOR
}

// -----------------------------------------------------------------------------
// --SECTION--                                               DOCUMENT COLLECTION
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief garbage-collect a collection's indexes
////////////////////////////////////////////////////////////////////////////////

static int CleanupIndexes (TRI_document_collection_t* document) {
  int res = TRI_ERROR_NO_ERROR;

  // cleaning indexes is expensive, so only do it if the flag is set for the
  // collection
  if (document->_cleanupIndexes > 0) {
    TRI_WRITE_LOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(document);

    for (auto& idx : document->allIndexes()) {
      if (idx->type() == triagens::arango::Index::TRI_IDX_TYPE_FULLTEXT_INDEX) {
        res = idx->cleanup();

        if (res != TRI_ERROR_NO_ERROR) {
          break;
        }
      }
    }

    TRI_WRITE_UNLOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(document);
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief post-insert operation
////////////////////////////////////////////////////////////////////////////////

static int PostInsertIndexes (TRI_transaction_collection_t* trxCollection,
                              TRI_doc_mptr_t* header) {

  TRI_document_collection_t* document = trxCollection->_collection->_collection;
  if (! document->useSecondaryIndexes()) {
    return TRI_ERROR_NO_ERROR;
  }

  auto const& indexes = document->allIndexes();
  size_t const n = indexes.size();

  for (size_t i = 1; i < n; ++i) {
    auto idx = indexes[i];
    idx->postInsert(trxCollection, header);
  }

  // post-insert will never return an error
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates a new revision id if not yet set
////////////////////////////////////////////////////////////////////////////////

static inline TRI_voc_rid_t GetRevisionId (TRI_voc_rid_t previous) {
  if (previous != 0) {
    return previous;
  }

  // generate new revision id
  return static_cast<TRI_voc_rid_t>(TRI_NewTickServer());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief insert a document
////////////////////////////////////////////////////////////////////////////////

static int InsertDocument (TRI_transaction_collection_t* trxCollection,
                           TRI_doc_mptr_t* header,
                           triagens::wal::DocumentOperation& operation,
                           TRI_doc_mptr_copy_t* mptr,
                           bool& waitForSync) {

  TRI_ASSERT(header != nullptr);
  TRI_ASSERT(mptr != nullptr);
  TRI_document_collection_t* document = trxCollection->_collection->_collection;

  // .............................................................................
  // insert into indexes
  // .............................................................................

  // insert into primary index first
  int res = InsertPrimaryIndex(document, header, false);

  if (res != TRI_ERROR_NO_ERROR) {
    // insert has failed
    return res;
  }

  // insert into secondary indexes
  res = InsertSecondaryIndexes(document, header, false);

  if (res != TRI_ERROR_NO_ERROR) {
    DeleteSecondaryIndexes(document, header, true);
    DeletePrimaryIndex(document, header, true);
    return res;
  }

  document->_numberDocuments++;

  operation.indexed();

  TRI_IF_FAILURE("InsertDocumentNoOperation") {
    return TRI_ERROR_DEBUG;
  }

  TRI_IF_FAILURE("InsertDocumentNoOperationExcept") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  res = TRI_AddOperationTransaction(operation, waitForSync);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  *mptr = *header;

  res = PostInsertIndexes(trxCollection, header);

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up a document by key
/// the caller must make sure the read lock on the collection is held
////////////////////////////////////////////////////////////////////////////////

static int LookupDocument (TRI_document_collection_t* document,
                           TRI_voc_key_t key,
                           TRI_doc_update_policy_t const* policy,
                           TRI_doc_mptr_t*& header) {
  auto primaryIndex = document->primaryIndex();
  header = static_cast<TRI_doc_mptr_t*>(primaryIndex->lookupKey(key));

  if (header == nullptr) {
    return TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND;
  }

  if (policy != nullptr) {
    return policy->check(header->_rid);
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief updates an existing document
////////////////////////////////////////////////////////////////////////////////

static int UpdateDocument (TRI_transaction_collection_t* trxCollection,
                           TRI_doc_mptr_t* oldHeader,
                           triagens::wal::DocumentOperation& operation,
                           TRI_doc_mptr_copy_t* mptr,
                           bool syncRequested) {
  TRI_document_collection_t* document = trxCollection->_collection->_collection;

  // save the old data, remember
  TRI_doc_mptr_copy_t oldData = *oldHeader;

  // .............................................................................
  // update indexes
  // .............................................................................

  // remove old document from secondary indexes
  // (it will stay in the primary index as the key won't change)

  int res = DeleteSecondaryIndexes(document, oldHeader, false);

  if (res != TRI_ERROR_NO_ERROR) {
    // re-enter the document in case of failure, ignore errors during rollback
    InsertSecondaryIndexes(document, oldHeader, true);

    return res;
  }

  // .............................................................................
  // update header
  // .............................................................................

  TRI_doc_mptr_t* newHeader = oldHeader;

  // update the header. this will modify oldHeader, too !!!
  newHeader->_rid = operation.rid;
  newHeader->setDataPtr(operation.marker->mem());  // PROTECTED by trx in trxCollection

  // insert new document into secondary indexes
  res = InsertSecondaryIndexes(document, newHeader, false);

  if (res != TRI_ERROR_NO_ERROR) {
    // rollback
    DeleteSecondaryIndexes(document, newHeader, true);

    // copy back old header data
    oldHeader->copy(oldData);

    InsertSecondaryIndexes(document, oldHeader, true);

    return res;
  }

  operation.indexed();

  TRI_IF_FAILURE("UpdateDocumentNoOperation") {
    return TRI_ERROR_DEBUG;
  }

  TRI_IF_FAILURE("UpdateDocumentNoOperationExcept") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  res = TRI_AddOperationTransaction(operation, syncRequested);

  if (res == TRI_ERROR_NO_ERROR) {
    // write new header into result
    *mptr = *((TRI_doc_mptr_t*) newHeader);
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a document or edge marker, without using a legend
////////////////////////////////////////////////////////////////////////////////
      
static int CreateMarkerNoLegend (triagens::wal::Marker*& marker,
                                 TRI_document_collection_t* document,
                                 TRI_voc_rid_t rid,
                                 TRI_transaction_collection_t* trxCollection,
                                 std::string const& keyString,
                                 TRI_shaped_json_t const* shaped,
                                 TRI_document_edge_t const* edge) {

  TRI_ASSERT(marker == nullptr);
  
  TRI_IF_FAILURE("InsertDocumentNoLegend") {
    // test what happens when no legend can be created
    return TRI_ERROR_DEBUG;
  }
    
  TRI_IF_FAILURE("InsertDocumentNoLegendExcept") {
    // test what happens if no legend can be created
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  
  TRI_IF_FAILURE("InsertDocumentNoMarker") {
    // test what happens when no marker can be created
    return TRI_ERROR_DEBUG;
  }

  TRI_IF_FAILURE("InsertDocumentNoMarkerExcept") {
    // test what happens if no marker can be created
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }


  if (edge == nullptr) {
    // document
    marker = new triagens::wal::DocumentMarker(document->_vocbase->_id,
                                               document->_info._cid,
                                               rid,
                                               TRI_MarkerIdTransaction(trxCollection->_transaction),
                                               keyString,
                                               8,
                                               shaped);
  }
  else {
    // edge
    marker = new triagens::wal::EdgeMarker(document->_vocbase->_id,
                                           document->_info._cid,
                                           rid,
                                           TRI_MarkerIdTransaction(trxCollection->_transaction),
                                           keyString,
                                           edge,
                                           8,
                                           shaped);
  }
  
  TRI_ASSERT(marker != nullptr);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief clone a document or edge marker, without using a legend
////////////////////////////////////////////////////////////////////////////////
      
static int CloneMarkerNoLegend (triagens::wal::Marker*& marker,
                                TRI_df_marker_t const* original,
                                TRI_document_collection_t* document,
                                TRI_voc_rid_t rid,
                                TRI_transaction_collection_t* trxCollection,
                                TRI_shaped_json_t const* shaped) {

  TRI_ASSERT(marker == nullptr);
      
  TRI_IF_FAILURE("UpdateDocumentNoLegend") {
    // test what happens when no legend can be created
    return TRI_ERROR_DEBUG;
  }

  TRI_IF_FAILURE("UpdateDocumentNoLegendExcept") {
    // test what happens when no legend can be created
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  if (original->_type == TRI_WAL_MARKER_DOCUMENT ||
      original->_type == TRI_DOC_MARKER_KEY_DOCUMENT) {
    marker = triagens::wal::DocumentMarker::clone(original,
                                                  document->_vocbase->_id,
                                                  document->_info._cid,
                                                  rid,
                                                  TRI_MarkerIdTransaction(trxCollection->_transaction),
                                                  8,
                                                  shaped);

    return TRI_ERROR_NO_ERROR;
  }
  else if (original->_type == TRI_WAL_MARKER_EDGE ||
           original->_type == TRI_DOC_MARKER_KEY_EDGE) {
    marker = triagens::wal::EdgeMarker::clone(original,
                                              document->_vocbase->_id,
                                              document->_info._cid,
                                              rid,
                                              TRI_MarkerIdTransaction(trxCollection->_transaction),
                                              8,
                                              shaped);
    return TRI_ERROR_NO_ERROR;
  }
        
  // invalid marker type
  return TRI_ERROR_INTERNAL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief read locks a collection
////////////////////////////////////////////////////////////////////////////////

static int BeginRead (TRI_document_collection_t* document) {
  if (triagens::arango::Transaction::_makeNolockHeaders != nullptr) {
    std::string collName(document->_info._name);
    auto it = triagens::arango::Transaction::_makeNolockHeaders->find(collName);
    if (it != triagens::arango::Transaction::_makeNolockHeaders->end()) {
      // do not lock by command
      // LOCKING-DEBUG
      // std::cout << "BeginRead blocked: " << document->_info._name << std::endl;
      return TRI_ERROR_NO_ERROR;
    }
  }
  // LOCKING-DEBUG
  // std::cout << "BeginRead: " << document->_info._name << std::endl;
  TRI_READ_LOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(document);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief read unlocks a collection
////////////////////////////////////////////////////////////////////////////////

static int EndRead (TRI_document_collection_t* document) {
  if (triagens::arango::Transaction::_makeNolockHeaders != nullptr) {
    std::string collName(document->_info._name);
    auto it = triagens::arango::Transaction::_makeNolockHeaders->find(collName);
    if (it != triagens::arango::Transaction::_makeNolockHeaders->end()) {
      // do not lock by command
      // LOCKING-DEBUG
      // std::cout << "EndRead blocked: " << document->_info._name << std::endl;
      return TRI_ERROR_NO_ERROR;
    }
  }
  // LOCKING-DEBUG
  // std::cout << "EndRead: " << document->_info._name << std::endl;
  TRI_READ_UNLOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(document);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief write locks a collection
////////////////////////////////////////////////////////////////////////////////

static int BeginWrite (TRI_document_collection_t* document) {
  if (triagens::arango::Transaction::_makeNolockHeaders != nullptr) {
    std::string collName(document->_info._name);
    auto it = triagens::arango::Transaction::_makeNolockHeaders->find(collName);
    if (it != triagens::arango::Transaction::_makeNolockHeaders->end()) {
      // do not lock by command
      // LOCKING-DEBUG
      // std::cout << "BeginWrite blocked: " << document->_info._name << std::endl;
      return TRI_ERROR_NO_ERROR;
    }
  }
  // LOCKING_DEBUG
  // std::cout << "BeginWrite: " << document->_info._name << std::endl;
  TRI_WRITE_LOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(document);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief write unlocks a collection
////////////////////////////////////////////////////////////////////////////////

static int EndWrite (TRI_document_collection_t* document) {
  if (triagens::arango::Transaction::_makeNolockHeaders != nullptr) {
    std::string collName(document->_info._name);
    auto it = triagens::arango::Transaction::_makeNolockHeaders->find(collName);
    if (it != triagens::arango::Transaction::_makeNolockHeaders->end()) {
      // do not lock by command
      // LOCKING-DEBUG
      // std::cout << "EndWrite blocked: " << document->_info._name << std::endl;
      return TRI_ERROR_NO_ERROR;
    }
  }
  // LOCKING-DEBUG
  // std::cout << "EndWrite: " << document->_info._name << std::endl;
  TRI_WRITE_UNLOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(document);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief read locks a collection, with a timeout (in µseconds)
////////////////////////////////////////////////////////////////////////////////

static int BeginReadTimed (TRI_document_collection_t* document,
                           uint64_t timeout,
                           uint64_t sleepPeriod) {
  if (triagens::arango::Transaction::_makeNolockHeaders != nullptr) {
    std::string collName(document->_info._name);
    auto it = triagens::arango::Transaction::_makeNolockHeaders->find(collName);
    if (it != triagens::arango::Transaction::_makeNolockHeaders->end()) {
      // do not lock by command
      // LOCKING-DEBUG
      // std::cout << "BeginReadTimed blocked: " << document->_info._name << std::endl;
      return TRI_ERROR_NO_ERROR;
    }
  }
  uint64_t waited = 0;

  // LOCKING-DEBUG
  // std::cout << "BeginReadTimed: " << document->_info._name << std::endl;
  while (! TRI_TRY_READ_LOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(document)) {
#ifdef _WIN32
    usleep((unsigned long) sleepPeriod);
#else
    usleep((useconds_t) sleepPeriod);
#endif

    waited += sleepPeriod;

    if (waited > timeout) {
      return TRI_ERROR_LOCK_TIMEOUT;
    }
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief write locks a collection, with a timeout
////////////////////////////////////////////////////////////////////////////////

static int BeginWriteTimed (TRI_document_collection_t* document,
                            uint64_t timeout,
                            uint64_t sleepPeriod) {
  if (triagens::arango::Transaction::_makeNolockHeaders != nullptr) {
    std::string collName(document->_info._name);
    auto it = triagens::arango::Transaction::_makeNolockHeaders->find(collName);
    if (it != triagens::arango::Transaction::_makeNolockHeaders->end()) {
      // do not lock by command
      // LOCKING-DEBUG
      // std::cout << "BeginWriteTimed blocked: " << document->_info._name << std::endl;
      return TRI_ERROR_NO_ERROR;
    }
  }
  uint64_t waited = 0;

  // LOCKING-DEBUG
  // std::cout << "BeginWriteTimed: " << document->_info._name << std::endl;
  while (! TRI_TRY_WRITE_LOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(document)) {
#ifdef _WIN32
    usleep((unsigned long) sleepPeriod);
#else
    usleep((useconds_t) sleepPeriod);
#endif

    waited += sleepPeriod;

    if (waited > timeout) {
      return TRI_ERROR_LOCK_TIMEOUT;
    }
  }

  return TRI_ERROR_NO_ERROR;
}

// -----------------------------------------------------------------------------
// --SECTION--                                               DOCUMENT COLLECTION
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                     Open iterator
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief size of operations buffer for the open iterator
////////////////////////////////////////////////////////////////////////////////

static size_t OpenIteratorBufferSize = 128;

// -----------------------------------------------------------------------------
// --SECTION--                                                     private types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief state during opening of a collection
////////////////////////////////////////////////////////////////////////////////

typedef struct open_iterator_state_s {
  TRI_document_collection_t* _document;
  TRI_voc_tid_t              _tid;
  TRI_voc_fid_t              _fid;
  TRI_doc_datafile_info_t*   _dfi;
  TRI_vector_t               _operations;
  TRI_vocbase_t*             _vocbase;
  uint64_t                   _deletions;
  uint64_t                   _documents;
  int64_t                    _initialCount;
  uint32_t                   _trxCollections;
  uint32_t                   _numOps;
  bool                       _trxPrepared;
}
open_iterator_state_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief container for a single collection operation (used during opening)
////////////////////////////////////////////////////////////////////////////////

typedef struct open_iterator_operation_s {
  TRI_voc_document_operation_e  _type;
  TRI_df_marker_t const*        _marker;
  TRI_voc_fid_t                 _fid;
}
open_iterator_operation_t;

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief mark a transaction as failed during opening of a collection
////////////////////////////////////////////////////////////////////////////////

static int OpenIteratorNoteFailedTransaction (open_iterator_state_t const* state) {
  TRI_ASSERT(state->_tid > 0);

  if (state->_document->_failedTransactions == nullptr) {
    state->_document->_failedTransactions = new std::set<TRI_voc_tid_t>;
  }

  state->_document->_failedTransactions->insert(state->_tid);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief apply an insert/update operation when opening a collection
////////////////////////////////////////////////////////////////////////////////

static int OpenIteratorApplyInsert (open_iterator_state_t* state,
                                    open_iterator_operation_t const* operation) {
  TRI_document_collection_t* document = state->_document;

  TRI_df_marker_t const* marker = operation->_marker;
  TRI_doc_document_key_marker_t const* d = reinterpret_cast<TRI_doc_document_key_marker_t const*>(marker);

  if (state->_fid != operation->_fid) {
    // update the state
    state->_fid = operation->_fid;
    state->_dfi = TRI_FindDatafileInfoDocumentCollection(document, operation->_fid, true);
  }

  SetRevision(document, d->_rid, false);

#ifdef TRI_ENABLE_LOGGER
#ifdef TRI_ENABLE_MAINTAINER_MODE
  
#if 0
  // currently disabled because it is too chatty in trace mode
  if (marker->_type == TRI_DOC_MARKER_KEY_DOCUMENT) {
    LOG_TRACE("document: fid %llu, key %s, rid %llu, _offsetJson %lu, _offsetKey %lu",
              (unsigned long long) operation->_fid,
              ((char*) d + d->_offsetKey),
              (unsigned long long) d->_rid,
              (unsigned long) d->_offsetJson,
              (unsigned long) d->_offsetKey);
  }
  else {
    TRI_doc_edge_key_marker_t const* e = reinterpret_cast<TRI_doc_edge_key_marker_t const*>(marker);
    LOG_TRACE("edge: fid %llu, key %s, fromKey %s, toKey %s, rid %llu, _offsetJson %lu, _offsetKey %lu",
              (unsigned long long) operation->_fid,
              ((char*) d + d->_offsetKey),
              ((char*) e + e->_offsetFromKey),
              ((char*) e + e->_offsetToKey),
              (unsigned long long) d->_rid,
              (unsigned long) d->_offsetJson,
              (unsigned long) d->_offsetKey);

  }
#endif

#endif
#endif

  TRI_voc_key_t key = ((char*) d) + d->_offsetKey;
  document->_keyGenerator->track(key);

  ++state->_documents;

  auto primaryIndex = document->primaryIndex();

  // no primary index lock required here because we are the only ones reading from the index ATM
  auto found = static_cast<TRI_doc_mptr_t const*>(primaryIndex->lookupKey(key));

  // it is a new entry
  if (found == nullptr) {
    TRI_doc_mptr_t* header;

    // get a header
    int res = CreateHeader(document, (TRI_doc_document_key_marker_t*) marker, operation->_fid, &header);

    if (res != TRI_ERROR_NO_ERROR) {
      LOG_ERROR("out of memory");

      return TRI_set_errno(res);
    }

    TRI_ASSERT(header != nullptr);

    // insert into primary index
    if (state->_initialCount != -1) {
      // we know how many documents there will be, at least approximately...
      // the index was likely already allocated to be big enough for this number of documents

      if (state->_documents % 100 == 0) {
        // to be on the safe size, we still need to check if the index will burst from time to time
        res = primaryIndex->resize();
      
        if (res != TRI_ERROR_NO_ERROR) {
          // insertion failed
          LOG_ERROR("inserting document into indexes failed");
          document->_headersPtr->release(header, true);  // ONLY IN OPENITERATOR

          return res;
        }
      }
      
      // we can now use an optimized insert method
      primaryIndex->insertKey(header);
    }
    else {
      // use regular insert method
      res = InsertPrimaryIndex(document, header, false);

      if (res != TRI_ERROR_NO_ERROR) {
        // insertion failed
        LOG_ERROR("inserting document into indexes failed");
        document->_headersPtr->release(header, true);  // ONLY IN OPENITERATOR

        return res;
      }
    }

    document->_numberDocuments++;

    // update the datafile info
    if (state->_dfi != nullptr) {
      state->_dfi->_numberAlive++;
      state->_dfi->_sizeAlive += (int64_t) TRI_DF_ALIGN_BLOCK(marker->_size);
    }
  }

  // it is an update, but only if found has a smaller revision identifier
  else if (found->_rid < d->_rid ||
           (found->_rid == d->_rid && found->_fid <= operation->_fid)) {
    // save the old data
    TRI_doc_mptr_copy_t oldData = *found;

    TRI_doc_mptr_t* newHeader = static_cast<TRI_doc_mptr_t*>(CONST_CAST(found));

    // update the header info
    UpdateHeader(operation->_fid, marker, newHeader, found);
    document->_headersPtr->moveBack(newHeader, &oldData);  // ONLY IN OPENITERATOR

    // update the datafile info
    TRI_doc_datafile_info_t* dfi;
    if (oldData._fid == state->_fid) {
      dfi = state->_dfi;
    }
    else {
      dfi = TRI_FindDatafileInfoDocumentCollection(document, oldData._fid, true);
    }

    if (dfi != nullptr && found->getDataPtr() != nullptr) {  // ONLY IN OPENITERATOR, PROTECTED by RUNTIME
      int64_t size;

      TRI_ASSERT(found->getDataPtr() != nullptr);  // ONLY IN OPENITERATOR, PROTECTED by RUNTIME
      size = (int64_t) ((TRI_df_marker_t*) found->getDataPtr())->_size;  // ONLY IN OPENITERATOR, PROTECTED by RUNTIME

      dfi->_numberAlive--;
      dfi->_sizeAlive -= TRI_DF_ALIGN_BLOCK(size);

      dfi->_numberDead++;
      dfi->_sizeDead += TRI_DF_ALIGN_BLOCK(size);
    }

    if (state->_dfi != nullptr) {
      state->_dfi->_numberAlive++;
      state->_dfi->_sizeAlive += (int64_t) TRI_DF_ALIGN_BLOCK(marker->_size);
    }
  }

  // it is a stale update
  else {
    if (state->_dfi != nullptr) {
      TRI_ASSERT(found->getDataPtr() != nullptr);  // ONLY IN OPENITERATOR, PROTECTED by RUNTIME

      state->_dfi->_numberDead++;
      state->_dfi->_sizeDead += (int64_t) TRI_DF_ALIGN_BLOCK(((TRI_df_marker_t*) found->getDataPtr())->_size);  // ONLY IN OPENITERATOR, PROTECTED by RUNTIME
    }
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief apply a delete operation when opening a collection
////////////////////////////////////////////////////////////////////////////////

static int OpenIteratorApplyRemove (open_iterator_state_t* state,
                                    open_iterator_operation_t const* operation) {

  TRI_df_marker_t const* marker;
  TRI_doc_deletion_key_marker_t const* d;
  TRI_doc_mptr_t* found;
  TRI_voc_key_t key;

  TRI_document_collection_t* document = state->_document;

  marker = operation->_marker;
  d = (TRI_doc_deletion_key_marker_t const*) marker;

  SetRevision(document, d->_rid, false);

  ++state->_deletions;

  if (state->_fid != operation->_fid) {
    // update the state
    state->_fid = operation->_fid;
    state->_dfi = TRI_FindDatafileInfoDocumentCollection(document, operation->_fid, true);
  }

  key = ((char*) d) + d->_offsetKey;

#ifdef TRI_ENABLE_MAINTAINER_MODE
  LOG_TRACE("deletion: fid %llu, key %s, rid %llu, deletion %llu",
            (unsigned long long) operation->_fid,
            (char*) key,
            (unsigned long long) d->_rid,
            (unsigned long long) marker->_tick);
#endif

  document->_keyGenerator->track(key);

  // no primary index lock required here because we are the only ones reading from the index ATM
  auto primaryIndex = document->primaryIndex();
  found = static_cast<TRI_doc_mptr_t*>(primaryIndex->lookupKey(key));

  // it is a new entry, so we missed the create
  if (found == nullptr) {
    // update the datafile info
    if (state->_dfi != nullptr) {
      state->_dfi->_numberDeletion++;
    }
  }

  // it is a real delete
  else {
    TRI_doc_datafile_info_t* dfi;

    // update the datafile info
    if (found->_fid == state->_fid) {
      dfi = state->_dfi;
    }
    else {
      dfi = TRI_FindDatafileInfoDocumentCollection(document, found->_fid, true);
    }

    if (dfi != nullptr) {
      int64_t size;

      TRI_ASSERT(found->getDataPtr() != nullptr);  // ONLY IN OPENITERATOR, PROTECTED by RUNTIME

      size = (int64_t) ((TRI_df_marker_t*) found->getDataPtr())->_size;  // ONLY IN OPENITERATOR, PROTECTED by RUNTIME

      dfi->_numberAlive--;
      dfi->_sizeAlive -= TRI_DF_ALIGN_BLOCK(size);

      dfi->_numberDead++;
      dfi->_sizeDead += TRI_DF_ALIGN_BLOCK(size);
    }

    if (state->_dfi != nullptr) {
      state->_dfi->_numberDeletion++;
    }

    DeletePrimaryIndex(document, found, false);
    --document->_numberDocuments;

    // free the header
    document->_headersPtr->release(found, true);   // ONLY IN OPENITERATOR
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief apply an operation when opening a collection
////////////////////////////////////////////////////////////////////////////////

static int OpenIteratorApplyOperation (open_iterator_state_t* state,
                                       open_iterator_operation_t const* operation) {
  if (operation->_type == TRI_VOC_DOCUMENT_OPERATION_REMOVE) {
    return OpenIteratorApplyRemove(state, operation);
  }
  else if (operation->_type == TRI_VOC_DOCUMENT_OPERATION_INSERT) {
    return OpenIteratorApplyInsert(state, operation);
  }

  LOG_ERROR("logic error in %s", __FUNCTION__);
  return TRI_ERROR_INTERNAL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add an operation to the list of operations when opening a collection
/// if the operation does not belong to a designated transaction, it is
/// executed directly
////////////////////////////////////////////////////////////////////////////////

static int OpenIteratorAddOperation (open_iterator_state_t* state,
                                     TRI_voc_document_operation_e type,
                                     TRI_df_marker_t const* marker,
                                     TRI_voc_fid_t fid) {
  open_iterator_operation_t operation;
  operation._type   = type;
  operation._marker = marker;
  operation._fid    = fid;

  int res;
  if (state->_tid == 0) {
    res = OpenIteratorApplyOperation(state, &operation);
  }
  else {
    res = TRI_PushBackVector(&state->_operations, &operation);
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reset the list of operations during opening
////////////////////////////////////////////////////////////////////////////////

static void OpenIteratorResetOperations (open_iterator_state_t* state) {
  size_t n = TRI_LengthVector(&state->_operations);

  if (n > OpenIteratorBufferSize * 2) {
    // free some memory
    TRI_DestroyVector(&state->_operations);
    TRI_InitVector2(&state->_operations, TRI_UNKNOWN_MEM_ZONE, sizeof(open_iterator_operation_t), OpenIteratorBufferSize);
  }
  else {
    TRI_ClearVector(&state->_operations);
  }

  state->_tid            = 0;
  state->_trxPrepared    = false;
  state->_trxCollections = 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief start a transaction when opening a collection
////////////////////////////////////////////////////////////////////////////////

static int OpenIteratorStartTransaction (open_iterator_state_t* state,
                                         TRI_voc_tid_t tid,
                                         uint32_t numCollections) {
  state->_tid = tid;
  state->_trxCollections = numCollections;

  TRI_ASSERT(TRI_LengthVector(&state->_operations) == 0);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief prepare an ongoing transaction when opening a collection
////////////////////////////////////////////////////////////////////////////////

static int OpenIteratorPrepareTransaction (open_iterator_state_t* state) {
  if (state->_tid != 0) {
    state->_trxPrepared = true;
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief abort an ongoing transaction when opening a collection
////////////////////////////////////////////////////////////////////////////////

static int OpenIteratorAbortTransaction (open_iterator_state_t* state) {
  if (state->_tid != 0) {
    if (state->_trxCollections > 1 && state->_trxPrepared) {
      // multi-collection transaction...
      // check if we have a coordinator entry in _trx
      // if yes, then we'll recover the transaction, otherwise we'll abort it

      if (state->_vocbase->_oldTransactions != nullptr &&
          state->_vocbase->_oldTransactions->find(state->_tid) != state->_vocbase->_oldTransactions->end()) {
        // we have found a coordinator entry
        // otherwise we would have got TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND etc.
        int res = TRI_ERROR_NO_ERROR;

        LOG_INFO("recovering transaction %llu", (unsigned long long) state->_tid);
        size_t const n = TRI_LengthVector(&state->_operations);

        for (size_t i = 0; i < n; ++i) {
          open_iterator_operation_t* operation = static_cast<open_iterator_operation_t*>(TRI_AtVector(&state->_operations, i));

          int r = OpenIteratorApplyOperation(state, operation);

          if (r != TRI_ERROR_NO_ERROR) {
            res = r;
          }
        }

        OpenIteratorResetOperations(state);
        return res;
      }

      // fall-through
    }

    OpenIteratorNoteFailedTransaction(state);

    LOG_INFO("rolling back uncommitted transaction %llu", (unsigned long long) state->_tid);
    OpenIteratorResetOperations(state);
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief commit a transaction when opening a collection
////////////////////////////////////////////////////////////////////////////////

static int OpenIteratorCommitTransaction (open_iterator_state_t* state) {
  int res = TRI_ERROR_NO_ERROR;

  if (state->_trxCollections <= 1 || state->_trxPrepared) {
    size_t const n = TRI_LengthVector(&state->_operations);

    for (size_t i = 0; i < n; ++i) {
      open_iterator_operation_t* operation = static_cast<open_iterator_operation_t*>(TRI_AtVector(&state->_operations, i));

      int r = OpenIteratorApplyOperation(state, operation);
      if (r != TRI_ERROR_NO_ERROR) {
        res = r;
      }
    }
  }
  else if (state->_trxCollections > 1 && ! state->_trxPrepared) {
    OpenIteratorAbortTransaction(state);
  }

  // clean up
  OpenIteratorResetOperations(state);

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief process a document (or edge) marker when opening a collection
////////////////////////////////////////////////////////////////////////////////

static int OpenIteratorHandleDocumentMarker (TRI_df_marker_t const* marker,
                                             TRI_datafile_t* datafile,
                                             open_iterator_state_t* state) {

  TRI_doc_document_key_marker_t const* d = (TRI_doc_document_key_marker_t const*) marker;

  if (d->_tid > 0) {
    // marker has a transaction id
    if (d->_tid != state->_tid) {
      // we have a different transaction ongoing
      LOG_WARNING("logic error in %s, fid %llu. found tid: %llu, expected tid: %llu. "
                  "this may also be the result of an aborted transaction",
                  __FUNCTION__,
                  (unsigned long long) datafile->_fid,
                  (unsigned long long) d->_tid,
                  (unsigned long long) state->_tid);

      OpenIteratorAbortTransaction(state);

      return TRI_ERROR_INTERNAL;
    }
  }

  OpenIteratorAddOperation(state, TRI_VOC_DOCUMENT_OPERATION_INSERT, marker, datafile->_fid);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief process a deletion marker when opening a collection
////////////////////////////////////////////////////////////////////////////////

static int OpenIteratorHandleDeletionMarker (TRI_df_marker_t const* marker,
                                             TRI_datafile_t* datafile,
                                             open_iterator_state_t* state) {

  TRI_doc_deletion_key_marker_t const* d = (TRI_doc_deletion_key_marker_t const*) marker;

  if (d->_tid > 0) {
    // marker has a transaction id
    if (d->_tid != state->_tid) {
      // we have a different transaction ongoing
      LOG_WARNING("logic error in %s, fid %llu. found tid: %llu, expected tid: %llu. "
                  "this may also be the result of an aborted transaction",
                  __FUNCTION__,
                  (unsigned long long) datafile->_fid,
                  (unsigned long long) d->_tid,
                  (unsigned long long) state->_tid);

      OpenIteratorAbortTransaction(state);

      return TRI_ERROR_INTERNAL;
    }
  }

  OpenIteratorAddOperation(state, TRI_VOC_DOCUMENT_OPERATION_REMOVE, marker, datafile->_fid);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief process a shape marker when opening a collection
////////////////////////////////////////////////////////////////////////////////

static int OpenIteratorHandleShapeMarker (TRI_df_marker_t const* marker,
                                          TRI_datafile_t* datafile,
                                          open_iterator_state_t* state) {
  TRI_document_collection_t* document = state->_document;

  int res = TRI_InsertShapeVocShaper(document->getShaper(), marker, true);  // ONLY IN OPENITERATOR, PROTECTED by fake trx from above

  if (res == TRI_ERROR_NO_ERROR) {
    if (state->_fid != datafile->_fid) {
      state->_fid = datafile->_fid;
      state->_dfi = TRI_FindDatafileInfoDocumentCollection(document, state->_fid, true);
    }

    if (state->_dfi != nullptr) {
      state->_dfi->_numberShapes++;
      state->_dfi->_sizeShapes += (int64_t) TRI_DF_ALIGN_BLOCK(marker->_size);
    }
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief process an attribute marker when opening a collection
////////////////////////////////////////////////////////////////////////////////

static int OpenIteratorHandleAttributeMarker (TRI_df_marker_t const* marker,
                                              TRI_datafile_t* datafile,
                                              open_iterator_state_t* state) {
  TRI_document_collection_t* document = state->_document;

  int res = TRI_InsertAttributeVocShaper(document->getShaper(), marker, true);   // ONLY IN OPENITERATOR, PROTECTED by fake trx from above

  if (res == TRI_ERROR_NO_ERROR) {
    if (state->_fid != datafile->_fid) {
      state->_fid = datafile->_fid;
      state->_dfi = TRI_FindDatafileInfoDocumentCollection(document, state->_fid, true);
    }

    if (state->_dfi != nullptr) {
      state->_dfi->_numberAttributes++;
      state->_dfi->_sizeAttributes += (int64_t) TRI_DF_ALIGN_BLOCK(marker->_size);
    }
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief process a "begin transaction" marker when opening a collection
////////////////////////////////////////////////////////////////////////////////

static int OpenIteratorHandleBeginMarker (TRI_df_marker_t const* marker,
                                          TRI_datafile_t* datafile,
                                          open_iterator_state_t* state) {

  TRI_doc_begin_transaction_marker_t const* m = (TRI_doc_begin_transaction_marker_t const*) marker;

  if (m->_tid != state->_tid && state->_tid != 0) {
    // some incomplete transaction was going on before us...
    LOG_WARNING("logic error in %s, fid %llu. found tid: %llu, expected tid: %llu. "
                "this may also be the result of an aborted transaction",
                __FUNCTION__,
                (unsigned long long) datafile->_fid,
                (unsigned long long) m->_tid,
                (unsigned long long) state->_tid);

    OpenIteratorAbortTransaction(state);
  }

  OpenIteratorStartTransaction(state, m->_tid, (uint32_t) m->_numCollections);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief process a "commit transaction" marker when opening a collection
////////////////////////////////////////////////////////////////////////////////

static int OpenIteratorHandleCommitMarker (TRI_df_marker_t const* marker,
                                           TRI_datafile_t* datafile,
                                           open_iterator_state_t* state) {

  TRI_doc_commit_transaction_marker_t const* m = (TRI_doc_commit_transaction_marker_t const*) marker;

  if (m->_tid != state->_tid) {
    // we found a commit marker, but we did not find any begin marker beforehand. strange
    LOG_WARNING("logic error in %s, fid %llu. found tid: %llu, expected tid: %llu",
                __FUNCTION__,
                (unsigned long long) datafile->_fid,
                (unsigned long long) m->_tid,
                (unsigned long long) state->_tid);

    OpenIteratorAbortTransaction(state);
  }
  else {
    OpenIteratorCommitTransaction(state);
  }

  // reset transaction id
  state->_tid = 0;

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief process a "prepare transaction" marker when opening a collection
////////////////////////////////////////////////////////////////////////////////

static int OpenIteratorHandlePrepareMarker (TRI_df_marker_t const* marker,
                                            TRI_datafile_t* datafile,
                                            open_iterator_state_t* state) {

  TRI_doc_prepare_transaction_marker_t const* m = (TRI_doc_prepare_transaction_marker_t const*) marker;

  if (m->_tid != state->_tid) {
    // we found a commit marker, but we did not find any begin marker beforehand. strange
    LOG_WARNING("logic error in %s, fid %llu. found tid: %llu, expected tid: %llu",
                __FUNCTION__,
                (unsigned long long) datafile->_fid,
                (unsigned long long) m->_tid,
                (unsigned long long) state->_tid);

    OpenIteratorAbortTransaction(state);
  }
  else {
    OpenIteratorPrepareTransaction(state);
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief process an "abort transaction" marker when opening a collection
////////////////////////////////////////////////////////////////////////////////

static int OpenIteratorHandleAbortMarker (TRI_df_marker_t const* marker,
                                          TRI_datafile_t* datafile,
                                          open_iterator_state_t* state) {

  TRI_doc_abort_transaction_marker_t const* m = (TRI_doc_abort_transaction_marker_t const*) marker;

  if (m->_tid != state->_tid) {
    // we found an abort marker, but we did not find any begin marker beforehand. strange
    LOG_WARNING("logic error in %s, fid %llu. found tid: %llu, expected tid: %llu",
                __FUNCTION__,
                (unsigned long long) datafile->_fid,
                (unsigned long long) m->_tid,
                (unsigned long long) state->_tid);
  }

  OpenIteratorAbortTransaction(state);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief iterator for open
////////////////////////////////////////////////////////////////////////////////

static bool OpenIterator (TRI_df_marker_t const* marker,
                          void* data,
                          TRI_datafile_t* datafile) {
  TRI_document_collection_t* document = static_cast<open_iterator_state_t*>(data)->_document;
  TRI_voc_tick_t tick = marker->_tick;

  int res;

  if (marker->_type == TRI_DOC_MARKER_KEY_EDGE ||
      marker->_type == TRI_DOC_MARKER_KEY_DOCUMENT) {
    res = OpenIteratorHandleDocumentMarker(marker, datafile, (open_iterator_state_t*) data);
    
    if (datafile->_dataMin == 0) {
      datafile->_dataMin = tick;
    }

    if (tick > datafile->_dataMax) {
      datafile->_dataMax = tick;
    }
  }
  else if (marker->_type == TRI_DOC_MARKER_KEY_DELETION) {
    res = OpenIteratorHandleDeletionMarker(marker, datafile, (open_iterator_state_t*) data);
  }
  else if (marker->_type == TRI_DF_MARKER_SHAPE) {
    res = OpenIteratorHandleShapeMarker(marker, datafile, (open_iterator_state_t*) data);
  }
  else if (marker->_type == TRI_DF_MARKER_ATTRIBUTE) {
    res = OpenIteratorHandleAttributeMarker(marker, datafile, (open_iterator_state_t*) data);
  }
  else if (marker->_type == TRI_DOC_MARKER_BEGIN_TRANSACTION) {
    res = OpenIteratorHandleBeginMarker(marker, datafile, (open_iterator_state_t*) data);
  }
  else if (marker->_type == TRI_DOC_MARKER_COMMIT_TRANSACTION) {
    res = OpenIteratorHandleCommitMarker(marker, datafile, (open_iterator_state_t*) data);
  }
  else if (marker->_type == TRI_DOC_MARKER_PREPARE_TRANSACTION) {
    res = OpenIteratorHandlePrepareMarker(marker, datafile, (open_iterator_state_t*) data);
  }
  else if (marker->_type == TRI_DOC_MARKER_ABORT_TRANSACTION) {
    res = OpenIteratorHandleAbortMarker(marker, datafile, (open_iterator_state_t*) data);
  }
  else {
    if (marker->_type == TRI_DF_MARKER_HEADER) {
      // ensure there is a datafile info entry for each datafile of the collection
      TRI_FindDatafileInfoDocumentCollection(document, datafile->_fid, true);
    }

    LOG_TRACE("skipping marker type %lu", (unsigned long) marker->_type);
    res = TRI_ERROR_NO_ERROR;
  }

  if (datafile->_tickMin == 0) {
    datafile->_tickMin = tick;
  }

  if (tick > datafile->_tickMax) {
    datafile->_tickMax = tick;
  }

  if (tick > document->_tickMax) {
    if (marker->_type != TRI_DF_MARKER_HEADER &&
        marker->_type != TRI_DF_MARKER_FOOTER && 
        marker->_type != TRI_COL_MARKER_HEADER) { 
      document->_tickMax = tick;
    }
  }

  return (res == TRI_ERROR_NO_ERROR);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief iterator for index open
////////////////////////////////////////////////////////////////////////////////

static bool OpenIndexIterator (char const* filename,
                               void* data) {
  // load json description of the index
  TRI_json_t* json = TRI_JsonFile(TRI_CORE_MEM_ZONE, filename, nullptr);

  // json must be a index description
  if (! TRI_IsObjectJson(json)) {
    LOG_ERROR("cannot read index definition from '%s'", filename);

    if (json != nullptr) {
      TRI_FreeJson(TRI_CORE_MEM_ZONE, json);
    }

    return false;
  }

  int res = TRI_FromJsonIndexDocumentCollection(static_cast<TRI_document_collection_t*>(data), json, nullptr);
  TRI_FreeJson(TRI_CORE_MEM_ZONE, json);

  if (res != TRI_ERROR_NO_ERROR) {
    // error was already printed if we get here
    return false;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns information about the collection
/// note: the collection lock must be held when calling this function
////////////////////////////////////////////////////////////////////////////////

static TRI_doc_collection_info_t* Figures (TRI_document_collection_t* document) {
  // prefill with 0's to init counters
  TRI_doc_collection_info_t* info = static_cast<TRI_doc_collection_info_t*>(TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_doc_collection_info_t), true));

  if (info == nullptr) {
    return nullptr;
  }

  for (size_t i = 0;  i < document->_datafileInfo._nrAlloc;  ++i) {
    TRI_doc_datafile_info_t* d = static_cast<TRI_doc_datafile_info_t*>(document->_datafileInfo._table[i]);

    if (d != nullptr) {
      info->_numberAlive        += d->_numberAlive;
      info->_numberDead         += d->_numberDead;
      info->_numberDeletion     += d->_numberDeletion;
      info->_numberShapes       += d->_numberShapes;
      info->_numberAttributes   += d->_numberAttributes;
      info->_numberTransactions += d->_numberTransactions;

      info->_sizeAlive          += d->_sizeAlive;
      info->_sizeDead           += d->_sizeDead;
      info->_sizeShapes         += d->_sizeShapes;
      info->_sizeAttributes     += d->_sizeAttributes;
      info->_sizeTransactions   += d->_sizeTransactions;
    }
  }

  // add the file sizes for datafiles and journals
  TRI_collection_t* base = document;

  for (size_t i = 0; i < base->_datafiles._length; ++i) {
    TRI_datafile_t* df = (TRI_datafile_t*) base->_datafiles._buffer[i];

    info->_datafileSize += (int64_t) df->_maximalSize;
    ++info->_numberDatafiles;
  }

  for (size_t i = 0; i < base->_journals._length; ++i) {
    TRI_datafile_t* df = (TRI_datafile_t*) base->_journals._buffer[i];

    info->_journalfileSize += (int64_t) df->_maximalSize;
    ++info->_numberJournalfiles;
  }

  for (size_t i = 0; i < base->_compactors._length; ++i) {
    TRI_datafile_t* df = (TRI_datafile_t*) base->_compactors._buffer[i];

    info->_compactorfileSize += (int64_t) df->_maximalSize;
    ++info->_numberCompactorfiles;
  }

  // add index information
  info->_numberIndexes = 0;
  info->_sizeIndexes   = 0;

  for (auto& idx : document->allIndexes()) {
    info->_sizeIndexes += idx->memory();
    info->_numberIndexes++;
  }

  // get information about shape files (DEPRECATED, thus hard-coded to 0)
  info->_shapefileSize    = 0;
  info->_numberShapefiles = 0;

  info->_uncollectedLogfileEntries = document->_uncollectedLogfileEntries;
  info->_tickMax = document->_tickMax;

  return info;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises a document collection
////////////////////////////////////////////////////////////////////////////////

static int InitBaseDocumentCollection (TRI_document_collection_t* document,
                                       TRI_shaper_t* shaper) {
  TRI_ASSERT(document != nullptr);

  document->setShaper(shaper);
  document->_numberDocuments    = 0;
  document->_lastCompaction     = 0.0;

  document->size                = Count;


  int res = TRI_InitAssociativePointer(&document->_datafileInfo,
                                       TRI_UNKNOWN_MEM_ZONE,
                                       HashKeyDatafile,
                                       HashElementDatafile,
                                       IsEqualKeyElementDatafile,
                                       nullptr);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  TRI_InitReadWriteLock(&document->_compactionLock);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a primary collection
////////////////////////////////////////////////////////////////////////////////

static void DestroyBaseDocumentCollection (TRI_document_collection_t* document) {
  if (document->_keyGenerator != nullptr) {
    delete document->_keyGenerator;
    document->_keyGenerator = nullptr;
  }

  TRI_DestroyReadWriteLock(&document->_compactionLock);

  {
    TransactionBase trx(true);  // just to protect the following call
    if (document->getShaper() != nullptr) {  // PROTECTED by trx here
      TRI_FreeVocShaper(document->getShaper());  // PROTECTED by trx here
    }
  }

  if (document->_headersPtr != nullptr) {
    delete document->_headersPtr;
    document->_headersPtr = nullptr;
  }

  size_t const n = document->_datafileInfo._nrAlloc;

  for (size_t i = 0; i < n; ++i) {
    TRI_doc_datafile_info_t* dfi = static_cast<TRI_doc_datafile_info_t*>(document->_datafileInfo._table[i]);

    if (dfi != nullptr) {
      FreeDatafileInfo(dfi);
    }
  }

  TRI_DestroyAssociativePointer(&document->_datafileInfo);

  document->ditches()->destroy();
  TRI_DestroyCollection(document);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises a document collection
////////////////////////////////////////////////////////////////////////////////

static bool InitDocumentCollection (TRI_document_collection_t* document,
                                    TRI_shaper_t* shaper) {
  TRI_ASSERT(document != nullptr);

  document->_cleanupIndexes     = false;
  document->_failedTransactions = nullptr;

  document->_uncollectedLogfileEntries.store(0);

  int res = InitBaseDocumentCollection(document, shaper);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_DestroyCollection(document);
    TRI_set_errno(res);

    return false;
  }

  document->_headersPtr = new TRI_headers_t;  // ONLY IN CREATE COLLECTION

  if (document->_headersPtr == nullptr) {  // ONLY IN CREATE COLLECTION
    DestroyBaseDocumentCollection(document);

    return false;
  }

  // create primary index
  std::unique_ptr<triagens::arango::Index> primaryIndex(new triagens::arango::PrimaryIndex(document));

  try {
    document->addIndex(primaryIndex.get());
    primaryIndex.release();
  }
  catch (...) {
    DestroyBaseDocumentCollection(document);
    TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);

    return false;
  }

  // create edges index
  if (document->_info._type == TRI_COL_TYPE_EDGE) {
    TRI_idx_iid_t iid = document->_info._cid;
    if (document->_info._planId > 0) {
      iid = document->_info._planId;
    }

    try {
      std::unique_ptr<triagens::arango::Index> edgeIndex(new triagens::arango::EdgeIndex(iid, document));

      document->addIndex(edgeIndex.get());
      edgeIndex.release();
    }
    catch (...) {
      DestroyBaseDocumentCollection(document);
      TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);

      return false;
    }
  }

  TRI_InitCondition(&document->_journalsCondition);

  // setup methods
  document->beginRead         = BeginRead;
  document->endRead           = EndRead;

  document->beginWrite        = BeginWrite;
  document->endWrite          = EndWrite;

  document->beginReadTimed    = BeginReadTimed;
  document->beginWriteTimed   = BeginWriteTimed;

  document->figures           = Figures;

  // crud methods
  document->cleanupIndexes    = CleanupIndexes;

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief iterate all markers of the collection
////////////////////////////////////////////////////////////////////////////////

static int IterateMarkersCollection (TRI_collection_t* collection) {
  auto document = reinterpret_cast<TRI_document_collection_t*>(collection);

  // initialise state for iteration
  open_iterator_state_t openState;
  openState._document       = document;
  openState._vocbase        = collection->_vocbase;
  openState._tid            = 0;
  openState._trxPrepared    = false;
  openState._trxCollections = 0;
  openState._deletions      = 0;
  openState._documents      = 0;
  openState._fid            = 0;
  openState._dfi            = nullptr;
  openState._initialCount   = -1;

  if (collection->_info._initialCount != -1) {
    auto primaryIndex = document->primaryIndex();

    int res = primaryIndex->resize(static_cast<size_t>(collection->_info._initialCount * 1.1));

    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }

    openState._initialCount = collection->_info._initialCount;
  }

  int res = TRI_InitVector2(&openState._operations, TRI_UNKNOWN_MEM_ZONE, sizeof(open_iterator_operation_t), OpenIteratorBufferSize);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  // read all documents and fill primary index
  TRI_IterateCollection(collection, OpenIterator, &openState);

  LOG_TRACE("found %llu document markers, %llu deletion markers for collection '%s'",
            (unsigned long long) openState._documents,
            (unsigned long long) openState._deletions,
            collection->_info._name);

  // abort any transaction that's unfinished after iterating over all markers
  OpenIteratorAbortTransaction(&openState);

  TRI_DestroyVector(&openState._operations);

  return TRI_ERROR_NO_ERROR;
}

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new collection
////////////////////////////////////////////////////////////////////////////////

TRI_document_collection_t* TRI_CreateDocumentCollection (TRI_vocbase_t* vocbase,
                                                         char const* path,
                                                         TRI_col_info_t* parameters,
                                                         TRI_voc_cid_t cid) {
  if (cid > 0) {
    TRI_UpdateTickServer(cid);
  }
  else {
    cid = TRI_NewTickServer();
  }

  parameters->_cid = cid;

  // check if we can generate the key generator
  KeyGenerator* keyGenerator = KeyGenerator::factory(parameters->_keyOptions);

  if (keyGenerator == nullptr) {
    TRI_set_errno(TRI_ERROR_ARANGO_INVALID_KEY_GENERATOR);
    return nullptr;
  }

  // first create the document collection
  TRI_document_collection_t* document;
  try {
    document = new TRI_document_collection_t();
  }
  catch (std::exception&) {
    document = nullptr;
  }

  if (document == nullptr) {
    delete keyGenerator;
    LOG_WARNING("cannot create document collection");
    TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);

    return nullptr;
  }

  TRI_ASSERT(document != nullptr);

  document->_keyGenerator = keyGenerator;

  TRI_collection_t* collection = TRI_CreateCollection(vocbase, document, path, parameters);

  if (collection == nullptr) {
    delete document;
    LOG_ERROR("cannot create document collection");

    return nullptr;
  }

  TRI_shaper_t* shaper = TRI_CreateVocShaper(vocbase, document);

  if (shaper == nullptr) {
    LOG_ERROR("cannot create shaper");

    TRI_CloseCollection(collection);
    TRI_DestroyCollection(collection);
    delete document;
    return nullptr;
  }

  // create document collection and shaper
  if (false == InitDocumentCollection(document, shaper)) {
    LOG_ERROR("cannot initialise document collection");

    // TODO: shouldn't we free document->_headersPtr etc.?
    TRI_CloseCollection(collection);
    TRI_DestroyCollection(collection);
    delete document;
    return nullptr;
  }

  document->_keyGenerator = keyGenerator;

  // save the parameters block (within create, no need to lock)
  bool doSync = vocbase->_settings.forceSyncProperties;
  int res = TRI_SaveCollectionInfo(collection->_directory, parameters, doSync);

  if (res != TRI_ERROR_NO_ERROR) {
    // TODO: shouldn't we free document->_headersPtr etc.?
    LOG_ERROR("cannot save collection parameters in directory '%s': '%s'",
              collection->_directory,
              TRI_last_error());

    TRI_CloseCollection(collection);
    TRI_DestroyCollection(collection);
    delete document;
    return nullptr;
  }
        
  // remove the temporary file
  char* tmpfile = TRI_Concatenate2File(collection->_directory, ".tmp");
  TRI_UnlinkFile(tmpfile);
  TRI_Free(TRI_CORE_MEM_ZONE, tmpfile);

  TransactionBase trx(true);  // just to protect the following call
  TRI_ASSERT(document->getShaper() != nullptr);  // ONLY IN COLLECTION CREATION, PROTECTED by trx here

  return document;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the memory allocated, but does not free the pointer
///
/// Note that the collection must be closed first.
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyDocumentCollection (TRI_document_collection_t* document) {
  TRI_DestroyCondition(&document->_journalsCondition);

  // free memory allocated for indexes
  for (auto& idx : document->allIndexes()) {
    delete idx;
  }

  if (document->_failedTransactions != nullptr) {
    delete document->_failedTransactions;
  }

  DestroyBaseDocumentCollection(document);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the memory allocated and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeDocumentCollection (TRI_document_collection_t* document) {
  TRI_DestroyDocumentCollection(document);
  delete document;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief removes a datafile description
////////////////////////////////////////////////////////////////////////////////

void TRI_RemoveDatafileInfoDocumentCollection (TRI_document_collection_t* document,
                                              TRI_voc_fid_t fid) {
  TRI_RemoveKeyAssociativePointer(&document->_datafileInfo, &fid);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief finds a datafile description
////////////////////////////////////////////////////////////////////////////////

TRI_doc_datafile_info_t* TRI_FindDatafileInfoDocumentCollection (TRI_document_collection_t* document,
                                                                TRI_voc_fid_t fid,
                                                                bool create) {
  TRI_doc_datafile_info_t const* found = static_cast<TRI_doc_datafile_info_t const*>(TRI_LookupByKeyAssociativePointer(&document->_datafileInfo, &fid));

  if (found != nullptr) {
    return const_cast<TRI_doc_datafile_info_t*>(found);
  }

  if (! create) {
    return nullptr;
  }

  // allocate and set to 0
  TRI_doc_datafile_info_t* dfi = static_cast<TRI_doc_datafile_info_t*>(TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_doc_datafile_info_t), true));

  if (dfi == nullptr) {
    return nullptr;
  }

  dfi->_fid = fid;

  TRI_InsertKeyAssociativePointer(&document->_datafileInfo, &fid, dfi, true);

  return dfi;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a journal
///
/// Note that the caller must hold a lock protecting the _journals entry.
////////////////////////////////////////////////////////////////////////////////

TRI_datafile_t* TRI_CreateDatafileDocumentCollection (TRI_document_collection_t* document,
                                                      TRI_voc_fid_t fid,
                                                      TRI_voc_size_t journalSize,
                                                      bool isCompactor) {
  TRI_ASSERT(fid > 0);

  TRI_datafile_t* journal;

  if (document->_info._isVolatile) {
    // in-memory collection
    journal = TRI_CreateDatafile(nullptr, fid, journalSize, true);
  }
  else {
    // construct a suitable filename (which may be temporary at the beginning)
    char* number = TRI_StringUInt64(fid);
    char* jname;
    if (isCompactor) {
      jname = TRI_Concatenate3String("compaction-", number, ".db");
    }
    else {
      jname = TRI_Concatenate3String("temp-", number, ".db");
    }

    char* filename = TRI_Concatenate2File(document->_directory, jname);

    TRI_FreeString(TRI_CORE_MEM_ZONE, number);
    TRI_FreeString(TRI_CORE_MEM_ZONE, jname);

    TRI_IF_FAILURE("CreateJournalDocumentCollection") {
      // simulate disk full
      TRI_FreeString(TRI_CORE_MEM_ZONE, filename);
      document->_lastError = TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY_MMAP);

      EnsureErrorCode(ENOSPC);

      return nullptr;
    }

    // remove an existing temporary file first
    if (TRI_ExistsFile(filename)) {
      // remove an existing file first
      TRI_UnlinkFile(filename);
    }

    journal = TRI_CreateDatafile(filename, fid, journalSize, true);

    TRI_FreeString(TRI_CORE_MEM_ZONE, filename);
  }

  if (journal == nullptr) {
    if (TRI_errno() == TRI_ERROR_OUT_OF_MEMORY_MMAP) {
      document->_lastError = TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY_MMAP);
    }
    else {
      document->_lastError = TRI_set_errno(TRI_ERROR_ARANGO_NO_JOURNAL);
    }
    
    EnsureErrorCode(document->_lastError);  

    return nullptr;
  }

  // journal is there now
  TRI_ASSERT(journal != nullptr);

  if (isCompactor) {
    LOG_TRACE("created new compactor '%s'", journal->getName(journal));
  }
  else {
    LOG_TRACE("created new journal '%s'", journal->getName(journal));
  }

  // create a collection header, still in the temporary file
  TRI_df_marker_t* position;
  int res = TRI_ReserveElementDatafile(journal, sizeof(TRI_col_header_marker_t), &position, journalSize);
    
  TRI_IF_FAILURE("CreateJournalDocumentCollectionReserve1") {
    res = TRI_ERROR_DEBUG;
  }

  if (res != TRI_ERROR_NO_ERROR) {
    document->_lastError = journal->_lastError;
    LOG_ERROR("cannot create collection header in file '%s': %s", journal->getName(journal), TRI_errno_string(res));

    // close the journal and remove it
    TRI_CloseDatafile(journal);
    TRI_UnlinkFile(journal->getName(journal));
    TRI_FreeDatafile(journal);
    
    EnsureErrorCode(res);  

    return nullptr;
  }

  TRI_col_header_marker_t cm;
  TRI_InitMarkerDatafile((char*) &cm, TRI_COL_MARKER_HEADER, sizeof(TRI_col_header_marker_t));
  cm.base._tick = static_cast<TRI_voc_tick_t>(fid);
  cm._type = (TRI_col_type_t) document->_info._type;
  cm._cid  = document->_info._cid;

  res = TRI_WriteCrcElementDatafile(journal, position, &cm.base, false);

  TRI_IF_FAILURE("CreateJournalDocumentCollectionReserve2") {
    res = TRI_ERROR_DEBUG;
  }

  if (res != TRI_ERROR_NO_ERROR) {
    document->_lastError = journal->_lastError;
    LOG_ERROR("cannot create collection header in file '%s': %s", journal->getName(journal), TRI_last_error());

    // close the journal and remove it
    TRI_CloseDatafile(journal);
    TRI_UnlinkFile(journal->getName(journal));
    TRI_FreeDatafile(journal);
    
    EnsureErrorCode(document->_lastError);  

    return nullptr;
  }

  TRI_ASSERT(fid == journal->_fid);


  // if a physical file, we can rename it from the temporary name to the correct name
  if (! isCompactor) {
    if (journal->isPhysical(journal)) {
      // and use the correct name
      char* number = TRI_StringUInt64(journal->_fid);
      char* jname = TRI_Concatenate3String("journal-", number, ".db");
      char* filename = TRI_Concatenate2File(document->_directory, jname);

      TRI_FreeString(TRI_CORE_MEM_ZONE, number);
      TRI_FreeString(TRI_CORE_MEM_ZONE, jname);

      bool ok = TRI_RenameDatafile(journal, filename);

      if (! ok) {
        LOG_ERROR("failed to rename journal '%s' to '%s': %s", journal->getName(journal), filename, TRI_last_error());

        TRI_CloseDatafile(journal);
        TRI_UnlinkFile(journal->getName(journal));
        TRI_FreeDatafile(journal);
        TRI_FreeString(TRI_CORE_MEM_ZONE, filename);
    
        EnsureErrorCode(document->_lastError);  

        return nullptr;
      }
      else {
        LOG_TRACE("renamed journal from '%s' to '%s'", journal->getName(journal), filename);
      }

      TRI_FreeString(TRI_CORE_MEM_ZONE, filename);
    }

    TRI_PushBackVectorPointer(&document->_journals, journal);
  }

  // now create a datafile entry for the new journal
  TRI_FindDatafileInfoDocumentCollection(document, fid, true);

  return journal;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief iterate over all documents in the collection, using a user-defined
/// callback function. Returns the total number of documents in the collection
///
/// The user can abort the iteration by return "false" from the callback
/// function.
///
/// Note: the function will not acquire any locks. It is the task of the caller
/// to ensure the collection is properly locked
////////////////////////////////////////////////////////////////////////////////

size_t TRI_DocumentIteratorDocumentCollection (TransactionBase const*,
                                              TRI_document_collection_t* document,
                                              void* data,
                                              bool (*callback)(TRI_doc_mptr_t const*, TRI_document_collection_t*, void*)) {
  // The first argument is only used to make the compiler prove that a
  // transaction is ongoing. We need this to prove that accesses to
  // master pointers and their data pointers in the callback are
  // protected.

  auto primaryIndex = document->primaryIndex()->internals();
  size_t const nrUsed = static_cast<size_t>(primaryIndex->_nrUsed);

  if (nrUsed > 0) {
    void** ptr = primaryIndex->_table;
    void** end = ptr + primaryIndex->_nrAlloc;

    for (;  ptr < end;  ++ptr) {
      if (*ptr) {
        auto d = (TRI_doc_mptr_t const*) *ptr;

        if (! callback(d, document, data)) {
          break;
        }
      }
    }
  }

  return nrUsed;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an index, based on a JSON description
////////////////////////////////////////////////////////////////////////////////

int TRI_FromJsonIndexDocumentCollection (TRI_document_collection_t* document,
                                         TRI_json_t const* json,
                                         triagens::arango::Index** idx) {
  TRI_ASSERT(TRI_IsObjectJson(json));

  if (idx != nullptr) {
    *idx = nullptr;
  }

  // extract the type
  TRI_json_t const* type = TRI_LookupObjectJson(json, "type");

  if (! TRI_IsStringJson(type)) {
    return TRI_ERROR_INTERNAL;
  }

  char const* typeStr = type->_value._string.data;

  // extract the index identifier
  TRI_json_t const* iis = TRI_LookupObjectJson(json, "id");

  TRI_idx_iid_t iid;
  if (TRI_IsNumberJson(iis)) {
    iid = static_cast<TRI_idx_iid_t>(iis->_value._number);
  }
  else if (TRI_IsStringJson(iis)) {
    iid = (TRI_idx_iid_t) TRI_UInt64String2(iis->_value._string.data,
                                            iis->_value._string.length - 1);
  }
  else {
    LOG_ERROR("ignoring index, index identifier could not be located");

    return TRI_ERROR_INTERNAL;
  }

  TRI_UpdateTickServer(iid);

  // ...........................................................................
  // CAP CONSTRAINT
  // ...........................................................................

  if (TRI_EqualString(typeStr, "cap")) {
    return CapConstraintFromJson(document, json, iid, idx);
  }

  // ...........................................................................
  // GEO INDEX (list or attribute)
  // ...........................................................................

  else if (TRI_EqualString(typeStr, "geo1") || TRI_EqualString(typeStr, "geo2")) {
    return GeoIndexFromJson(document, json, iid, idx);
  }

  // ...........................................................................
  // HASH INDEX
  // ...........................................................................

  else if (TRI_EqualString(typeStr, "hash")) {
    return HashIndexFromJson(document, json, iid, idx);
  }

  // ...........................................................................
  // SKIPLIST INDEX
  // ...........................................................................

  else if (TRI_EqualString(typeStr, "skiplist")) {
    return SkiplistIndexFromJson(document, json, iid, idx);
  }

  // ...........................................................................
  // FULLTEXT INDEX
  // ...........................................................................

  else if (TRI_EqualString(typeStr, "fulltext")) {
    return FulltextIndexFromJson(document, json, iid, idx);
  }

  // ...........................................................................
  // EDGES INDEX
  // ...........................................................................

  else if (TRI_EqualString(typeStr, "edge")) {
    // we should never get here, as users cannot create their own edge indexes
    LOG_ERROR("logic error. there should never be a JSON file describing an edges index");
    return TRI_ERROR_INTERNAL;
  }

  LOG_WARNING("index type '%s' is not supported in this version of ArangoDB and is ignored", typeStr);
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief rolls back a document operation
////////////////////////////////////////////////////////////////////////////////

int TRI_RollbackOperationDocumentCollection (TRI_document_collection_t* document,
                                             TRI_voc_document_operation_e type,
                                             TRI_doc_mptr_t* header,
                                             TRI_doc_mptr_copy_t const* oldData) {

  if (type == TRI_VOC_DOCUMENT_OPERATION_INSERT) {
    // ignore any errors we're getting from this
    DeletePrimaryIndex(document, header, true);
    DeleteSecondaryIndexes(document, header, true);

    TRI_ASSERT(document->_numberDocuments > 0);
    document->_numberDocuments--;

    return TRI_ERROR_NO_ERROR;
  }
  else if (type == TRI_VOC_DOCUMENT_OPERATION_UPDATE) {
    // copy the existing header's state
    TRI_doc_mptr_copy_t copy = *header;
    
    // remove the current values from the indexes
    DeleteSecondaryIndexes(document, header, true);
    // revert to the old state
    header->copy(*oldData);
    // re-insert old state
    int res = InsertSecondaryIndexes(document, header, true);
    // revert again to the new state, because other parts of the new state
    // will be reverted at some other place
    header->copy(copy);
    
    return res;
  }
  else if (type == TRI_VOC_DOCUMENT_OPERATION_REMOVE) {
    int res = InsertPrimaryIndex(document, header, true);

    if (res == TRI_ERROR_NO_ERROR) {
      res = InsertSecondaryIndexes(document, header, true);
      document->_numberDocuments++;
    }
    else {
      LOG_ERROR("error rolling back remove operation");
    }
    return res;
  }

  return TRI_ERROR_INTERNAL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief closes an existing datafile
/// Note that the caller must hold a lock protecting the _datafiles and
/// _journals entry.
////////////////////////////////////////////////////////////////////////////////

bool TRI_CloseDatafileDocumentCollection (TRI_document_collection_t* document,
                                          size_t position,
                                          bool isCompactor) {
  TRI_vector_pointer_t* vector;

  // either use a journal or a compactor
  if (isCompactor) {
    vector = &document->_compactors;
  }
  else {
    vector = &document->_journals;
  }

  // no journal at this position
  if (vector->_length <= position) {
    TRI_set_errno(TRI_ERROR_ARANGO_NO_JOURNAL);
    return false;
  }

  // seal and rename datafile
  TRI_datafile_t* journal = static_cast<TRI_datafile_t*>(vector->_buffer[position]);
  int res = TRI_SealDatafile(journal);

  if (res != TRI_ERROR_NO_ERROR) {
    LOG_ERROR("failed to seal datafile '%s': %s", journal->getName(journal), TRI_last_error());

    if (! isCompactor) {
      TRI_RemoveVectorPointer(vector, position);
      TRI_PushBackVectorPointer(&document->_datafiles, journal);
    }

    return false;
  }

  if (! isCompactor && journal->isPhysical(journal)) {
    // rename the file
    char* number = TRI_StringUInt64(journal->_fid);
    char* dname = TRI_Concatenate3String("datafile-", number, ".db");
    char* filename = TRI_Concatenate2File(document->_directory, dname);

    TRI_FreeString(TRI_CORE_MEM_ZONE, dname);
    TRI_FreeString(TRI_CORE_MEM_ZONE, number);

    bool ok = TRI_RenameDatafile(journal, filename);

    if (! ok) {
      LOG_ERROR("failed to rename datafile '%s' to '%s': %s", journal->getName(journal), filename, TRI_last_error());

      TRI_RemoveVectorPointer(vector, position);
      TRI_PushBackVectorPointer(&document->_datafiles, journal);
      TRI_FreeString(TRI_CORE_MEM_ZONE, filename);

      return false;
    }

    TRI_FreeString(TRI_CORE_MEM_ZONE, filename);

    LOG_TRACE("closed file '%s'", journal->getName(journal));
  }

  if (! isCompactor) {
    TRI_RemoveVectorPointer(vector, position);
    TRI_PushBackVectorPointer(&document->_datafiles, journal);
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief helper struct for filling indexes
////////////////////////////////////////////////////////////////////////////////

class IndexFiller {
  public:
    IndexFiller (TRI_document_collection_t* document,
                 triagens::arango::Index* idx,
                 std::function<void(int)> callback) 
      : _document(document),
        _idx(idx),
        _callback(callback) {
    }

    void operator() () {
      TransactionBase trx(true);
      int res = TRI_ERROR_INTERNAL;

      try {
        res = FillIndex(_document, _idx);
      }
      catch (...) {
      }
        
      _callback(res);
    }

  private:

    TRI_document_collection_t* _document;
    triagens::arango::Index*   _idx;
    std::function<void(int)>   _callback;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief fill the additional (non-primary) indexes
////////////////////////////////////////////////////////////////////////////////

int TRI_FillIndexesDocumentCollection (TRI_vocbase_col_t* collection,
                                       TRI_document_collection_t* document) {
  auto old = document->useSecondaryIndexes();

  // turn filling of secondary indexes off. we're now only interested in getting
  // the indexes' definition. we'll fill them below ourselves.
  document->useSecondaryIndexes(false);

  try {
    TRI_collection_t* collection = reinterpret_cast<TRI_collection_t*>(document);
    TRI_IterateIndexCollection(collection, OpenIndexIterator, collection);
    document->useSecondaryIndexes(old);
  }
  catch (...) {
    document->useSecondaryIndexes(old);
    return TRI_ERROR_INTERNAL;
  }
 
  // distribute the work to index threads plus this thread
  auto const& indexes = document->allIndexes();
  size_t const n = indexes.size();

  double start = TRI_microtime();

  // only log performance infos for indexes with more than this number of entries
  static size_t const NotificationSizeThreshold = 131072; 
  auto primaryIndex = document->primaryIndex()->internals();

  if ((n > 1) && (primaryIndex->_nrUsed > NotificationSizeThreshold)) {
    LOG_ACTION("fill-indexes-document-collection { collection: %s/%s }, n: %d", 
               document->_vocbase->_name,
               document->_info._name,
               (int) (n - 1));
  }

  TRI_ASSERT(n >= 1);
    
  std::atomic<int> result(TRI_ERROR_NO_ERROR);
 
  { 
    triagens::basics::Barrier barrier(n - 1);

    auto indexPool = document->_vocbase->_server->_indexPool;

    auto callback = [&barrier, &result] (int res) -> void {
      // update the error code
      if (res != TRI_ERROR_NO_ERROR) {
        int expected = TRI_ERROR_NO_ERROR;
        result.compare_exchange_strong(expected, res, std::memory_order_acquire);
      }
      
      barrier.join();
    };

    // now actually fill the secondary indexes
    for (size_t i = 1;  i < n;  ++i) {
      auto idx = indexes[i];

      // index threads must come first, otherwise this thread will block the loop and
      // prevent distribution to threads
      if (indexPool != nullptr && i != (n - 1)) {
        // move task into thread pool
        IndexFiller indexTask(document, idx, callback);

        try {
          static_cast<triagens::basics::ThreadPool*>(indexPool)->enqueue(indexTask);
        }
        catch (...) {
          // set error code
          int expected = TRI_ERROR_NO_ERROR;
          result.compare_exchange_strong(expected, TRI_ERROR_INTERNAL, std::memory_order_acquire);
        
          barrier.join();
        }
      }
      else { 
        // fill index in this thread
        int res;
        
        try {
          res = FillIndex(document, idx);
        }
        catch (...) {
          res = TRI_ERROR_INTERNAL;
        }
        
        if (res != TRI_ERROR_NO_ERROR) {
          int expected = TRI_ERROR_NO_ERROR;
          result.compare_exchange_strong(expected, res, std::memory_order_acquire);
        }
        
        barrier.join();
      }
    }

    // barrier waits here until all threads have joined
  }
    
  LOG_TIMER((TRI_microtime() - start),
            "fill-indexes-document-collection { collection: %s/%s }, n: %d", 
            document->_vocbase->_name,
            document->_info._name, 
            (int) (n - 1)); 

  return result.load();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief opens an existing collection
////////////////////////////////////////////////////////////////////////////////

TRI_document_collection_t* TRI_OpenDocumentCollection (TRI_vocbase_t* vocbase,
                                                       TRI_vocbase_col_t* col,
                                                       bool ignoreErrors) {
  char const* path = col->_path;

  // first open the document collection
  TRI_document_collection_t* document = nullptr;
  try {
    document = new TRI_document_collection_t();
  }
  catch (std::exception&) {
  }

  if (document == nullptr) {
    return nullptr;
  }

  TRI_ASSERT(document != nullptr);
  
  double start = TRI_microtime();
  LOG_ACTION("open-document-collection { collection: %s/%s }", 
             vocbase->_name,
             col->_name);

  TRI_collection_t* collection = TRI_OpenCollection(vocbase, document, path, ignoreErrors);

  if (collection == nullptr) {
    delete document;
    LOG_ERROR("cannot open document collection from path '%s'", path);

    return nullptr;
  }

  TRI_shaper_t* shaper = TRI_CreateVocShaper(vocbase, document);

  if (shaper == nullptr) {
    LOG_ERROR("cannot create shaper");

    TRI_CloseCollection(collection);
    TRI_FreeCollection(collection);

    return nullptr;
  }

  // create document collection and shaper
  if (false == InitDocumentCollection(document, shaper)) {
    TRI_CloseCollection(collection);
    TRI_FreeCollection(collection);
    LOG_ERROR("cannot initialise document collection");

    return nullptr;
  }

  // check if we can generate the key generator
  KeyGenerator* keyGenerator = KeyGenerator::factory(collection->_info._keyOptions);

  if (keyGenerator == nullptr) {
    TRI_CloseCollection(collection);
    TRI_FreeCollection(collection);
    TRI_set_errno(TRI_ERROR_ARANGO_INVALID_KEY_GENERATOR);

    return nullptr;
  }

  document->_keyGenerator = keyGenerator;

  // create a fake transaction for loading the collection
  TransactionBase trx(true);

  // build the primary index
  {
    double start = TRI_microtime();

    LOG_ACTION("iterate-markers { collection: %s/%s }", 
               vocbase->_name,
               document->_info._name);

    // iterate over all markers of the collection
    int res = IterateMarkersCollection(collection);
  
    LOG_TIMER((TRI_microtime() - start),
              "iterate-markers { collection: %s/%s }", 
              vocbase->_name,
              document->_info._name);
  
    if (res != TRI_ERROR_NO_ERROR) {
      if (document->_failedTransactions != nullptr) {
        delete document->_failedTransactions;
      }
      TRI_CloseCollection(collection);
      TRI_FreeCollection(collection);

      LOG_ERROR("cannot iterate data of document collection");
      TRI_set_errno(res);

      return nullptr;
    }
  }

  TRI_ASSERT(document->getShaper() != nullptr);  // ONLY in OPENCOLLECTION, PROTECTED by fake trx here

  TRI_InitVocShaper(document->getShaper());  // ONLY in OPENCOLLECTION, PROTECTED by fake trx here

  if (! triagens::wal::LogfileManager::instance()->isInRecovery()) {
    TRI_FillIndexesDocumentCollection(col, document);
  }

  LOG_TIMER((TRI_microtime() - start),
            "open-document-collection { collection: %s/%s }", 
            vocbase->_name,
            document->_info._name);

  return document;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief closes an open collection
////////////////////////////////////////////////////////////////////////////////

int TRI_CloseDocumentCollection (TRI_document_collection_t* document,
                                 bool updateStats) {
  auto primaryIndex = document->primaryIndex()->internals();

  if (! document->_info._deleted &&
      document->_info._initialCount != static_cast<int64_t>(primaryIndex->_nrUsed)) {
    // update the document count
    document->_info._initialCount = primaryIndex->_nrUsed;
    
    bool doSync = document->_vocbase->_settings.forceSyncProperties;
    TRI_SaveCollectionInfo(document->_directory, &document->_info, doSync);
  }

  // closes all open compactors, journals, datafiles
  int res = TRI_CloseCollection(document);

  TransactionBase trx(true);  // just to protect the following call
  TRI_FreeVocShaper(document->getShaper());  // ONLY IN CLOSECOLLECTION, PROTECTED by fake trx here
  document->setShaper(nullptr);

  return res;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                           INDEXES
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                     private types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief pid name structure
////////////////////////////////////////////////////////////////////////////////

typedef struct pid_name_s {
  TRI_shape_pid_t _pid;
  char* _name;
}
pid_name_t;

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief converts extracts a field list from a json object
////////////////////////////////////////////////////////////////////////////////

static TRI_json_t* ExtractFields (TRI_json_t const* json,
                                  size_t* fieldCount,
                                  TRI_idx_iid_t iid) {
  TRI_json_t* fld = TRI_LookupObjectJson(json, "fields");

  if (! TRI_IsArrayJson(fld)) {
    LOG_ERROR("ignoring index %llu, 'fields' must be an array", (unsigned long long) iid);

    TRI_set_errno(TRI_ERROR_BAD_PARAMETER);
    return nullptr;
  }

  *fieldCount = TRI_LengthArrayJson(fld);

  for (size_t j = 0;  j < *fieldCount;  ++j) {
    TRI_json_t* sub = static_cast<TRI_json_t*>(TRI_AtVector(&fld->_value._objects, j));

    if (! TRI_IsStringJson(sub)) {
      LOG_ERROR("ignoring index %llu, 'fields' must be an array of attribute paths", (unsigned long long) iid);

      TRI_set_errno(TRI_ERROR_BAD_PARAMETER);
      return nullptr;
    }
  }

  return fld;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises an index with all existing documents
////////////////////////////////////////////////////////////////////////////////

static int FillIndex (TRI_document_collection_t* document,
                      triagens::arango::Index* idx) {

  if (! document->useSecondaryIndexes()) {
    return TRI_ERROR_NO_ERROR;
  }

  auto primaryIndex = document->primaryIndex()->internals();
  void** ptr = primaryIndex->_table;
  void** end = ptr + primaryIndex->_nrAlloc;

  try {
    // give the index a size hint
    idx->sizeHint(static_cast<size_t>(primaryIndex->_nrUsed));

#ifdef TRI_ENABLE_MAINTAINER_MODE
    static const int LoopSize = 10000;
    int counter = 0;
    int loops = 0;
#endif

    for (;  ptr < end;  ++ptr) {
      auto mptr = static_cast<TRI_doc_mptr_t const*>(*ptr);

      if (mptr != nullptr) {
        int res = idx->insert(mptr, false);

        if (res != TRI_ERROR_NO_ERROR) {
          return res;
        }

#ifdef TRI_ENABLE_MAINTAINER_MODE
        if (++counter == LoopSize) {
          counter = 0;
          ++loops;

          LOG_TRACE("indexed %llu documents of collection %llu",
                    (unsigned long long) (LoopSize * loops),
                    (unsigned long long) document->_info._cid);
        }
#endif

      }
    }

    return TRI_ERROR_NO_ERROR;
  }
  catch (triagens::basics::Exception const& ex) {
    return ex.code();
  }
  catch (std::bad_alloc&) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }
  catch (...) {
    return TRI_ERROR_INTERNAL;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief finds a path based, unique or non-unique index
////////////////////////////////////////////////////////////////////////////////

static triagens::arango::Index* LookupPathIndexDocumentCollection (TRI_document_collection_t* collection,
                                                                   std::vector<std::string> const& paths,
                                                                   triagens::arango::Index::IndexType type,
                                                                   int sparsity,
                                                                   bool unique,
                                                                   bool allowAnyAttributeOrder) {

  for (auto const& idx : collection->allIndexes()) {
    if (idx->type() != type) {
      continue;
    }

    // .........................................................................
    // Now perform checks which are specific to the type of index
    // .........................................................................

    switch (idx->type()) {
      case triagens::arango::Index::TRI_IDX_TYPE_HASH_INDEX: {
        auto hashIndex = static_cast<triagens::arango::HashIndex*>(idx);

        if (unique != hashIndex->unique() ||
            (sparsity != -1 && sparsity != (hashIndex->sparse() ? 1 : 0 ))) {
          continue;
        }
        break;
      }

      case triagens::arango::Index::TRI_IDX_TYPE_SKIPLIST_INDEX: {
        auto skiplistIndex = static_cast<triagens::arango::SkiplistIndex2*>(idx);
        
        if (unique != skiplistIndex->unique() ||
            (sparsity != -1 && sparsity != (skiplistIndex->sparse() ? 1 : 0 ))) {
          continue;
        }
        break;
      }

      default: {
        continue;
      }
    }

    // .........................................................................
    // check that the number of paths (fields) in the index matches that
    // of the number of attributes
    // .........................................................................

    auto const& idxFields = idx->fields();
    size_t const n = idxFields.size();

    if (n != paths.size()) {
      continue;
    }

    // .........................................................................
    // go through all the attributes and see if they match
    // .........................................................................

    bool found = true;

    if (allowAnyAttributeOrder) {
      // any permutation of attributes is allowed
      for (size_t i = 0; i < n; ++i) {
        found = false;

        for (size_t j = 0; j < n; ++j) {
          if (idxFields[i] == paths[j]) {
            found = true;
            break;
          }
        }

        if (! found) {
          break;
        }
      }
    }
    else {
      // attributes need to be present in a given order
      for (size_t i = 0; i < n; ++i) {
        if (idxFields[i] != paths[i]) {
          found = false;
          break;
        }
      }
    }

    // stop if we found a match
    if (found) {
      return idx;
    }
  }

  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief restores a path based index (template)
////////////////////////////////////////////////////////////////////////////////

static int PathBasedIndexFromJson (TRI_document_collection_t* document,
                                   TRI_json_t const* definition,
                                   TRI_idx_iid_t iid,
                                   triagens::arango::Index* (*creator) (TRI_document_collection_t*,
                                                                        std::vector<std::string> const&,
                                                                        TRI_idx_iid_t,
                                                                        bool,
                                                                        bool,
                                                                        bool*),
                                   triagens::arango::Index** dst) {

  if (dst != nullptr) {
    *dst = nullptr;
  }

  // extract fields
  size_t fieldCount;
  TRI_json_t const* fld = ExtractFields(definition, &fieldCount, iid);

  if (fld == nullptr) {
    return TRI_errno();
  }

  // extract the list of fields
  if (fieldCount < 1) {
    LOG_ERROR("ignoring index %llu, need at least one attribute path", (unsigned long long) iid);

    return TRI_set_errno(TRI_ERROR_BAD_PARAMETER);
  }

  // determine if the index is unique or non-unique
  TRI_json_t const* bv = TRI_LookupObjectJson(definition, "unique");

  if (! TRI_IsBooleanJson(bv)) {
    LOG_ERROR("ignoring index %llu, could not determine if unique or non-unique", (unsigned long long) iid);
    return TRI_set_errno(TRI_ERROR_BAD_PARAMETER);
  }

  bool unique = bv->_value._boolean;

  // determine sparsity
  bool sparse = false;

  bv = TRI_LookupObjectJson(definition, "sparse"); 

  if (TRI_IsBooleanJson(bv)) {
    sparse = bv->_value._boolean;
  }
  else {
    // no sparsity information given for index
    // now use pre-2.5 defaults: unique hash indexes were sparse, all other indexes were non-sparse
    bool isHashIndex = false;
    TRI_json_t const* typeJson = TRI_LookupObjectJson(definition, "type");
    if (TRI_IsStringJson(typeJson)) {
      isHashIndex = (strcmp(typeJson->_value._string.data, "hash") == 0);
    }

    if (isHashIndex && unique) {
      sparse = true;
    } 
  }

  // Initialise the vector in which we store the fields on which the hashing
  // will be based.
  std::vector<std::string> attributes;
  attributes.reserve(fieldCount);

  // find fields
  for (size_t j = 0;  j < fieldCount;  ++j) {
    auto fieldStr = static_cast<TRI_json_t const*>(TRI_AtVector(&fld->_value._objects, j));

    attributes.emplace_back(std::string(fieldStr->_value._string.data, fieldStr->_value._string.length - 1));;
  }

  // create the index
  auto idx = creator(document, attributes, iid, sparse, unique, nullptr);

  if (dst != nullptr) {
    *dst = idx;
  }

  if (idx == nullptr) {
    LOG_ERROR("cannot create index %llu in collection '%s'", (unsigned long long) iid, document->_info._name);
    return TRI_errno();
  }

  return TRI_ERROR_NO_ERROR;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief update statistics for a collection
/// note: the write-lock for the collection must be held to call this
////////////////////////////////////////////////////////////////////////////////

void TRI_UpdateRevisionDocumentCollection (TRI_document_collection_t* document,
                                           TRI_voc_rid_t rid,
                                           bool force) {
  if (rid > 0) {
    SetRevision(document, rid, force);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not a collection is fully collected
////////////////////////////////////////////////////////////////////////////////

bool TRI_IsFullyCollectedDocumentCollection (TRI_document_collection_t* document) {
  TRI_READ_LOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(document);

  int64_t uncollected = document->_uncollectedLogfileEntries.load();

  TRI_READ_UNLOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(document);

  return (uncollected == 0);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief saves an index
////////////////////////////////////////////////////////////////////////////////

int TRI_SaveIndex (TRI_document_collection_t* document,
                   triagens::arango::Index* idx,
                   bool writeMarker) {
  // convert into JSON
  auto json = idx->toJson(TRI_UNKNOWN_MEM_ZONE);

  // construct filename
  char* number   = TRI_StringUInt64(idx->id());
  char* name     = TRI_Concatenate3String("index-", number, ".json");
  char* filename = TRI_Concatenate2File(document->_directory, name);

  TRI_FreeString(TRI_CORE_MEM_ZONE, name);
  TRI_FreeString(TRI_CORE_MEM_ZONE, number);

  TRI_vocbase_t* vocbase = document->_vocbase;

  // and save
  bool ok = TRI_SaveJson(filename, json.json(), document->_vocbase->_settings.forceSyncProperties);

  TRI_FreeString(TRI_CORE_MEM_ZONE, filename);

  if (! ok) {
    LOG_ERROR("cannot save index definition: %s", TRI_last_error());

    return TRI_errno();
  }

  if (! writeMarker) {
    return TRI_ERROR_NO_ERROR;
  }

  int res = TRI_ERROR_NO_ERROR;

  try {
    triagens::wal::CreateIndexMarker marker(vocbase->_id, document->_info._cid, idx->id(), triagens::basics::JsonHelper::toString(json.json()));
    triagens::wal::SlotInfoCopy slotInfo = triagens::wal::LogfileManager::instance()->allocateAndWrite(marker, false);

    if (slotInfo.errorCode != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION(slotInfo.errorCode);
    }

    return TRI_ERROR_NO_ERROR;
  }
  catch (triagens::basics::Exception const& ex) {
    res = ex.code();
  }
  catch (...) {
    res = TRI_ERROR_INTERNAL;
  }

  // TODO: what to do here?
  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a description of all indexes
///
/// the caller must have read-locked the underlying collection!
////////////////////////////////////////////////////////////////////////////////

TRI_vector_pointer_t* TRI_IndexesDocumentCollection (TRI_document_collection_t* document) {
  TRI_vector_pointer_t* vector = static_cast<TRI_vector_pointer_t*>(TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_vector_pointer_t), false));

  if (vector == nullptr) {
    return nullptr;
  }
  
  TRI_InitVectorPointer(vector, TRI_UNKNOWN_MEM_ZONE);

  for (auto const& idx : document->allIndexes()) {
    auto json = idx->toJson(TRI_UNKNOWN_MEM_ZONE);

    TRI_PushBackVectorPointer(vector, json.steal());
  }

  return vector;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief drops an index, including index file removal and replication
////////////////////////////////////////////////////////////////////////////////

bool TRI_DropIndexDocumentCollection (TRI_document_collection_t* document,
                                      TRI_idx_iid_t iid,
                                      bool writeMarker) {
  if (iid == 0) {
    // invalid index id or primary index
    return true;
  }

  TRI_vocbase_t* vocbase = document->_vocbase;

  TRI_ReadLockReadWriteLock(&vocbase->_inventoryLock);

  // .............................................................................
  // inside write-lock
  // .............................................................................

  TRI_WRITE_LOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(document);
  
  triagens::arango::Index* found = document->removeIndex(iid);
  
  TRI_WRITE_UNLOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(document);

  TRI_ReadUnlockReadWriteLock(&vocbase->_inventoryLock);

  // .............................................................................
  // outside write-lock
  // .............................................................................

  if (found != nullptr) {
    bool result = RemoveIndexFile(document, found->id());

    delete found;
    found = nullptr;

    if (writeMarker) {
      int res = TRI_ERROR_NO_ERROR;

      try {
        triagens::wal::DropIndexMarker marker(vocbase->_id, document->_info._cid, iid);
        triagens::wal::SlotInfoCopy slotInfo = triagens::wal::LogfileManager::instance()->allocateAndWrite(marker, false);

        if (slotInfo.errorCode != TRI_ERROR_NO_ERROR) {
          THROW_ARANGO_EXCEPTION(slotInfo.errorCode);
        }

        return true;
      }
      catch (triagens::basics::Exception const& ex) {
        res = ex.code();
      }
      catch (...) {
        res = TRI_ERROR_INTERNAL;
      }

      LOG_WARNING("could not save index drop marker in log: %s", TRI_errno_string(res));
    }

    // TODO: what to do here?
    return result;
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts attribute names to lists of pids and names
///
/// In case of an error, all allocated memory in pids and names will be
/// freed.
////////////////////////////////////////////////////////////////////////////////

static int PidNamesByAttributeNames (std::vector<std::string> const& attributes,
                                     TRI_shaper_t* shaper,
                                     std::vector<TRI_shape_pid_t>& pids,
                                     std::vector<std::string>& names,
                                     bool sorted,
                                     bool create) {
  pids.reserve(attributes.size());
  names.reserve(attributes.size());
  
  // .............................................................................
  // sorted case
  // .............................................................................

  if (sorted) {
    // combine name and pid
    typedef std::pair<std::string, TRI_shape_pid_t> PidNameType;
    std::vector<PidNameType> pidNames;
    pidNames.reserve(attributes.size()); 

    for (auto const& name : attributes) {
      TRI_shape_pid_t pid;

      if (create) {
        pid = shaper->findOrCreateAttributePathByName(shaper, name.c_str());
      }
      else {
        pid = shaper->lookupAttributePathByName(shaper, name.c_str());
      }

      if (pid == 0) {
        return TRI_set_errno(TRI_ERROR_ARANGO_ILLEGAL_NAME);
      }

      pidNames.emplace_back(std::make_pair(name, pid));
    }

    // sort according to pid
    std::sort(pidNames.begin(), pidNames.end(), [] (PidNameType const& l, PidNameType const& r) -> bool {
      return l.second < r.second;
    });

    for (auto const& it : pidNames) {
      pids.emplace_back(it.second);
      names.emplace_back(it.first);
    }
  }

  // .............................................................................
  // unsorted case
  // .............................................................................

  else {
    for (auto const& name : attributes) {
      TRI_shape_pid_t pid;
    
      if (create) {
        pid = shaper->findOrCreateAttributePathByName(shaper, name.c_str());
      }
      else {
        pid = shaper->lookupAttributePathByName(shaper, name.c_str());
      }

      if (pid == 0) {
        return TRI_set_errno(TRI_ERROR_ARANGO_ILLEGAL_NAME);
      }

      pids.emplace_back(pid);
      names.emplace_back(name);
    }
  }

  return TRI_ERROR_NO_ERROR;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    CAP CONSTRAINT
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a cap constraint to a collection
////////////////////////////////////////////////////////////////////////////////

static triagens::arango::Index* CreateCapConstraintDocumentCollection (TRI_document_collection_t* document,
                                                                       size_t count,
                                                                       int64_t size,
                                                                       TRI_idx_iid_t iid,
                                                                       bool* created) {
  if (created != nullptr) {
    *created = false;
  }

  // check if we already know a cap constraint
  auto existing = document->capConstraint();

  if (existing != nullptr) {
    if (static_cast<size_t>(existing->count()) == count &&
        existing->size() == size) {
      return static_cast<triagens::arango::Index*>(existing);
    }
      
    TRI_set_errno(TRI_ERROR_ARANGO_CAP_CONSTRAINT_ALREADY_DEFINED);
    return nullptr;
  }
  
  if (iid == 0) {
    iid = triagens::arango::Index::generateId();
  }

  // create a new index
  std::unique_ptr<triagens::arango::Index> capConstraint(new triagens::arango::CapConstraint(iid, document, count, size));
  triagens::arango::Index* idx = static_cast<triagens::arango::Index*>(capConstraint.get());

  // initialises the index with all existing documents
  int res = FillIndex(document, idx);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_set_errno(res);

    return nullptr;
  }

  // and store index
  try {
    document->addIndex(idx);
    capConstraint.release();
  }
  catch (...) {
    TRI_set_errno(res);

    return nullptr;
  }

  if (created != nullptr) {
    *created = true;
  }

  return idx;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief restores an index
////////////////////////////////////////////////////////////////////////////////

static int CapConstraintFromJson (TRI_document_collection_t* document,
                                  TRI_json_t const* definition,
                                  TRI_idx_iid_t iid,
                                  triagens::arango::Index** dst) {
  if (dst != nullptr) {
    *dst = nullptr;
  }

  TRI_json_t const* val1 = TRI_LookupObjectJson(definition, "size");
  TRI_json_t const* val2 = TRI_LookupObjectJson(definition, "byteSize");

  if (! TRI_IsNumberJson(val1) && ! TRI_IsNumberJson(val2)) {
    LOG_ERROR("ignoring cap constraint %llu, 'size' and 'byteSize' missing",
              (unsigned long long) iid);

    return TRI_set_errno(TRI_ERROR_BAD_PARAMETER);
  }

  size_t count = 0;
  if (TRI_IsNumberJson(val1) && val1->_value._number > 0.0) {
    count = static_cast<size_t>(val1->_value._number);
  }

  int64_t size = 0;
  if (TRI_IsNumberJson(val2) && 
      val2->_value._number > static_cast<double>(triagens::arango::CapConstraint::MinSize)) {
    size = static_cast<int64_t>(val2->_value._number);
  }

  if (count == 0 && size == 0) {
    LOG_ERROR("ignoring cap constraint %llu, 'size' must be at least 1, "
              "or 'byteSize' must be at least %lu",
              (unsigned long long) iid,
              (unsigned long) triagens::arango::CapConstraint::MinSize);

    return TRI_set_errno(TRI_ERROR_BAD_PARAMETER);
  }

  auto idx = CreateCapConstraintDocumentCollection(document, count, size, iid, nullptr);

  if (dst != nullptr) {
    *dst = idx;
  }

  return idx == nullptr ? TRI_errno() : TRI_ERROR_NO_ERROR;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up a cap constraint
////////////////////////////////////////////////////////////////////////////////

triagens::arango::Index* TRI_LookupCapConstraintDocumentCollection (TRI_document_collection_t* document) {
  return static_cast<triagens::arango::Index*>(document->capConstraint());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures that a cap constraint exists
////////////////////////////////////////////////////////////////////////////////

triagens::arango::Index* TRI_EnsureCapConstraintDocumentCollection (TRI_document_collection_t* document,
                                                                    TRI_idx_iid_t iid,
                                                                    size_t count,
                                                                    int64_t size,
                                                                    bool* created) {
  // .............................................................................
  // inside write-lock
  // .............................................................................

  TRI_ReadLockReadWriteLock(&document->_vocbase->_inventoryLock);

  TRI_WRITE_LOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(document);

  auto idx = CreateCapConstraintDocumentCollection(document, count, size, iid, created);

  if (idx != nullptr) {
    if (created) {
      int res = TRI_SaveIndex(document, idx, true);

      if (res != TRI_ERROR_NO_ERROR) {
        // TODO: doesn't this leak idx?
        idx = nullptr;
      }
    }
  }

  TRI_WRITE_UNLOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(document);

  // .............................................................................
  // outside write-lock
  // .............................................................................

  TRI_ReadUnlockReadWriteLock(&document->_vocbase->_inventoryLock);

  return idx;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                         GEO INDEX
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a geo index to a collection
////////////////////////////////////////////////////////////////////////////////

static triagens::arango::Index* CreateGeoIndexDocumentCollection (TRI_document_collection_t* document,
                                                                  std::string const& location,
                                                                  std::string const& latitude,
                                                                  std::string const& longitude,
                                                                  bool geoJson,
                                                                  TRI_idx_iid_t iid,
                                                                  bool* created) {
  TRI_shape_pid_t lat = 0;
  TRI_shape_pid_t lon = 0;
  TRI_shape_pid_t loc = 0;

  TRI_shaper_t* shaper = document->getShaper();  // ONLY IN INDEX, PROTECTED by RUNTIME

  if (! location.empty()) {
    loc = shaper->findOrCreateAttributePathByName(shaper, location.c_str());

    if (loc == 0) {
      TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
      return nullptr;
    }
  }

  if (! latitude.empty()) {
    lat = shaper->findOrCreateAttributePathByName(shaper, latitude.c_str());

    if (lat == 0) {
      TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
      return nullptr;
    }
  }

  if (! longitude.empty()) {
    lon = shaper->findOrCreateAttributePathByName(shaper, longitude.c_str());

    if (lon == 0) {
      TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
      return nullptr;
    }
  }

  // check, if we know the index
  triagens::arango::Index* idx = nullptr;

  if (! location.empty()) {
    idx = TRI_LookupGeoIndex1DocumentCollection(document, location, geoJson);
  }
  else if (! longitude.empty() && ! latitude.empty()) {
    idx = TRI_LookupGeoIndex2DocumentCollection(document, latitude, longitude);
  }
  else {
    TRI_set_errno(TRI_ERROR_INTERNAL);
    LOG_TRACE("expecting either 'location' or 'latitude' and 'longitude'");
    return nullptr;
  }

  if (idx != nullptr) {
    LOG_TRACE("geo-index already created for location '%s'", location.c_str());

    if (created != nullptr) {
      *created = false;
    }

    return idx;
  }
  
  if (iid == 0) {
    iid = triagens::arango::Index::generateId();
  }

  std::unique_ptr<triagens::arango::GeoIndex2> geoIndex;

  // create a new index
  if (! location.empty()) {
    geoIndex.reset(new triagens::arango::GeoIndex2(iid, document, std::vector<std::string>{ location }, std::vector<TRI_shape_pid_t>{ loc }, geoJson));

    LOG_TRACE("created geo-index for location '%s': %ld",
              location.c_str(),
              (unsigned long) loc);
  }
  else if (! longitude.empty() && ! latitude.empty()) {
    geoIndex.reset(new triagens::arango::GeoIndex2(iid, document, std::vector<std::string>{ latitude, longitude }, std::vector<TRI_shape_pid_t>{ lat, lon }));

    LOG_TRACE("created geo-index for location '%s': %ld, %ld",
              location.c_str(),
              (unsigned long) lat,
              (unsigned long) lon);
  }

  idx = static_cast<triagens::arango::GeoIndex2*>(geoIndex.get());

  if (idx == nullptr) {
    TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
    return nullptr;
  }

  // initialises the index with all existing documents
  int res = FillIndex(document, idx);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_set_errno(res);

    return nullptr;
  }

  // and store index
  try {
    document->addIndex(idx);
    geoIndex.release();
  }
  catch (...) {
    TRI_set_errno(res);

    return nullptr;
  }

  if (created != nullptr) {
    *created = true;
  }

  return idx;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief restores an index
////////////////////////////////////////////////////////////////////////////////

static int GeoIndexFromJson (TRI_document_collection_t* document,
                             TRI_json_t const* definition,
                             TRI_idx_iid_t iid,
                             triagens::arango::Index** dst) {
  if (dst != nullptr) {
    *dst = nullptr;
  }

  TRI_json_t const* type = TRI_LookupObjectJson(definition, "type");

  if (! TRI_IsStringJson(type)) {
    return TRI_ERROR_INTERNAL;
  }

  char const* typeStr = type->_value._string.data;

  // extract fields
  size_t fieldCount;
  TRI_json_t* fld = ExtractFields(definition, &fieldCount, iid);

  if (fld == nullptr) {
    return TRI_errno();
  }
  
  triagens::arango::Index* idx = nullptr;

  // list style
  if (TRI_EqualString(typeStr, "geo1")) {
    // extract geo json
    bool geoJson = false;
    TRI_json_t const* bv = TRI_LookupObjectJson(definition, "geoJson");

    if (TRI_IsBooleanJson(bv)) {
      geoJson = bv->_value._boolean;
    }

    // need just one field
    if (fieldCount == 1) {
      auto loc = static_cast<TRI_json_t const*>(TRI_AtVector(&fld->_value._objects, 0));

      idx = CreateGeoIndexDocumentCollection(document,
                                             std::string(loc->_value._string.data, loc->_value._string.length - 1),
                                             std::string(),
                                             std::string(),
                                             geoJson,
                                             iid,
                                             nullptr);

      if (dst != nullptr) {
        *dst = idx;
      }

      return idx == nullptr ? TRI_errno() : TRI_ERROR_NO_ERROR;
    }
    else {
      LOG_ERROR("ignoring %s-index %llu, 'fields' must be a list with 1 entries",
                typeStr, (unsigned long long) iid);

      return TRI_set_errno(TRI_ERROR_BAD_PARAMETER);
    }
  }

  // attribute style
  else if (TRI_EqualString(typeStr, "geo2")) {
    if (fieldCount == 2) {
      auto lat = static_cast<TRI_json_t const*>(TRI_AtVector(&fld->_value._objects, 0));
      auto lon = static_cast<TRI_json_t const*>(TRI_AtVector(&fld->_value._objects, 1));

      idx = CreateGeoIndexDocumentCollection(document,
                                             std::string(),
                                             std::string(lat->_value._string.data, lat->_value._string.length - 1),
                                             std::string(lon->_value._string.data, lon->_value._string.length - 1),
                                             false,
                                             iid,
                                             nullptr);

      if (dst != nullptr) {
        *dst = idx;
      }

      return idx == nullptr ? TRI_errno() : TRI_ERROR_NO_ERROR;
    }
    else {
      LOG_ERROR("ignoring %s-index %llu, 'fields' must be a list with 2 entries",
                typeStr, (unsigned long long) iid);

      return TRI_set_errno(TRI_ERROR_BAD_PARAMETER);
    }
  }

  else {
    TRI_ASSERT(false);
  }

  return TRI_ERROR_NO_ERROR; // shut the vc++ up
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief finds a geo index, list style
////////////////////////////////////////////////////////////////////////////////

triagens::arango::Index* TRI_LookupGeoIndex1DocumentCollection (TRI_document_collection_t* document,
                                                                std::string const& location,
                                                                bool geoJson) {
  TRI_shaper_t* shaper = document->getShaper();  // ONLY IN INDEX, PROTECTED by RUNTIME

  TRI_shape_pid_t loc = shaper->lookupAttributePathByName(shaper, location.c_str());

  if (loc == 0) {
    return nullptr;
  }

  for (auto const& idx : document->allIndexes()) {
    if (idx->type() == triagens::arango::Index::TRI_IDX_TYPE_GEO1_INDEX) {
      auto geoIndex = static_cast<triagens::arango::GeoIndex2*>(idx);

      if (geoIndex->isSame(loc, geoJson)) {
        return idx;
      }
    }
  }
  
  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief finds a geo index, attribute style
////////////////////////////////////////////////////////////////////////////////

triagens::arango::Index* TRI_LookupGeoIndex2DocumentCollection (TRI_document_collection_t* document,
                                                                std::string const& latitude,
                                                                std::string const& longitude) {
  TRI_shaper_t* shaper = document->getShaper();  // ONLY IN INDEX, PROTECTED by RUNTIME

  TRI_shape_pid_t lat = shaper->lookupAttributePathByName(shaper, latitude.c_str());
  TRI_shape_pid_t lon = shaper->lookupAttributePathByName(shaper, longitude.c_str());

  if (lat == 0 || lon == 0) {
    return nullptr;
  }

  for (auto const& idx : document->allIndexes()) {
    if (idx->type() == triagens::arango::Index::TRI_IDX_TYPE_GEO2_INDEX) {
      auto geoIndex = static_cast<triagens::arango::GeoIndex2*>(idx);

      if (geoIndex->isSame(lat, lon)) {
        return idx;
      }
    }
  }

  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures that a geo index exists, list style
////////////////////////////////////////////////////////////////////////////////

triagens::arango::Index* TRI_EnsureGeoIndex1DocumentCollection (TRI_document_collection_t* document,
                                                                TRI_idx_iid_t iid,
                                                                std::string const& location,
                                                                bool geoJson,
                                                                bool* created) {
  TRI_ReadLockReadWriteLock(&document->_vocbase->_inventoryLock);

  // .............................................................................
  // inside write-lock
  // .............................................................................

  TRI_WRITE_LOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(document);

  auto idx = CreateGeoIndexDocumentCollection(document, location, std::string(), std::string(), geoJson, iid, created);

  if (idx != nullptr) {
    if (created) {
      int res = TRI_SaveIndex(document, idx, true);

      if (res != TRI_ERROR_NO_ERROR) {
        idx = nullptr;
      }
    }
  }

  TRI_WRITE_UNLOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(document);

  // .............................................................................
  // outside write-lock
  // .............................................................................

  TRI_ReadUnlockReadWriteLock(&document->_vocbase->_inventoryLock);

  return idx;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures that a geo index exists, attribute style
////////////////////////////////////////////////////////////////////////////////

triagens::arango::Index* TRI_EnsureGeoIndex2DocumentCollection (TRI_document_collection_t* document,
                                                                TRI_idx_iid_t iid,
                                                                std::string const& latitude,
                                                                std::string const& longitude,
                                                                bool* created) {
  TRI_ReadLockReadWriteLock(&document->_vocbase->_inventoryLock);

  // .............................................................................
  // inside write-lock
  // .............................................................................

  TRI_WRITE_LOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(document);

  auto idx = CreateGeoIndexDocumentCollection(document, std::string(), latitude, longitude, false, iid, created);

  if (idx != nullptr) {
    if (created) {
      int res = TRI_SaveIndex(document, idx, true);

      if (res != TRI_ERROR_NO_ERROR) {
        idx = nullptr;
      }
    }
  }

  TRI_WRITE_UNLOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(document);

  // .............................................................................
  // outside write-lock
  // .............................................................................

  TRI_ReadUnlockReadWriteLock(&document->_vocbase->_inventoryLock);

  return idx;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                        HASH INDEX
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a hash index to the collection
////////////////////////////////////////////////////////////////////////////////

static triagens::arango::Index* CreateHashIndexDocumentCollection (TRI_document_collection_t* document,
                                                                   std::vector<std::string> const& attributes,
                                                                   TRI_idx_iid_t iid,
                                                                   bool sparse,
                                                                   bool unique,
                                                                   bool* created) {
  std::vector<TRI_shape_pid_t> paths;
  std::vector<std::string> fields;

  // determine the sorted shape ids for the attributes
  int res = PidNamesByAttributeNames(attributes,
                                     document->getShaper(),  // ONLY IN INDEX, PROTECTED by RUNTIME
                                     paths,
                                     fields,
                                     true,
                                     true);

  if (res != TRI_ERROR_NO_ERROR) {
    if (created != nullptr) {
      *created = false;
    }

    return nullptr;
  }

  // ...........................................................................
  // Attempt to find an existing index which matches the attributes above.
  // If a suitable index is found, return that one otherwise we need to create
  // a new one.
  // ...........................................................................

  int sparsity = sparse ? 1 : 0;
  auto idx = LookupPathIndexDocumentCollection(document, fields, triagens::arango::Index::TRI_IDX_TYPE_HASH_INDEX, sparsity, unique, false);

  if (idx != nullptr) {
    LOG_TRACE("hash-index already created");

    if (created != nullptr) {
      *created = false;
    }

    return idx;
  }

  if (iid == 0) {
    iid = triagens::arango::Index::generateId();
  }


  // create the hash index. we'll provide it with the current number of documents
  // in the collection so the index can do a sensible memory preallocation
  std::unique_ptr<triagens::arango::HashIndex> hashIndex(new triagens::arango::HashIndex(iid, document, fields, paths, unique, sparse));
  idx = static_cast<triagens::arango::Index*>(hashIndex.get());

  // initialises the index with all existing documents
  res = FillIndex(document, idx);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_set_errno(res);

    return nullptr;
  }

  // store index and return
  try {
    document->addIndex(idx);
    hashIndex.release();
  }
  catch (...) {
    TRI_set_errno(res);

    return nullptr;
  }

  if (created != nullptr) {
    *created = true;
  }

  return idx;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief restores an index
////////////////////////////////////////////////////////////////////////////////

static int HashIndexFromJson (TRI_document_collection_t* document,
                              TRI_json_t const* definition,
                              TRI_idx_iid_t iid,
                              triagens::arango::Index** dst) {
  return PathBasedIndexFromJson(document, definition, iid, CreateHashIndexDocumentCollection, dst);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief finds a hash index (unique or non-unique)
/// the index lock must be held when calling this function
////////////////////////////////////////////////////////////////////////////////

triagens::arango::Index* TRI_LookupHashIndexDocumentCollection (TRI_document_collection_t* document,
                                                                std::vector<std::string> const& attributes,
                                                                int sparsity,
                                                                bool unique) {
  std::vector<TRI_shape_pid_t> paths;
  std::vector<std::string> fields;

  // determine the sorted shape ids for the attributes
  int res = PidNamesByAttributeNames(attributes,
                                     document->getShaper(),  // ONLY IN INDEX, PROTECTED by RUNTIME
                                     paths,
                                     fields,
                                     true,
                                     false);

  if (res != TRI_ERROR_NO_ERROR) {
    return nullptr;
  }

  return LookupPathIndexDocumentCollection(document, fields, triagens::arango::Index::TRI_IDX_TYPE_HASH_INDEX, sparsity, unique, true);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures that a hash index exists
////////////////////////////////////////////////////////////////////////////////

triagens::arango::Index* TRI_EnsureHashIndexDocumentCollection (TRI_document_collection_t* document,
                                                                TRI_idx_iid_t iid,
                                                                std::vector<std::string> const& attributes,
                                                                bool sparse,
                                                                bool unique,
                                                                bool* created) {
  TRI_ReadLockReadWriteLock(&document->_vocbase->_inventoryLock);

  // .............................................................................
  // inside write-lock
  // .............................................................................

  TRI_WRITE_LOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(document);

  // given the list of attributes (as strings)
  auto idx = CreateHashIndexDocumentCollection(document, attributes, iid, sparse, unique, created);

  if (idx != nullptr) {
    if (created) {
      int res = TRI_SaveIndex(document, idx, true);

      if (res != TRI_ERROR_NO_ERROR) {
        idx = nullptr;
      }
    }
  }

  TRI_WRITE_UNLOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(document);

  // .............................................................................
  // outside write-lock
  // .............................................................................

  TRI_ReadUnlockReadWriteLock(&document->_vocbase->_inventoryLock);

  return idx;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    SKIPLIST INDEX
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a skiplist index to the collection
////////////////////////////////////////////////////////////////////////////////

static triagens::arango::Index* CreateSkiplistIndexDocumentCollection (TRI_document_collection_t* document,
                                                                       std::vector<std::string> const& attributes,
                                                                       TRI_idx_iid_t iid,
                                                                       bool sparse,
                                                                       bool unique,
                                                                       bool* created) {
  std::vector<TRI_shape_pid_t> paths;
  std::vector<std::string> fields;
  
  int res = PidNamesByAttributeNames(attributes,
                                     document->getShaper(),  // ONLY IN INDEX, PROTECTED by RUNTIME
                                     paths,
                                     fields,
                                     false,
                                     true);

  if (res != TRI_ERROR_NO_ERROR) {
    if (created != nullptr) {
      *created = false;
    }

    return nullptr;
  }

  // ...........................................................................
  // Attempt to find an existing index which matches the attributes above.
  // If a suitable index is found, return that one otherwise we need to create
  // a new one.
  // ...........................................................................

  int sparsity = sparse ? 1 : 0;
  auto idx = LookupPathIndexDocumentCollection(document, fields, triagens::arango::Index::TRI_IDX_TYPE_SKIPLIST_INDEX, sparsity, unique, false);

  if (idx != nullptr) {
    LOG_TRACE("skiplist-index already created");

    if (created != nullptr) {
      *created = false;
    }

    return idx;
  }
  
  if (iid == 0) {
    iid = triagens::arango::Index::generateId();
  }

  // Create the skiplist index
  std::unique_ptr<triagens::arango::SkiplistIndex2> skiplistIndex(new triagens::arango::SkiplistIndex2(iid, document, fields, paths, unique, sparse));
  idx = static_cast<triagens::arango::Index*>(skiplistIndex.get());

  // initialises the index with all existing documents
  res = FillIndex(document, idx);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_set_errno(res);

    return nullptr;
  }

  // store index and return
  try {
    document->addIndex(idx);
    skiplistIndex.release();
  }
  catch (...) {
    TRI_set_errno(res);

    return nullptr;
  }

  if (created != nullptr) {
    *created = true;
  }

  return idx;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief restores an index
////////////////////////////////////////////////////////////////////////////////

static int SkiplistIndexFromJson (TRI_document_collection_t* document,
                                  TRI_json_t const* definition,
                                  TRI_idx_iid_t iid,
                                  triagens::arango::Index** dst) {
  return PathBasedIndexFromJson(document, definition, iid, CreateSkiplistIndexDocumentCollection, dst);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief finds a skiplist index (unique or non-unique)
/// the index lock must be held when calling this function
////////////////////////////////////////////////////////////////////////////////

triagens::arango::Index* TRI_LookupSkiplistIndexDocumentCollection (TRI_document_collection_t* document,
                                                                    std::vector<std::string> const& attributes,
                                                                    int sparsity,
                                                                    bool unique) {
  std::vector<TRI_shape_pid_t> paths;
  std::vector<std::string> fields;

  // determine the unsorted shape ids for the attributes
  int res = PidNamesByAttributeNames(attributes,
                                     document->getShaper(),  // ONLY IN INDEX, PROTECTED by RUNTIME
                                     paths,
                                     fields,
                                     false,
                                     false);

  if (res != TRI_ERROR_NO_ERROR) {
    return nullptr;
  }

  return LookupPathIndexDocumentCollection(document, fields, triagens::arango::Index::TRI_IDX_TYPE_SKIPLIST_INDEX, sparsity, unique, true);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures that a skiplist index exists
////////////////////////////////////////////////////////////////////////////////

triagens::arango::Index* TRI_EnsureSkiplistIndexDocumentCollection (TRI_document_collection_t* document,
                                                                    TRI_idx_iid_t iid,
                                                                    std::vector<std::string> const& attributes,
                                                                    bool sparse,
                                                                    bool unique,
                                                                    bool* created) {
  TRI_ReadLockReadWriteLock(&document->_vocbase->_inventoryLock);

  // .............................................................................
  // inside write-lock the collection
  // .............................................................................

  TRI_WRITE_LOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(document);

  auto idx = CreateSkiplistIndexDocumentCollection(document, attributes, iid, sparse, unique, created);

  if (idx != nullptr) {
    if (created) {
      int res = TRI_SaveIndex(document, idx, true);

      if (res != TRI_ERROR_NO_ERROR) {
        idx = nullptr;
      }
    }
  }

  TRI_WRITE_UNLOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(document);

  // .............................................................................
  // outside write-lock
  // .............................................................................

  TRI_ReadUnlockReadWriteLock(&document->_vocbase->_inventoryLock);

  return idx;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    FULLTEXT INDEX
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

static triagens::arango::Index* LookupFulltextIndexDocumentCollection (TRI_document_collection_t* document,
                                                                       std::string const& attribute,
                                                                       int minWordLength) {
  for (auto const& idx : document->allIndexes()) {
    if (idx->type() == triagens::arango::Index::TRI_IDX_TYPE_FULLTEXT_INDEX) {
      auto fulltextIndex = static_cast<triagens::arango::FulltextIndex*>(idx);
      
      if (fulltextIndex->isSame(attribute, minWordLength)) {
        return idx;
      }
    }
  }

  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a fulltext index to the collection
////////////////////////////////////////////////////////////////////////////////

static triagens::arango::Index* CreateFulltextIndexDocumentCollection (TRI_document_collection_t* document,
                                                                       std::string const& attribute,
                                                                       int minWordLength,
                                                                       TRI_idx_iid_t iid,
                                                                       bool* created) {
  // ...........................................................................
  // Attempt to find an existing index with the same attribute
  // If a suitable index is found, return that one otherwise we need to create
  // a new one.
  // ...........................................................................

  auto idx = LookupFulltextIndexDocumentCollection(document, attribute, minWordLength);

  if (idx != nullptr) {
    LOG_TRACE("fulltext-index already created");

    if (created != nullptr) {
      *created = false;
    }
    return idx;
  }
  
  if (iid == 0) {
    iid = triagens::arango::Index::generateId();
  }

  // Create the fulltext index
  std::unique_ptr<triagens::arango::FulltextIndex> fulltextIndex(new triagens::arango::FulltextIndex(iid, document, attribute, minWordLength));
  idx = static_cast<triagens::arango::Index*>(fulltextIndex.get());

  // initialises the index with all existing documents
  int res = FillIndex(document, idx);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_set_errno(res);

    return nullptr;
  }

  // store index and return
  try {
    document->addIndex(idx);
    fulltextIndex.release();
  }
  catch (...) {
    TRI_set_errno(res);

    return nullptr;
  }

  if (created != nullptr) {
    *created = true;
  }

  return idx;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief restores an index
////////////////////////////////////////////////////////////////////////////////

static int FulltextIndexFromJson (TRI_document_collection_t* document,
                                  TRI_json_t const* definition,
                                  TRI_idx_iid_t iid,
                                  triagens::arango::Index** dst) {
  if (dst != nullptr) {
    *dst = nullptr;
  }

  // extract fields
  size_t fieldCount;
  TRI_json_t* fld = ExtractFields(definition, &fieldCount, iid);

  if (fld == nullptr) {
    return TRI_errno();
  }

  // extract the list of fields
  if (fieldCount != 1) {
    LOG_ERROR("ignoring index %llu, has an invalid number of attributes", (unsigned long long) iid);

    return TRI_set_errno(TRI_ERROR_BAD_PARAMETER);
  }

  auto value = static_cast<TRI_json_t const*>(TRI_AtVector(&fld->_value._objects, 0));

  if (! TRI_IsStringJson(value)) {
    return TRI_set_errno(TRI_ERROR_BAD_PARAMETER);
  }

  std::string const attribute(value->_value._string.data, value->_value._string.length - 1);

  // 2013-01-17: deactivated substring indexing
  // indexSubstrings = TRI_LookupObjectJson(definition, "indexSubstrings");

  int minWordLengthValue = TRI_FULLTEXT_MIN_WORD_LENGTH_DEFAULT;
  TRI_json_t const* minWordLength = TRI_LookupObjectJson(definition, "minLength");

  if (minWordLength != nullptr && minWordLength->_type == TRI_JSON_NUMBER) {
    minWordLengthValue = (int) minWordLength->_value._number;
  }

  // create the index
  auto idx = LookupFulltextIndexDocumentCollection(document, attribute, minWordLengthValue);

  if (idx == nullptr) {
    bool created;
    idx = CreateFulltextIndexDocumentCollection(document, attribute, minWordLengthValue, iid, &created);
  }

  if (dst != nullptr) {
    *dst = idx;
  }

  if (idx == nullptr) {
    LOG_ERROR("cannot create fulltext index %llu", (unsigned long long) iid);
    return TRI_errno();
  }

  return TRI_ERROR_NO_ERROR;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief finds a fulltext index (unique or non-unique)
/// the index lock must be held when calling this function
////////////////////////////////////////////////////////////////////////////////

triagens::arango::Index* TRI_LookupFulltextIndexDocumentCollection (TRI_document_collection_t* document,
                                                                    std::string const& attribute,
                                                                    int minWordLength) {
  return LookupFulltextIndexDocumentCollection(document, attribute, minWordLength);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures that a fulltext index exists
////////////////////////////////////////////////////////////////////////////////

triagens::arango::Index* TRI_EnsureFulltextIndexDocumentCollection (TRI_document_collection_t* document,
                                                                    TRI_idx_iid_t iid,
                                                                    std::string const& attribute,
                                                                    int minWordLength,
                                                                    bool* created) {
  TRI_ReadLockReadWriteLock(&document->_vocbase->_inventoryLock);

  // .............................................................................
  // inside write-lock the collection
  // .............................................................................

  TRI_WRITE_LOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(document);

  auto idx = CreateFulltextIndexDocumentCollection(document, attribute, minWordLength, iid, created);

  if (idx != nullptr) {
    if (created) {
      int res = TRI_SaveIndex(document, idx, true);

      if (res != TRI_ERROR_NO_ERROR) {
        idx = nullptr;
      }
    }
  }

  TRI_WRITE_UNLOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(document);

  // .............................................................................
  // outside write-lock
  // .............................................................................

  TRI_ReadUnlockReadWriteLock(&document->_vocbase->_inventoryLock);

  return idx;
}

// -----------------------------------------------------------------------------
// --SECTION--                                           SELECT BY EXAMPLE QUERY
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a select-by-example query
////////////////////////////////////////////////////////////////////////////////

std::vector<TRI_doc_mptr_copy_t> TRI_SelectByExample (
                          TRI_transaction_collection_t* trxCollection,
                          ExampleMatcher& matcher) {

  TRI_document_collection_t* document = trxCollection->_collection->_collection;


  // use filtered to hold copies of the master pointer
  std::vector<TRI_doc_mptr_copy_t> filtered;

  // do a full scan
  auto primaryIndex = document->primaryIndex()->internals();
  TRI_doc_mptr_t** ptr = (TRI_doc_mptr_t**) primaryIndex->_table;
  TRI_doc_mptr_t** end = (TRI_doc_mptr_t**) ptr + primaryIndex->_nrAlloc;

  for (;  ptr < end;  ++ptr) {
    if (matcher.matches(*ptr)) {
      filtered.emplace_back(**ptr);
    }
  }
  return filtered;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a select-by-example query
////////////////////////////////////////////////////////////////////////////////

CountingAggregation* TRI_AggregateBySingleAttribute (
                          TRI_transaction_collection_t* trxCollection,
                          TRI_shape_pid_t pid ) {

  TRI_document_collection_t* document = trxCollection->_collection->_collection;

  CountingAggregation* agg = new CountingAggregation();

  TRI_shaper_t* shaper = document->getShaper();

  // do a full scan
  auto primaryIndex = document->primaryIndex()->internals();
  TRI_doc_mptr_t** ptr = (TRI_doc_mptr_t**) primaryIndex->_table;
  TRI_doc_mptr_t** end = (TRI_doc_mptr_t**) ptr + primaryIndex->_nrAlloc;

  for (;  ptr < end;  ++ptr) {
    if (*ptr != nullptr) {
      TRI_doc_mptr_t* m = *ptr;
      TRI_shape_sid_t sid;
      TRI_EXTRACT_SHAPE_IDENTIFIER_MARKER(sid, m->getDataPtr());
      TRI_shape_access_t const* accessor = TRI_FindAccessorVocShaper(shaper, 
                                                                     sid, pid);
      TRI_shaped_json_t shapedJson;
      TRI_EXTRACT_SHAPED_JSON_MARKER(shapedJson, m->getDataPtr());
      TRI_shaped_json_t resultJson;
      TRI_ExecuteShapeAccessor(accessor, &shapedJson, &resultJson);
      auto it = agg->find(resultJson);
      if (it == agg->end()) {
        agg->insert(std::make_pair(resultJson, 1));
      }
      else {
        it->second++;
      }
    }
  }
  return agg;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes a document given by a master pointer
////////////////////////////////////////////////////////////////////////////////

int TRI_DeleteDocumentDocumentCollection (TRI_transaction_collection_t* trxCollection,
                                          TRI_doc_update_policy_t const* policy,
                                          TRI_doc_mptr_t* doc) {
  return TRI_RemoveShapedJsonDocumentCollection(trxCollection,
                                                (const TRI_voc_key_t) TRI_EXTRACT_MARKER_KEY(doc),
                                                0,
                                                nullptr,
                                                policy,
                                                false,
                                                false);  // PROTECTED by trx in trxCollection
}

////////////////////////////////////////////////////////////////////////////////
/// @brief rotate the current journal of the collection
/// use this for testing only
////////////////////////////////////////////////////////////////////////////////

int TRI_RotateJournalDocumentCollection (TRI_document_collection_t* document) {
  int res = TRI_ERROR_ARANGO_NO_JOURNAL;

  TRI_LOCK_JOURNAL_ENTRIES_DOC_COLLECTION(document);

  if (document->_state == TRI_COL_STATE_WRITE) {
    size_t const n = document->_journals._length;

    if (n > 0) {
      TRI_ASSERT(document->_journals._buffer[0] != nullptr);
      TRI_CloseDatafileDocumentCollection(document, 0, false);

      res = TRI_ERROR_NO_ERROR;
    }
  }

  TRI_UNLOCK_JOURNAL_ENTRIES_DOC_COLLECTION(document);

  return res;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                      CRUD methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief reads an element from the document collection
////////////////////////////////////////////////////////////////////////////////

int TRI_ReadShapedJsonDocumentCollection (TRI_transaction_collection_t* trxCollection,
                                          const TRI_voc_key_t key,
                                          TRI_doc_mptr_copy_t* mptr,
                                          bool lock) {
  TRI_ASSERT(mptr != nullptr);
  mptr->setDataPtr(nullptr);  // PROTECTED by trx in trxCollection

  {
    TRI_IF_FAILURE("ReadDocumentNoLock") {
      // test what happens if no lock can be acquired
      return TRI_ERROR_DEBUG;
    }

    TRI_IF_FAILURE("ReadDocumentNoLockExcept") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }


    TRI_document_collection_t* document = trxCollection->_collection->_collection;
    triagens::arango::CollectionReadLocker collectionLocker(document, lock);

    TRI_doc_mptr_t* header;
    int res = LookupDocument(document, key, nullptr, header);

    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }

    // we found a document, now copy it over
    *mptr = *header;
  }

  TRI_ASSERT(mptr->getDataPtr() != nullptr);  // PROTECTED by trx in trxCollection
  TRI_ASSERT(mptr->_rid > 0);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes a shaped-json document (or edge)
////////////////////////////////////////////////////////////////////////////////

int TRI_RemoveShapedJsonDocumentCollection (TRI_transaction_collection_t* trxCollection,
                                            TRI_voc_key_t key,
                                            TRI_voc_rid_t rid,
                                            triagens::wal::Marker* marker,
                                            TRI_doc_update_policy_t const* policy,
                                            bool lock,
                                            bool forceSync) {
  bool const freeMarker = (marker == nullptr);
  rid = GetRevisionId(rid);

  TRI_ASSERT(key != nullptr);

  TRI_document_collection_t* document = trxCollection->_collection->_collection;

  TRI_IF_FAILURE("RemoveDocumentNoMarker") {
    // test what happens when no marker can be created
    return TRI_ERROR_DEBUG;
  }

  TRI_IF_FAILURE("RemoveDocumentNoMarkerExcept") {
    // test what happens if no marker can be created
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  if (marker == nullptr) {
    marker = new triagens::wal::RemoveMarker(document->_vocbase->_id,
                                             document->_info._cid,
                                             rid,
                                             TRI_MarkerIdTransaction(trxCollection->_transaction),
                                             std::string(key));

  }

  TRI_ASSERT(marker != nullptr);

  TRI_doc_mptr_t* header;
  int res;
  TRI_voc_tick_t markerTick = 0;
  {
    TRI_IF_FAILURE("RemoveDocumentNoLock") {
      // test what happens if no lock can be acquired
      if (freeMarker) {
        delete marker;
      }
      return TRI_ERROR_DEBUG;
    }

    triagens::arango::CollectionWriteLocker collectionLocker(document, lock);

    triagens::wal::DocumentOperation operation(marker, freeMarker, trxCollection, TRI_VOC_DOCUMENT_OPERATION_REMOVE, rid);

    res = LookupDocument(document, key, policy, header);

    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }

    // we found a document to remove
    TRI_ASSERT(header != nullptr);
    operation.header = header;
    operation.init();

    // delete from indexes
    res = DeleteSecondaryIndexes(document, header, false);

    if (res != TRI_ERROR_NO_ERROR) {
      InsertSecondaryIndexes(document, header, true);
      return res;
    }

    res = DeletePrimaryIndex(document, header, false);

    if (res != TRI_ERROR_NO_ERROR) {
      InsertSecondaryIndexes(document, header, true);
      return res;
    }

    operation.indexed();

    document->_headersPtr->unlink(header);  // PROTECTED by trx in trxCollection
    document->_numberDocuments--;

    TRI_IF_FAILURE("RemoveDocumentNoOperation") {
      return TRI_ERROR_DEBUG;
    }

    TRI_IF_FAILURE("RemoveDocumentNoOperationExcept") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }

    res = TRI_AddOperationTransaction(operation, forceSync);
    
    if (res != TRI_ERROR_NO_ERROR) {
      operation.revert();
    }
    else if (forceSync) {
      markerTick = operation.tick;
    }
  }

  if (markerTick > 0) {
    // need to wait for tick, outside the lock
    triagens::wal::LogfileManager::instance()->slots()->waitForTick(markerTick);
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief insert a shaped-json document (or edge)
/// note: key might be NULL. in this case, a key is auto-generated
////////////////////////////////////////////////////////////////////////////////

int TRI_InsertShapedJsonDocumentCollection (TRI_transaction_collection_t* trxCollection,
                                            const TRI_voc_key_t key,
                                            TRI_voc_rid_t rid,
                                            triagens::wal::Marker* marker,
                                            TRI_doc_mptr_copy_t* mptr,
                                            TRI_shaped_json_t const* shaped,
                                            TRI_document_edge_t const* edge,
                                            bool lock,
                                            bool forceSync,
                                            bool isRestore) {
  
  bool const freeMarker = (marker == nullptr);

  TRI_ASSERT(mptr != nullptr);
  mptr->setDataPtr(nullptr);  // PROTECTED by trx in trxCollection

  rid = GetRevisionId(rid);
  TRI_voc_tick_t tick = static_cast<TRI_voc_tick_t>(rid);

  TRI_document_collection_t* document = trxCollection->_collection->_collection;
  //TRI_ASSERT_EXPENSIVE(lock || TRI_IsLockedCollectionTransaction(trxCollection, TRI_TRANSACTION_WRITE, 0));

  std::string keyString;

  if (key == nullptr) {
    // no key specified, now generate a new one
    keyString.assign(document->_keyGenerator->generate(tick));

    if (keyString.empty()) {
      return TRI_ERROR_ARANGO_OUT_OF_KEYS;
    }
  }
  else {
    // key was specified, now validate it
    int res = document->_keyGenerator->validate(key, isRestore);

    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }

    keyString = key;
  }

  uint64_t const hash = document->primaryIndex()->calculateHash(keyString.c_str(), keyString.size());

  int res = TRI_ERROR_NO_ERROR;

  if (marker == nullptr) {
    res = CreateMarkerNoLegend(marker, document, rid, trxCollection, keyString, shaped, edge);
  
    if (res != TRI_ERROR_NO_ERROR) {
      if (marker != nullptr) {
        // avoid memleak
        delete marker;
      }

      return res;
    }
  }

  TRI_ASSERT(marker != nullptr);

  TRI_voc_tick_t markerTick = 0;
  // now insert into indexes
  {
    TRI_IF_FAILURE("InsertDocumentNoLock") {
      // test what happens if no lock can be acquired
      
      if (freeMarker) {
        delete marker;
      }

      return TRI_ERROR_DEBUG;
    }

    triagens::arango::CollectionWriteLocker collectionLocker(document, lock);

    triagens::wal::DocumentOperation operation(marker, freeMarker, trxCollection, TRI_VOC_DOCUMENT_OPERATION_INSERT, rid);

    TRI_IF_FAILURE("InsertDocumentNoHeader") {
      // test what happens if no header can be acquired
      return TRI_ERROR_DEBUG;
    }

    TRI_IF_FAILURE("InsertDocumentNoHeaderExcept") {
      // test what happens if no header can be acquired
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }

    // create a new header
    TRI_doc_mptr_t* header = operation.header = document->_headersPtr->request(marker->size());  // PROTECTED by trx in trxCollection

    if (header == nullptr) {
      // out of memory. no harm done here. just return the error
      return TRI_ERROR_OUT_OF_MEMORY;
    }

    // update the header we got
    void* mem = operation.marker->mem();
    header->_rid  = rid;
    header->setDataPtr(mem);  // PROTECTED by trx in trxCollection
    header->_hash = hash;

    // insert into indexes
    res = InsertDocument(trxCollection, header, operation, mptr, forceSync);

    if (res != TRI_ERROR_NO_ERROR) {
      operation.revert();
    }
    else {
      TRI_ASSERT(mptr->getDataPtr() != nullptr);  // PROTECTED by trx in trxCollection

      if (forceSync) {
        markerTick = operation.tick;
      }
    }
  }

  if (markerTick > 0) {
    // need to wait for tick, outside the lock
    triagens::wal::LogfileManager::instance()->slots()->waitForTick(markerTick);
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief updates a document in the collection from shaped json
////////////////////////////////////////////////////////////////////////////////

int TRI_UpdateShapedJsonDocumentCollection (TRI_transaction_collection_t* trxCollection,
                                            TRI_voc_key_t key,
                                            TRI_voc_rid_t rid,
                                            triagens::wal::Marker* marker,
                                            TRI_doc_mptr_copy_t* mptr,
                                            TRI_shaped_json_t const* shaped,
                                            TRI_doc_update_policy_t const* policy,
                                            bool lock,
                                            bool forceSync) {
  bool const freeMarker = (marker == nullptr);

  rid = GetRevisionId(rid);

  TRI_ASSERT(key != nullptr);

  // initialise the result
  TRI_ASSERT(mptr != nullptr);
  mptr->setDataPtr(nullptr);  // PROTECTED by trx in trxCollection

  TRI_document_collection_t* document = trxCollection->_collection->_collection;
  //TRI_ASSERT_EXPENSIVE(lock || TRI_IsLockedCollectionTransaction(trxCollection, TRI_TRANSACTION_WRITE, 0));

  int res = TRI_ERROR_NO_ERROR;
  TRI_voc_tick_t markerTick = 0;
  {
    TRI_IF_FAILURE("UpdateDocumentNoLock") {
      return TRI_ERROR_DEBUG;
    }

    triagens::arango::CollectionWriteLocker collectionLocker(document, lock);

    // get the header pointer of the previous revision
    TRI_doc_mptr_t* oldHeader;
    res = LookupDocument(document, key, policy, oldHeader);

    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }

    TRI_IF_FAILURE("UpdateDocumentNoMarker") {
      // test what happens when no marker can be created
      return TRI_ERROR_DEBUG;
    }

    TRI_IF_FAILURE("UpdateDocumentNoMarkerExcept") {
      // test what happens when no marker can be created
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }

    if (marker == nullptr) {
      TRI_IF_FAILURE("UpdateDocumentNoLegend") {
        // test what happens when no legend can be created
        return TRI_ERROR_DEBUG;
      }

      TRI_IF_FAILURE("UpdateDocumentNoLegendExcept") {
        // test what happens when no legend can be created
        THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
      }

      TRI_df_marker_t const* original = static_cast<TRI_df_marker_t const*>(oldHeader->getDataPtr());  // PROTECTED by trx in trxCollection

      res = CloneMarkerNoLegend(marker, original, document, rid, trxCollection, shaped);

      if (res != TRI_ERROR_NO_ERROR) {
        if (marker != nullptr) {
          // avoid memleak
          delete marker;
        }
        return res;
      }
    }

    TRI_ASSERT(marker != nullptr);

    triagens::wal::DocumentOperation operation(marker, freeMarker, trxCollection, TRI_VOC_DOCUMENT_OPERATION_UPDATE, rid);
    operation.header = oldHeader;
    operation.init();

    res = UpdateDocument(trxCollection, oldHeader, operation, mptr, forceSync);
    
    if (res != TRI_ERROR_NO_ERROR) {
      operation.revert();
    }
    else if (forceSync) {
      markerTick = operation.tick;
    }
  }

  if (res == TRI_ERROR_NO_ERROR) {
    TRI_ASSERT(mptr->getDataPtr() != nullptr);  // PROTECTED by trx in trxCollection
    TRI_ASSERT(mptr->_rid > 0);
  }

  if (markerTick > 0) {
    // need to wait for tick, outside the lock
    triagens::wal::LogfileManager::instance()->slots()->waitForTick(markerTick);
  }

  return res;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:

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

#include "Basics/conversions.h"
#include "Basics/files.h"
#include "Basics/logging.h"
#include "Basics/tri-strings.h"
#include "CapConstraint/cap-constraint.h"
#include "FulltextIndex/fulltext-index.h"
#include "GeoIndex/geo-index.h"
#include "HashIndex/hash-index.h"
#include "ShapedJson/shape-accessor.h"
#include "Utils/transactions.h"
#include "Utils/CollectionReadLocker.h"
#include "Utils/CollectionWriteLocker.h"
#include "Utils/Exception.h"
#include "VocBase/edge-collection.h"
#include "VocBase/index.h"
#include "VocBase/key-generator.h"
#include "VocBase/primary-index.h"
#include "VocBase/server.h"
#include "VocBase/update-policy.h"
#include "VocBase/voc-shaper.h"
#include "VocBase/barrier.h"
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
    _keyGenerator(nullptr),
    _uncollectedLogfileEntries(0) {

  _tickMax = 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy a document collection
////////////////////////////////////////////////////////////////////////////////

TRI_document_collection_t::~TRI_document_collection_t () {
  if (_keyGenerator != nullptr) {
    delete _keyGenerator;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return a pointer to the shaper
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_MAINTAINER_MODE
TRI_shaper_t* TRI_document_collection_t::getShaper () const {
  if (! TRI_ContainsBarrierList(&_barrierList, TRI_BARRIER_ELEMENT)) {
    TransactionBase::assertSomeTrxInScope();
  }
  return _shaper;
}
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief add a WAL operation for a transaction collection
////////////////////////////////////////////////////////////////////////////////

int TRI_AddOperationTransaction (triagens::wal::DocumentOperation&, bool);

// -----------------------------------------------------------------------------
// --SECTION--                                              forward declarations
// -----------------------------------------------------------------------------

static int FillIndex (TRI_document_collection_t*,
                      TRI_index_t*);

static int CapConstraintFromJson (TRI_document_collection_t*,
                                  TRI_json_t const*,
                                  TRI_idx_iid_t,
                                  TRI_index_t**);

static int GeoIndexFromJson (TRI_document_collection_t*,
                             TRI_json_t const*,
                             TRI_idx_iid_t,
                             TRI_index_t**);

static int HashIndexFromJson (TRI_document_collection_t*,
                              TRI_json_t const*,
                              TRI_idx_iid_t,
                              TRI_index_t**);

static int SkiplistIndexFromJson (TRI_document_collection_t*,
                                  TRI_json_t const*,
                                  TRI_idx_iid_t,
                                  TRI_index_t**);

static int FulltextIndexFromJson (TRI_document_collection_t*,
                                  TRI_json_t const*,
                                  TRI_idx_iid_t,
                                  TRI_index_t**);

// -----------------------------------------------------------------------------
// --SECTION--                                                   private defines
// -----------------------------------------------------------------------------

#define MAX_DOCUMENT_SIZE (1024 * 1024 * 512)

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
/// @brief creates a new entry in the primary index
////////////////////////////////////////////////////////////////////////////////

static int InsertPrimaryIndex (TRI_document_collection_t* document,
                               TRI_doc_mptr_t const* header,
                               bool isRollback) {
  TRI_doc_mptr_t* found;

  TRI_ASSERT(document != nullptr);
  TRI_ASSERT(header != nullptr);
  TRI_ASSERT(header->getDataPtr() != nullptr);  // ONLY IN INDEX, PROTECTED by RUNTIME

  // insert into primary index
  int res = TRI_InsertKeyPrimaryIndex(&document->_primaryIndex, header, (void const**) &found);

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
  if (! document->useSecondaryIndexes()) {
    return TRI_ERROR_NO_ERROR;
  }

  int result = TRI_ERROR_NO_ERROR;
  size_t const n = document->_allIndexes._length;

  // we can start at index #1 here (index #0 is the primary index)
  for (size_t i = 1;  i < n;  ++i) {
    TRI_index_t* idx = static_cast<TRI_index_t*>(document->_allIndexes._buffer[i]);
    int res = idx->insert(idx, header, isRollback);

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
  // .............................................................................
  // remove from main index
  // .............................................................................

  TRI_doc_mptr_t* found = static_cast<TRI_doc_mptr_t*>(TRI_RemoveKeyPrimaryIndex(&document->_primaryIndex, TRI_EXTRACT_MARKER_KEY(header))); // ONLY IN INDEX, PROTECTED by RUNTIME

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

  int result = TRI_ERROR_NO_ERROR;
  size_t const n = document->_allIndexes._length;

  // we can start at index #1 here (index #0 is the primary index)
  for (size_t i = 1;  i < n;  ++i) {
    TRI_index_t* idx = static_cast<TRI_index_t*>(document->_allIndexes._buffer[i]);
    int res = idx->remove(idx, header, isRollback);

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
  TRI_doc_mptr_t* header;
  size_t markerSize;

  markerSize = (size_t) marker->base._size;
  TRI_ASSERT(markerSize > 0);

  // get a new header pointer
  header = document->_headersPtr->request(markerSize);  // ONLY IN OPENITERATOR

  if (header == nullptr) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  header->_rid     = marker->_rid;
  header->_fid     = fid;
  header->setDataPtr(marker);  // ONLY IN OPENITERATOR
  header->_hash    = TRI_HashKeyPrimaryIndex(TRI_EXTRACT_MARKER_KEY(header));  // ONLY IN OPENITERATOR, PROTECTED by RUNTIME
  *result = header;

  return TRI_ERROR_NO_ERROR;
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
/// @brief set the index cleanup flag for the collection
////////////////////////////////////////////////////////////////////////////////

static void SetIndexCleanupFlag (TRI_document_collection_t* document,
                                 bool value) {
  document->_cleanupIndexes = value;

  LOG_DEBUG("setting cleanup indexes flag for collection '%s' to %d",
             document->_info._name,
             (int) value);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds an index to the collection
///
/// The caller must hold the index lock for the collection
////////////////////////////////////////////////////////////////////////////////

static int AddIndex (TRI_document_collection_t* document,
                     TRI_index_t* idx) {
  TRI_ASSERT(idx != nullptr);

  LOG_DEBUG("adding index of type %s for collection '%s'",
            TRI_TypeNameIndex(idx->_type),
            document->_info._name);

  int res = TRI_PushBackVectorPointer(&document->_allIndexes, idx);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  if (idx->cleanup != nullptr) {
    SetIndexCleanupFlag(document, true);
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief gather aggregate information about the collection's indexes
///
/// The caller must hold the index lock for the collection
////////////////////////////////////////////////////////////////////////////////

static void RebuildIndexInfo (TRI_document_collection_t* document) {
  bool result = false;
  size_t const n = document->_allIndexes._length;

  for (size_t i = 0 ; i < n ; ++i) {
    TRI_index_t* idx = static_cast<TRI_index_t*>(document->_allIndexes._buffer[i]);

    if (idx->cleanup != nullptr) {
      result = true;
      break;
    }
  }

  SetIndexCleanupFlag(document, result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief garbage-collect a collection's indexes
////////////////////////////////////////////////////////////////////////////////

static int CleanupIndexes (TRI_document_collection_t* document) {
  int res = TRI_ERROR_NO_ERROR;

  // cleaning indexes is expensive, so only do it if the flag is set for the
  // collection
  if (document->_cleanupIndexes) {
    TRI_WRITE_LOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(document);
    size_t const n = document->_allIndexes._length;

    for (size_t i = 0 ; i < n ; ++i) {
      TRI_index_t* idx = (TRI_index_t*) document->_allIndexes._buffer[i];

      if (idx->cleanup != nullptr) {
        res = idx->cleanup(idx);
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

  size_t const n = document->_allIndexes._length;

  // we can start at index #1 here (index #0 is the primary index)
  for (size_t i = 1;  i < n;  ++i) {
    TRI_index_t* idx = static_cast<TRI_index_t*>(document->_allIndexes._buffer[i]);

    if (idx->postInsert != nullptr) {
      idx->postInsert(trxCollection, idx, header);
    }
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
                           bool syncRequested) {

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

  res = TRI_AddOperationTransaction(operation, syncRequested);

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
  header = static_cast<TRI_doc_mptr_t*>(TRI_LookupByKeyPrimaryIndex(&document->_primaryIndex, key));

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
  TRI_READ_LOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(document);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief read unlocks a collection
////////////////////////////////////////////////////////////////////////////////

static int EndRead (TRI_document_collection_t* document) {
  TRI_READ_UNLOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(document);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief write locks a collection
////////////////////////////////////////////////////////////////////////////////

static int BeginWrite (TRI_document_collection_t* document) {
  TRI_WRITE_LOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(document);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief write unlocks a collection
////////////////////////////////////////////////////////////////////////////////

static int EndWrite (TRI_document_collection_t* document) {
  TRI_WRITE_UNLOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(document);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief read locks a collection, with a timeout (in µseconds)
////////////////////////////////////////////////////////////////////////////////

static int BeginReadTimed (TRI_document_collection_t* document,
                           uint64_t timeout,
                           uint64_t sleepPeriod) {
  uint64_t waited = 0;

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
  uint64_t waited = 0;

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
  uint32_t                   _trxCollections;
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
/// @brief update dead counter and size values for an obsolete marker
////////////////////////////////////////////////////////////////////////////////

static void TrackDeadMarker (TRI_df_marker_t const* marker,
                             TRI_datafile_t const* datafile,
                             open_iterator_state_t* state) {
  // TODO: decide whether we can get rid of old transaction markers or not
  return;
  /*
  if (state->_fid != datafile->_fid) {
    TRI_document_collection_t* document = state->_document;

    state->_fid = datafile->_fid;
    state->_dfi = TRI_FindDatafileInfoDocumentCollection(document, datafile->_fid, true);
  }

  if (state->_dfi != nullptr) {
    state->_dfi->_numberDead++;
    state->_dfi->_sizeDead += (int64_t) TRI_DF_ALIGN_BLOCK(marker->_size);
  }
  */
}

////////////////////////////////////////////////////////////////////////////////
/// @brief apply an insert/update operation when opening a collection
////////////////////////////////////////////////////////////////////////////////

static int OpenIteratorApplyInsert (open_iterator_state_t* state,
                                    open_iterator_operation_t* operation) {

  TRI_doc_mptr_t const* found;
  TRI_voc_key_t key;

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

  key = ((char*) d) + d->_offsetKey;
  document->_keyGenerator->track(key);

  ++state->_documents;

  // no primary index lock required here because we are the only ones reading from the index ATM
  found = static_cast<TRI_doc_mptr_t const*>(TRI_LookupByKeyPrimaryIndex(&document->_primaryIndex, key));

  // it is a new entry
  if (found == nullptr) {
    TRI_doc_mptr_t* header;
    int res;

    // get a header
    res = CreateHeader(document, (TRI_doc_document_key_marker_t*) marker, operation->_fid, &header);

    if (res != TRI_ERROR_NO_ERROR) {
      LOG_ERROR("out of memory");

      return TRI_set_errno(res);
    }

    TRI_ASSERT(header != nullptr);

    // insert into primary index
    res = InsertPrimaryIndex(document, header, false);

    if (res != TRI_ERROR_NO_ERROR) {
      // insertion failed
      LOG_ERROR("inserting document into indexes failed");
      document->_headersPtr->release(header, true);  // ONLY IN OPENITERATOR

      return res;
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
    TRI_doc_mptr_t* newHeader;
    TRI_doc_mptr_copy_t oldData;
    TRI_doc_datafile_info_t* dfi;

    // save the old data
    oldData = *found;

    newHeader = static_cast<TRI_doc_mptr_t*>(CONST_CAST(found));

    // update the header info
    UpdateHeader(operation->_fid, marker, newHeader, found);
    document->_headersPtr->moveBack(newHeader, &oldData);  // ONLY IN OPENITERATOR

    // update the datafile info
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
                                    open_iterator_operation_t* operation) {

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
  found = static_cast<TRI_doc_mptr_t*>(TRI_LookupByKeyPrimaryIndex(&document->_primaryIndex, key));

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
                                       open_iterator_operation_t* operation) {
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
                                     const TRI_voc_document_operation_e type,
                                     TRI_df_marker_t const* marker,
                                     const TRI_voc_fid_t fid) {
  open_iterator_operation_t operation;
  int res;

  operation._type   = type;
  operation._marker = marker;
  operation._fid    = fid;

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
  size_t n = state->_operations._length;

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

  TRI_ASSERT(state->_operations._length == 0);

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
        size_t const n = state->_operations._length;

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
  int res;

  res = TRI_ERROR_NO_ERROR;

  if (state->_trxCollections <= 1 || state->_trxPrepared) {
    size_t i, n;

    n = state->_operations._length;

    for (i = 0; i < n; ++i) {
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
  TrackDeadMarker(marker, datafile, state);

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
  TrackDeadMarker(marker, datafile, state);

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
  TrackDeadMarker(marker, datafile, state);

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
  TrackDeadMarker(marker, datafile, state);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief iterator for open
////////////////////////////////////////////////////////////////////////////////

static bool OpenIterator (TRI_df_marker_t const* marker,
                          void* data,
                          TRI_datafile_t* datafile) {
  int res;

  TRI_document_collection_t* document = static_cast<open_iterator_state_t*>(data)->_document;

  if (marker->_type == TRI_DOC_MARKER_KEY_EDGE ||
      marker->_type == TRI_DOC_MARKER_KEY_DOCUMENT) {
    res = OpenIteratorHandleDocumentMarker(marker, datafile, (open_iterator_state_t*) data);
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
    LOG_TRACE("skipping marker type %lu", (unsigned long) marker->_type);
    res = TRI_ERROR_NO_ERROR;
  }

  TRI_voc_tick_t tick = marker->_tick;

  if (datafile->_tickMin == 0) {
    datafile->_tickMin = tick;
  }

  if (tick > datafile->_tickMax) {
    datafile->_tickMax = tick;
  }

  // set tick values for data markers (document/edge), too
  if (marker->_type == TRI_DOC_MARKER_KEY_DOCUMENT ||
      marker->_type == TRI_DOC_MARKER_KEY_EDGE) {

    if (datafile->_dataMin == 0) {
      datafile->_dataMin = tick;
    }

    if (tick > datafile->_dataMax) {
      datafile->_dataMax = tick;
    }
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
/// @brief fill the internal (non-user-definable) indexes
/// currently, this will only fill edge indexes
////////////////////////////////////////////////////////////////////////////////

static int FillInternalIndexes (TRI_document_collection_t* document) {
  int res = TRI_ERROR_NO_ERROR;

  for (size_t i = 0;  i < document->_allIndexes._length;  ++i) {
    TRI_index_t* idx = static_cast<TRI_index_t*>(document->_allIndexes._buffer[i]);

    if (idx->_type == TRI_IDX_TYPE_EDGE_INDEX) {
      int r = FillIndex(document, idx);

      if (r != TRI_ERROR_NO_ERROR) {
        // return first error, but continue
        res = r;
      }
    }
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief iterator for index open
////////////////////////////////////////////////////////////////////////////////

static bool OpenIndexIterator (char const* filename,
                               void* data) {
  // load json description of the index
  TRI_json_t* json = TRI_JsonFile(TRI_CORE_MEM_ZONE, filename, nullptr);

  // json must be a index description
  if (! TRI_IsArrayJson(json)) {
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

  for (size_t i = 0; i < document->_allIndexes._length; ++i) {
    TRI_index_t const* idx = static_cast<TRI_index_t const*>(TRI_AtVectorPointer(&document->_allIndexes, i));

    if (idx->memory != nullptr) {
      info->_sizeIndexes += idx->memory(idx);
    }
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
  document->setShaper(shaper);
  document->_capConstraint      = nullptr;
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

  res = TRI_InitPrimaryIndex(&document->_primaryIndex);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_DestroyAssociativePointer(&document->_datafileInfo);

    return res;
  }

  TRI_InitBarrierList(&document->_barrierList, document);

  TRI_InitReadWriteLock(&document->_lock);
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
  TRI_DestroyReadWriteLock(&document->_lock);

  TRI_DestroyPrimaryIndex(&document->_primaryIndex);

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

  TRI_DestroyBarrierList(&document->_barrierList);

  TRI_DestroyCollection(document);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises a document collection
////////////////////////////////////////////////////////////////////////////////

static bool InitDocumentCollection (TRI_document_collection_t* document,
                                    TRI_shaper_t* shaper) {
  document->_cleanupIndexes   = false;
  document->_failedTransactions = nullptr;

  document->_uncollectedLogfileEntries = 0;

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

  res = TRI_InitVectorPointer2(&document->_allIndexes, TRI_UNKNOWN_MEM_ZONE, 2);

  if (res != TRI_ERROR_NO_ERROR) {
    DestroyBaseDocumentCollection(document);
    TRI_set_errno(res);

    return false;
  }

  // create primary index
  TRI_index_t* primaryIndex = TRI_CreatePrimaryIndex(document);

  if (primaryIndex == nullptr) {
    TRI_DestroyVectorPointer(&document->_allIndexes);
    DestroyBaseDocumentCollection(document);
    TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);

    return false;
  }

  res = AddIndex(document, primaryIndex);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_FreeIndex(primaryIndex);
    TRI_DestroyVectorPointer(&document->_allIndexes);
    DestroyBaseDocumentCollection(document);
    TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);

    return false;
  }

  // create edges index
  if (document->_info._type == TRI_COL_TYPE_EDGE) {
    TRI_index_t* edgesIndex = TRI_CreateEdgeIndex(document, document->_info._cid);

    if (edgesIndex == nullptr) {
      TRI_FreeIndex(primaryIndex);
      TRI_DestroyVectorPointer(&document->_allIndexes);
      DestroyBaseDocumentCollection(document);
      TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);

      return false;
    }

    res = AddIndex(document, edgesIndex);

    if (res != TRI_ERROR_NO_ERROR) {
      TRI_FreeIndex(edgesIndex);
      TRI_FreeIndex(primaryIndex);
      TRI_DestroyVectorPointer(&document->_allIndexes);
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
  open_iterator_state_t openState;

  // initialise state for iteration
  openState._document       = reinterpret_cast<TRI_document_collection_t*>(collection);
  openState._tid            = 0;
  openState._trxPrepared    = false;
  openState._trxCollections = 0;
  openState._deletions      = 0;
  openState._documents      = 0;
  openState._fid            = 0;
  openState._dfi            = nullptr;
  openState._vocbase        = collection->_vocbase;

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

    // TODO: shouldn't we destroy &document->_allIndexes, free document->_headersPtr etc.?
    TRI_CloseCollection(collection);
    TRI_DestroyCollection(collection);
    delete document;
    return nullptr;
  }

  document->_keyGenerator = keyGenerator;

  // save the parameter block (within create, no need to lock)
  bool doSync = vocbase->_settings.forceSyncProperties;
  int res = TRI_SaveCollectionInfo(collection->_directory, parameters, doSync);

  if (res != TRI_ERROR_NO_ERROR) {
    // TODO: shouldn't we destroy &document->_allIndexes, free document->_headersPtr etc.?
    LOG_ERROR("cannot save collection parameters in directory '%s': '%s'",
              collection->_directory,
              TRI_last_error());

    TRI_CloseCollection(collection);
    TRI_DestroyCollection(collection);
    delete document;
    return nullptr;
  }

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
  size_t const n = document->_allIndexes._length;
  for (size_t i = 0 ; i < n ; ++i) {
    TRI_index_t* idx = (TRI_index_t*) document->_allIndexes._buffer[i];

    TRI_FreeIndex(idx);
  }

  // free index vector
  TRI_DestroyVectorPointer(&document->_allIndexes);

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
      errno = ENOSPC;
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

  size_t const nrUsed = (size_t) document->_primaryIndex._nrUsed;

  if (nrUsed > 0) {
    void** ptr = document->_primaryIndex._table;
    void** end = ptr + document->_primaryIndex._nrAlloc;

    for (;  ptr < end;  ++ptr) {
      if (*ptr) {
        TRI_doc_mptr_t const* d = (TRI_doc_mptr_t const*) *ptr;

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
                                         TRI_index_t** idx) {
  TRI_ASSERT(json != nullptr);
  TRI_ASSERT(json->_type == TRI_JSON_ARRAY);

  if (idx != nullptr) {
    *idx = nullptr;
  }

  // extract the type
  TRI_json_t const* type = TRI_LookupArrayJson(json, "type");

  if (! TRI_IsStringJson(type)) {
    return TRI_ERROR_INTERNAL;
  }

  char const* typeStr = type->_value._string.data;

  // extract the index identifier
  TRI_json_t const* iis = TRI_LookupArrayJson(json, "id");

  TRI_idx_iid_t iid;
  if (iis != nullptr && iis->_type == TRI_JSON_NUMBER) {
    iid = (TRI_idx_iid_t) iis->_value._number;
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
  }

  else if (TRI_EqualString(typeStr, "priorityqueue") ||
           TRI_EqualString(typeStr, "bitarray")) {
    LOG_WARNING("index type '%s' is not supported in this version of ArangoDB and is ignored", typeStr);
    return TRI_ERROR_NO_ERROR;
  }

  // .........................................................................
  // oops, unknown index type
  // .........................................................................

  else {
    LOG_ERROR("ignoring unknown index type '%s' for index %llu",
              typeStr,
              (unsigned long long) iid);
  }

  return TRI_ERROR_INTERNAL;
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
/// @brief fill the additional (non-primary) indexes
////////////////////////////////////////////////////////////////////////////////

int TRI_FillIndexesDocumentCollection (TRI_document_collection_t* document) {
  // fill internal indexes (this is, the edges index at the moment)
  FillInternalIndexes(document);

  // fill user-defined secondary indexes
  TRI_collection_t* collection = reinterpret_cast<TRI_collection_t*>(document);
  TRI_IterateIndexCollection(collection, OpenIndexIterator, collection);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief opens an existing collection
////////////////////////////////////////////////////////////////////////////////

TRI_document_collection_t* TRI_OpenDocumentCollection (TRI_vocbase_t* vocbase,
                                                       TRI_vocbase_col_t* col) {
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

  TRI_collection_t* collection = TRI_OpenCollection(vocbase, document, path);

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

  // iterate over all markers of the collection
  int res = IterateMarkersCollection(collection);

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

  TRI_ASSERT(document->getShaper() != nullptr);  // ONLY in OPENCOLLECTION, PROTECTED by fake trx here

  TRI_InitVocShaper(document->getShaper());  // ONLY in OPENCOLLECTION, PROTECTED by fake trx here

  if (! triagens::wal::LogfileManager::instance()->isInRecovery()) {
    TRI_FillIndexesDocumentCollection(document);
  }

  return document;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief closes an open collection
////////////////////////////////////////////////////////////////////////////////

int TRI_CloseDocumentCollection (TRI_document_collection_t* document) {
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
  TRI_json_t* fld = TRI_LookupArrayJson(json, "fields");

  if (! TRI_IsListJson(fld)) {
    LOG_ERROR("ignoring index %llu, 'fields' must be a list", (unsigned long long) iid);

    TRI_set_errno(TRI_ERROR_BAD_PARAMETER);
    return nullptr;
  }

  *fieldCount = fld->_value._objects._length;

  for (size_t j = 0;  j < *fieldCount;  ++j) {
    TRI_json_t* sub = static_cast<TRI_json_t*>(TRI_AtVector(&fld->_value._objects, j));

    if (! TRI_IsStringJson(sub)) {
      LOG_ERROR("ignoring index %llu, 'fields' must be a list of attribute paths", (unsigned long long) iid);

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
                      TRI_index_t* idx) {

  void** ptr = document->_primaryIndex._table;
  void** end = ptr + document->_primaryIndex._nrAlloc;

  if (idx->sizeHint != nullptr) {
    // give the index a size hint
    idx->sizeHint(idx, (size_t) document->_primaryIndex._nrUsed);
  }

#ifdef TRI_ENABLE_MAINTAINER_MODE
  static const int LoopSize = 10000;
  int counter = 0;
  int loops = 0;
#endif

  for (;  ptr < end;  ++ptr) {
    TRI_doc_mptr_t const* p = static_cast<TRI_doc_mptr_t const*>(*ptr);

    if (p != nullptr) {
      TRI_doc_mptr_t const* mptr = p;

      int res = idx->insert(idx, mptr, false);

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

////////////////////////////////////////////////////////////////////////////////
/// @brief finds a path based, unique or non-unique index
////////////////////////////////////////////////////////////////////////////////

static TRI_index_t* LookupPathIndexDocumentCollection (TRI_document_collection_t* collection,
                                                       TRI_vector_t const* paths,
                                                       TRI_idx_type_e type,
                                                       bool unique,
                                                       bool allowAnyAttributeOrder) {
  TRI_vector_t* indexPaths = nullptr;

  // ...........................................................................
  // go through every index and see if we have a match
  // ...........................................................................

  for (size_t j = 0;  j < collection->_allIndexes._length;  ++j) {
    TRI_index_t* idx = static_cast<TRI_index_t*>(collection->_allIndexes._buffer[j]);

    // .........................................................................
    // check if the type of the index matches
    // .........................................................................

    if (idx->_type != type || idx->_unique != unique) {
      continue;
    }

    // .........................................................................
    // Now perform checks which are specific to the type of index
    // .........................................................................

    switch (type) {
      case TRI_IDX_TYPE_HASH_INDEX: {
        TRI_hash_index_t* hashIndex = (TRI_hash_index_t*) idx;
        indexPaths = &(hashIndex->_paths);
        break;
      }

      case TRI_IDX_TYPE_SKIPLIST_INDEX: {
        TRI_skiplist_index_t* slIndex = (TRI_skiplist_index_t*) idx;
        indexPaths = &(slIndex->_paths);
        break;
      }

      default: {
        TRI_ASSERT(false);
        break;
      }

    }

    if (indexPaths == nullptr) {
      // this may actually happen if compiled with -DNDEBUG
      return nullptr;
    }

    // .........................................................................
    // check that the number of paths (fields) in the index matches that
    // of the number of attributes
    // .........................................................................

    if (paths->_length != indexPaths->_length) {
      continue;
    }

    // .........................................................................
    // go through all the attributes and see if they match
    // .........................................................................

    bool found = true;

    if (allowAnyAttributeOrder) {
      // any permutation of attributes is allowed
      for (size_t k = 0;  k < paths->_length;  ++k) {
        TRI_shape_pid_t indexShape = *((TRI_shape_pid_t*) TRI_AtVector(indexPaths, k));

        found = false;

        for (size_t l = 0;  l < paths->_length;  ++l) {
          TRI_shape_pid_t givenShape = *((TRI_shape_pid_t*) TRI_AtVector(paths, l));

          if (indexShape == givenShape) {
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
      // attributes need to present in a given order
      for (size_t k = 0;  k < paths->_length;  ++k) {
        TRI_shape_pid_t indexShape = *((TRI_shape_pid_t*) TRI_AtVector(indexPaths, k));
        TRI_shape_pid_t givenShape = *((TRI_shape_pid_t*) TRI_AtVector(paths, k));

        if (indexShape != givenShape) {
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
                                   TRI_index_t* (*creator)(TRI_document_collection_t*,
                                                           TRI_vector_pointer_t const*,
                                                           TRI_idx_iid_t,
                                                           bool,
                                                           bool*),
                                   TRI_index_t** dst) {
  TRI_json_t* bv;
  TRI_vector_pointer_t attributes;
  bool unique;
  size_t fieldCount;

  if (dst != nullptr) {
    *dst = nullptr;
  }

  // extract fields
  TRI_json_t* fld = ExtractFields(definition, &fieldCount, iid);

  if (fld == nullptr) {
    return TRI_errno();
  }

  // extract the list of fields
  if (fieldCount < 1) {
    LOG_ERROR("ignoring index %llu, need at least one attribute path",(unsigned long long) iid);

    return TRI_set_errno(TRI_ERROR_BAD_PARAMETER);
  }

  // determine if the hash index is unique or non-unique
  bv = TRI_LookupArrayJson(definition, "unique");

  if (! TRI_IsBooleanJson(bv)) {
    LOG_ERROR("ignoring index %llu, could not determine if unique or non-unique", (unsigned long long) iid);
    return TRI_set_errno(TRI_ERROR_BAD_PARAMETER);
  }

  unique = bv->_value._boolean;

  // Initialise the vector in which we store the fields on which the hashing
  // will be based.
  TRI_InitVectorPointer(&attributes, TRI_CORE_MEM_ZONE);

  // find fields
  for (size_t j = 0;  j < fieldCount;  ++j) {
    TRI_json_t* fieldStr = static_cast<TRI_json_t*>(TRI_AtVector(&fld->_value._objects, j));

    TRI_PushBackVectorPointer(&attributes, fieldStr->_value._string.data);
  }

  // create the index
  TRI_index_t* idx = creator(document, &attributes, iid, unique, nullptr);

  if (dst != nullptr) {
    *dst = idx;
  }

  // cleanup
  TRI_DestroyVectorPointer(&attributes);

  if (idx == nullptr) {
    LOG_ERROR("cannot create index %llu in collection '%s'", (unsigned long long) iid, document->_info._name);
    return TRI_errno();
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compares pid and name
////////////////////////////////////////////////////////////////////////////////

static int ComparePidName (void const* left, void const* right) {
  pid_name_t const* l = static_cast<pid_name_t const*>(left);
  pid_name_t const* r = static_cast<pid_name_t const*>(right);

  return (int) (l->_pid - r->_pid);
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

  int64_t uncollected = document->_uncollectedLogfileEntries;

  TRI_READ_UNLOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(document);

  return (uncollected == 0);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a description of all indexes
///
/// the caller must have read-locked the underlying collection!
////////////////////////////////////////////////////////////////////////////////

TRI_vector_pointer_t* TRI_IndexesDocumentCollection (TRI_document_collection_t* document) {
  TRI_vector_pointer_t* vector = static_cast<TRI_vector_pointer_t*>(TRI_Allocate(TRI_CORE_MEM_ZONE, sizeof(TRI_vector_pointer_t), false));

  if (vector == nullptr) {
    return nullptr;
  }

  TRI_InitVectorPointer(vector, TRI_CORE_MEM_ZONE);

  size_t const n = document->_allIndexes._length;

  for (size_t i = 0;  i < n;  ++i) {
    TRI_index_t* idx = static_cast<TRI_index_t*>(document->_allIndexes._buffer[i]);
    TRI_json_t* json = idx->json(idx);

    if (json != nullptr) {
      TRI_PushBackVectorPointer(vector, json);
    }
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

  TRI_index_t* found = nullptr;
  TRI_vocbase_t* vocbase = document->_vocbase;

  TRI_ReadLockReadWriteLock(&vocbase->_inventoryLock);

  // .............................................................................
  // inside write-lock
  // .............................................................................

  TRI_WRITE_LOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(document);

  size_t const n = document->_allIndexes._length;

  for (size_t i = 0;  i < n;  ++i) {
    TRI_index_t* idx = static_cast<TRI_index_t*>(document->_allIndexes._buffer[i]);

    if (idx->_iid == iid) {
      if (idx->_type == TRI_IDX_TYPE_PRIMARY_INDEX ||
          idx->_type == TRI_IDX_TYPE_EDGE_INDEX) {
        // cannot remove these index types
        break;
      }

      found = static_cast<TRI_index_t*>(TRI_RemoveVectorPointer(&document->_allIndexes, i));

      if (found != nullptr && found->removeIndex != nullptr) {
        // notify the index about its removal
        found->removeIndex(found, document);
      }
      break;
    }
  }

  if (found != nullptr) {
    RebuildIndexInfo(document);
  }

  TRI_WRITE_UNLOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(document);

  TRI_ReadUnlockReadWriteLock(&vocbase->_inventoryLock);

  // .............................................................................
  // outside write-lock
  // .............................................................................

  if (found != nullptr) {
    bool result = TRI_RemoveIndexFile(document, found);

    TRI_FreeIndex(found);

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
      catch (triagens::arango::Exception const& ex) {
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

static int PidNamesByAttributeNames (TRI_vector_pointer_t const* attributes,
                                     TRI_shaper_t* shaper,
                                     TRI_vector_t* pids,
                                     TRI_vector_pointer_t* names,
                                     bool sorted,
                                     bool create) {
  pid_name_t* pidnames;

  // .............................................................................
  // sorted case
  // .............................................................................

  if (sorted) {
    // combine name and pid
    pidnames = static_cast<pid_name_t*>(TRI_Allocate(TRI_CORE_MEM_ZONE, sizeof(pid_name_t) * attributes->_length, false));

    if (pidnames == nullptr) {
      LOG_ERROR("out of memory in PidNamesByAttributeNames");
      return TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
    }

    for (size_t j = 0;  j < attributes->_length;  ++j) {
      pidnames[j]._name = static_cast<char*>(attributes->_buffer[j]);

      if (create) {
        pidnames[j]._pid = shaper->findOrCreateAttributePathByName(shaper, pidnames[j]._name, true);
      }
      else {
        pidnames[j]._pid = shaper->lookupAttributePathByName(shaper, pidnames[j]._name);
      }

      if (pidnames[j]._pid == 0) {
        TRI_Free(TRI_CORE_MEM_ZONE, pidnames);

        return TRI_set_errno(TRI_ERROR_ARANGO_ILLEGAL_NAME);
      }
    }

    // sort according to pid
    qsort(pidnames, attributes->_length, sizeof(pid_name_t), ComparePidName);

    // split again
    TRI_InitVector(pids, TRI_CORE_MEM_ZONE, sizeof(TRI_shape_pid_t));
    TRI_InitVectorPointer(names, TRI_CORE_MEM_ZONE);

    for (size_t j = 0;  j < attributes->_length;  ++j) {
      TRI_PushBackVector(pids, &pidnames[j]._pid);
      TRI_PushBackVectorPointer(names, pidnames[j]._name);
    }

    TRI_Free(TRI_CORE_MEM_ZONE, pidnames);
  }

  // .............................................................................
  // unsorted case
  // .............................................................................

  else {
    TRI_InitVector(pids, TRI_CORE_MEM_ZONE, sizeof(TRI_shape_pid_t));
    TRI_InitVectorPointer(names, TRI_CORE_MEM_ZONE);

    for (size_t j = 0;  j < attributes->_length;  ++j) {
      char* name = static_cast<char*>(attributes->_buffer[j]);

      TRI_shape_pid_t pid;
      if (create) {
        pid = shaper->findOrCreateAttributePathByName(shaper, name, true);
      }
      else {
        pid = shaper->lookupAttributePathByName(shaper, name);
      }

      if (pid == 0) {
        TRI_DestroyVector(pids);
        TRI_DestroyVectorPointer(names);

        return TRI_set_errno(TRI_ERROR_ARANGO_ILLEGAL_NAME);
      }

      TRI_PushBackVector(pids, &pid);
      TRI_PushBackVectorPointer(names, name);
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

static TRI_index_t* CreateCapConstraintDocumentCollection (TRI_document_collection_t* document,
                                                           size_t count,
                                                           int64_t size,
                                                           TRI_idx_iid_t iid,
                                                           bool* created) {
  TRI_index_t* idx;
  int res;

  if (created != nullptr) {
    *created = false;
  }

  // check if we already know a cap constraint
  if (document->_capConstraint != nullptr) {
    if (document->_capConstraint->_count == count &&
        document->_capConstraint->_size == size) {
      return &document->_capConstraint->base;
    }
    else {
      TRI_set_errno(TRI_ERROR_ARANGO_CAP_CONSTRAINT_ALREADY_DEFINED);
      return nullptr;
    }
  }

  // create a new index
  idx = TRI_CreateCapConstraint(document, iid, count, size);

  if (idx == nullptr) {
    TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);

    return nullptr;
  }

  // initialises the index with all existing documents
  res = FillIndex(document, idx);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_FreeCapConstraint(idx);

    return nullptr;
  }

  // and store index
  res = AddIndex(document, idx);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_FreeCapConstraint(idx);

    return nullptr;
  }

  if (created != nullptr) {
    *created = true;
  }

  document->_capConstraint = (TRI_cap_constraint_t*) idx;

  return idx;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief restores an index
////////////////////////////////////////////////////////////////////////////////

static int CapConstraintFromJson (TRI_document_collection_t* document,
                                  TRI_json_t const* definition,
                                  TRI_idx_iid_t iid,
                                  TRI_index_t** dst) {
  if (dst != nullptr) {
    *dst = nullptr;
  }

  TRI_json_t const* val1 = TRI_LookupArrayJson(definition, "size");
  TRI_json_t const* val2 = TRI_LookupArrayJson(definition, "byteSize");

  if (! TRI_IsNumberJson(val1) && ! TRI_IsNumberJson(val2)) {
    LOG_ERROR("ignoring cap constraint %llu, 'size' and 'byteSize' missing",
              (unsigned long long) iid);

    return TRI_set_errno(TRI_ERROR_BAD_PARAMETER);
  }

  size_t count = 0;
  if (TRI_IsNumberJson(val1) && val1->_value._number > 0.0) {
    count = (size_t) val1->_value._number;
  }

  int64_t size = 0;
  if (TRI_IsNumberJson(val2) && val2->_value._number > (double) TRI_CAP_CONSTRAINT_MIN_SIZE) {
    size = (int64_t) val2->_value._number;
  }

  if (count == 0 && size == 0) {
    LOG_ERROR("ignoring cap constraint %llu, 'size' must be at least 1, "
              "or 'byteSize' must be at least "
              TRI_CAP_CONSTRAINT_MIN_SIZE_STR,
              (unsigned long long) iid);

    return TRI_set_errno(TRI_ERROR_BAD_PARAMETER);
  }

  TRI_index_t* idx = CreateCapConstraintDocumentCollection(document, count, size, iid, nullptr);
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

TRI_index_t* TRI_LookupCapConstraintDocumentCollection (TRI_document_collection_t* document) {
  // check if we already know a cap constraint
  if (document->_capConstraint != nullptr) {
    return &document->_capConstraint->base;
  }

  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures that a cap constraint exists
////////////////////////////////////////////////////////////////////////////////

TRI_index_t* TRI_EnsureCapConstraintDocumentCollection (TRI_document_collection_t* document,
                                                        TRI_idx_iid_t iid,
                                                        size_t count,
                                                        int64_t size,
                                                        bool* created) {
  TRI_index_t* idx;

  // .............................................................................
  // inside write-lock
  // .............................................................................

  TRI_ReadLockReadWriteLock(&document->_vocbase->_inventoryLock);

  TRI_WRITE_LOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(document);

  idx = CreateCapConstraintDocumentCollection(document, count, size, iid, created);

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
// --SECTION--                                                         GEO INDEX
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a geo index to a collection
////////////////////////////////////////////////////////////////////////////////

static TRI_index_t* CreateGeoIndexDocumentCollection (TRI_document_collection_t* document,
                                                      char const* location,
                                                      char const* latitude,
                                                      char const* longitude,
                                                      bool geoJson,
                                                      bool unique,
                                                      bool ignoreNull,
                                                      TRI_idx_iid_t iid,
                                                      bool* created) {
  TRI_index_t* idx;
  TRI_shape_pid_t lat;
  TRI_shape_pid_t loc;
  TRI_shape_pid_t lon;
  TRI_shaper_t* shaper;
  int res;

  lat = 0;
  lon = 0;
  loc = 0;
  idx = nullptr;

  shaper = document->getShaper();  // ONLY IN INDEX, PROTECTED by RUNTIME

  if (location != nullptr) {
    loc = shaper->findOrCreateAttributePathByName(shaper, location, true);

    if (loc == 0) {
      TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
      return nullptr;
    }
  }

  if (latitude != nullptr) {
    lat = shaper->findOrCreateAttributePathByName(shaper, latitude, true);

    if (lat == 0) {
      TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
      return nullptr;
    }
  }

  if (longitude != nullptr) {
    lon = shaper->findOrCreateAttributePathByName(shaper, longitude, true);

    if (lon == 0) {
      TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
      return nullptr;
    }
  }

  // check, if we know the index
  if (location != nullptr) {
    idx = TRI_LookupGeoIndex1DocumentCollection(document, location, geoJson, unique, ignoreNull);
  }
  else if (longitude != nullptr && latitude != nullptr) {
    idx = TRI_LookupGeoIndex2DocumentCollection(document, latitude, longitude, unique, ignoreNull);
  }
  else {
    TRI_set_errno(TRI_ERROR_INTERNAL);
    LOG_TRACE("expecting either 'location' or 'latitude' and 'longitude'");
    return nullptr;
  }

  if (idx != nullptr) {
    LOG_TRACE("geo-index already created for location '%s'", location);

    if (created != nullptr) {
      *created = false;
    }

    return idx;
  }

  // create a new index
  if (location != nullptr) {
    idx = TRI_CreateGeo1Index(document, iid, location, loc, geoJson, unique, ignoreNull);

    LOG_TRACE("created geo-index for location '%s': %ld",
              location,
              (unsigned long) loc);
  }
  else if (longitude != nullptr && latitude != nullptr) {
    idx = TRI_CreateGeo2Index(document, iid, latitude, lat, longitude, lon, unique, ignoreNull);

    LOG_TRACE("created geo-index for location '%s': %ld, %ld",
              location,
              (unsigned long) lat,
              (unsigned long) lon);
  }

  if (idx == nullptr) {
    TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
    return nullptr;
  }

  // initialises the index with all existing documents
  res = FillIndex(document, idx);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_FreeGeoIndex(idx);

    return nullptr;
  }

  // and store index
  res = AddIndex(document, idx);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_FreeGeoIndex(idx);

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
                             TRI_index_t** dst) {
  TRI_index_t* idx;
  TRI_json_t* type;
  TRI_json_t* bv;
  TRI_json_t* fld;
  bool unique;
  bool ignoreNull;
  char const* typeStr;
  size_t fieldCount;

  if (dst != nullptr) {
    *dst = nullptr;
  }

  type = TRI_LookupArrayJson(definition, "type");

  if (! TRI_IsStringJson(type)) {
    return TRI_ERROR_INTERNAL;
  }

  typeStr = type->_value._string.data;

  // extract fields
  fld = ExtractFields(definition, &fieldCount, iid);

  if (fld == nullptr) {
    return TRI_errno();
  }

  // extract unique
  unique = false;
  // first try "unique" attribute
  bv = TRI_LookupArrayJson(definition, "unique");

  if (bv != nullptr && bv->_type == TRI_JSON_BOOLEAN) {
    unique = bv->_value._boolean;
  }
  else {
    // then "constraint"
    bv = TRI_LookupArrayJson(definition, "constraint");

    if (TRI_IsBooleanJson(bv)) {
      unique = bv->_value._boolean;
    }
  }

  // extract ignore null
  ignoreNull = false;
  bv = TRI_LookupArrayJson(definition, "ignoreNull");

  if (TRI_IsBooleanJson(bv)) {
    ignoreNull = bv->_value._boolean;
  }

  // list style
  if (TRI_EqualString(typeStr, "geo1")) {
    bool geoJson;

    // extract geo json
    geoJson = false;
    bv = TRI_LookupArrayJson(definition, "geoJson");

    if (TRI_IsBooleanJson(bv)) {
      geoJson = bv->_value._boolean;
    }

    // need just one field
    if (fieldCount == 1) {
      TRI_json_t* loc = static_cast<TRI_json_t*>(TRI_AtVector(&fld->_value._objects, 0));

      idx = CreateGeoIndexDocumentCollection(document,
                                        loc->_value._string.data,
                                        nullptr,
                                        nullptr,
                                        geoJson,
                                        unique,
                                        ignoreNull,
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
      TRI_json_t* lat = static_cast<TRI_json_t*>(TRI_AtVector(&fld->_value._objects, 0));
      TRI_json_t* lon = static_cast<TRI_json_t*>(TRI_AtVector(&fld->_value._objects, 1));

      idx = CreateGeoIndexDocumentCollection(document,
                                             nullptr,
                                             lat->_value._string.data,
                                             lon->_value._string.data,
                                             false,
                                             unique,
                                             ignoreNull,
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

  return 0; // shut the vc++ up
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief finds a geo index, list style
////////////////////////////////////////////////////////////////////////////////

TRI_index_t* TRI_LookupGeoIndex1DocumentCollection (TRI_document_collection_t* document,
                                                    char const* location,
                                                    bool geoJson,
                                                    bool unique,
                                                    bool ignoreNull) {
  TRI_shape_pid_t loc;
  TRI_shaper_t* shaper;
  size_t i, n;

  shaper = document->getShaper();  // ONLY IN INDEX, PROTECTED by RUNTIME

  loc = shaper->lookupAttributePathByName(shaper, location);

  if (loc == 0) {
    return nullptr;
  }

  n = document->_allIndexes._length;

  for (i = 0;  i < n;  ++i) {
    TRI_index_t* idx = static_cast<TRI_index_t*>(document->_allIndexes._buffer[i]);

    if (idx->_type == TRI_IDX_TYPE_GEO1_INDEX) {
      TRI_geo_index_t* geo = (TRI_geo_index_t*) idx;

      if (geo->_location != 0 && geo->_location == loc &&
          geo->_geoJson == geoJson &&
          idx->_unique == unique) {
        if (! unique || geo->base._ignoreNull == ignoreNull) {
          return idx;
        }
      }
    }
  }

  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief finds a geo index, attribute style
////////////////////////////////////////////////////////////////////////////////

TRI_index_t* TRI_LookupGeoIndex2DocumentCollection (TRI_document_collection_t* document,
                                                    char const* latitude,
                                                    char const* longitude,
                                                    bool unique,
                                                    bool ignoreNull) {
  TRI_shape_pid_t lat;
  TRI_shape_pid_t lon;
  TRI_shaper_t* shaper;
  size_t i, n;

  shaper = document->getShaper();  // ONLY IN INDEX, PROTECTED by RUNTIME

  lat = shaper->lookupAttributePathByName(shaper, latitude);
  lon = shaper->lookupAttributePathByName(shaper, longitude);

  if (lat == 0 || lon == 0) {
    return nullptr;
  }

  n = document->_allIndexes._length;

  for (i = 0;  i < n;  ++i) {
    TRI_index_t* idx;

    idx = static_cast<TRI_index_t*>(document->_allIndexes._buffer[i]);

    if (idx->_type == TRI_IDX_TYPE_GEO2_INDEX) {
      TRI_geo_index_t* geo = (TRI_geo_index_t*) idx;

      if (geo->_latitude != 0 &&
          geo->_longitude != 0 &&
          geo->_latitude == lat &&
          geo->_longitude == lon &&
          idx->_unique == unique) {
        if (! unique || geo->base._ignoreNull == ignoreNull) {
          return idx;
        }
      }
    }
  }

  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures that a geo index exists, list style
////////////////////////////////////////////////////////////////////////////////

TRI_index_t* TRI_EnsureGeoIndex1DocumentCollection (TRI_document_collection_t* document,
                                                    TRI_idx_iid_t iid,
                                                    char const* location,
                                                    bool geoJson,
                                                    bool unique,
                                                    bool ignoreNull,
                                                    bool* created) {
  TRI_index_t* idx;

  TRI_ReadLockReadWriteLock(&document->_vocbase->_inventoryLock);

  // .............................................................................
  // inside write-lock
  // .............................................................................

  TRI_WRITE_LOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(document);

  idx = CreateGeoIndexDocumentCollection(document, location, nullptr, nullptr, geoJson, unique, ignoreNull, iid, created);

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

TRI_index_t* TRI_EnsureGeoIndex2DocumentCollection (TRI_document_collection_t* document,
                                                    TRI_idx_iid_t iid,
                                                    char const* latitude,
                                                    char const* longitude,
                                                    bool unique,
                                                    bool ignoreNull,
                                                    bool* created) {
  TRI_index_t* idx;

  TRI_ReadLockReadWriteLock(&document->_vocbase->_inventoryLock);

  // .............................................................................
  // inside write-lock
  // .............................................................................

  TRI_WRITE_LOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(document);

  idx = CreateGeoIndexDocumentCollection(document, nullptr, latitude, longitude, false, unique, ignoreNull, iid, created);

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

static TRI_index_t* CreateHashIndexDocumentCollection (TRI_document_collection_t* document,
                                                       TRI_vector_pointer_t const* attributes,
                                                       TRI_idx_iid_t iid,
                                                       bool unique,
                                                       bool* created) {
  TRI_vector_pointer_t fields;
  TRI_vector_t paths;

  TRI_index_t* idx = nullptr;

  // determine the sorted shape ids for the attributes
  int res = PidNamesByAttributeNames(attributes,
                                     document->getShaper(),  // ONLY IN INDEX, PROTECTED by RUNTIME
                                     &paths,
                                     &fields,
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

  idx = LookupPathIndexDocumentCollection(document, &paths, TRI_IDX_TYPE_HASH_INDEX, unique, false);

  if (idx != nullptr) {
    TRI_DestroyVector(&paths);
    TRI_DestroyVectorPointer(&fields);
    LOG_TRACE("hash-index already created");

    if (created != nullptr) {
      *created = false;
    }

    return idx;
  }

  // create the hash index. we'll provide it with the current number of documents
  // in the collection so the index can do a sensible memory preallocation
  idx = TRI_CreateHashIndex(document,
                            iid,
                            &fields,
                            &paths,
                            unique);

  if (idx == nullptr) {
    TRI_DestroyVector(&paths);
    TRI_DestroyVectorPointer(&fields);
    TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
    return nullptr;
  }

  // release memory allocated to vector
  TRI_DestroyVector(&paths);
  TRI_DestroyVectorPointer(&fields);

  // initialises the index with all existing documents
  res = FillIndex(document, idx);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_FreeHashIndex(idx);

    return nullptr;
  }

  // store index and return
  res = AddIndex(document, idx);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_FreeHashIndex(idx);

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
                              TRI_index_t** dst) {
  return PathBasedIndexFromJson(document, definition, iid, CreateHashIndexDocumentCollection, dst);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief finds a hash index (unique or non-unique)
/// the index lock must be held when calling this function
////////////////////////////////////////////////////////////////////////////////

TRI_index_t* TRI_LookupHashIndexDocumentCollection (TRI_document_collection_t* document,
                                                    TRI_vector_pointer_t const* attributes,
                                                    bool unique) {
  TRI_index_t* idx;
  TRI_vector_pointer_t fields;
  TRI_vector_t paths;
  int res;

  // determine the sorted shape ids for the attributes
  res = PidNamesByAttributeNames(attributes,
                                 document->getShaper(),  // ONLY IN INDEX, PROTECTED by RUNTIME
                                 &paths,
                                 &fields,
                                 true,
                                 false);

  if (res != TRI_ERROR_NO_ERROR) {
    return nullptr;
  }

  idx = LookupPathIndexDocumentCollection(document, &paths, TRI_IDX_TYPE_HASH_INDEX, unique, true);

  // release memory allocated to vector
  TRI_DestroyVector(&paths);
  TRI_DestroyVectorPointer(&fields);

  return idx;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures that a hash index exists
////////////////////////////////////////////////////////////////////////////////

TRI_index_t* TRI_EnsureHashIndexDocumentCollection (TRI_document_collection_t* document,
                                                    TRI_idx_iid_t iid,
                                                    TRI_vector_pointer_t const* attributes,
                                                    bool unique,
                                                    bool* created) {
  TRI_index_t* idx;

  TRI_ReadLockReadWriteLock(&document->_vocbase->_inventoryLock);

  // .............................................................................
  // inside write-lock
  // .............................................................................

  TRI_WRITE_LOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(document);

  // given the list of attributes (as strings)
  idx = CreateHashIndexDocumentCollection(document, attributes, iid, unique, created);

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

static TRI_index_t* CreateSkiplistIndexDocumentCollection (TRI_document_collection_t* document,
                                                           TRI_vector_pointer_t const* attributes,
                                                           TRI_idx_iid_t iid,
                                                           bool unique,
                                                           bool* created) {
  TRI_index_t* idx;
  TRI_vector_pointer_t fields;
  TRI_vector_t paths;
  int res;

  res = PidNamesByAttributeNames(attributes,
                                 document->getShaper(),  // ONLY IN INDEX, PROTECTED by RUNTIME
                                 &paths,
                                 &fields,
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

  idx = LookupPathIndexDocumentCollection(document, &paths, TRI_IDX_TYPE_SKIPLIST_INDEX, unique, false);

  if (idx != nullptr) {
    TRI_DestroyVector(&paths);
    TRI_DestroyVectorPointer(&fields);
    LOG_TRACE("skiplist-index already created");

    if (created != nullptr) {
      *created = false;
    }

    return idx;
  }

  // Create the skiplist index
  idx = TRI_CreateSkiplistIndex(document, iid, &fields, &paths, unique);

  if (idx == nullptr) {
    TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
    return nullptr;
  }

  // release memory allocated to vector
  TRI_DestroyVector(&paths);
  TRI_DestroyVectorPointer(&fields);

  // initialises the index with all existing documents
  res = FillIndex(document, idx);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_FreeSkiplistIndex(idx);

    TRI_set_errno(res);
    return nullptr;
  }

  // store index and return
  res = AddIndex(document, idx);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_FreeSkiplistIndex(idx);

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
                                  TRI_index_t** dst) {
  return PathBasedIndexFromJson(document, definition, iid, CreateSkiplistIndexDocumentCollection, dst);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief finds a skiplist index (unique or non-unique)
/// the index lock must be held when calling this function
////////////////////////////////////////////////////////////////////////////////

TRI_index_t* TRI_LookupSkiplistIndexDocumentCollection (TRI_document_collection_t* document,
                                                        TRI_vector_pointer_t const* attributes,
                                                        bool unique) {
  TRI_index_t* idx;
  TRI_vector_pointer_t fields;
  TRI_vector_t paths;
  int res;

  // determine the unsorted shape ids for the attributes
  res = PidNamesByAttributeNames(attributes,
                                 document->getShaper(),  // ONLY IN INDEX, PROTECTED by RUNTIME
                                 &paths,
                                 &fields,
                                 false,
                                 false);

  if (res != TRI_ERROR_NO_ERROR) {
    return nullptr;
  }

  idx = LookupPathIndexDocumentCollection(document, &paths, TRI_IDX_TYPE_SKIPLIST_INDEX, unique, true);

  // release memory allocated to vector
  TRI_DestroyVector(&paths);
  TRI_DestroyVectorPointer(&fields);

  return idx;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures that a skiplist index exists
////////////////////////////////////////////////////////////////////////////////

TRI_index_t* TRI_EnsureSkiplistIndexDocumentCollection (TRI_document_collection_t* document,
                                                        TRI_idx_iid_t iid,
                                                        TRI_vector_pointer_t const* attributes,
                                                        bool unique,
                                                        bool* created) {
  TRI_index_t* idx;

  TRI_ReadLockReadWriteLock(&document->_vocbase->_inventoryLock);

  // .............................................................................
  // inside write-lock the collection
  // .............................................................................

  TRI_WRITE_LOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(document);

  idx = CreateSkiplistIndexDocumentCollection(document, attributes, iid, unique, created);

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

static TRI_index_t* LookupFulltextIndexDocumentCollection (TRI_document_collection_t* document,
                                                           const char* attributeName,
                                                           const bool indexSubstrings,
                                                           int minWordLength) {
  size_t i;

  TRI_ASSERT(attributeName);

  for (i = 0; i < document->_allIndexes._length; ++i) {
    TRI_index_t* idx = (TRI_index_t*) document->_allIndexes._buffer[i];

    if (idx->_type == TRI_IDX_TYPE_FULLTEXT_INDEX) {
      TRI_fulltext_index_t* fulltext = (TRI_fulltext_index_t*) idx;
      char* fieldName;

      // 2013-01-17: deactivated substring indexing
      // if (fulltext->_indexSubstrings != indexSubstrings) {
      //   continue;
      // }

      if (fulltext->_minWordLength != minWordLength) {
        continue;
      }

      if (fulltext->base._fields._length != 1) {
        continue;
      }

      fieldName = (char*) fulltext->base._fields._buffer[0];

      if (fieldName != 0 && TRI_EqualString(fieldName, attributeName)) {
        return idx;
      }
    }
  }

  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a fulltext index to the collection
////////////////////////////////////////////////////////////////////////////////

static TRI_index_t* CreateFulltextIndexDocumentCollection (TRI_document_collection_t* document,
                                                           const char* attributeName,
                                                           const bool indexSubstrings,
                                                           int minWordLength,
                                                           TRI_idx_iid_t iid,
                                                           bool* created) {
  TRI_index_t* idx;
  int res;

  // ...........................................................................
  // Attempt to find an existing index with the same attribute
  // If a suitable index is found, return that one otherwise we need to create
  // a new one.
  // ...........................................................................

  idx = LookupFulltextIndexDocumentCollection(document, attributeName, indexSubstrings, minWordLength);
  if (idx != nullptr) {
    LOG_TRACE("fulltext-index already created");

    if (created != nullptr) {
      *created = false;
    }
    return idx;
  }

  // Create the fulltext index
  idx = TRI_CreateFulltextIndex(document, iid, attributeName, indexSubstrings, minWordLength);

  if (idx == nullptr) {
    TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
    return nullptr;
  }

  // initialises the index with all existing documents
  res = FillIndex(document, idx);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_FreeFulltextIndex(idx);

    return nullptr;
  }

  // store index and return
  res = AddIndex(document, idx);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_FreeFulltextIndex(idx);

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
                                  TRI_index_t** dst) {
  TRI_index_t* idx;
  TRI_json_t* attribute;
  TRI_json_t* fld;
  // TRI_json_t* indexSubstrings;
  TRI_json_t* minWordLength;
  char* attributeName;
  size_t fieldCount;
  bool doIndexSubstrings;
  int minWordLengthValue;

  if (dst != nullptr) {
    *dst = nullptr;
  }

  // extract fields
  fld = ExtractFields(definition, &fieldCount, iid);

  if (fld == nullptr) {
    return TRI_errno();
  }

  // extract the list of fields
  if (fieldCount != 1) {
    LOG_ERROR("ignoring index %llu, has an invalid number of attributes", (unsigned long long) iid);

    return TRI_set_errno(TRI_ERROR_BAD_PARAMETER);
  }

  attribute = static_cast<TRI_json_t*>(TRI_AtVector(&fld->_value._objects, 0));

  if (! TRI_IsStringJson(attribute)) {
    return TRI_set_errno(TRI_ERROR_BAD_PARAMETER);
  }

  attributeName = attribute->_value._string.data;

  // 2013-01-17: deactivated substring indexing
  // indexSubstrings = TRI_LookupArrayJson(definition, "indexSubstrings");

  doIndexSubstrings = false;
  // if (indexSubstrings != nullptr && indexSubstrings->_type == TRI_JSON_BOOLEAN) {
  //  doIndexSubstrings = indexSubstrings->_value._boolean;
  // }

  minWordLength = TRI_LookupArrayJson(definition, "minLength");
  minWordLengthValue = TRI_FULLTEXT_MIN_WORD_LENGTH_DEFAULT;

  if (minWordLength != nullptr && minWordLength->_type == TRI_JSON_NUMBER) {
    minWordLengthValue = (int) minWordLength->_value._number;
  }

  // create the index
  idx = LookupFulltextIndexDocumentCollection(document, attributeName, doIndexSubstrings, minWordLengthValue);

  if (idx == nullptr) {
    bool created;
    idx = CreateFulltextIndexDocumentCollection(document, attributeName, doIndexSubstrings, minWordLengthValue, iid, &created);
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

TRI_index_t* TRI_LookupFulltextIndexDocumentCollection (TRI_document_collection_t* document,
                                                        const char* attributeName,
                                                        const bool indexSubstrings,
                                                        int minWordLength) {
  TRI_index_t* idx;

  idx = LookupFulltextIndexDocumentCollection(document, attributeName, indexSubstrings, minWordLength);

  return idx;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures that a fulltext index exists
////////////////////////////////////////////////////////////////////////////////

TRI_index_t* TRI_EnsureFulltextIndexDocumentCollection (TRI_document_collection_t* document,
                                                        TRI_idx_iid_t iid,
                                                        const char* attributeName,
                                                        const bool indexSubstrings,
                                                        int minWordLength,
                                                        bool* created) {
  TRI_index_t* idx;

  TRI_ReadLockReadWriteLock(&document->_vocbase->_inventoryLock);

  // .............................................................................
  // inside write-lock the collection
  // .............................................................................

  TRI_WRITE_LOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(document);

  idx = CreateFulltextIndexDocumentCollection(document, attributeName, indexSubstrings, minWordLength, iid, created);

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

////////////////////////////////////////////////////////////////////////////////
/// @brief checks for match of an example
////////////////////////////////////////////////////////////////////////////////

static bool IsExampleMatch (TRI_transaction_collection_t*,
                            TRI_shaper_t* shaper,
                            TRI_doc_mptr_t const* doc,
                            size_t len,
                            TRI_shape_pid_t* pids,
                            TRI_shaped_json_t** values) {
  // The first argument is only there to make the compiler check that
  // a transaction is ongoing. We need this to verify the protection
  // of master pointer and data pointer access.
  TRI_shaped_json_t document;
  TRI_shaped_json_t result;
  TRI_shape_t const* shape;

  TRI_EXTRACT_SHAPED_JSON_MARKER(document, doc->getDataPtr());  // PROTECTED by trx coming from above

  for (size_t i = 0;  i < len;  ++i) {
    TRI_shaped_json_t* example = values[i];

    bool ok = TRI_ExtractShapedJsonVocShaper(shaper,
                                             &document,
                                             example->_sid,
                                             pids[i],
                                             &result,
                                             &shape);

    if (! ok || shape == nullptr) {
      return false;
    }

    if (result._data.length != example->_data.length) {
      // suppress excessive log spam
      // LOG_TRACE("expecting length %lu, got length %lu for path %lu",
      //           (unsigned long) result._data.length,
      //           (unsigned long) example->_data.length,
      //           (unsigned long) pids[i]);

      return false;
    }

    if (memcmp(result._data.data, example->_data.data, example->_data.length) != 0) {
      // suppress excessive log spam
      // LOG_TRACE("data mismatch at path %lu", (unsigned long) pids[i]);
      return false;
    }
  }

  return true;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a select-by-example query
////////////////////////////////////////////////////////////////////////////////

std::vector<TRI_doc_mptr_copy_t> TRI_SelectByExample (
                          TRI_transaction_collection_t* trxCollection,
                          size_t length,
                          TRI_shape_pid_t* pids,
                          TRI_shaped_json_t** values) {

  TRI_document_collection_t* document = trxCollection->_collection->_collection;

  TRI_shaper_t* shaper = document->getShaper();  // PROTECTED by trx in trxCollection

  // use filtered to hold copies of the master pointer
  std::vector<TRI_doc_mptr_copy_t> filtered;

  // do a full scan
  TRI_doc_mptr_t** ptr = (TRI_doc_mptr_t**) (document->_primaryIndex._table);
  TRI_doc_mptr_t** end = (TRI_doc_mptr_t**) ptr + document->_primaryIndex._nrAlloc;

  for (;  ptr < end;  ++ptr) {
    if (*ptr != nullptr &&
        IsExampleMatch(trxCollection, shaper, *ptr, length, pids, values)) {
      filtered.push_back(**ptr);
    }
  }
  return filtered;
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
  {
    TRI_IF_FAILURE("RemoveDocumentNoLock") {
      // test what happens if no lock can be acquired
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
    keyString = document->_keyGenerator->generate(tick);

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

  uint64_t const hash = TRI_HashKeyPrimaryIndex(keyString.c_str());


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

    if (res == TRI_ERROR_NO_ERROR) {
      TRI_ASSERT(mptr->getDataPtr() != nullptr);  // PROTECTED by trx in trxCollection
    }
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
  }

  if (res == TRI_ERROR_NO_ERROR) {
    TRI_ASSERT(mptr->getDataPtr() != nullptr);  // PROTECTED by trx in trxCollection
    TRI_ASSERT(mptr->_rid > 0);
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

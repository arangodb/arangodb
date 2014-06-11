////////////////////////////////////////////////////////////////////////////////
/// @brief document collection with global read-write lock
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2013 triAGENS GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "document-collection.h"

#include "BasicsC/conversions.h"
#include "BasicsC/files.h"
#include "BasicsC/hashes.h"
#include "BasicsC/logging.h"
#include "BasicsC/tri-strings.h"
#include "CapConstraint/cap-constraint.h"
#include "FulltextIndex/fulltext-index.h"
#include "GeoIndex/geo-index.h"
#include "HashIndex/hash-index.h"
#include "ShapedJson/shape-accessor.h"
#include "Utils/transactions.h"
#include "Utils/CollectionReadLocker.h"
#include "Utils/CollectionWriteLocker.h"
#include "VocBase/edge-collection.h"
#include "VocBase/index.h"
#include "VocBase/key-generator.h"
#include "VocBase/primary-index.h"
#include "VocBase/replication-logger.h"
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

static int BitarrayIndexFromJson (TRI_document_collection_t*,
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
/// @brief creates a compactor file
////////////////////////////////////////////////////////////////////////////////

static TRI_datafile_t* CreateCompactor (TRI_document_collection_t* document, 
                                        TRI_voc_fid_t fid,
                                        TRI_voc_size_t maximalSize) {
  TRI_col_header_marker_t cm;
  TRI_collection_t* collection;
  TRI_datafile_t* journal;
  TRI_df_marker_t* position;
  int res;

  collection = &document->base;

  if (collection->_info._isVolatile) {
    // in-memory collection
    journal = TRI_CreateDatafile(NULL, fid, maximalSize, true);
  }
  else {
    char* jname;
    char* number;
    char* filename;

    number   = TRI_StringUInt64(fid);
    jname    = TRI_Concatenate3String("compaction-", number, ".db");
    filename = TRI_Concatenate2File(collection->_directory, jname);

    TRI_FreeString(TRI_CORE_MEM_ZONE, number);
    TRI_FreeString(TRI_CORE_MEM_ZONE, jname);

    if (TRI_ExistsFile(filename)) {
      // remove any existing temporary file first
      TRI_UnlinkFile(filename);
    }

    journal = TRI_CreateDatafile(filename, fid, maximalSize, true);
    TRI_FreeString(TRI_CORE_MEM_ZONE, filename);
  }

  if (journal == NULL) {
    if (TRI_errno() == TRI_ERROR_OUT_OF_MEMORY_MMAP) {
      collection->_lastError = TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY_MMAP);
      collection->_state = TRI_COL_STATE_READ;
    }
    else {
      collection->_lastError = TRI_set_errno(TRI_ERROR_ARANGO_NO_JOURNAL);
      collection->_state = TRI_COL_STATE_WRITE_ERROR;
    }

    return NULL;
  }

  LOG_TRACE("created new compactor '%s'", journal->getName(journal));


  // create a collection header, still in the temporary file
  res = TRI_ReserveElementDatafile(journal, sizeof(TRI_col_header_marker_t), &position, maximalSize);

  if (res != TRI_ERROR_NO_ERROR) {
    collection->_lastError = journal->_lastError;
    LOG_ERROR("cannot create document header in compactor '%s': %s", journal->getName(journal), TRI_last_error());

    TRI_FreeDatafile(journal);

    return NULL;
  }


  TRI_InitMarkerDatafile((char*) &cm, TRI_COL_MARKER_HEADER, sizeof(TRI_col_header_marker_t));
  cm.base._tick = (TRI_voc_tick_t) fid;
  cm._type = (TRI_col_type_t) collection->_info._type;
  cm._cid  = collection->_info._cid;

  res = TRI_WriteCrcElementDatafile(journal, position, &cm.base, sizeof(cm), false);

  if (res != TRI_ERROR_NO_ERROR) {
    collection->_lastError = journal->_lastError;
    LOG_ERROR("cannot create document header in compactor '%s': %s", journal->getName(journal), TRI_last_error());

    TRI_FreeDatafile(journal);

    return NULL;
  }

  TRI_ASSERT(fid == journal->_fid);

  return journal;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief closes a journal
///
/// Note that the caller must hold a lock protecting the _datafiles and
/// _journals entry.
////////////////////////////////////////////////////////////////////////////////

static bool CloseJournalDocumentCollection (TRI_document_collection_t* document,
                                            size_t position,
                                            bool compactor) {
  TRI_collection_t* collection;
  TRI_vector_pointer_t* vector;
  int res;

  collection = &document->base;

  // either use a journal or a compactor
  if (compactor) {
    vector = &collection->_compactors;
  }
  else {
    vector = &collection->_journals;
  }

  // no journal at this position
  if (vector->_length <= position) {
    TRI_set_errno(TRI_ERROR_ARANGO_NO_JOURNAL);
    return false;
  }

  // seal and rename datafile
  TRI_datafile_t* journal = static_cast<TRI_datafile_t*>(vector->_buffer[position]);
  res = TRI_SealDatafile(journal);

  if (res != TRI_ERROR_NO_ERROR) {
    LOG_ERROR("failed to seal datafile '%s': %s", journal->getName(journal), TRI_last_error());

    if (! compactor) {
      TRI_RemoveVectorPointer(vector, position);
      TRI_PushBackVectorPointer(&collection->_datafiles, journal);
    }

    return false;
  }

  if (! compactor && journal->isPhysical(journal)) {
    // rename the file
    char* dname;
    char* filename;
    char* number;
    bool ok;

    number = TRI_StringUInt64(journal->_fid);
    dname = TRI_Concatenate3String("datafile-", number, ".db");
    filename = TRI_Concatenate2File(collection->_directory, dname);

    TRI_FreeString(TRI_CORE_MEM_ZONE, dname);
    TRI_FreeString(TRI_CORE_MEM_ZONE, number);

    ok = TRI_RenameDatafile(journal, filename);

    if (! ok) {
      LOG_ERROR("failed to rename datafile '%s' to '%s': %s", journal->getName(journal), filename, TRI_last_error());

      TRI_RemoveVectorPointer(vector, position);
      TRI_PushBackVectorPointer(&collection->_datafiles, journal);
      TRI_FreeString(TRI_CORE_MEM_ZONE, filename);

      return false;
    }

    TRI_FreeString(TRI_CORE_MEM_ZONE, filename);

    LOG_TRACE("closed file '%s'", journal->getName(journal));
  }

  if (! compactor) {
    TRI_RemoveVectorPointer(vector, position);
    TRI_PushBackVectorPointer(&collection->_datafiles, journal);
  }

  return true;
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
  TRI_col_info_t* info = &document->base._info;

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
  TRI_ASSERT(header->getDataPtr() != nullptr);  // ONLY IN INDEX
  
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
            TRI_EXTRACT_MARKER_KEY(header),  // ONLY IN INDEX
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
  int result = TRI_ERROR_NO_ERROR;
  size_t const n = document->_allIndexes._length;

  for (size_t i = 0;  i < n;  ++i) {
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

  TRI_doc_mptr_t* found = static_cast<TRI_doc_mptr_t*>(TRI_RemoveKeyPrimaryIndex(&document->_primaryIndex, TRI_EXTRACT_MARKER_KEY(header))); // ONLY IN INDEX

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
  size_t const n = document->_allIndexes._length;
  int result = TRI_ERROR_NO_ERROR;

  for (size_t i = 0;  i < n;  ++i) {
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
  header = document->_headersPtr->request(document->_headersPtr, markerSize);  // ONLY IN OPENITERATOR

  if (header == nullptr) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  header->_rid     = marker->_rid;
  header->_fid     = fid;
  header->setDataPtr(marker);  // ONLY IN OPENITERATOR
  header->_hash    = TRI_FnvHashString(TRI_EXTRACT_MARKER_KEY(header));  // ONLY IN OPENITERATOR
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
/// @brief rolls back a single update operation
////////////////////////////////////////////////////////////////////////////////

static int RollbackUpdate (TRI_document_collection_t* document,
                           TRI_doc_mptr_t* header,
                           TRI_doc_mptr_copy_t const* oldHeader) {
  TRI_ASSERT(header != nullptr);
  TRI_ASSERT(oldHeader != nullptr);

  // ignore any errors we're getting from this
  DeleteSecondaryIndexes(document, header, true);

  header->copy(*oldHeader); 

  int res = InsertSecondaryIndexes(document, header, true);

  if (res != TRI_ERROR_NO_ERROR) {
    LOG_ERROR("error rolling back update operation");
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief updates an existing header
////////////////////////////////////////////////////////////////////////////////

static void UpdateHeader (TRI_voc_fid_t fid,
                          TRI_df_marker_t const* m,
                          TRI_doc_mptr_t* newHeader,
                          TRI_doc_mptr_t const* oldHeader) {
  TRI_doc_document_key_marker_t const* marker;

  marker = (TRI_doc_document_key_marker_t const*) m;

  TRI_ASSERT(marker != NULL);
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
             document->base._info._name,
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
            document->base._info._name);

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
    TRI_index_t* idx = (TRI_index_t*) document->_allIndexes._buffer[i];

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

      if (idx->cleanup != NULL) {
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
  size_t const n = document->_allIndexes._length;

  for (size_t i = 0;  i < n;  ++i) {
    TRI_index_t* idx = static_cast<TRI_index_t*>(document->_allIndexes._buffer[i]);

    if (idx->postInsert != NULL) {
      idx->postInsert(trxCollection, idx, header);
    }
  }

  // TODO: post-insert will never return an error
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

  operation.indexed();

  res = TRI_AddOperationTransaction(operation, syncRequested);
      
  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  *mptr = *header;

  res = PostInsertIndexes(trxCollection, header);

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief insert a shaped-json document (or edge)
/// note: key might be NULL. in this case, a key is auto-generated
////////////////////////////////////////////////////////////////////////////////

static int InsertDocumentShapedJson (TRI_transaction_collection_t* trxCollection,
                                     TRI_voc_key_t key,
                                     TRI_voc_rid_t rid,
                                     TRI_doc_mptr_copy_t* mptr,
                                     TRI_df_marker_type_e markerType,
                                     TRI_shaped_json_t const* shaped,
                                     TRI_document_edge_t const* edge,
                                     bool lock,
                                     bool forceSync,
                                     bool isRestore) {

  // TODO: isRestore is not used yet!
  TRI_ASSERT(mptr != nullptr);
  mptr->setDataPtr(nullptr);  // PROTECTED by trx in trxCollection

  rid = GetRevisionId(rid);
  TRI_voc_tick_t tick = static_cast<TRI_voc_tick_t>(rid);
  
  TRI_document_collection_t* document = trxCollection->_collection->_collection;
  TRI_key_generator_t* keyGenerator = static_cast<TRI_key_generator_t*>(document->_keyGenerator);

  std::string keyString;

  if (key == nullptr) {
    // no key specified, now generate a new one
    keyString = keyGenerator->generateKey(keyGenerator, tick);

    if (keyString.empty()) {
      return TRI_ERROR_ARANGO_OUT_OF_KEYS;
    }
  }
  else {
    // key was specified, now validate it
    int res = keyGenerator->validateKey(keyGenerator, key);

    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }

    keyString = key;
  }

  uint64_t hash = TRI_FnvHashPointer(keyString.c_str(), keyString.size());

  // construct a legend for the shaped json
  triagens::basics::JsonLegend legend(document->_shaper);
  int res = legend.addShape(shaped->_sid, &shaped->_data);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  triagens::wal::Marker* marker = nullptr;

  if (markerType == TRI_DOC_MARKER_KEY_DOCUMENT) {
    // document
    TRI_ASSERT(edge == nullptr);

    marker = new triagens::wal::DocumentMarker(document->base._vocbase->_id,
                                               document->base._info._cid,
                                               rid,
                                               trxCollection->_transaction->_id,
                                               keyString,
                                               legend,
                                               shaped);
  }
  else if (markerType == TRI_DOC_MARKER_KEY_EDGE) {
    // edge
    TRI_ASSERT(edge != nullptr);

    marker = new triagens::wal::EdgeMarker(document->base._vocbase->_id,
                                           document->base._info._cid,
                                           rid,
                                           trxCollection->_transaction->_id,
                                           keyString,
                                           edge,
                                           legend,
                                           shaped);
  }
  else {
    // invalid marker type
    return TRI_ERROR_INTERNAL;
  }


  TRI_ASSERT(marker != nullptr);

  // now insert into indexes
  {
    triagens::arango::CollectionWriteLocker collectionLocker(document, lock);
  
    triagens::wal::DocumentOperation operation(marker, trxCollection, TRI_VOC_DOCUMENT_OPERATION_INSERT, rid);

    // create a new header
    TRI_doc_mptr_t* header = operation.header = document->_headersPtr->request(document->_headersPtr, marker->size());  // PROTECTED by trx in trxCollection

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
/// @brief reads an element from the document collection
////////////////////////////////////////////////////////////////////////////////

static int ReadDocumentShapedJson (TRI_transaction_collection_t* trxCollection,
                                   const TRI_voc_key_t key,
                                   TRI_doc_mptr_copy_t* mptr,
                                   bool lock) {
  TRI_ASSERT(mptr != nullptr);
  mptr->setDataPtr(nullptr);  // PROTECTED by trx in trxCollection

  {
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
  newHeader->_rid  = operation.rid;
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

  res = TRI_AddOperationTransaction(operation, syncRequested);

  if (res == TRI_ERROR_NO_ERROR) {
    // write new header into result  
    *mptr = *((TRI_doc_mptr_t*) newHeader);
  }
  else {
    // TODO: check if rollbackupdate will do any harm here
    RollbackUpdate(document, newHeader, &oldData);
  }
    
  return res;
}
 
////////////////////////////////////////////////////////////////////////////////
/// @brief updates a document in the collection from shaped json
////////////////////////////////////////////////////////////////////////////////

static int UpdateDocumentShapedJson (TRI_transaction_collection_t* trxCollection,
                                     TRI_voc_key_t key,
                                     TRI_voc_rid_t rid,
                                     TRI_doc_mptr_copy_t* mptr,
                                     TRI_shaped_json_t const* shaped,
                                     TRI_doc_update_policy_t const* policy,
                                     bool lock,
                                     bool forceSync) {

  rid = GetRevisionId(rid);

  TRI_ASSERT(key != nullptr);

  // initialise the result
  TRI_ASSERT(mptr != nullptr);
  mptr->setDataPtr(nullptr);  // PROTECTED by trx in trxCollection
    
  TRI_document_collection_t* document = trxCollection->_collection->_collection;
  
  // create legend  
  triagens::basics::JsonLegend legend(document->_shaper);
  int res = legend.addShape(shaped->_sid, &shaped->_data);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }
    
  {
    triagens::arango::CollectionWriteLocker collectionLocker(document, lock);

    // get the header pointer of the previous revision
    TRI_doc_mptr_t* oldHeader;
    res = LookupDocument(document, key, policy, oldHeader);
    
    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }

    triagens::wal::Marker* marker = nullptr;
    TRI_df_marker_t const* original = static_cast<TRI_df_marker_t const*>(oldHeader->getDataPtr());  // PROTECTED by trx in trxCollection

    if (original->_type == TRI_WAL_MARKER_DOCUMENT ||
        original->_type == TRI_DOC_MARKER_KEY_DOCUMENT) {
      // create a WAL document marker
    
      marker = triagens::wal::DocumentMarker::clone(original,
                                                    document->base._vocbase->_id,
                                                    document->base._info._cid,
                                                    rid,
                                                    trxCollection->_transaction->_id,
                                                    legend,
                                                    shaped);
    }
    else if (original->_type == TRI_WAL_MARKER_EDGE ||
             original->_type == TRI_DOC_MARKER_KEY_EDGE) {
      // create a WAL edge marker

      marker = triagens::wal::EdgeMarker::clone(original,
                                                document->base._vocbase->_id,
                                                document->base._info._cid,
                                                rid,
                                                trxCollection->_transaction->_id,
                                                legend,
                                                shaped);
    }
    else {
      // invalid marker type
      return TRI_ERROR_INTERNAL;
    }
  
    triagens::wal::DocumentOperation operation(marker, trxCollection, TRI_VOC_DOCUMENT_OPERATION_UPDATE, rid);
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
  
////////////////////////////////////////////////////////////////////////////////
/// @brief removes a shaped-json document (or edge)
////////////////////////////////////////////////////////////////////////////////

static int RemoveDocumentShapedJson (TRI_transaction_collection_t* trxCollection,
                                     TRI_voc_key_t key,
                                     TRI_voc_rid_t rid,
                                     TRI_doc_update_policy_t const* policy,
                                     bool lock,
                                     bool forceSync) {
  rid = GetRevisionId(rid);
 
  TRI_ASSERT(key != nullptr);
 
  TRI_document_collection_t* document = trxCollection->_collection->_collection;

  triagens::wal::Marker* marker = new triagens::wal::RemoveMarker(document->base._vocbase->_id,
                                                                  document->base._info._cid,
                                                                  rid,
                                                                  trxCollection->_transaction->_id,
                                                                  std::string(key));

  TRI_doc_mptr_t* header;
  int res;
  {
    triagens::arango::CollectionWriteLocker collectionLocker(document, lock);
  
    triagens::wal::DocumentOperation operation(marker, trxCollection, TRI_VOC_DOCUMENT_OPERATION_REMOVE, rid);

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

    res = TRI_AddOperationTransaction(operation, forceSync);
  }

  return res;
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
  if (state->_fid != datafile->_fid) {
    TRI_document_collection_t* document = state->_document; 

    state->_fid = datafile->_fid;
    state->_dfi = TRI_FindDatafileInfoDocumentCollection(document, datafile->_fid, true);
  }

  if (state->_dfi != nullptr) {
    state->_dfi->_numberDead++;
    state->_dfi->_sizeDead += (int64_t) TRI_DF_ALIGN_BLOCK(marker->_size);
  }
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
  
  key = ((char*) d) + d->_offsetKey;
  if (document->_keyGenerator->trackKey != nullptr) {
    document->_keyGenerator->trackKey(document->_keyGenerator, key);
  }

  // no primary index lock required here because we are the only ones reading from the index ATM
  found = static_cast<TRI_doc_mptr_t const*>(TRI_LookupByKeyPrimaryIndex(&document->_primaryIndex, key));

  // it is a new entry
  if (found == NULL) {
    TRI_doc_mptr_t* header;
    int res;

    // get a header
    res = CreateHeader(document, (TRI_doc_document_key_marker_t*) marker, operation->_fid, &header);

    if (res != TRI_ERROR_NO_ERROR) {
      LOG_ERROR("out of memory");
      
      return TRI_set_errno(res);
    }

    TRI_ASSERT(header != NULL);

    // insert into primary index
    res = InsertPrimaryIndex(document, header, false);

    if (res != TRI_ERROR_NO_ERROR) {
      // insertion failed
      LOG_ERROR("inserting document into indexes failed");
      document->_headersPtr->release(document->_headersPtr, header, true);  // ONLY IN OPENITERATOR

      return res;
    }
    
    document->_numberDocuments++;

    // update the datafile info
    if (state->_dfi != NULL) {
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
    document->_headersPtr->moveBack(document->_headersPtr, newHeader, &oldData);  // ONLY IN OPENITERATOR
      
    // update the datafile info
    if (oldData._fid == state->_fid) {
      dfi = state->_dfi;
    }
    else {
      dfi = TRI_FindDatafileInfoDocumentCollection(document, oldData._fid, true);
    }

    if (dfi != NULL && found->getDataPtr() != NULL) {  // ONLY IN OPENITERATOR
      int64_t size;

      TRI_ASSERT(found->getDataPtr() != NULL);  // ONLY IN OPENITERATOR
      size = (int64_t) ((TRI_df_marker_t*) found->getDataPtr())->_size;  // ONLY IN OPENITERATOR

      dfi->_numberAlive--;
      dfi->_sizeAlive -= TRI_DF_ALIGN_BLOCK(size);

      dfi->_numberDead++;
      dfi->_sizeDead += TRI_DF_ALIGN_BLOCK(size);
    }

    if (state->_dfi != NULL) {
      state->_dfi->_numberAlive++;
      state->_dfi->_sizeAlive += (int64_t) TRI_DF_ALIGN_BLOCK(marker->_size);
    }
  }

  // it is a stale update
  else {
    if (state->_dfi != NULL) {
      TRI_ASSERT(found->getDataPtr() != NULL);  // ONLY IN OPENITERATOR

      state->_dfi->_numberDead++;
      state->_dfi->_sizeDead += (int64_t) TRI_DF_ALIGN_BLOCK(((TRI_df_marker_t*) found->getDataPtr())->_size);  // ONLY IN OPENITERATOR
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
  
  if (state->_fid != operation->_fid) {
    // update the state
    state->_fid = operation->_fid;
    state->_dfi = TRI_FindDatafileInfoDocumentCollection(document, operation->_fid, true);
  }

  key = ((char*) d) + d->_offsetKey;

  LOG_TRACE("deletion: fid %llu, key %s, rid %llu, deletion %llu",
            (unsigned long long) operation->_fid,
            (char*) key,
            (unsigned long long) d->_rid,
            (unsigned long long) marker->_tick);

  if (document->_keyGenerator->trackKey != nullptr) {
    document->_keyGenerator->trackKey(document->_keyGenerator, key);
  }

  // no primary index lock required here because we are the only ones reading from the index ATM
  found = static_cast<TRI_doc_mptr_t*>(TRI_LookupByKeyPrimaryIndex(&document->_primaryIndex, key));

  // it is a new entry, so we missed the create
  if (found == NULL) {
    // update the datafile info
    if (state->_dfi != NULL) {
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

    if (dfi != NULL) {
      int64_t size;

      TRI_ASSERT(found->getDataPtr() != NULL);  // ONLY IN OPENITERATOR

      size = (int64_t) ((TRI_df_marker_t*) found->getDataPtr())->_size;  // ONLY IN OPENITERATOR

      dfi->_numberAlive--;
      dfi->_sizeAlive -= TRI_DF_ALIGN_BLOCK(size);

      dfi->_numberDead++;
      dfi->_sizeDead += TRI_DF_ALIGN_BLOCK(size);
    }

    if (state->_dfi != NULL) {
      state->_dfi->_numberDeletion++;
    }

    DeletePrimaryIndex(document, found, false);

    // free the header
    document->_headersPtr->release(document->_headersPtr, found, true);   // ONLY IN OPENITERATOR
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
  int res = TRI_InsertShapeVocShaper(document->_shaper, marker);
  
  if (res == TRI_ERROR_NO_ERROR) {
    if (state->_fid != datafile->_fid) {
      state->_fid = datafile->_fid;
      state->_dfi = TRI_FindDatafileInfoDocumentCollection(document, state->_fid, true);
    }

    if (state->_dfi != NULL) {
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

  int res = TRI_InsertAttributeVocShaper(document->_shaper, marker); 

  if (res == TRI_ERROR_NO_ERROR) { 
    if (state->_fid != datafile->_fid) {
      state->_fid = datafile->_fid;
      state->_dfi = TRI_FindDatafileInfoDocumentCollection(document, state->_fid, true);
    }

    if (state->_dfi != NULL) {
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

  TRI_document_collection_t* document = static_cast<open_iterator_state_t*>(data)->_document;
  if (document->base._tickMax < tick) {
    document->base._tickMax = tick;
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
  TRI_json_t* json;
  int res;

  // load json description of the index
  json = TRI_JsonFile(TRI_CORE_MEM_ZONE, filename, NULL);
  
  // json must be a index description
  if (! TRI_IsArrayJson(json)) {
    LOG_ERROR("cannot read index definition from '%s'", filename);
    if (json != NULL) {
      TRI_FreeJson(TRI_CORE_MEM_ZONE, json);
    }

    return false;
  }

  res = TRI_FromJsonIndexDocumentCollection(static_cast<TRI_document_collection_t*>(data), json, NULL);
  TRI_FreeJson(TRI_CORE_MEM_ZONE, json);

  if (res != TRI_ERROR_NO_ERROR) {
    LOG_ERROR("cannot read index definition from '%s': %s", filename, TRI_errno_string(res));

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
  TRI_collection_t* base = &document->base;

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

  return info;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises a document collection
////////////////////////////////////////////////////////////////////////////////

static int InitBaseDocumentCollection (TRI_document_collection_t* document,
                                       TRI_shaper_t* shaper) {
  document->_shaper             = shaper;
  document->_capConstraint      = nullptr;
  document->_keyGenerator       = nullptr;
  document->_numberDocuments    = 0;
  document->_lastCompaction     = 0.0;

  document->size                = Count;


  int res = TRI_InitAssociativePointer(&document->_datafileInfo,
                                       TRI_UNKNOWN_MEM_ZONE,
                                       HashKeyDatafile,
                                       HashElementDatafile,
                                       IsEqualKeyElementDatafile,
                                       NULL);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  res = TRI_InitPrimaryIndex(&document->_primaryIndex, TRI_UNKNOWN_MEM_ZONE);

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
    TRI_FreeKeyGenerator(document->_keyGenerator);
  }

  TRI_DestroyReadWriteLock(&document->_compactionLock);
  TRI_DestroyReadWriteLock(&document->_lock);

  TRI_DestroyPrimaryIndex(&document->_primaryIndex);

  if (document->_shaper != nullptr) {
    TRI_FreeVocShaper(document->_shaper);
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

  TRI_DestroyCollection(&document->base);
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
    TRI_DestroyCollection(&document->base);
    TRI_set_errno(res);

    return false;
  }

  document->_headersPtr = TRI_CreateSimpleHeaders();  // ONLY IN CREATE COLLECTION

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
  if (document->base._info._type == TRI_COL_TYPE_EDGE) {
    TRI_index_t* edgesIndex;

    edgesIndex = TRI_CreateEdgeIndex(document, document->base._info._cid);

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
  document->insertDocument    = InsertDocumentShapedJson;
  document->removeDocument    = RemoveDocumentShapedJson;
  document->updateDocument    = UpdateDocumentShapedJson;
  document->readDocument      = ReadDocumentShapedJson;
  document->cleanupIndexes    = CleanupIndexes;

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief iterate all markers of the collection
////////////////////////////////////////////////////////////////////////////////

static int IterateMarkersCollection (TRI_collection_t* collection) {
  open_iterator_state_t openState;
  int res;

  // initialise state for iteration
  openState._document       = reinterpret_cast<TRI_document_collection_t*>(collection);
  openState._tid            = 0;
  openState._trxPrepared    = false;
  openState._trxCollections = 0;
  openState._fid            = 0;
  openState._dfi            = NULL;
  openState._vocbase        = collection->_vocbase;
  
  res = TRI_InitVector2(&openState._operations, TRI_UNKNOWN_MEM_ZONE, sizeof(open_iterator_operation_t), OpenIteratorBufferSize);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }
  
  // read all documents and fill primary index
  TRI_IterateCollection(collection, OpenIterator, &openState);

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
                                                         TRI_col_info_t* parameter,
                                                         TRI_voc_cid_t cid) {
  TRI_collection_t* collection;
  TRI_shaper_t* shaper;
  TRI_document_collection_t* document;
  TRI_key_generator_t* keyGenerator;
  int res;

  if (cid > 0) {
    TRI_UpdateTickServer(cid);
  }
  else {
    cid = TRI_NewTickServer();
  }

  parameter->_cid = cid;

  // check if we can generate the key generator
  res = TRI_CreateKeyGenerator(parameter->_keyOptions, &keyGenerator);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_set_errno(res);

    return nullptr;
  }

  TRI_ASSERT(keyGenerator != nullptr);


  // first create the document collection
  document = static_cast<TRI_document_collection_t*>(TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_document_collection_t), false));

  if (document == nullptr) {
    TRI_FreeKeyGenerator(keyGenerator);
    LOG_WARNING("cannot create document collection");
    TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);

    return nullptr;
  }

  collection = TRI_CreateCollection(vocbase, &document->base, path, parameter);

  if (collection == nullptr) {
    TRI_FreeKeyGenerator(keyGenerator);
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, document);
    LOG_ERROR("cannot create document collection");

    return nullptr;
  }

  shaper = TRI_CreateVocShaper(vocbase, document);

  if (shaper == NULL) {
    LOG_ERROR("cannot create shaper");

    TRI_FreeKeyGenerator(keyGenerator);
    TRI_CloseCollection(collection);
    TRI_FreeCollection(collection); // will free document

    return nullptr;
  }

  // create document collection and shaper
  if (false == InitDocumentCollection(document, shaper)) {
    LOG_ERROR("cannot initialise document collection");

    // TODO: shouldn't we destroy &document->_allIndexes, free document->_headersPtr etc.?
    TRI_FreeKeyGenerator(keyGenerator);
    TRI_CloseCollection(collection);
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, collection); // will free document

    return nullptr;
  }

  document->_keyGenerator = keyGenerator;
  
  // save the parameter block (within create, no need to lock)
  res = TRI_SaveCollectionInfo(collection->_directory, parameter, vocbase->_settings.forceSyncProperties);

  if (res != TRI_ERROR_NO_ERROR) {
    // TODO: shouldn't we destroy &document->_allIndexes, free document->_headersPtr etc.?
    LOG_ERROR("cannot save collection parameters in directory '%s': '%s'", 
              collection->_directory, 
              TRI_last_error());

    TRI_CloseCollection(collection);
    TRI_FreeCollection(collection); // will free document

    return nullptr;
  }

  TRI_ASSERT(document->_shaper != nullptr);

  return document;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the memory allocated, but does not free the pointer
///
/// Note that the collection must be closed first.
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyDocumentCollection (TRI_document_collection_t* document) {
  TRI_DestroyCondition(&document->_journalsCondition);

  TRI_FreeSimpleHeaders(document->_headersPtr);  // PROTECTED because collection is already closed

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
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, document);
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

TRI_datafile_t* TRI_CreateJournalDocumentCollection (TRI_document_collection_t* document,
                                                     TRI_voc_size_t journalSize) {
  TRI_col_header_marker_t cm;
  TRI_collection_t* collection;
  TRI_datafile_t* journal;
  TRI_df_marker_t* position;
  TRI_voc_fid_t fid;
  int res;

  collection = &document->base;

  fid = (TRI_voc_fid_t) TRI_NewTickServer();

  if (collection->_info._isVolatile) {
    // in-memory collection
    journal = TRI_CreateDatafile(nullptr, fid, journalSize, true);
  }
  else {
    char* jname;
    char* number;
    char* filename;

    // construct a suitable filename (which is temporary at the beginning)
    number   = TRI_StringUInt64(fid);
    jname    = TRI_Concatenate3String("temp-", number, ".db");
    filename = TRI_Concatenate2File(collection->_directory, jname);

    TRI_FreeString(TRI_CORE_MEM_ZONE, number);
    TRI_FreeString(TRI_CORE_MEM_ZONE, jname);

    journal = TRI_CreateDatafile(filename, fid, journalSize, true);
    TRI_FreeString(TRI_CORE_MEM_ZONE, filename);
  }

  if (journal == NULL) {
    if (TRI_errno() == TRI_ERROR_OUT_OF_MEMORY_MMAP) {
      collection->_lastError = TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY_MMAP);
      collection->_state = TRI_COL_STATE_READ;
    }
    else {
      collection->_lastError = TRI_set_errno(TRI_ERROR_ARANGO_NO_JOURNAL);
      collection->_state = TRI_COL_STATE_WRITE_ERROR;
    }

    return NULL;
  }

  LOG_TRACE("created new journal '%s'", journal->getName(journal));


  // create a collection header, still in the temporary file
  res = TRI_ReserveElementDatafile(journal, sizeof(TRI_col_header_marker_t), &position, journalSize);

  if (res != TRI_ERROR_NO_ERROR) {
    collection->_lastError = journal->_lastError;
    LOG_ERROR("cannot create document header in journal '%s': %s", journal->getName(journal), TRI_last_error());

    TRI_FreeDatafile(journal);

    return NULL;
  }


  TRI_InitMarkerDatafile((char*) &cm, TRI_COL_MARKER_HEADER, sizeof(TRI_col_header_marker_t));
  cm.base._tick = (TRI_voc_tick_t) fid;
  cm._type = (TRI_col_type_t) collection->_info._type;
  cm._cid  = collection->_info._cid;

  res = TRI_WriteCrcElementDatafile(journal, position, &cm.base, sizeof(cm), true);

  if (res != TRI_ERROR_NO_ERROR) {
    collection->_lastError = journal->_lastError;
    LOG_ERROR("cannot create document header in journal '%s': %s", journal->getName(journal), TRI_last_error());

    TRI_FreeDatafile(journal);

    return NULL;
  }

  TRI_ASSERT(fid == journal->_fid);


  // if a physical file, we can rename it from the temporary name to the correct name
  if (journal->isPhysical(journal)) {
    char* jname;
    char* number;
    char* filename;
    bool ok;

    // and use the correct name
    number = TRI_StringUInt64(journal->_fid);
    jname = TRI_Concatenate3String("journal-", number, ".db");

    filename = TRI_Concatenate2File(collection->_directory, jname);

    TRI_FreeString(TRI_CORE_MEM_ZONE, number);
    TRI_FreeString(TRI_CORE_MEM_ZONE, jname);

    ok = TRI_RenameDatafile(journal, filename);

    if (! ok) {
      LOG_ERROR("failed to rename the journal to '%s': %s", filename, TRI_last_error());
      TRI_FreeDatafile(journal);
      TRI_FreeString(TRI_CORE_MEM_ZONE, filename);

      return NULL;
    }
    else {
      LOG_TRACE("renamed journal from %s to '%s'", journal->getName(journal), filename);
    }

    TRI_FreeString(TRI_CORE_MEM_ZONE, filename);
  }

  TRI_PushBackVectorPointer(&collection->_journals, journal);

  return journal;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new compactor file
///
/// Note that the caller must hold a lock protecting the _journals entry.
////////////////////////////////////////////////////////////////////////////////

TRI_datafile_t* TRI_CreateCompactorDocumentCollection (TRI_document_collection_t* document,
                                                      TRI_voc_fid_t fid,
                                                      TRI_voc_size_t maximalSize) {
  return CreateCompactor(document, fid, maximalSize);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief closes an existing compactor file
///
/// Note that the caller must hold a lock protecting the _datafiles and
/// _journals entry.
////////////////////////////////////////////////////////////////////////////////

bool TRI_CloseCompactorDocumentCollection (TRI_document_collection_t* document,
                                          size_t position) {
  return CloseJournalDocumentCollection(document, position, true);
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
  TRI_json_t const* type;
  TRI_json_t const* iis;
  char const* typeStr;
  TRI_idx_iid_t iid;
 
  TRI_ASSERT(json != NULL); 
  TRI_ASSERT(json->_type == TRI_JSON_ARRAY); 

  if (idx != NULL) {
    *idx = NULL;
  }

  // extract the type
  type = TRI_LookupArrayJson(json, "type");

  if (! TRI_IsStringJson(type)) {
    return TRI_ERROR_INTERNAL;
  }

  typeStr = type->_value._string.data;

  // extract the index identifier
  iis = TRI_LookupArrayJson(json, "id");

  if (iis != NULL && iis->_type == TRI_JSON_NUMBER) {
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
    int res = CapConstraintFromJson(document, json, iid, idx);

    return res;
  }

  // ...........................................................................
  // BITARRAY INDEX
  // ...........................................................................

  else if (TRI_EqualString(typeStr, "bitarray")) {
    int res = BitarrayIndexFromJson(document, json, iid, idx);

    return res;
  }

  // ...........................................................................
  // GEO INDEX (list or attribute)
  // ...........................................................................

  else if (TRI_EqualString(typeStr, "geo1") || TRI_EqualString(typeStr, "geo2")) {
    int res = GeoIndexFromJson(document, json, iid, idx);

    return res;
  }

  // ...........................................................................
  // HASH INDEX
  // ...........................................................................

  else if (TRI_EqualString(typeStr, "hash")) {
    int res = HashIndexFromJson(document, json, iid, idx);

    return res;
  }

  // ...........................................................................
  // SKIPLIST INDEX
  // ...........................................................................

  else if (TRI_EqualString(typeStr, "skiplist")) {
    int res = SkiplistIndexFromJson(document, json, iid, idx);

    return res;
  }

  // ...........................................................................
  // FULLTEXT INDEX
  // ...........................................................................

  else if (TRI_EqualString(typeStr, "fulltext")) {
    int res = FulltextIndexFromJson(document, json, iid, idx);

    return res;
  }

  // ...........................................................................
  // EDGES INDEX
  // ...........................................................................
  
  else if (TRI_EqualString(typeStr, "edge")) {
    // we should never get here, as users cannot create their own edge indexes
    LOG_ERROR("logic error. there should never be a JSON file describing an edges index");

    return TRI_ERROR_INTERNAL;
  }

  // .........................................................................
  // oops, unknown index type
  // .........................................................................

  else {
    LOG_ERROR("ignoring unknown index type '%s' for index %llu",
              typeStr,
              (unsigned long long) iid);

    return TRI_ERROR_INTERNAL;
  }
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
    DeleteSecondaryIndexes(document, header, true);
    int res = InsertSecondaryIndexes(document, oldData, true);

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
/// @brief closes an existing journal
////////////////////////////////////////////////////////////////////////////////

bool TRI_CloseJournalDocumentCollection (TRI_document_collection_t* document,
                                         size_t position) {
  return CloseJournalDocumentCollection(document, position, false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief opens an existing collection
////////////////////////////////////////////////////////////////////////////////

TRI_document_collection_t* TRI_OpenDocumentCollection (TRI_vocbase_t* vocbase, 
                                                       TRI_vocbase_col_t* col) {
  TRI_collection_t* collection;
  TRI_shaper_t* shaper;
  TRI_key_generator_t* keyGenerator;
  int res;

  char const* path = col->_path;

  // first open the document collection
  TRI_document_collection_t* document = static_cast<TRI_document_collection_t*>(TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_document_collection_t), false));

  if (document == nullptr) {
    return nullptr;
  }
 
  collection = TRI_OpenCollection(vocbase, &document->base, path);

  if (collection == nullptr) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, document);
    LOG_ERROR("cannot open document collection from path '%s'", path);

    return nullptr;
  }

  shaper = TRI_CreateVocShaper(vocbase, document);

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
  res = TRI_CreateKeyGenerator(collection->_info._keyOptions, &keyGenerator);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_CloseCollection(collection);
    TRI_FreeCollection(collection);
    LOG_ERROR("cannot initialise document collection");
    TRI_set_errno(res);

    return nullptr;
  }

  TRI_ASSERT(keyGenerator != nullptr);
  document->_keyGenerator = keyGenerator;

  // create a fake transaction for loading the collection
  TransactionBase trx(true);

  // iterate over all markers of the collection
  res = IterateMarkersCollection(collection);

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

  TRI_ASSERT(document->_shaper != nullptr);

  TRI_InitVocShaper(document->_shaper);

  // fill internal indexes (this is, the edges index at the moment) 
  FillInternalIndexes(document);

  // fill user-defined secondary indexes
  TRI_IterateIndexCollection(collection, OpenIndexIterator, collection);

  return document;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief closes an open collection
////////////////////////////////////////////////////////////////////////////////

int TRI_CloseDocumentCollection (TRI_document_collection_t* document) {
  // closes all open compactors, journals, datafiles
  int res = TRI_CloseCollection(&document->base);

  TRI_FreeVocShaper(document->_shaper);
  document->_shaper = nullptr;

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
  TRI_json_t* fld;
  size_t j;

  fld = TRI_LookupArrayJson(json, "fields");

  if (fld == NULL || fld->_type != TRI_JSON_LIST) {
    LOG_ERROR("ignoring index %llu, 'fields' must be a list", (unsigned long long) iid);

    TRI_set_errno(TRI_ERROR_BAD_PARAMETER);
    return NULL;
  }

  *fieldCount = fld->_value._objects._length;

  for (j = 0;  j < *fieldCount;  ++j) {
    TRI_json_t* sub = static_cast<TRI_json_t*>(TRI_AtVector(&fld->_value._objects, j));

    if (! TRI_IsStringJson(sub)) {
      LOG_ERROR("ignoring index %llu, 'fields' must be a list of attribute paths", (unsigned long long) iid);

      TRI_set_errno(TRI_ERROR_BAD_PARAMETER);
      return NULL;
    }
  }

  return fld;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the list of attribute/value pairs
///
/// Attribute/value pairs are used in the construction of static bitarray
/// indexes. These pairs are stored in a json object from which they can be
/// later extracted. Here is the extraction function given the index definition
/// as a json object
////////////////////////////////////////////////////////////////////////////////

static TRI_json_t* ExtractFieldValues (TRI_json_t const* jsonIndex, 
                                       size_t* fieldCount, 
                                       TRI_idx_iid_t iid) {
  TRI_json_t* keyValues;
  size_t j;

  keyValues = TRI_LookupArrayJson(jsonIndex, "fields");

  if (keyValues == NULL || keyValues->_type != TRI_JSON_LIST) {
    LOG_ERROR("ignoring index %llu, 'fields' must be a list", (unsigned long long) iid);

    TRI_set_errno(TRI_ERROR_BAD_PARAMETER);
    return NULL;
  }


  *fieldCount = keyValues->_value._objects._length;


  // ...........................................................................
  // Some simple checks
  // ...........................................................................

  for (j = 0;  j < *fieldCount;  ++j) {

    // .........................................................................
    // Extract the jth key value pair
    // .........................................................................

    TRI_json_t* keyValue = static_cast<TRI_json_t*>(TRI_AtVector(&keyValues->_value._objects, j));

    // .........................................................................
    // The length of this key value pair must be two
    // .........................................................................

    if (keyValue == NULL  || keyValue->_value._objects._length != 2) {
      LOG_ERROR("ignoring index %llu, 'fields' must be a list of key value pairs", (unsigned long long) iid);
      TRI_set_errno(TRI_ERROR_BAD_PARAMETER);
      return NULL;
    }


    // .........................................................................
    // Extract the key
    // .........................................................................

    TRI_json_t* key = static_cast<TRI_json_t*>(TRI_AtVector(&keyValue->_value._objects, 0));

    if (! TRI_IsStringJson(key)) {
      LOG_ERROR("ignoring index %llu, key in 'fields' pair must be an attribute (string)", (unsigned long long) iid);
      TRI_set_errno(TRI_ERROR_BAD_PARAMETER);
      return NULL;
    }


    // .........................................................................
    // Extract the value
    // .........................................................................

    TRI_json_t* value = static_cast<TRI_json_t*>(TRI_AtVector(&keyValue->_value._objects, 1));

    if (value == NULL || value->_type != TRI_JSON_LIST) {
      LOG_ERROR("ignoring index %llu, value in 'fields' pair must be a list ([...])", (unsigned long long) iid);
      TRI_set_errno(TRI_ERROR_BAD_PARAMETER);
      return NULL;
    }

  }

  return keyValues;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief drops an index
////////////////////////////////////////////////////////////////////////////////

static bool DropIndex (TRI_document_collection_t* document, 
                       TRI_idx_iid_t iid,
                       TRI_server_id_t generatingServer,
                       bool full) {
  if (iid == 0) {
    // invalid index id or primary index
    return true;
  }

  TRI_index_t* found = nullptr;
  
  TRI_vocbase_t* vocbase = document->base._vocbase;
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

  RebuildIndexInfo(document);

  TRI_WRITE_UNLOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(document);

  TRI_ReadUnlockReadWriteLock(&vocbase->_inventoryLock);

  // .............................................................................
  // outside write-lock
  // .............................................................................

  if (found != nullptr) {
    bool result = true;

    if (full) {
      result = TRI_RemoveIndexFile(document, found);

      // it is safe to use _name as we hold a read-lock on the collection status
      TRI_LogDropIndexReplication(vocbase,
                                  document->base._info._cid, 
                                  document->base._info._name, 
                                  iid,
                                  generatingServer);
    }
      
    TRI_FreeIndex(found);

    return result;
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises an index with all existing documents
////////////////////////////////////////////////////////////////////////////////

static int FillIndex (TRI_document_collection_t* document, 
                      TRI_index_t* idx) {
  void** end;
  void** ptr;

  ptr = document->_primaryIndex._table;
  end = ptr + document->_primaryIndex._nrAlloc;
    
  if (idx->sizeHint != nullptr) {
    // give the index a size hint
    idx->sizeHint(idx, document->_primaryIndex._nrUsed);
  }


  static const int LoopSize = 10000;
  int counter = 0;
  int loops = 0;

  for (;  ptr < end;  ++ptr) {
    TRI_doc_mptr_t const* p = static_cast<TRI_doc_mptr_t const*>(*ptr);

    if (p != nullptr) {
      TRI_doc_mptr_t const* mptr = p;

      int res = idx->insert(idx, mptr, false);

      if (res != TRI_ERROR_NO_ERROR) {
        LOG_WARNING("failed to insert document '%llu/%s' for index %llu",
                    (unsigned long long) document->base._info._cid,
                    (char*) TRI_EXTRACT_MARKER_KEY(mptr),  // ONLY IN INDEX
                    (unsigned long long) idx->_iid);
  
        return res;
      }

      if (++counter == LoopSize) {
        counter = 0;
        ++loops;

        LOG_TRACE("indexed %llu documents of collection %llu", 
                 (unsigned long long) (LoopSize * loops),
                 (unsigned long long) document->base._info._cid);
      }
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
  TRI_vector_t* indexPaths = NULL;
  size_t j;

  // ...........................................................................
  // go through every index and see if we have a match
  // ...........................................................................

  for (j = 0;  j < collection->_allIndexes._length;  ++j) {
    TRI_index_t* idx = static_cast<TRI_index_t*>(collection->_allIndexes._buffer[j]);
    bool found;
    size_t k;

    // .........................................................................
    // check if the type of the index matches
    // .........................................................................

    if (idx->_type != type) {
      continue;
    }

    // .........................................................................
    // check if uniqueness matches
    // .........................................................................

    if (idx->_unique != unique) {
      continue;
    }

    // .........................................................................
    // Now perform checks which are specific to the type of index
    // .........................................................................

    switch (type) {
      case TRI_IDX_TYPE_BITARRAY_INDEX: {
        TRI_bitarray_index_t* baIndex = (TRI_bitarray_index_t*) idx;
        indexPaths = &(baIndex->_paths);
        break;
      }

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

    if (indexPaths == NULL) {
      // this may actually happen if compiled with -DNDEBUG
      return NULL;
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

    found = true;

    if (allowAnyAttributeOrder) {
      // any permutation of attributes is allowed
      for (k = 0;  k < paths->_length;  ++k) {
        TRI_shape_pid_t indexShape = *((TRI_shape_pid_t*) TRI_AtVector(indexPaths, k));
        size_t l;

        found = false;

        for (l = 0;  l < paths->_length;  ++l) {
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
      for (k = 0;  k < paths->_length;  ++k) {
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

  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief restores a bitarray based index (template)
////////////////////////////////////////////////////////////////////////////////

static int BitarrayBasedIndexFromJson (TRI_document_collection_t* document,
                                       TRI_json_t const* definition,
                                       TRI_idx_iid_t iid,
                                       TRI_index_t* (*creator)(TRI_document_collection_t*,
                                                               const TRI_vector_pointer_t*,
                                                               const TRI_vector_pointer_t*,
                                                               TRI_idx_iid_t,
                                                               bool,
                                                               bool*, int*, char**),
                                       TRI_index_t** dst) {
  TRI_index_t* idx;
  TRI_json_t* uniqueIndex;
  TRI_json_t* supportUndefIndex;
  TRI_json_t* keyValues;
  TRI_vector_pointer_t attributes;
  TRI_vector_pointer_t values;
  // bool unique;
  bool supportUndef;
  bool created;
  size_t fieldCount;
  size_t j;
  int errorNum;
  char* errorStr;

  if (dst != NULL) {
    *dst = NULL;
  }

  // ...........................................................................
  // extract fields list (which is a list of key/value pairs for a bitarray index
  // ...........................................................................

  keyValues = ExtractFieldValues(definition, &fieldCount, iid);

  if (keyValues == NULL) {
    return TRI_errno();
  }


  // ...........................................................................
  // For a bitarray index we require at least one attribute path and one set of
  // possible values for that attribute (that is, we require at least one pair)
  // ...........................................................................

  if (fieldCount < 1) {
    LOG_ERROR("ignoring index %llu, need at least one attribute path and one list of values",(unsigned long long) iid);
    return TRI_set_errno(TRI_ERROR_BAD_PARAMETER);
  }


  // ...........................................................................
  // A bitarray index is always (for now) non-unique. Irrespective of this fact
  // attempt to extract the 'uniqueness value' from the json object representing
  // the bitarray index.
  // ...........................................................................

  // unique = false;
  uniqueIndex = TRI_LookupArrayJson(definition, "unique");
  if (uniqueIndex == NULL || uniqueIndex->_type != TRI_JSON_BOOLEAN) {
    LOG_ERROR("ignoring index %llu, could not determine if unique or non-unique", (unsigned long long) iid);
    return TRI_set_errno(TRI_ERROR_BAD_PARAMETER);
  }
  // unique = uniqueIndex->_value._boolean;


  // ...........................................................................
  // A bitarray index can support documents where one or more attributes are
  // undefined. Determine if this is the case.
  // ...........................................................................

  supportUndefIndex = TRI_LookupArrayJson(definition, "undefined");

  if (supportUndefIndex == NULL || supportUndefIndex->_type != TRI_JSON_BOOLEAN) {
    LOG_ERROR("ignoring index %llu, could not determine if index supports undefined values", (unsigned long long) iid);
    return TRI_set_errno(TRI_ERROR_BAD_PARAMETER);
  }

  supportUndef = supportUndefIndex->_value._boolean;

  // ...........................................................................
  // Initialise the vectors in which we store the fields and their corresponding
  // values
  // ...........................................................................

  TRI_InitVectorPointer(&attributes, TRI_CORE_MEM_ZONE);
  TRI_InitVectorPointer(&values, TRI_CORE_MEM_ZONE);


  // ...........................................................................
  // find fields and values and store them in the vector pointers
  // ...........................................................................

  for (j = 0;  j < fieldCount;  ++j) {
    TRI_json_t* keyValue = static_cast<TRI_json_t*>(TRI_AtVector(&keyValues->_value._objects, j));
    TRI_json_t* key      = static_cast<TRI_json_t*>(TRI_AtVector(&keyValue->_value._objects, 0));
    TRI_json_t* value    = static_cast<TRI_json_t*>(TRI_AtVector(&keyValue->_value._objects, 1));

    TRI_PushBackVectorPointer(&attributes, key->_value._string.data);
    TRI_PushBackVectorPointer(&values, value);
  }


  // ...........................................................................
  // attempt to create the index or retrieve an existing one
  // ...........................................................................
  errorStr = NULL;
  idx = creator(document, &attributes, &values, iid, supportUndef, &created, &errorNum, &errorStr);


  // ...........................................................................
  // cleanup
  // ...........................................................................

  TRI_DestroyVectorPointer(&attributes);
  TRI_DestroyVectorPointer(&values);


  // ...........................................................................
  // Check if the creation or lookup succeeded
  // ...........................................................................

  if (idx == NULL) {
    LOG_ERROR("cannot create bitarray index %llu", (unsigned long long) iid);
    if (errorStr != NULL) {
      LOG_TRACE("%s", errorStr);
      TRI_Free(TRI_CORE_MEM_ZONE, errorStr);
    }
    return errorNum;
  }

  if (dst != NULL) {
    *dst = idx;
  }

  return TRI_ERROR_NO_ERROR;
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
  TRI_index_t* idx;
  TRI_json_t* bv;
  TRI_json_t* fld;
  TRI_vector_pointer_t attributes;
  bool unique;
  size_t fieldCount;
  size_t j;

  if (dst != NULL) {
    *dst = NULL;
  }

  // extract fields
  fld = ExtractFields(definition, &fieldCount, iid);

  if (fld == NULL) {
    return TRI_errno();
  }

  // extract the list of fields
  if (fieldCount < 1) {
    LOG_ERROR("ignoring index %llu, need at least one attribute path",(unsigned long long) iid);

    return TRI_set_errno(TRI_ERROR_BAD_PARAMETER);
  }

  // determine if the hash index is unique or non-unique
  bv = TRI_LookupArrayJson(definition, "unique");

  if (bv == NULL || bv->_type != TRI_JSON_BOOLEAN) {
    LOG_ERROR("ignoring index %llu, could not determine if unique or non-unique", (unsigned long long) iid);
    return TRI_set_errno(TRI_ERROR_BAD_PARAMETER);
  }

  unique = bv->_value._boolean;

  // Initialise the vector in which we store the fields on which the hashing
  // will be based.
  TRI_InitVectorPointer(&attributes, TRI_CORE_MEM_ZONE);

  // find fields
  for (j = 0;  j < fieldCount;  ++j) {
    TRI_json_t* fieldStr = static_cast<TRI_json_t*>(TRI_AtVector(&fld->_value._objects, j));

    TRI_PushBackVectorPointer(&attributes, fieldStr->_value._string.data);
  }

  // create the index
  idx = creator(document, &attributes, iid, unique, NULL);

  if (dst != NULL) {
    *dst = idx;
  }

  // cleanup
  TRI_DestroyVectorPointer(&attributes);

  if (idx == NULL) {
    LOG_ERROR("cannot create hash index %llu", (unsigned long long) iid);
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

void TRI_UpdateStatisticsDocumentCollection (TRI_document_collection_t* document, 
                                             TRI_voc_rid_t rid,
                                             bool force,
                                             int64_t logfileEntries) {
  if (rid > 0) {
    SetRevision(document, rid, force);
  }

  if (! document->base._info._isVolatile) {
    // only count logfileEntries if the collection is durable
    document->_uncollectedLogfileEntries += logfileEntries;
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

  if (vector == NULL) {
    return NULL;
  }

  TRI_InitVectorPointer(vector, TRI_CORE_MEM_ZONE);

  size_t const n = document->_allIndexes._length;

  for (size_t i = 0;  i < n;  ++i) {
    TRI_index_t* idx = static_cast<TRI_index_t*>(document->_allIndexes._buffer[i]);
    TRI_json_t* json = idx->json(idx);

    if (json != NULL) {
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
                                      TRI_server_id_t generatingServer) {
  return DropIndex(document, iid, generatingServer, true);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief drops an index, without index file removal and replication
////////////////////////////////////////////////////////////////////////////////

bool TRI_DropIndex2DocumentCollection (TRI_document_collection_t* document, 
                                       TRI_idx_iid_t iid,
                                       TRI_server_id_t generatingServer) {
  return DropIndex(document, iid, generatingServer, false);
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
  size_t j;

  // .............................................................................
  // sorted case
  // .............................................................................

  if (sorted) {
    // combine name and pid
    pidnames = static_cast<pid_name_t*>(TRI_Allocate(TRI_CORE_MEM_ZONE, sizeof(pid_name_t) * attributes->_length, false));

    if (pidnames == NULL) {
      LOG_ERROR("out of memory in PidNamesByAttributeNames");
      return TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
    }

    for (j = 0;  j < attributes->_length;  ++j) {
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

    for (j = 0;  j < attributes->_length;  ++j) {
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

    for (j = 0;  j < attributes->_length;  ++j) {
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
  TRI_json_t* val1;
  TRI_json_t* val2;
  TRI_index_t* idx;
  size_t count;
  int64_t size;

  if (dst != NULL) {
    *dst = NULL;
  } 

  val1 = TRI_LookupArrayJson(definition, "size");
  val2 = TRI_LookupArrayJson(definition, "byteSize");

  if ((val1 == NULL || val1->_type != TRI_JSON_NUMBER) &&
      (val2 == NULL || val2->_type != TRI_JSON_NUMBER)) {
    LOG_ERROR("ignoring cap constraint %llu, 'size' and 'byteSize' missing", 
              (unsigned long long) iid);

    return TRI_set_errno(TRI_ERROR_BAD_PARAMETER);
  }

  count = 0;
  if (val1 != NULL && val1->_value._number > 0.0) {
    count = (size_t) val1->_value._number;
  }

  size = 0;
  if (val2 != NULL && val2->_value._number > (double) TRI_CAP_CONSTRAINT_MIN_SIZE) {
    size = (int64_t) val2->_value._number;
  }

  if (count == 0 && size == 0) {
    LOG_ERROR("ignoring cap constraint %llu, 'size' must be at least 1, "
              "or 'byteSize' must be at least " 
              TRI_CAP_CONSTRAINT_MIN_SIZE_STR,
              (unsigned long long) iid); 

    return TRI_set_errno(TRI_ERROR_BAD_PARAMETER);
  }
  
  idx = CreateCapConstraintDocumentCollection(document, count, size, iid, NULL);
  if (dst != NULL) {
    *dst = idx;
  }

  return idx == NULL ? TRI_errno() : TRI_ERROR_NO_ERROR;
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
                                                        bool* created,
                                                        TRI_server_id_t generatingServer) {
  TRI_index_t* idx;

  // .............................................................................
  // inside write-lock
  // .............................................................................

  TRI_ReadLockReadWriteLock(&document->base._vocbase->_inventoryLock);

  TRI_WRITE_LOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(document);

  idx = CreateCapConstraintDocumentCollection(document, count, size, iid, created);

  if (idx != nullptr) {
    if (created) {
      int res = TRI_SaveIndex(document, idx, generatingServer);

      if (res != TRI_ERROR_NO_ERROR) {
        idx = nullptr;
      }
    }
  }
  
  TRI_WRITE_UNLOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(document);

  // .............................................................................
  // outside write-lock
  // .............................................................................

  TRI_ReadUnlockReadWriteLock(&document->base._vocbase->_inventoryLock);

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
  idx = NULL;

  shaper = document->_shaper;

  if (location != NULL) {
    loc = shaper->findOrCreateAttributePathByName(shaper, location, true);

    if (loc == 0) {
      TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
      return NULL;
    }
  }

  if (latitude != NULL) {
    lat = shaper->findOrCreateAttributePathByName(shaper, latitude, true);

    if (lat == 0) {
      TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
      return NULL;
    }
  }

  if (longitude != NULL) {
    lon = shaper->findOrCreateAttributePathByName(shaper, longitude, true);

    if (lon == 0) {
      TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
      return NULL;
    }
  }

  // check, if we know the index
  if (location != NULL) {
    idx = TRI_LookupGeoIndex1DocumentCollection(document, location, geoJson, unique, ignoreNull);
  }
  else if (longitude != NULL && latitude != NULL) {
    idx = TRI_LookupGeoIndex2DocumentCollection(document, latitude, longitude, unique, ignoreNull);
  }
  else {
    TRI_set_errno(TRI_ERROR_INTERNAL);
    LOG_TRACE("expecting either 'location' or 'latitude' and 'longitude'");
    return NULL;
  }

  if (idx != NULL) {
    LOG_TRACE("geo-index already created for location '%s'", location);

    if (created != NULL) {
      *created = false;
    }

    return idx;
  }

  // create a new index
  if (location != NULL) {
    idx = TRI_CreateGeo1Index(document, iid, location, loc, geoJson, unique, ignoreNull);

    LOG_TRACE("created geo-index for location '%s': %ld",
              location,
              (unsigned long) loc);
  }
  else if (longitude != NULL && latitude != NULL) {
    idx = TRI_CreateGeo2Index(document, iid, latitude, lat, longitude, lon, unique, ignoreNull);

    LOG_TRACE("created geo-index for location '%s': %ld, %ld",
              location,
              (unsigned long) lat,
              (unsigned long) lon);
  }

  if (idx == NULL) {
    TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
    return NULL;
  }

  // initialises the index with all existing documents
  res = FillIndex(document, idx);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_FreeGeoIndex(idx);

    return NULL;
  }

  // and store index
  res = AddIndex(document, idx);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_FreeGeoIndex(idx);

    return NULL;
  }

  if (created != NULL) {
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

  if (dst != NULL) {
    *dst = NULL;
  }

  type = TRI_LookupArrayJson(definition, "type");

  if (! TRI_IsStringJson(type)) {
    return TRI_ERROR_INTERNAL;
  }

  typeStr = type->_value._string.data;

  // extract fields
  fld = ExtractFields(definition, &fieldCount, iid);

  if (fld == NULL) {
    return TRI_errno();
  }

  // extract unique
  unique = false;
  // first try "unique" attribute
  bv = TRI_LookupArrayJson(definition, "unique");

  if (bv != NULL && bv->_type == TRI_JSON_BOOLEAN) {
    unique = bv->_value._boolean;
  }
  else {
    // then "constraint" 
    bv = TRI_LookupArrayJson(definition, "constraint");

    if (bv != NULL && bv->_type == TRI_JSON_BOOLEAN) {
      unique = bv->_value._boolean;
    }
  }

  // extract ignore null
  ignoreNull = false;
  bv = TRI_LookupArrayJson(definition, "ignoreNull");

  if (bv != NULL && bv->_type == TRI_JSON_BOOLEAN) {
    ignoreNull = bv->_value._boolean;
  }

  // list style
  if (TRI_EqualString(typeStr, "geo1")) {
    bool geoJson;

    // extract geo json
    geoJson = false;
    bv = TRI_LookupArrayJson(definition, "geoJson");

    if (bv != NULL && bv->_type == TRI_JSON_BOOLEAN) {
      geoJson = bv->_value._boolean;
    }

    // need just one field
    if (fieldCount == 1) {
      TRI_json_t* loc = static_cast<TRI_json_t*>(TRI_AtVector(&fld->_value._objects, 0));

      idx = CreateGeoIndexDocumentCollection(document,
                                        loc->_value._string.data,
                                        NULL,
                                        NULL,
                                        geoJson,
                                        unique,
                                        ignoreNull,
                                        iid,
                                        NULL);
  
      if (dst != NULL) {
        *dst = idx;
      }

      return idx == NULL ? TRI_errno() : TRI_ERROR_NO_ERROR;
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
                                        NULL,
                                        lat->_value._string.data,
                                        lon->_value._string.data,
                                        false,
                                        unique,
                                        ignoreNull,
                                        iid,
                                        NULL);
      
      if (dst != NULL) {
        *dst = idx;
      }

      return idx == NULL ? TRI_errno() : TRI_ERROR_NO_ERROR;
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

  shaper = document->_shaper;
    
  loc = shaper->lookupAttributePathByName(shaper, location);

  if (loc == 0) {
    return NULL;
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

  return NULL;
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
  
  shaper = document->_shaper;
  
  lat = shaper->lookupAttributePathByName(shaper, latitude);
  lon = shaper->lookupAttributePathByName(shaper, longitude);

  if (lat == 0 || lon == 0) {
    return NULL;
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

  return NULL;
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
                                                    bool* created,
                                                    TRI_server_id_t generatingServer) {
  TRI_index_t* idx;

  TRI_ReadLockReadWriteLock(&document->base._vocbase->_inventoryLock);

  // .............................................................................
  // inside write-lock
  // .............................................................................

  TRI_WRITE_LOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(document);

  idx = CreateGeoIndexDocumentCollection(document, location, NULL, NULL, geoJson, unique, ignoreNull, iid, created);

  if (idx != nullptr) {
    if (created) {
      int res = TRI_SaveIndex(document, idx, generatingServer);

      if (res != TRI_ERROR_NO_ERROR) {
        idx = nullptr;
      }
    }
  }
  
  TRI_WRITE_UNLOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(document);

  // .............................................................................
  // outside write-lock
  // .............................................................................

  TRI_ReadUnlockReadWriteLock(&document->base._vocbase->_inventoryLock);

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
                                                    bool* created,
                                                    TRI_server_id_t generatingServer) {
  TRI_index_t* idx;

  TRI_ReadLockReadWriteLock(&document->base._vocbase->_inventoryLock);

  // .............................................................................
  // inside write-lock
  // .............................................................................

  TRI_WRITE_LOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(document);

  idx = CreateGeoIndexDocumentCollection(document, NULL, latitude, longitude, false, unique, ignoreNull, iid, created);

  if (idx != nullptr) {
    if (created) {
      int res = TRI_SaveIndex(document, idx, generatingServer);

      if (res != TRI_ERROR_NO_ERROR) {
        idx = nullptr;
      }
    }
  }
  
  TRI_WRITE_UNLOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(document);

  // .............................................................................
  // outside write-lock
  // .............................................................................

  TRI_ReadUnlockReadWriteLock(&document->base._vocbase->_inventoryLock);

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
  TRI_index_t* idx;
  TRI_vector_pointer_t fields;
  TRI_vector_t paths;
  int res;

  idx = NULL;

  // determine the sorted shape ids for the attributes
  res = PidNamesByAttributeNames(attributes,
                                 document->_shaper,
                                 &paths,
                                 &fields,
                                 true,
                                 true);

  if (res != TRI_ERROR_NO_ERROR) {
    if (created != NULL) {
      *created = false;
    }

    return NULL;
  }

  // ...........................................................................
  // Attempt to find an existing index which matches the attributes above.
  // If a suitable index is found, return that one otherwise we need to create
  // a new one.
  // ...........................................................................

  idx = LookupPathIndexDocumentCollection(document, &paths, TRI_IDX_TYPE_HASH_INDEX, unique, false);

  if (idx != NULL) {
    TRI_DestroyVector(&paths);
    TRI_DestroyVectorPointer(&fields);
    LOG_TRACE("hash-index already created");

    if (created != NULL) {
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
  
  if (idx == NULL) {
    TRI_DestroyVector(&paths);
    TRI_DestroyVectorPointer(&fields);
    TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
    return NULL;
  }

  // release memory allocated to vector
  TRI_DestroyVector(&paths);
  TRI_DestroyVectorPointer(&fields);

  // initialises the index with all existing documents
  res = FillIndex(document, idx);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_FreeHashIndex(idx);

    return NULL;
  }

  // store index and return
  res = AddIndex(document, idx);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_FreeHashIndex(idx);

    return NULL;
  }

  if (created != NULL) {
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
                                 document->_shaper,
                                 &paths,
                                 &fields,
                                 true, 
                                 false);

  if (res != TRI_ERROR_NO_ERROR) {
    return NULL;
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
                                                    bool* created,
                                                    TRI_server_id_t generatingServer) {
  TRI_index_t* idx;

  TRI_ReadLockReadWriteLock(&document->base._vocbase->_inventoryLock);

  // .............................................................................
  // inside write-lock
  // .............................................................................

  TRI_WRITE_LOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(document);

  // given the list of attributes (as strings)
  idx = CreateHashIndexDocumentCollection(document, attributes, iid, unique, created);

  if (idx != nullptr) {
    if (created) {
      int res = TRI_SaveIndex(document, idx, generatingServer);

      if (res != TRI_ERROR_NO_ERROR) {
        idx = nullptr;
      }
    }
  }
  
  TRI_WRITE_UNLOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(document);

  // .............................................................................
  // outside write-lock
  // .............................................................................

  TRI_ReadUnlockReadWriteLock(&document->base._vocbase->_inventoryLock);

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
                                 document->_shaper,
                                 &paths,
                                 &fields,
                                 false,
                                 true);

  if (res != TRI_ERROR_NO_ERROR) {
    if (created != NULL) {
      *created = false;
    }

    return NULL;
  }

  // ...........................................................................
  // Attempt to find an existing index which matches the attributes above.
  // If a suitable index is found, return that one otherwise we need to create
  // a new one.
  // ...........................................................................

  idx = LookupPathIndexDocumentCollection(document, &paths, TRI_IDX_TYPE_SKIPLIST_INDEX, unique, false);

  if (idx != NULL) {
    TRI_DestroyVector(&paths);
    TRI_DestroyVectorPointer(&fields);
    LOG_TRACE("skiplist-index already created");

    if (created != NULL) {
      *created = false;
    }

    return idx;
  }

  // Create the skiplist index
  idx = TRI_CreateSkiplistIndex(document, iid, &fields, &paths, unique);
  
  if (idx == NULL) {
    TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
    return NULL;
  }

  // release memory allocated to vector
  TRI_DestroyVector(&paths);
  TRI_DestroyVectorPointer(&fields);

  // initialises the index with all existing documents
  res = FillIndex(document, idx);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_FreeSkiplistIndex(idx);

    TRI_set_errno(res);
    return NULL;
  }

  // store index and return
  res = AddIndex(document, idx);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_FreeSkiplistIndex(idx);

    return NULL;
  }

  if (created != NULL) {
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
                                 document->_shaper,
                                 &paths,
                                 &fields,
                                 false,
                                 false);

  if (res != TRI_ERROR_NO_ERROR) {
    return NULL;
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
                                                        bool* created, 
                                                        TRI_server_id_t generatingServer) {
  TRI_index_t* idx;

  TRI_ReadLockReadWriteLock(&document->base._vocbase->_inventoryLock);

  // .............................................................................
  // inside write-lock the collection
  // .............................................................................

  TRI_WRITE_LOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(document);

  idx = CreateSkiplistIndexDocumentCollection(document, attributes, iid, unique, created);

  if (idx != nullptr) {
    if (created) {
      int res = TRI_SaveIndex(document, idx, generatingServer);

      if (res != TRI_ERROR_NO_ERROR) {
        idx = nullptr;
      }
    }
  }
  
  TRI_WRITE_UNLOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(document);

  // .............................................................................
  // outside write-lock
  // .............................................................................

  TRI_ReadUnlockReadWriteLock(&document->base._vocbase->_inventoryLock);

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

  return NULL;
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
  if (idx != NULL) {
    LOG_TRACE("fulltext-index already created");

    if (created != NULL) {
      *created = false;
    }
    return idx;
  }

  // Create the fulltext index
  idx = TRI_CreateFulltextIndex(document, iid, attributeName, indexSubstrings, minWordLength);
  
  if (idx == NULL) {
    TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
    return NULL;
  }

  // initialises the index with all existing documents
  res = FillIndex(document, idx);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_FreeFulltextIndex(idx);

    return NULL;
  }

  // store index and return
  res = AddIndex(document, idx);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_FreeFulltextIndex(idx);

    return NULL;
  }

  if (created != NULL) {
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

  if (dst != NULL) {
    *dst = NULL;
  }

  // extract fields
  fld = ExtractFields(definition, &fieldCount, iid);

  if (fld == NULL) {
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
  // if (indexSubstrings != NULL && indexSubstrings->_type == TRI_JSON_BOOLEAN) {
  //  doIndexSubstrings = indexSubstrings->_value._boolean;
  // }

  minWordLength = TRI_LookupArrayJson(definition, "minLength");
  minWordLengthValue = TRI_FULLTEXT_MIN_WORD_LENGTH_DEFAULT;

  if (minWordLength != NULL && minWordLength->_type == TRI_JSON_NUMBER) {
    minWordLengthValue = (int) minWordLength->_value._number;
  }

  // create the index
  idx = LookupFulltextIndexDocumentCollection(document, attributeName, doIndexSubstrings, minWordLengthValue);

  if (idx == NULL) {
    bool created;
    idx = CreateFulltextIndexDocumentCollection(document, attributeName, doIndexSubstrings, minWordLengthValue, iid, &created);
  }

  if (dst != NULL) {
    *dst = idx;
  }

  if (idx == NULL) {
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
                                                        bool* created,
                                                        TRI_server_id_t generatingServer) {
  TRI_index_t* idx;

  TRI_ReadLockReadWriteLock(&document->base._vocbase->_inventoryLock);

  // .............................................................................
  // inside write-lock the collection
  // .............................................................................

  TRI_WRITE_LOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(document);

  idx = CreateFulltextIndexDocumentCollection(document, attributeName, indexSubstrings, minWordLength, iid, created);

  if (idx != nullptr) {
    if (created) {
      int res = TRI_SaveIndex(document, idx, generatingServer);

      if (res != TRI_ERROR_NO_ERROR) {
        idx = nullptr;
      }
    }
  }
  
  TRI_WRITE_UNLOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(document);

  // .............................................................................
  // outside write-lock
  // .............................................................................

  TRI_ReadUnlockReadWriteLock(&document->base._vocbase->_inventoryLock);

  return idx;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    BITARRAY INDEX
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a bitarray index to the collection
////////////////////////////////////////////////////////////////////////////////

static TRI_index_t* CreateBitarrayIndexDocumentCollection (TRI_document_collection_t* document,
                                                           const TRI_vector_pointer_t* attributes,
                                                           const TRI_vector_pointer_t* values,
                                                           TRI_idx_iid_t iid,
                                                           bool supportUndef,
                                                           bool* created,
                                                           int* errorNum,
                                                           char** errorStr) {
  TRI_index_t* idx;
  TRI_vector_pointer_t fields;
  TRI_vector_t paths;
  int res;

  res = PidNamesByAttributeNames(attributes,
                                 document->_shaper,
                                 &paths,
                                 &fields,
                                 false,
                                 true);

  if (res != TRI_ERROR_NO_ERROR) {
    if (created != NULL) {
      *created = false;
    }
    *errorNum = res;
    *errorStr  = TRI_DuplicateString("Bitarray index attributes could not be accessed.");
    return NULL;
  }

  // ...........................................................................
  // Attempt to find an existing index which matches the attributes above.
  // If a suitable index is found, return that one otherwise we need to create
  // a new one.
  // ...........................................................................

  idx = LookupPathIndexDocumentCollection(document, &paths, TRI_IDX_TYPE_BITARRAY_INDEX, false, false);

  if (idx != NULL) {

    // .........................................................................
    // existing index has been located which matches the list of attributes
    // return this one
    // .........................................................................

    TRI_DestroyVector(&paths);
    TRI_DestroyVectorPointer(&fields);
    LOG_TRACE("bitarray-index previously created");

    if (created != NULL) {
      *created = false;
    }

    return idx;
  }


  // ...........................................................................
  // Create the bitarray index
  // ...........................................................................

  idx = TRI_CreateBitarrayIndex(document, iid, &fields, &paths, (TRI_vector_pointer_t*)(values), supportUndef, errorNum, errorStr);
  
  if (idx == NULL) {
    TRI_DestroyVector(&paths);
    TRI_DestroyVectorPointer(&fields);
    TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
    return NULL;
  }

  // ...........................................................................
  // release memory allocated to fields & paths vectors
  // ...........................................................................

  TRI_DestroyVector(&paths);
  TRI_DestroyVectorPointer(&fields);

  // ...........................................................................
  // initialises the index with all existing documents
  // ...........................................................................

  res = FillIndex(document, idx);

  if (res != TRI_ERROR_NO_ERROR) {

    // .........................................................................
    // for some reason one or more of the existing documents has caused the
    // index to fail. Remove the index from the collection and return null.
    // .........................................................................

    *errorNum = res;
    *errorStr = TRI_DuplicateString("Bitarray index creation aborted due to documents within collection.");
    TRI_FreeBitarrayIndex(idx);

    return NULL;
  }

  // ...........................................................................
  // store index within the collection and return
  // ...........................................................................

  res = AddIndex(document, idx);

  if (res != TRI_ERROR_NO_ERROR) {
    *errorNum = res;
    *errorStr = TRI_DuplicateString(TRI_errno_string(res));
    TRI_FreeBitarrayIndex(idx);

    return NULL;
  }

  if (created != NULL) {
    *created = true;
  }

  return idx;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief restores an index
////////////////////////////////////////////////////////////////////////////////

static int BitarrayIndexFromJson (TRI_document_collection_t* document,
                                  TRI_json_t const* definition,
                                  TRI_idx_iid_t iid,
                                  TRI_index_t** dst) {
  return BitarrayBasedIndexFromJson(document, definition, iid, CreateBitarrayIndexDocumentCollection, dst);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief finds a bitarray index
////////////////////////////////////////////////////////////////////////////////

TRI_index_t* TRI_LookupBitarrayIndexDocumentCollection (TRI_document_collection_t* document,
                                                        const TRI_vector_pointer_t* attributes) {
  TRI_index_t* idx;
  TRI_vector_pointer_t fields;
  TRI_vector_t paths;
  int result;

  // ...........................................................................
  // determine the unsorted shape ids for the attributes
  // ...........................................................................

  result = PidNamesByAttributeNames(attributes, 
                                    document->_shaper,
                                    &paths, 
                                    &fields, 
                                    false,
                                    false);

  if (result != TRI_ERROR_NO_ERROR) {
    return NULL;
  }

  idx = LookupPathIndexDocumentCollection(document, &paths, TRI_IDX_TYPE_BITARRAY_INDEX, false, true);

  // .............................................................................
  // release memory allocated to vector
  // .............................................................................

  TRI_DestroyVector(&paths);
  TRI_DestroyVectorPointer(&fields);

  return idx;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures that a bitarray index exists
////////////////////////////////////////////////////////////////////////////////

TRI_index_t* TRI_EnsureBitarrayIndexDocumentCollection (TRI_document_collection_t* document,
                                                        TRI_idx_iid_t iid,
                                                        const TRI_vector_pointer_t* attributes,
                                                        const TRI_vector_pointer_t* values,
                                                        bool supportUndef,
                                                        bool* created,
                                                        int* errorCode,
                                                        char** errorStr,
                                                        TRI_server_id_t generatingServer) {
  TRI_index_t* idx;

  *errorCode = TRI_ERROR_NO_ERROR;
  *errorStr  = NULL;

  TRI_ReadLockReadWriteLock(&document->base._vocbase->_inventoryLock);

  // .............................................................................
  // inside write-lock the collection
  // .............................................................................

  TRI_WRITE_LOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(document);
  
  idx = CreateBitarrayIndexDocumentCollection(document, attributes, values, iid, supportUndef, created, errorCode, errorStr);

  if (idx != NULL) {
    if (created) {
      int res = TRI_SaveIndex(document, idx, generatingServer);

      // ...........................................................................
      // If index could not be saved, report the error and return NULL
      // TODO: get TRI_SaveIndex to report the error
      // ...........................................................................

      if (res != TRI_ERROR_NO_ERROR) {
        idx = NULL;
        *errorCode = res;
        *errorStr  = TRI_DuplicateString("Bitarray index could not be saved.");
      }
    }
  }
  
  TRI_WRITE_UNLOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(document);

  // .............................................................................
  // outside write-lock
  // .............................................................................

  TRI_ReadUnlockReadWriteLock(&document->base._vocbase->_inventoryLock);

  // .............................................................................
  // Index already exists so simply return it
  // .............................................................................

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

  TRI_shaper_t* shaper = document->_shaper;

  // use filtered to hold copies of the master pointer
  std::vector<TRI_doc_mptr_copy_t> filtered;

  // do a full scan
  TRI_doc_mptr_t** ptr = (TRI_doc_mptr_t**) (document->_primaryIndex._table);
  TRI_doc_mptr_t** end = (TRI_doc_mptr_t**) ptr + document->_primaryIndex._nrAlloc;

  for (;  ptr < end;  ++ptr) {
    if (*ptr != nullptr && IsExampleMatch(trxCollection, shaper, *ptr, length, pids, values)) {
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
  // no extra locking here as the collection is already locked
  return RemoveDocumentShapedJson(trxCollection, (TRI_voc_key_t) TRI_EXTRACT_MARKER_KEY(doc), 0, policy, false, false);  // PROTECTED by trx in trxCollection
}

////////////////////////////////////////////////////////////////////////////////
/// @brief rotate the current journal of the collection
/// use this for testing only
////////////////////////////////////////////////////////////////////////////////

int TRI_RotateJournalDocumentCollection (TRI_document_collection_t* document) {
  // TODO: re-create this functionality
  return TRI_ERROR_NOT_IMPLEMENTED;
}

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:

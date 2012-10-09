////////////////////////////////////////////////////////////////////////////////
/// @brief primary collection
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2011 triagens GmbH, Cologne, Germany
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
/// @author Copyright 2011, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "primary-collection.h"

#include <BasicsC/conversions.h>
#include <BasicsC/files.h>
#include <BasicsC/hashes.h>
#include <BasicsC/logging.h>
#include <BasicsC/strings.h>

#include <VocBase/voc-shaper.h>

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief hashs the document id
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashKeyHeader (TRI_associative_pointer_t* array, void const* key) {
  TRI_voc_did_t const* k = key;

  return TRI_FnvHashPointer(k, sizeof(TRI_voc_did_t));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief hashs the document header
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashElementDocument (TRI_associative_pointer_t* array, void const* element) {
  TRI_doc_mptr_t const* e = element;

  return TRI_FnvHashPointer(&e->_did, sizeof(TRI_voc_did_t));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compares a document id and a document
////////////////////////////////////////////////////////////////////////////////

static bool IsEqualKeyDocument (TRI_associative_pointer_t* array, void const* key, void const* element) {
  TRI_voc_did_t const* k = key;
  TRI_doc_mptr_t const* e = element;

  return *k == e->_did;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new document in the collection from shaped json
////////////////////////////////////////////////////////////////////////////////

static TRI_voc_did_t CreateLock (TRI_primary_collection_t* document,
                                 TRI_df_marker_type_e type,
                                 TRI_shaped_json_t const* json,
                                 void const* data,
                                 bool forceSync) {
  document->beginWrite(document);
  return document->create(document, type, json, data, 0, 0, true, forceSync)._did;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new document in the collection from json
////////////////////////////////////////////////////////////////////////////////

static TRI_doc_mptr_t CreateJson (TRI_primary_collection_t* collection,
                                  TRI_df_marker_type_e type,
                                  TRI_json_t const* json,
                                  void const* data,
                                  bool reuseId,
                                  bool release,
                                  bool forceSync) {
  TRI_shaped_json_t* shaped;
  TRI_doc_mptr_t result;
  TRI_voc_did_t did = 0;
  TRI_voc_rid_t rid = 0;

  shaped = TRI_ShapedJsonJson(collection->_shaper, json);

  if (shaped == 0) {
    collection->base._lastError = TRI_set_errno(TRI_ERROR_ARANGO_SHAPER_FAILED);
    memset(&result, 0, sizeof(result));
    return result;
  }
  
  if (reuseId && json != NULL && json->_type == TRI_JSON_ARRAY) {
    TRI_json_t* id = TRI_LookupArrayJson((TRI_json_t*) json, "_id");
    TRI_json_t* rev = TRI_LookupArrayJson((TRI_json_t*) json, "_rev");

    if (id != NULL && id->_type == TRI_JSON_NUMBER && 
        rev != NULL && rev->_type == TRI_JSON_NUMBER) {
      // read existing document id and revision id from document
      did = (TRI_voc_did_t) id->_value._number;
      rid = (TRI_voc_rid_t) rev->_value._number;
    }
  }

  result = collection->create(collection, type, shaped, data, did, rid, release, forceSync);

  TRI_FreeShapedJson(collection->_shaper, shaped);

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief updates a document in the collection from shaped json
////////////////////////////////////////////////////////////////////////////////

static int UpdateLock (TRI_primary_collection_t* document,
                       TRI_shaped_json_t const* json,
                       TRI_voc_did_t did,
                       TRI_voc_rid_t rid,
                       TRI_voc_rid_t* oldRid,
                       TRI_doc_update_policy_e policy,
                       bool forceSync) {
  TRI_doc_mptr_t result;

  document->beginWrite(document);
  result = document->update(document, json, did, rid, oldRid, policy, true, forceSync);

  if (result._did == 0) {
    return TRI_errno();
  }
  else {
    return TRI_ERROR_NO_ERROR;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief updates a document in the collection from json
////////////////////////////////////////////////////////////////////////////////

static TRI_doc_mptr_t UpdateJson (TRI_primary_collection_t* collection,
                                  TRI_json_t const* json,
                                  TRI_voc_did_t did,
                                  TRI_voc_rid_t rid,
                                  TRI_voc_rid_t* oldRid,
                                  TRI_doc_update_policy_e policy,
                                  bool release,
                                  bool forceSync) {
  TRI_shaped_json_t* shaped;
  TRI_doc_mptr_t result;

  shaped = TRI_ShapedJsonJson(collection->_shaper, json);

  if (shaped == 0) {
    collection->base._lastError = TRI_set_errno(TRI_ERROR_ARANGO_SHAPER_FAILED);
    memset(&result, 0, sizeof(result));
    return result;
  }

  result = collection->update(collection, shaped, did, rid, oldRid, policy, release, forceSync);

  TRI_FreeShapedJson(collection->_shaper, shaped);

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes a json document given the identifier under a write lock
////////////////////////////////////////////////////////////////////////////////

static int DestroyLock (TRI_primary_collection_t* document,
                        TRI_voc_did_t did,
                        TRI_voc_rid_t rid,
                        TRI_voc_rid_t* oldRid,
                        TRI_doc_update_policy_e policy,
                        bool forceSync) {
  document->beginWrite(document);
  return document->destroy(document, did, rid, oldRid, policy, true, forceSync);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns information about the collection
////////////////////////////////////////////////////////////////////////////////

static TRI_doc_collection_info_t* Figures (TRI_primary_collection_t* document) {
  TRI_doc_collection_info_t* info;
  TRI_collection_t* base;
  size_t i;

  // prefill with 0's to init counters
  info = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_doc_collection_info_t), true);

  if (info == NULL) {
    return NULL;
  }

  for (i = 0;  i < document->_datafileInfo._nrAlloc;  ++i) {
    TRI_doc_datafile_info_t* d = document->_datafileInfo._table[i];

    if (d != NULL) {
      info->_numberAlive += d->_numberAlive;
      info->_numberDead += d->_numberDead;
      info->_sizeAlive += d->_sizeAlive;
      info->_sizeDead += d->_sizeDead;
      info->_numberDeletion += d->_numberDeletion;
    }
  }

  // add the file sizes for datafiles and journals
  base = &document->base;
  for (i = 0; i < base->_datafiles._length; ++i) {
    TRI_datafile_t* df = (TRI_datafile_t*) base->_datafiles._buffer[i];

    info->_datafileSize += df->_maximalSize;
    ++info->_numberDatafiles;
  }

  for (i = 0; i < base->_journals._length; ++i) {
    TRI_datafile_t* df = (TRI_datafile_t*) base->_journals._buffer[i];

    info->_journalfileSize += df->_maximalSize;
    ++info->_numberJournalfiles;
  }

  return info;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief hashs a datafile identifier
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashKeyDatafile (TRI_associative_pointer_t* array, void const* key) {
  TRI_voc_tick_t const* k = key;

  return *k;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief hashs a datafile identifier
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashElementDatafile (TRI_associative_pointer_t* array, void const* element) {
  TRI_doc_datafile_info_t const* e = element;

  return e->_fid;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compares a datafile identifier and a datafile info
////////////////////////////////////////////////////////////////////////////////

static bool IsEqualKeyElementDatafile (TRI_associative_pointer_t* array, void const* key, void const* element) {
  TRI_voc_tick_t const* k = key;
  TRI_doc_datafile_info_t const* e = element;

  return *k == e->_fid;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a journal or a compactor journal
////////////////////////////////////////////////////////////////////////////////

static TRI_datafile_t* CreateJournal (TRI_primary_collection_t* collection, bool compactor) {
  TRI_col_header_marker_t cm;
  TRI_datafile_t* journal;
  TRI_df_marker_t* position;
  bool ok;
  int res;
  char* filename;
  char* jname;
  char* number;

  // construct a suitable filename
  number = TRI_StringUInt32(TRI_NewTickVocBase());

  if (compactor) {
    jname = TRI_Concatenate3String("journal-", number, ".db");
  }
  else {
    jname = TRI_Concatenate3String("compactor-", number, ".db");
  }

  filename = TRI_Concatenate2File(collection->base._directory, jname);

  TRI_FreeString(TRI_CORE_MEM_ZONE, number);
  TRI_FreeString(TRI_CORE_MEM_ZONE, jname);

  // create journal file
  journal = TRI_CreateDatafile(filename, collection->base._maximalSize);

  if (journal == NULL) {
    if (TRI_errno() == TRI_ERROR_OUT_OF_MEMORY_MMAP) {
      collection->base._lastError = TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY_MMAP);
      collection->base._state = TRI_COL_STATE_READ;
    }
    else {
      collection->base._lastError = TRI_set_errno(TRI_ERROR_ARANGO_NO_JOURNAL);
      collection->base._state = TRI_COL_STATE_WRITE_ERROR;
    }

    LOG_ERROR("cannot create new journal in '%s'", filename);

    TRI_FreeString(TRI_CORE_MEM_ZONE, filename);
    return NULL;
  }

  TRI_FreeString(TRI_CORE_MEM_ZONE, filename);
  LOG_TRACE("created a new journal '%s'", journal->_filename);

  // and use the correct name
  number = TRI_StringUInt32(journal->_fid);

  if (compactor) {
    jname = TRI_Concatenate3String("compactor-", number, ".db");
  }
  else {
    jname = TRI_Concatenate3String("journal-", number, ".db");
  }

  filename = TRI_Concatenate2File(collection->base._directory, jname);

  TRI_FreeString(TRI_CORE_MEM_ZONE, number);
  TRI_FreeString(TRI_CORE_MEM_ZONE, jname);

  ok = TRI_RenameDatafile(journal, filename);

  if (! ok) {
    LOG_WARNING("failed to rename the journal to '%s': %s", filename, TRI_last_error());
  }
  else {
    LOG_TRACE("renamed journal to '%s'", filename);
  }

  TRI_FreeString(TRI_CORE_MEM_ZONE, filename);

  // create a collection header
  res = TRI_ReserveElementDatafile(journal, sizeof(TRI_col_header_marker_t), &position);

  if (res != TRI_ERROR_NO_ERROR) {
    collection->base._lastError = journal->_lastError;
    TRI_FreeDatafile(journal);

    LOG_ERROR("cannot create document header in journal '%s': %s", filename, TRI_last_error());

    return NULL;
  }

  memset(&cm, 0, sizeof(cm));

  cm.base._size = sizeof(TRI_col_header_marker_t);
  cm.base._type = TRI_COL_MARKER_HEADER;
  cm.base._tick = TRI_NewTickVocBase();

  cm._cid = collection->base._cid;

  TRI_FillCrcMarkerDatafile(&cm.base, sizeof(cm), 0, 0);

  res = TRI_WriteElementDatafile(journal, position, &cm.base, sizeof(cm), 0, 0, true);

  if (res != TRI_ERROR_NO_ERROR) {
    collection->base._lastError = journal->_lastError;
    TRI_FreeDatafile(journal);

    LOG_ERROR("cannot create document header in journal '%s': %s", filename, TRI_last_error());

    return NULL;
  }

  // that's it
  if (compactor) {
    TRI_PushBackVectorPointer(&collection->base._compactors, journal);
  }
  else {
    TRI_PushBackVectorPointer(&collection->base._journals, journal);
  }

  return journal;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief closes a journal
///
/// Note that the caller must hold a lock protecting the _datafiles and
/// _journals entry.
////////////////////////////////////////////////////////////////////////////////

static bool CloseJournalPrimaryCollection (TRI_primary_collection_t* collection,
                                           size_t position,
                                           bool compactor) {
  TRI_datafile_t* journal;
  TRI_vector_pointer_t* vector;
  bool ok;
  int res;
  char* dname;
  char* filename;
  char* number;

  // either use a journal or a compactor
  if (compactor) {
    vector = &collection->base._compactors;
  }
  else {
    vector = &collection->base._journals;
  }

  // no journal at this position
  if (vector->_length <= position) {
    TRI_set_errno(TRI_ERROR_ARANGO_NO_JOURNAL);
    return false;
  }

  // seal and rename datafile
  journal = vector->_buffer[position];
  res = TRI_SealDatafile(journal);

  if (res != TRI_ERROR_NO_ERROR) {
    LOG_ERROR("failed to seal datafile '%s': %s", journal->_filename, TRI_last_error());

    TRI_RemoveVectorPointer(vector, position);
    TRI_PushBackVectorPointer(&collection->base._datafiles, journal);

    return false;
  }

  number = TRI_StringUInt32(journal->_fid);
  dname = TRI_Concatenate3String("datafile-", number, ".db");
  filename = TRI_Concatenate2File(collection->base._directory, dname);

  TRI_FreeString(TRI_CORE_MEM_ZONE, dname);
  TRI_FreeString(TRI_CORE_MEM_ZONE, number);

  ok = TRI_RenameDatafile(journal, filename);

  if (! ok) {
    LOG_ERROR("failed to rename datafile '%s' to '%s': %s", journal->_filename, filename, TRI_last_error());

    TRI_RemoveVectorPointer(vector, position);
    TRI_PushBackVectorPointer(&collection->base._datafiles, journal);
    TRI_FreeString(TRI_CORE_MEM_ZONE, filename);

    return false;
  }

  TRI_FreeString(TRI_CORE_MEM_ZONE, filename);

  LOG_TRACE("closed journal '%s'", journal->_filename);

  TRI_RemoveVectorPointer(vector, position);
  TRI_PushBackVectorPointer(&collection->base._datafiles, journal);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief free an assoc array of datafile infos
////////////////////////////////////////////////////////////////////////////////
  
static void FreeDatafileInfo (TRI_associative_pointer_t* const files) {
  size_t i;
  size_t n;

  n = files->_nrAlloc;
  for (i = 0; i < n; ++i) {
    TRI_doc_datafile_info_t* file = files->_table[i];
    if (!file) {
      continue;
    }

    TRI_Free(TRI_UNKNOWN_MEM_ZONE, file);
  }

  TRI_DestroyAssociativePointer(files);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises a primary collection
////////////////////////////////////////////////////////////////////////////////

void TRI_InitPrimaryCollection (TRI_primary_collection_t* collection,
                                TRI_shaper_t* shaper) {
  collection->_shaper = shaper;
  collection->_capConstraint = NULL;

  collection->createLock = CreateLock;
  collection->createJson = CreateJson;

  collection->updateLock = UpdateLock;
  collection->updateJson = UpdateJson;

  collection->destroyLock = DestroyLock;

  collection->figures = Figures;

  TRI_InitBarrierList(&collection->_barrierList, collection);

  TRI_InitAssociativePointer(&collection->_datafileInfo,
                             TRI_UNKNOWN_MEM_ZONE, 
                             HashKeyDatafile,
                             HashElementDatafile,
                             IsEqualKeyElementDatafile,
                             NULL);

  TRI_InitAssociativePointer(&collection->_primaryIndex,
                             TRI_UNKNOWN_MEM_ZONE, 
                             HashKeyHeader,
                             HashElementDocument,
                             IsEqualKeyDocument,
                             0);
  
  TRI_InitReadWriteLock(&collection->_lock);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a primary collection
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyPrimaryCollection (TRI_primary_collection_t* collection) {
  TRI_DestroyReadWriteLock(&collection->_lock);
  TRI_DestroyAssociativePointer(&collection->_primaryIndex);

  if (collection->_shaper != NULL) {
    TRI_FreeVocShaper(collection->_shaper);
  }
  
  FreeDatafileInfo(&collection->_datafileInfo);
  TRI_DestroyBarrierList(&collection->_barrierList);

  TRI_DestroyCollection(&collection->base);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                               protected functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief finds a datafile description
////////////////////////////////////////////////////////////////////////////////

TRI_doc_datafile_info_t* TRI_FindDatafileInfoPrimaryCollection (TRI_primary_collection_t* collection,
                                                                TRI_voc_fid_t fid) {
  TRI_doc_datafile_info_t const* found;
  TRI_doc_datafile_info_t* dfi;

  found = TRI_LookupByKeyAssociativePointer(&collection->_datafileInfo, &fid);

  if (found != NULL) {
    union { TRI_doc_datafile_info_t const* c; TRI_doc_datafile_info_t* v; } cnv;

    cnv.c = found;
    return cnv.v;
  }

  dfi = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_doc_datafile_info_t), true);

  if (dfi == NULL) {
    return NULL;
  }

  dfi->_fid = fid;

  TRI_InsertKeyAssociativePointer(&collection->_datafileInfo, &fid, dfi, true);

  return dfi;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a journal
///
/// Note that the caller must hold a lock protecting the _journals entry.
////////////////////////////////////////////////////////////////////////////////

TRI_datafile_t* TRI_CreateJournalPrimaryCollection (TRI_primary_collection_t* collection) {
  return CreateJournal(collection, false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief closes a journal
///
/// Note that the caller must hold a lock protecting the _datafiles and
/// _journals entry.
////////////////////////////////////////////////////////////////////////////////

bool TRI_CloseJournalPrimaryCollection (TRI_primary_collection_t* collection,
                                    size_t position) {
  return CloseJournalPrimaryCollection(collection, position, false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new compactor file
///
/// Note that the caller must hold a lock protecting the _journals entry.
////////////////////////////////////////////////////////////////////////////////

TRI_datafile_t* TRI_CreateCompactorPrimaryCollection (TRI_primary_collection_t* collection) {
  return CreateJournal(collection, true);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief closes an existing compactor file
///
/// Note that the caller must hold a lock protecting the _datafiles and
/// _journals entry.
////////////////////////////////////////////////////////////////////////////////

bool TRI_CloseCompactorPrimaryCollection (TRI_primary_collection_t* collection,
                                      size_t position) {
  return CloseJournalPrimaryCollection(collection, position, true);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Convert a raw data document pointer into a master pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_MarkerMasterPointer (void const* data, TRI_doc_mptr_t* header) {
  TRI_doc_document_marker_t const* marker;
  marker = (TRI_doc_document_marker_t const*) data;

  header->_did = marker->_did;
  header->_rid = marker->_rid;
  header->_fid = 0; // should be datafile->_fid, but we do not have this info here
  header->_deletion = 0;
  header->_data = data;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts the data length from a master pointer
////////////////////////////////////////////////////////////////////////////////

size_t TRI_LengthDataMasterPointer (const TRI_doc_mptr_t* const mptr) {
  if (mptr != NULL) {
    void const* data = mptr->_data;

    if (((TRI_df_marker_t const*) data)->_type == TRI_DOC_MARKER_DOCUMENT) {
      return ((TRI_df_marker_t*) data)->_size - sizeof(TRI_doc_document_marker_t);
    }
    else if (((TRI_df_marker_t const*) data)->_type == TRI_DOC_MARKER_EDGE) {
      return ((TRI_df_marker_t*) data)->_size - sizeof(TRI_doc_edge_marker_t);
    }
  }

  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:

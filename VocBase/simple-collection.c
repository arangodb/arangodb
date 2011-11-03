////////////////////////////////////////////////////////////////////////////////
/// @brief document collection with global read-write lock
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

#include "simple-collection.h"

#include <Basics/conversions.h>
#include <Basics/files.h>
#include <Basics/hashes.h>
#include <Basics/logging.h>
#include <Basics/strings.h>

#include <VocBase/index.h>
#include <VocBase/result-set.h>
#include <VocBase/voc-shaper.h>

// -----------------------------------------------------------------------------
// --SECTION--                                              forward declarations
// -----------------------------------------------------------------------------

static bool CreateImmediateIndexes (TRI_sim_collection_t* collection,
                                    TRI_doc_mptr_t* header);

static bool UpdateImmediateIndexes (TRI_sim_collection_t* collection,
                                    TRI_doc_mptr_t const* header,
                                    TRI_doc_mptr_t const* update);

static bool DeleteImmediateIndexes (TRI_sim_collection_t* collection,
                                    TRI_doc_mptr_t const* header,
                                    TRI_voc_tick_t);

static TRI_index_t* CreateGeoIndexSimCollection (TRI_sim_collection_t* collection,
                                                 char const* location,
                                                 char const* latitude,
                                                 char const* longitude,
                                                 bool geoJson,
                                                 TRI_idx_iid iid);

static uint64_t HashKeyHeader (TRI_associative_pointer_t* array, void const* key);

static uint64_t HashElementDocument (TRI_associative_pointer_t* array, void const* element);

static bool EqualKeyDocument (TRI_associative_pointer_t* array, void const* key, void const* element);

// -----------------------------------------------------------------------------
// --SECTION--                                                          JOURNALS
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBasePrivate VocBase (Private)
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief selects a journal, possibly waits until a journal appears
///
/// Note that the function is called while holding a write-lock. We have to
/// release this lock, in order to allow the gc to start.
////////////////////////////////////////////////////////////////////////////////

static TRI_datafile_t* SelectJournal (TRI_sim_collection_t* collection,
                                      TRI_voc_size_t size,
                                      TRI_df_marker_t** result) {
  TRI_datafile_t* datafile;
  bool ok;
  size_t i;
  size_t n;

  TRI_LockCondition(&collection->_journalsCondition);

  while (true) {
    n = TRI_SizeVectorPointer(&collection->base.base._journals);

    for (i = 0;  i < n;  ++i) {

      // select datafile
      datafile = TRI_AtVectorPointer(&collection->base.base._journals, i);

      // try to reserve space
      ok = TRI_ReserveElementDatafile(datafile, size, result);

      // in case of full datafile, try next
      if (ok) {
        TRI_UnlockCondition(&collection->_journalsCondition);
        return datafile;
      }
      else if (! ok && TRI_errno() != TRI_VOC_ERROR_DATAFILE_FULL) {
        TRI_UnlockCondition(&collection->_journalsCondition);
        return NULL;
      }
    }

    TRI_WaitCondition(&collection->_journalsCondition);
  }

  TRI_UnlockCondition(&collection->_journalsCondition);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief waits for synchronisation
////////////////////////////////////////////////////////////////////////////////

static void WaitSync (TRI_sim_collection_t* collection,
                      TRI_datafile_t* journal,
                      char const* position) {
  TRI_collection_t* base;
  bool done;

  base = &collection->base.base;

  // no condition at all
  if (0 == base->_syncAfterObjects && 0 == base->_syncAfterBytes && 0 == base->_syncAfterTime) {
    return;
  }

  TRI_LockCondition(&collection->_journalsCondition);

  // wait until the sync condition is fullfilled
  while (true) {
    done = true;

    // check for error
    if (journal->_state == TRI_DF_STATE_WRITE_ERROR) {
      break;
    }

    // always sync
    if (1 == base->_syncAfterObjects) {
      if (journal->_synced < position) {
        done = false;
      }
    }

    // at most that many outstanding objects
    else if (1 < base->_syncAfterObjects) {
      if (journal->_nWritten - journal->_nSynced < base->_syncAfterObjects) {
        done = false;
      }
    }

    // at most that many outstanding bytes
    if (0 < base->_syncAfterBytes) {
      if (journal->_written - journal->_synced < base->_syncAfterBytes) {
        done = false;
      }
    }

    // at most that many seconds
    if (0 < base->_syncAfterTime && journal->_synced < journal->_written) {
      double t = TRI_microtime();

      if (journal->_lastSynced < t - base->_syncAfterTime) {
        done = false;
      }
    }

    // stop waiting of we reached our limits
    if (done) {
      break;
    }

    // we have to wait a bit longer
    TRI_WaitCondition(&collection->_journalsCondition);
  }

  TRI_UnlockCondition(&collection->_journalsCondition);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief writes data to the journal and updates the barriers
////////////////////////////////////////////////////////////////////////////////

static bool WriteElement (TRI_sim_collection_t* collection,
                          TRI_datafile_t* journal,
                          TRI_df_marker_t* marker,
                          TRI_voc_size_t markerSize,
                          void const* body,
                          TRI_voc_size_t bodySize,
                          TRI_df_marker_t* result) {
  bool ok;

  ok = TRI_WriteElementDatafile(journal,
                                result,
                                marker, markerSize,
                                body, bodySize,
                                false);

  if (! ok) {
    return false;
  }

  TRI_LockCondition(&collection->_journalsCondition);
  journal->_written = ((char*) result) + marker->_size;
  journal->_nWritten++;
  TRI_UnlockCondition(&collection->_journalsCondition);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                     DOCUMENT CRUD
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBasePrivate VocBase (Private)
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new document splitted into marker and body to file
////////////////////////////////////////////////////////////////////////////////

static bool CreateDocument (TRI_sim_collection_t* collection,
                            TRI_doc_document_marker_t* marker,
                            void const* body,
                            TRI_voc_size_t bodySize,
                            TRI_df_marker_t** result,
                            TRI_voc_did_t* did) {
  TRI_datafile_t* journal;
  TRI_doc_mptr_t* header;
  TRI_voc_size_t total;
  bool ok;

  // get a new header pointer
  header = collection->_headers->request(collection->_headers);

  // generate a new tick
  *did = marker->_rid = marker->_did = marker->base._tick = TRI_NewTickVocBase();

  // find and select a journal
  total = sizeof(TRI_doc_document_marker_t) + bodySize;
  journal = SelectJournal(collection, total, result);

  if (journal == NULL) {
    collection->base.base._lastError = TRI_set_errno(TRI_VOC_ERROR_NO_JOURNAL);
    return false;
  }

  // verify the header pointer
  header = collection->_headers->verify(collection->_headers, header);

  // generate crc
  TRI_FillCrcMarkerDatafile(&marker->base, sizeof(TRI_doc_document_marker_t), body, bodySize);

  // and write marker and blob
  ok = WriteElement(collection, journal, &marker->base, sizeof(TRI_doc_document_marker_t), body, bodySize, *result);

  // generate create header
  if (ok) {
    header->_did = marker->_did;
    header->_rid = marker->_rid;
    header->_deletion = 0;
    header->_data = *result;
    header->_document._sid = ((TRI_doc_document_marker_t*) marker)->_shape;
    header->_document._data.length = ((TRI_df_marker_t const*) (header->_data))->_size - sizeof(TRI_doc_document_marker_t);
    header->_document._data.data = ((char*) header->_data) + sizeof(TRI_doc_document_marker_t);

    // update immediate indexes
    CreateImmediateIndexes(collection, header);

    // wait for sync
    WaitSync(collection, journal, ((char const*) *result) + sizeof(TRI_doc_document_marker_t) + bodySize);
  }
  else {
    LOG_ERROR("cannot write element: %s", TRI_last_error());
  }

  // return any error encountered
  return ok;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief updates an existing document splitted into marker and body to file
////////////////////////////////////////////////////////////////////////////////

static bool UpdateDocument (TRI_sim_collection_t* collection,
                            TRI_doc_document_marker_t* marker,
                            void const* body,
                            TRI_voc_size_t bodySize,
                            TRI_df_marker_t** result) {
  TRI_datafile_t* journal;
  TRI_doc_mptr_t const* header;
  TRI_voc_size_t total;
  bool ok;

  // get an existing header pointer
  header = TRI_FindByKeyAssociativePointer(&collection->_primaryIndex, &marker->_did);

  if (header == NULL || header->_deletion != 0) {
    TRI_set_errno(TRI_VOC_ERROR_DOCUMENT_NOT_FOUND);
    return false;
  }

  // generate a new tick
  marker->_rid = marker->base._tick = TRI_NewTickVocBase();

  // find and select a journal
  total = sizeof(TRI_doc_document_marker_t) + bodySize;
  journal = SelectJournal(collection, total, result);

  if (journal == NULL) {
    collection->base.base._lastError = TRI_set_errno(TRI_VOC_ERROR_NO_JOURNAL);
    return false;
  }

  // generate crc
  TRI_FillCrcMarkerDatafile(&marker->base, sizeof(TRI_doc_document_marker_t), body, bodySize);

  // and write marker and blob
  ok = WriteElement(collection, journal, &marker->base, sizeof(TRI_doc_document_marker_t), body, bodySize, *result);

  // update the header
  if (ok) {
    TRI_doc_mptr_t update;

    update = *header;

    update._rid = marker->_rid;
    update._data = *result;
    update._document._sid = ((TRI_doc_document_marker_t*) marker)->_shape;
    update._document._data.length = ((TRI_df_marker_t const*) (update._data))->_size - sizeof(TRI_doc_document_marker_t);
    update._document._data.data = ((char*) update._data) + sizeof(TRI_doc_document_marker_t);

    UpdateImmediateIndexes(collection, header, &update);
  }
  else {
    LOG_ERROR("cannot write element");
  }

  // return any error encountered
  return ok;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes an element and removes it from the index
////////////////////////////////////////////////////////////////////////////////

static bool DeleteDocument (TRI_sim_collection_t* collection,
                            TRI_doc_deletion_marker_t* marker) {
  TRI_datafile_t* journal;
  TRI_df_marker_t* result;
  TRI_doc_mptr_t const* header;
  TRI_voc_size_t total;
  bool ok;

  // get an existing header pointer
  header = TRI_FindByKeyAssociativePointer(&collection->_primaryIndex, &marker->_did);

  if (header == NULL || header->_deletion != 0) {
    TRI_set_errno(TRI_VOC_ERROR_DOCUMENT_NOT_FOUND);
    return false;
  }

  // generate a new tick
  marker->base._tick = TRI_NewTickVocBase();

  // find and select a journal
  total = sizeof(TRI_doc_deletion_marker_t);
  journal = SelectJournal(collection, total, &result);

  if (journal == NULL) {
    collection->base.base._lastError = TRI_set_errno(TRI_VOC_ERROR_NO_JOURNAL);
    return false;
  }

  // generate crc
  TRI_FillCrcMarkerDatafile(&marker->base, sizeof(TRI_doc_deletion_marker_t), 0, 0);

  // and write marker and blob
  ok = WriteElement(collection, journal, &marker->base, sizeof(TRI_doc_deletion_marker_t), 0, 0, result);

  // update the header
  if (ok) {
    DeleteImmediateIndexes(collection, header, marker->base._tick);
  }
  else {
    LOG_ERROR("cannot delete element");
  }

  return ok;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                               DOCUMENT COLLECTION
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBasePrivate VocBase (Private)
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new document in the collection from shaped json
////////////////////////////////////////////////////////////////////////////////

static TRI_voc_did_t CreateShapedJson (TRI_doc_collection_t* document,
                                       TRI_shaped_json_t const* json) {
  TRI_df_marker_t* result;
  TRI_doc_document_marker_t marker;
  TRI_sim_collection_t* collection;
  TRI_voc_did_t did;
  bool ok;

  collection = (TRI_sim_collection_t*) document;
  memset(&marker, 0, sizeof(marker));

  marker.base._size = sizeof(marker) + json->_data.length;
  marker.base._type = TRI_DOC_MARKER_DOCUMENT;

  marker._sid = 0;
  marker._shape = json->_sid;

  ok = CreateDocument(collection, &marker,
                      json->_data.data, json->_data.length,
                      &result,
                      &did);

  return ok ? did : 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reads an element from the document collection
////////////////////////////////////////////////////////////////////////////////

static TRI_doc_mptr_t const* ReadShapedJson (TRI_doc_collection_t* document,
                                             TRI_voc_did_t did) {
  TRI_sim_collection_t* collection;
  TRI_doc_mptr_t const* header;

  collection = (TRI_sim_collection_t*) document;

  header = TRI_FindByKeyAssociativePointer(&collection->_primaryIndex, &did);

  if (header == NULL || header->_deletion != 0) {
    return NULL;
  }
  else {
    return header;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief updates a document in the collection from shaped json
////////////////////////////////////////////////////////////////////////////////

static bool UpdateShapedJson (TRI_doc_collection_t* document,
                              TRI_shaped_json_t const* json,
                              TRI_voc_did_t did) {
  TRI_sim_collection_t* collection;
  TRI_doc_document_marker_t marker;
  TRI_df_marker_t* result;

  collection = (TRI_sim_collection_t*) document;

  memset(&marker, 0, sizeof(marker));

  marker.base._size = sizeof(marker) + json->_data.length;
  marker.base._type = TRI_DOC_MARKER_DOCUMENT;

  marker._did = did;
  marker._sid = 0;
  marker._shape = json->_sid;

  return UpdateDocument(collection, &marker,
                        json->_data.data, json->_data.length,
                        &result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes a json document given the identifier
////////////////////////////////////////////////////////////////////////////////

static bool DeleteShapedJson (TRI_doc_collection_t* document,
                              TRI_voc_did_t did) {
  TRI_sim_collection_t* collection;
  TRI_doc_deletion_marker_t marker;

  collection = (TRI_sim_collection_t*) document;

  memset(&marker, 0, sizeof(marker));

  marker.base._size = sizeof(marker);
  marker.base._type = TRI_DOC_MARKER_DELETION;

  marker._did = did;
  marker._sid = 0;

  return DeleteDocument(collection, &marker);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief read locks a collection
////////////////////////////////////////////////////////////////////////////////

static bool BeginRead (TRI_doc_collection_t* document) {
  TRI_sim_collection_t* collection;

  collection = (TRI_sim_collection_t*) document;
  TRI_ReadLockReadWriteLock(&collection->_lock);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief read unlocks a collection
////////////////////////////////////////////////////////////////////////////////

static bool EndRead (TRI_doc_collection_t* document) {
  TRI_sim_collection_t* collection;

  collection = (TRI_sim_collection_t*) document;
  TRI_ReadUnlockReadWriteLock(&collection->_lock);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief write locks a collection
////////////////////////////////////////////////////////////////////////////////

static bool BeginWrite (TRI_doc_collection_t* document) {
  TRI_sim_collection_t* collection;

  collection = (TRI_sim_collection_t*) document;
  TRI_WriteLockReadWriteLock(&collection->_lock);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief write unlocks a collection
////////////////////////////////////////////////////////////////////////////////

static bool EndWrite (TRI_doc_collection_t* document) {
  TRI_sim_collection_t* collection;

  collection = (TRI_sim_collection_t*) document;
  TRI_WriteUnlockReadWriteLock(&collection->_lock);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 SIMPLE COLLECTION
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBasePrivate VocBase (Private)
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief iterator for open
////////////////////////////////////////////////////////////////////////////////

static void OpenIterator (TRI_df_marker_t const* marker, void* data, bool journal) {
  TRI_sim_collection_t* collection = data;
  TRI_doc_mptr_t const* found;

  // new or updated document
  if (marker->_type == TRI_DOC_MARKER_DOCUMENT) {
    TRI_doc_document_marker_t const* d = (TRI_doc_document_marker_t const*) marker;

    LOG_TRACE("document did %lu, rid %lu",
              (unsigned long) d->_did,
              (unsigned long) d->_rid);

    found = TRI_FindByKeyAssociativePointer(&collection->_primaryIndex, &d->_did);

    // it is a new entry
    if (found == NULL) {
      TRI_doc_mptr_t* header;

      header = collection->_headers->request(collection->_headers);
      header = collection->_headers->verify(collection->_headers, header);

      header->_did = d->_did;
      header->_rid = d->_rid;
      header->_deletion = 0;
      header->_data = marker;
      header->_document._sid = d->_shape;
      header->_document._data.length = marker->_size - sizeof(TRI_doc_document_marker_t);
      header->_document._data.data = ((char*) marker) + sizeof(TRI_doc_document_marker_t);

      CreateImmediateIndexes(collection, header);
    }

    // it is an update, but only if found has a smaller revision identifier
    else if (found->_rid < d->_rid) {
      TRI_doc_mptr_t update;

      update = *found;

      update._rid = d->_rid;
      update._data = marker;
      update._document._sid = d->_shape;
      update._document._data.length = marker->_size - sizeof(TRI_doc_document_marker_t);
      update._document._data.data = ((char*) marker) + sizeof(TRI_doc_document_marker_t);

      UpdateImmediateIndexes(collection, found, &update);
    }
  }
  else if (marker->_type == TRI_DOC_MARKER_DELETION) {
    TRI_doc_deletion_marker_t const* d;

    d = (TRI_doc_deletion_marker_t const*) marker;

    LOG_TRACE("deletion did %lu, rid %lu, deletion %lu",
              (unsigned long) d->_did,
              (unsigned long) d->_rid,
              (unsigned long) marker->_tick);

    found = TRI_FindByKeyAssociativePointer(&collection->_primaryIndex, &d->_did);

    // it is a new entry, so we missed the create
    if (found == NULL) {
      TRI_doc_mptr_t* header;

      header = collection->_headers->request(collection->_headers);
      header = collection->_headers->verify(collection->_headers, header);

      header->_did = d->_did;
      header->_rid = d->_rid;
      header->_deletion = marker->_tick;
      header->_data = 0;
      header->_document._data.length = 0;
      header->_document._data.data = 0;

      CreateImmediateIndexes(collection, header);
    }

    // it is a delete
    else {
      DeleteImmediateIndexes(collection, found, marker->_tick);
    }
  }
  else {
    LOG_TRACE("skipping marker %lu", (unsigned long) marker->_type);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief iterator for index open
////////////////////////////////////////////////////////////////////////////////

static void OpenIndex (char const* filename, void* data) {
  TRI_idx_iid iid;
  TRI_json_t* gjs;
  TRI_json_t* iis;
  TRI_json_t* json;
  TRI_json_t* lat;
  TRI_json_t* loc;
  TRI_json_t* lon;
  TRI_json_t* type;
  TRI_sim_collection_t* doc;
  bool geoJson;
  char const* typeStr;
  char* error;

  json = TRI_JsonFile(filename, &error);

  if (json == NULL) {
    LOG_ERROR("cannot read index definition from '%s': %s", filename, error);
    return;
  }

  if (json->_type != TRI_JSON_ARRAY) {
    LOG_ERROR("cannot read index definition from '%s': expecting an array", filename);
    TRI_FreeJson(json);
    return;
  }

  type = TRI_LookupArrayJson(json, "type");

  if (type->_type != TRI_JSON_STRING) {
    LOG_ERROR("cannot read index definition from '%s': expecting a string for type", filename);
    TRI_FreeJson(json);
    return;
  }

  typeStr = type->_value._string.data;
  doc = (TRI_sim_collection_t*) data;

  // geo index
  if (TRI_EqualString(typeStr, "geo")) {
    loc = TRI_LookupArrayJson(json, "location");
    lat = TRI_LookupArrayJson(json, "latitude");
    lon = TRI_LookupArrayJson(json, "longitude");
    iis = TRI_LookupArrayJson(json, "iid");
    gjs = TRI_LookupArrayJson(json, "geoJson");
    iid = 0;
    geoJson = false;

    if (iis != NULL && iis->_type == TRI_JSON_NUMBER) {
      iid = iis->_value._number;
    }

    if (gjs != NULL && gjs->_type == TRI_JSON_BOOLEAN) {
      geoJson = gjs->_value._boolean;
    }

    if (loc != NULL && loc->_type == TRI_JSON_STRING) {
      CreateGeoIndexSimCollection(doc, loc->_value._string.data, NULL, NULL, geoJson, iid);
    }
    else if (lat != NULL && lat->_type == TRI_JSON_STRING && lon != NULL && lon ->_type == TRI_JSON_STRING) {
      CreateGeoIndexSimCollection(doc, NULL, lat->_value._string.data, lon->_value._string.data, false, iid);
    }
    else {
      LOG_WARNING("ignore geo-index, need either 'location' or 'latitude' and 'longitude'");
    }
  }

  // ups
  else {
    LOG_WARNING("ignoring unknown index type '%s'", typeStr);
  }

  TRI_FreeJson(json);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises a document collection
////////////////////////////////////////////////////////////////////////////////

static void InitSimCollection (TRI_sim_collection_t* collection,
                               TRI_shaper_t* shaper) {
  TRI_InitDocCollection(&collection->base, shaper);

  TRI_InitReadWriteLock(&collection->_lock);

  collection->_headers = TRI_CreateSimpleHeaders();

  TRI_InitAssociativePointer(&collection->_primaryIndex,
                             HashKeyHeader,
                             HashElementDocument,
                             EqualKeyDocument,
                             0);

  TRI_InitCondition(&collection->_journalsCondition);

  TRI_InitVectorPointer(&collection->_indexes);
  collection->_geoIndex = NULL;

  collection->base.beginRead = BeginRead;
  collection->base.endRead = EndRead;

  collection->base.beginWrite = BeginWrite;
  collection->base.endWrite = EndWrite;

  collection->base.create = CreateShapedJson;
  collection->base.read = ReadShapedJson;
  collection->base.update = UpdateShapedJson;
  collection->base.destroy = DeleteShapedJson;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new collection
////////////////////////////////////////////////////////////////////////////////

TRI_sim_collection_t* TRI_CreateSimCollection (char const* path, TRI_col_parameter_t* parameter) {
  TRI_col_info_t info;
  TRI_collection_t* collection;
  TRI_shaper_t* shaper;
  TRI_sim_collection_t* doc;

  memset(&info, 0, sizeof(info));
  info._version = TRI_COL_VERSION;
  info._type = TRI_COL_TYPE_SIMPLE_DOCUMENT;
  info._cid = TRI_NewTickVocBase();
  TRI_CopyString(info._name, parameter->_name, sizeof(info._name));
  info._maximalSize = parameter->_maximalSize;
  info._size = sizeof(TRI_col_info_t);

  // first create the document collection
  doc = TRI_Allocate(sizeof(TRI_sim_collection_t));
  collection = TRI_CreateCollection(&doc->base.base, path, &info);

  if (collection == NULL) {
    LOG_ERROR("cannot create document collection");

    TRI_Free(doc);
    return NULL;
  }

  // then the shape collection
  shaper = TRI_CreateVocShaper(collection->_directory, "SHAPES");

  if (shaper == NULL) {
    LOG_ERROR("cannot create shapes collection");

    TRI_CloseCollection(collection);
    TRI_FreeCollection(collection);

    TRI_Free(doc);
    return NULL;
  }

  // create document collection and shaper
  InitSimCollection(doc, shaper);

  return doc;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the memory allocated, but does not free the pointer
///
/// Note that the collection must be closed first.
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroySimCollection (TRI_sim_collection_t* collection) {
  size_t n;
  size_t i;

  n = TRI_SizeVectorPointer(&collection->_indexes);

  for (i = 0;  i < n;  ++i) {
    TRI_index_t* idx;

    idx = TRI_AtVectorPointer(&collection->_indexes, i);
  }

  TRI_DestroyCondition(&collection->_journalsCondition);

  TRI_DestroyAssociativePointer(&collection->_primaryIndex);

  TRI_FreeSimpleHeaders(collection->_headers);

  TRI_DestroyReadWriteLock(&collection->_lock);

  if (collection->base._shaper != NULL) {
    TRI_FreeVocShaper(collection->base._shaper);
  }

  TRI_DestroyDocCollection(&collection->base);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the memory allocated and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeSimCollection (TRI_sim_collection_t* collection) {
  TRI_DestroySimCollection(collection);
  TRI_Free(collection);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new journal
////////////////////////////////////////////////////////////////////////////////

TRI_datafile_t* TRI_CreateJournalSimCollection (TRI_sim_collection_t* collection) {
  return TRI_CreateJournalDocCollection(&collection->base);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief closes an existing journal
////////////////////////////////////////////////////////////////////////////////

bool TRI_CloseJournalSimCollection (TRI_sim_collection_t* collection,
                                    size_t position) {
  return TRI_CloseJournalDocCollection(&collection->base, position);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief opens an existing collection
////////////////////////////////////////////////////////////////////////////////

TRI_sim_collection_t* TRI_OpenSimCollection (char const* path) {
  TRI_collection_t* collection;
  TRI_shaper_t* shaper;
  TRI_sim_collection_t* doc;
  char* shapes;

  // first open the document collection
  doc = TRI_Allocate(sizeof(TRI_sim_collection_t));
  collection = TRI_OpenCollection(&doc->base.base, path);

  if (collection == NULL) {
    LOG_ERROR("cannot open document collection");

    TRI_Free(doc);
    return NULL;
  }

  // then the shape collection
  shapes = TRI_Concatenate2File(collection->_directory, "SHAPES");
  shaper = TRI_OpenVocShaper(shapes);
  TRI_FreeString(shapes);

  if (shaper == NULL) {
    LOG_ERROR("cannot open shapes collection");

    TRI_CloseCollection(collection);
    TRI_FreeCollection(collection);

    return NULL;
  }

  // create document collection and shaper
  InitSimCollection(doc, shaper);

  // read all documents and fill indexes
  TRI_IterateCollection(collection, OpenIterator, collection);
  TRI_IterateIndexCollection(collection, OpenIndex, collection);

  return doc;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief closes an open collection
////////////////////////////////////////////////////////////////////////////////

bool TRI_CloseSimCollection (TRI_sim_collection_t* collection) {
  bool ok;

  ok = TRI_CloseCollection(&collection->base.base);

  if (! ok) {
    return false;
  }

  ok = TRI_CloseVocShaper(collection->base._shaper);

  if (! ok) {
    return false;
  }

  collection->base._shaper = NULL;

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                           INDEXES
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBasePrivate VocBase (Private)
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new entry in the immediate indexes
////////////////////////////////////////////////////////////////////////////////

static bool CreateImmediateIndexes (TRI_sim_collection_t* collection,
                                    TRI_doc_mptr_t* header) {
  TRI_doc_mptr_t* found;
  size_t n;
  size_t i;
  bool ok;
  bool result;

  // add a new header
  found = TRI_InsertKeyAssociativePointer(&collection->_primaryIndex, &header->_did, header, false);

  if (found != NULL) {
    LOG_ERROR("document %lu already existed with revision %lu while creating revision %lu",
              (unsigned long) header->_did,
              (unsigned long) found->_rid,
              (unsigned long) header->_rid);

    collection->_headers->release(collection->_headers, header);
    return false;
  }

  // return in case of a delete
  if (header->_deletion != 0) {
    return true;
  }

  // update all the other indices
  n = TRI_SizeVectorPointer(&collection->_indexes);
  result = true;

  for (i = 0;  i < n;  ++i) {
    TRI_index_t* idx;

    idx = TRI_AtVectorPointer(&collection->_indexes, i);

    ok = idx->insert(idx, header);

    if (! ok) {
      result = false;
    }
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief updates the immediate indexes
////////////////////////////////////////////////////////////////////////////////

static bool UpdateImmediateIndexes (TRI_sim_collection_t* collection,
                                    TRI_doc_mptr_t const* header,
                                    TRI_doc_mptr_t const* update) {

  union { TRI_doc_mptr_t const* c; TRI_doc_mptr_t* v; } change;
  TRI_shaped_json_t old;
  bool ok;
  bool result;
  size_t i;
  size_t n;

  // get the old document
  old = header->_document;

  // update all fields, the document identifier stays the same
  change.c = header;

  change.v->_rid = update->_rid;
  change.v->_eid = update->_eid;
  change.v->_deletion = update->_deletion;

  change.v->_data = update->_data;
  change.v->_document = update->_document;

  // update all other indexes
  n = TRI_SizeVectorPointer(&collection->_indexes);
  result = true;

  for (i = 0;  i < n;  ++i) {
    TRI_index_t* idx;

    idx = TRI_AtVectorPointer(&collection->_indexes, i);

    ok = idx->update(idx, header, &old);

    if (! ok) {
      result = false;
    }
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes an entry from the immediate indexes
////////////////////////////////////////////////////////////////////////////////

static bool DeleteImmediateIndexes (TRI_sim_collection_t* collection,
                                    TRI_doc_mptr_t const* header,
                                    TRI_voc_tick_t deletion) {
  union { TRI_doc_mptr_t const* c; TRI_doc_mptr_t* v; } change;
  TRI_doc_mptr_t* found;
  size_t n;
  size_t i;
  bool result;
  bool ok;

  // set the deletion flag
  change.c = header;
  change.v->_deletion = deletion;

  // remove from main index
  found = TRI_RemoveKeyAssociativePointer(&collection->_primaryIndex, &header->_did);

  if (found == NULL) {
    TRI_set_errno(TRI_VOC_ERROR_DOCUMENT_NOT_FOUND);
    return false;
  }

  // remove all other indexes
  n = TRI_SizeVectorPointer(&collection->_indexes);
  result = true;

  for (i = 0;  i < n;  ++i) {
    TRI_index_t* idx;

    idx = TRI_AtVectorPointer(&collection->_indexes, i);

    ok = idx->remove(idx, header);

    if (! ok) {
      result = false;
    }
  }

  // and release the header pointer
  collection->_headers->release(collection->_headers, change.v);

  // that's it
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises an index with all existing documents
////////////////////////////////////////////////////////////////////////////////

static void FillIndex (TRI_sim_collection_t* collection,
                       TRI_index_t* idx) {
  size_t n;
  size_t scanned;
  void** end;
  void** ptr;

  // update index
  n = collection->_primaryIndex._nrUsed;
  ptr = collection->_primaryIndex._table;
  end = collection->_primaryIndex._table + collection->_primaryIndex._nrAlloc;

  scanned = 0;

  for (;  ptr < end;  ++ptr) {
    if (*ptr) {
      ++scanned;

      idx->insert(idx, *ptr);

      if (scanned % 10000 == 0) {
        LOG_INFO("indexed %ld of %ld documents", scanned, n);
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                     PRIMARY INDEX
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBasePrivate VocBase (Private)
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

static bool EqualKeyDocument (TRI_associative_pointer_t* array, void const* key, void const* element) {
  TRI_voc_did_t const* k = key;
  TRI_doc_mptr_t const* e = element;

  return *k == e->_did;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                         GEO INDEX
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBasePrivate VocBase (Private)
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a geo index to a collection
////////////////////////////////////////////////////////////////////////////////

static TRI_index_t* CreateGeoIndexSimCollection (TRI_sim_collection_t* collection,
                                                 char const* location,
                                                 char const* latitude,
                                                 char const* longitude,
                                                 bool geoJson,
                                                 TRI_idx_iid iid) {
  TRI_index_t* idx;
  TRI_shape_pid_t lat;
  TRI_shape_pid_t loc;
  TRI_shape_pid_t lon;
  TRI_shaper_t* shaper;

  lat = 0;
  lon = 0;
  loc = 0;
  idx = NULL;

  shaper = collection->base._shaper;

  if (location != NULL) {
    loc = shaper->findAttributePathByName(shaper, location);
  }

  if (latitude != NULL) {
    lat = shaper->findAttributePathByName(shaper, latitude);
  }

  if (longitude != NULL) {
    lon = shaper->findAttributePathByName(shaper, longitude);
  }

  // check, if we know the index
  if (location != NULL) {
    idx = TRI_LookupGeoIndexSimCollection(collection, loc);
  }
  else if (longitude != NULL && latitude != NULL) {
    idx = TRI_LookupGeoIndex2SimCollection(collection, lat, lon);
  }
  else {
    TRI_set_errno(TRI_VOC_ERROR_ILLEGAL_PARAMETER);

    LOG_TRACE("expecting either 'location' or 'latitude' and 'longitude'");

    return NULL;
  }

  if (idx != NULL) {
    TRI_set_errno(TRI_VOC_ERROR_INDEX_EXISTS);

    LOG_TRACE("geo-index already created for location '%s'", location);

    return NULL;
  }

  // create a new index
  if (location != NULL) {
    idx = TRI_CreateGeoIndex(&collection->base, loc, geoJson);

    LOG_TRACE("created geo-index for location '%s': %d",
              location,
              (unsigned long) loc);
  }
  else if (longitude != NULL && latitude != NULL) {
    idx = TRI_CreateGeoIndex2(&collection->base, lat, lon);

    LOG_TRACE("created geo-index for location '%s': %d, %d",
              location,
              (unsigned long) lat,
              (unsigned long) lon);
  }

  if (iid) {
    idx->_iid = iid;
  }

  // initialises the index with all existing documents
  FillIndex(collection, idx);

  // and store index
  TRI_PushBackVectorPointer(&collection->_indexes, idx);

  if (collection->_geoIndex == NULL) {
    collection->_geoIndex = (TRI_geo_index_t*) idx;
  }
  else if (collection->_geoIndex->base._iid > idx->_iid) {
    collection->_geoIndex = (TRI_geo_index_t*) idx;
  }

  return idx;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief finds a geo index
////////////////////////////////////////////////////////////////////////////////

TRI_index_t* TRI_LookupGeoIndexSimCollection (TRI_sim_collection_t* collection,
                                              TRI_shape_pid_t location) {
  size_t n;
  size_t i;

  n = TRI_SizeVectorPointer(&collection->_indexes);

  for (i = 0;  i < n;  ++i) {
    TRI_index_t* idx;

    idx = TRI_AtVectorPointer(&collection->_indexes, i);

    if (idx->_type == TRI_IDX_TYPE_GEO_INDEX) {
      TRI_geo_index_t* geo = (TRI_geo_index_t*) idx;

      if (geo->_location != 0 && geo->_location == location) {
        return idx;
      }
    }
  }

  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief finds a geo index
////////////////////////////////////////////////////////////////////////////////

TRI_index_t* TRI_LookupGeoIndex2SimCollection (TRI_sim_collection_t* collection,
                                               TRI_shape_pid_t latitude,
                                               TRI_shape_pid_t longitude) {
  size_t n;
  size_t i;

  n = TRI_SizeVectorPointer(&collection->_indexes);

  for (i = 0;  i < n;  ++i) {
    TRI_index_t* idx;

    idx = TRI_AtVectorPointer(&collection->_indexes, i);

    if (idx->_type == TRI_IDX_TYPE_GEO_INDEX) {
      TRI_geo_index_t* geo = (TRI_geo_index_t*) idx;

      if (geo->_latitude != 0 && geo->_longitude != 0 && geo->_latitude == latitude && geo->_longitude == longitude) {
        return idx;
      }
    }
  }

  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures that a geo index exists
////////////////////////////////////////////////////////////////////////////////

bool TRI_EnsureGeoIndexSimCollection (TRI_sim_collection_t* collection,
                                      char const* location,
                                      bool geoJson) {
  TRI_index_t* idx;
  bool ok;

  // .............................................................................
  // within write-lock the collection
  // .............................................................................

  TRI_WriteLockReadWriteLock(&collection->_lock);

  idx = CreateGeoIndexSimCollection(collection, location, NULL, NULL, geoJson, 0);

  if (idx == NULL) {
    TRI_WriteUnlockReadWriteLock(&collection->_lock);

    if (TRI_errno() == TRI_VOC_ERROR_INDEX_EXISTS) {
      return true;
    }

    return false;
  }

  ok = TRI_SaveIndex(&collection->base, idx);

  TRI_WriteUnlockReadWriteLock(&collection->_lock);

  // .............................................................................
  // without write-lock
  // .............................................................................

  return ok;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures that a geo index exists
////////////////////////////////////////////////////////////////////////////////

bool TRI_EnsureGeoIndex2SimCollection (TRI_sim_collection_t* collection,
                                       char const* latitude,
                                       char const* longitude) {
  TRI_index_t* idx;
  bool ok;

  // .............................................................................
  // within write-lock the collection
  // .............................................................................

  TRI_WriteLockReadWriteLock(&collection->_lock);

  idx = CreateGeoIndexSimCollection(collection, NULL, latitude, longitude, false, 0);

  if (idx == NULL) {
    TRI_WriteUnlockReadWriteLock(&collection->_lock);

    if (TRI_errno() == TRI_VOC_ERROR_INDEX_EXISTS) {
      return true;
    }

    return false;
  }

  ok = TRI_SaveIndex(&collection->base, idx);

  TRI_WriteUnlockReadWriteLock(&collection->_lock);

  // .............................................................................
  // without write-lock
  // .............................................................................

  return ok;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\)"
// End:

////////////////////////////////////////////////////////////////////////////////
/// @brief document collection
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

#include "document-collection.h"

#include <Basics/conversions.h>
#include <Basics/files.h>
#include <Basics/logging.h>
#include <Basics/strings.h>

#include <VocBase/voc-shaper.h>

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

static TRI_voc_did_t CreateLock (TRI_doc_collection_t* document,
                                 TRI_shaped_json_t const* json) {
  TRI_voc_did_t did;

  document->beginWrite(document);
  did = document->create(document, json);
  document->endWrite(document);

  return did;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new document in the collection from json
////////////////////////////////////////////////////////////////////////////////

static TRI_voc_did_t CreateJson (TRI_doc_collection_t* collection,
                                 TRI_json_t const* json) {
  TRI_shaped_json_t* shaped;
  TRI_voc_did_t did;

  shaped = TRI_ShapedJsonJson(collection->_shaper, json);

  if (shaped == 0) {
    collection->base._lastError = TRI_set_errno(TRI_VOC_ERROR_SHAPER_FAILED);
    return false;
  }

  did = collection->create(collection, shaped);

  TRI_FreeShapedJson(shaped);

  return did;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief updates a document in the collection from shaped json
////////////////////////////////////////////////////////////////////////////////

static bool UpdateLock (TRI_doc_collection_t* document,
                              TRI_shaped_json_t const* json,
                              TRI_voc_did_t did) {
  bool ok;

  document->beginWrite(document);
  ok = document->update(document, json, did);
  document->endWrite(document);

  return ok;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief updates a document in the collection from json
////////////////////////////////////////////////////////////////////////////////

static bool UpdateJson (TRI_doc_collection_t* collection,
                        TRI_json_t const* json,
                        TRI_voc_did_t did) {
  TRI_shaped_json_t* shaped;
  bool ok;

  shaped = TRI_ShapedJsonJson(collection->_shaper, json);

  if (shaped == 0) {
    collection->base._lastError = TRI_set_errno(TRI_VOC_ERROR_SHAPER_FAILED);
    return false;
  }

  ok = collection->update(collection, shaped, did);

  TRI_FreeShapedJson(shaped);

  return ok;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes a json document given the identifier
////////////////////////////////////////////////////////////////////////////////

static bool DeleteLock (TRI_doc_collection_t* document,
                        TRI_voc_did_t did) {
  bool ok;

  document->beginWrite(document);
  ok = document->destroy(document, did);
  document->endWrite(document);

  return ok;
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
/// @brief initialises a document collection
////////////////////////////////////////////////////////////////////////////////

void TRI_InitDocCollection (TRI_doc_collection_t* collection,
                            TRI_shaper_t* shaper) {
  collection->_shaper = shaper;

  collection->createLock = CreateLock;
  collection->createJson = CreateJson;

  collection->updateLock = UpdateLock;
  collection->updateJson = UpdateJson;

  collection->destroyLock = DeleteLock;

  TRI_InitRSContainer(&collection->_resultSets, collection);

  TRI_InitAssociativePointer(&collection->_datafileInfo,
                             HashKeyDatafile,
                             HashElementDatafile,
                             IsEqualKeyElementDatafile,
                             NULL);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a document collection
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyDocCollection (TRI_doc_collection_t* collection) {
  if (collection->_shaper != NULL) {
    TRI_FreeVocShaper(collection->_shaper);
  }

  TRI_DestroyAssociativePointer(&collection->_datafileInfo);
  TRI_DestroyRSContainer(&collection->_resultSets);

  TRI_DestroyCollection(&collection->base);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                               protected functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief finds a datafile description
////////////////////////////////////////////////////////////////////////////////

TRI_doc_datafile_info_t* TRI_FindDatafileInfoDocCollection (TRI_doc_collection_t* collection,
                                                            TRI_voc_tick_t fid) {
  TRI_doc_datafile_info_t const* found;
  TRI_doc_datafile_info_t* dfi;

  found = TRI_LookupByKeyAssociativePointer(&collection->_datafileInfo, &fid);

  if (found != NULL) {
    union { TRI_doc_datafile_info_t const* c; TRI_doc_datafile_info_t* v; } cnv;

    cnv.c = found;
    return cnv.v;
  }

  dfi = TRI_Allocate(sizeof(TRI_doc_datafile_info_t));

  dfi->_fid = fid;

  TRI_InsertKeyAssociativePointer(&collection->_datafileInfo, &fid, dfi, true);

  return dfi;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a journal
////////////////////////////////////////////////////////////////////////////////

TRI_datafile_t* TRI_CreateJournalDocCollection (TRI_doc_collection_t* collection) {
  TRI_col_header_marker_t cm;
  TRI_datafile_t* journal;
  TRI_df_marker_t* position;
  bool ok;
  char* filename;
  char* jname;
  char* number;

  number = TRI_StringUInt32(TRI_NewTickVocBase());
  jname = TRI_Concatenate3String("journal-", number, ".db");
  filename = TRI_Concatenate2File(collection->base._directory, jname);
  TRI_FreeString(number);
  TRI_FreeString(jname);

  journal = TRI_CreateDatafile(filename, collection->base._maximalSize);

  if (journal == NULL) {
    collection->base._lastError = TRI_set_errno(TRI_VOC_ERROR_NO_JOURNAL);
    collection->base._state = TRI_COL_STATE_WRITE_ERROR;

    LOG_ERROR("cannot create new journal in '%s'", filename);

    TRI_FreeString(filename);
    return NULL;
  }

  TRI_FreeString(filename);
  LOG_TRACE("created a new journal '%s'", journal->_filename);

  // and use the correct name
  number = TRI_StringUInt32(journal->_fid);
  jname = TRI_Concatenate3String("journal-", number, ".db");
  filename = TRI_Concatenate2File(collection->base._directory, jname);
  TRI_FreeString(number);
  TRI_FreeString(jname);

  ok = TRI_RenameDatafile(journal, filename);

  if (! ok) {
    LOG_WARNING("failed to rename the journal to '%s': %s", filename, TRI_last_error());
  }
  else {
    LOG_TRACE("renamed journal to '%s'", filename);
  }

  TRI_FreeString(filename);

  // create a collection header
  ok = TRI_ReserveElementDatafile(journal, sizeof(TRI_col_header_marker_t), &position);

  if (! ok) {
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
  ok = TRI_WriteElementDatafile(journal, position, &cm.base, sizeof(cm), 0, 0, true);

  if (! ok) {
    collection->base._lastError = journal->_lastError;
    TRI_FreeDatafile(journal);

    LOG_ERROR("cannot create document header in journal '%s': %s", filename, TRI_last_error());

    return NULL;
  }

  // that's it
  TRI_PushBackVectorPointer(&collection->base._journals, journal);

  return journal;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief closes a journal
////////////////////////////////////////////////////////////////////////////////

bool TRI_CloseJournalDocCollection (TRI_doc_collection_t* collection,
                                    size_t position) {
  TRI_datafile_t* journal;
  bool ok;
  char* dname;
  char* filename;
  char* number;

  // no journal at this position
  if (TRI_SizeVectorPointer(&collection->base._journals) <= position) {
    TRI_set_errno(TRI_VOC_ERROR_NO_JOURNAL);
    return false;
  }

  // seal and rename datafile
  journal = TRI_AtVectorPointer(&collection->base._journals, position);
  ok = TRI_SealDatafile(journal);

  if (! ok) {
    TRI_RemoveVectorPointer(&collection->base._journals, position);
    TRI_PushBackVectorPointer(&collection->base._datafiles, journal);

    return false;
  }

  number = TRI_StringUInt32(journal->_fid);
  dname = TRI_Concatenate3String("datafile-", number, ".db");
  filename = TRI_Concatenate2File(collection->base._directory, dname);
  TRI_FreeString(dname);
  TRI_FreeString(number);

  ok = TRI_RenameDatafile(journal, filename);

  if (! ok) {
    TRI_RemoveVectorPointer(&collection->base._journals, position);
    TRI_PushBackVectorPointer(&collection->base._datafiles, journal);

    return false;
  }

  LOG_TRACE("closed journal '%s'", journal->_filename);

  TRI_RemoveVectorPointer(&collection->base._journals, position);
  TRI_PushBackVectorPointer(&collection->base._datafiles, journal);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\)"
// End:

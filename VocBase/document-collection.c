////////////////////////////////////////////////////////////////////////////////
/// @brief document collection
///
/// @file
///
/// DISCLAIMER
///
/// Copyright by triAGENS GmbH - All rights reserved.
///
/// The Programs (which include both the software and documentation)
/// contain proprietary information of triAGENS GmbH; they are
/// provided under a license agreement containing restrictions on use and
/// disclosure and are also protected by copyright, patent and other
/// intellectual and industrial property laws. Reverse engineering,
/// disassembly or decompilation of the Programs, except to the extent
/// required to obtain interoperability with other independently created
/// software or as specified by law, is prohibited.
///
/// The Programs are not intended for use in any nuclear, aviation, mass
/// transit, medical, or other inherently dangerous applications. It shall
/// be the licensee's responsibility to take all appropriate fail-safe,
/// backup, redundancy, and other measures to ensure the safe use of such
/// applications if the Programs are used for such purposes, and triAGENS
/// GmbH disclaims liability for any damages caused by such use of
/// the Programs.
///
/// This software is the confidential and proprietary information of
/// triAGENS GmbH. You shall not disclose such confidential and
/// proprietary information and shall use it only in accordance with the
/// terms of the license agreement you entered into with triAGENS GmbH.
///
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
/// @author Copyright 2011, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "document-collection.h"

#include <BasicsC/conversions.h>
#include <BasicsC/files.h>
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
/// @brief creates a new document in the collection from shaped json
////////////////////////////////////////////////////////////////////////////////

static TRI_voc_did_t CreateLock (TRI_doc_collection_t* document,
                                 TRI_df_marker_type_e type,
                                 TRI_shaped_json_t const* json,
                                 void const* data) {
  TRI_doc_mptr_t const* result;

  document->beginWrite(document);
  result = document->create(document, type, json, data);
  document->endWrite(document);

  return result == NULL ? 0 : result->_did;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new document in the collection from json
////////////////////////////////////////////////////////////////////////////////

static TRI_doc_mptr_t const* CreateJson (TRI_doc_collection_t* collection,
                                         TRI_df_marker_type_e type,
                                         TRI_json_t const* json,
                                         void const* data) {
  TRI_shaped_json_t* shaped;
  TRI_doc_mptr_t const* result;

  shaped = TRI_ShapedJsonJson(collection->_shaper, json);

  if (shaped == 0) {
    collection->base._lastError = TRI_set_errno(TRI_VOC_ERROR_SHAPER_FAILED);
    return false;
  }

  result = collection->create(collection, type, shaped, data);

  TRI_FreeShapedJson(shaped);

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief updates a document in the collection from shaped json
////////////////////////////////////////////////////////////////////////////////

static bool UpdateLock (TRI_doc_collection_t* document,
                        TRI_shaped_json_t const* json,
                        TRI_voc_did_t did,
                        TRI_voc_rid_t rid,
                        TRI_doc_update_policy_e policy) {
  TRI_doc_mptr_t const* result;

  document->beginWrite(document);
  result = document->update(document, json, did, rid, policy);
  document->endWrite(document);

  return result != NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief updates a document in the collection from json
////////////////////////////////////////////////////////////////////////////////

static TRI_doc_mptr_t const* UpdateJson (TRI_doc_collection_t* collection,
                                         TRI_json_t const* json,
                                         TRI_voc_did_t did,
                                         TRI_voc_rid_t rid,
                                         TRI_doc_update_policy_e policy) {
  TRI_shaped_json_t* shaped;
  TRI_doc_mptr_t const* result;

  shaped = TRI_ShapedJsonJson(collection->_shaper, json);

  if (shaped == 0) {
    collection->base._lastError = TRI_set_errno(TRI_VOC_ERROR_SHAPER_FAILED);
    return false;
  }

  result = collection->update(collection, shaped, did, rid, policy);

  TRI_FreeShapedJson(shaped);

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes a json document given the identifier
////////////////////////////////////////////////////////////////////////////////

static bool DestroyLock (TRI_doc_collection_t* document,
                         TRI_voc_did_t did,
                         TRI_voc_rid_t rid,
                         TRI_doc_update_policy_e policy) {
  bool ok;

  document->beginWrite(document);
  ok = document->destroy(document, did, rid, policy);
  document->endWrite(document);

  return ok;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns information about the collection
////////////////////////////////////////////////////////////////////////////////

static TRI_doc_collection_info_t* Figures (TRI_doc_collection_t* document) {
  TRI_doc_collection_info_t* info;
  size_t i;

  info = TRI_Allocate(sizeof(TRI_doc_collection_info_t));

  info->_numberDatafiles = document->_datafileInfo._nrUsed;

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

TRI_datafile_t* CreateJournalDocCollection (TRI_doc_collection_t* collection, bool compactor) {
  TRI_col_header_marker_t cm;
  TRI_datafile_t* journal;
  TRI_df_marker_t* position;
  bool ok;
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
  TRI_FreeString(number);
  TRI_FreeString(jname);

  // create journal file
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

  if (compactor) {
    jname = TRI_Concatenate3String("compactor-", number, ".db");
  }
  else {
    jname = TRI_Concatenate3String("journal-", number, ".db");
  }

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

bool CloseJournalDocCollection (TRI_doc_collection_t* collection,
                                size_t position,
                                bool compactor) {
  TRI_datafile_t* journal;
  TRI_vector_pointer_t* vector;
  bool ok;
  char* dname;
  char* filename;
  char* number;

  if (compactor) {
    vector = &collection->base._journals;
  }
  else {
    vector = &collection->base._journals;
  }

  // no journal at this position
  if (vector->_length <= position) {
    TRI_set_errno(TRI_VOC_ERROR_NO_JOURNAL);
    return false;
  }

  // seal and rename datafile
  journal = vector->_buffer[position];
  ok = TRI_SealDatafile(journal);

  if (! ok) {
    TRI_RemoveVectorPointer(vector, position);
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
    TRI_RemoveVectorPointer(vector, position);
    TRI_PushBackVectorPointer(&collection->base._datafiles, journal);

    return false;
  }

  LOG_TRACE("closed journal '%s'", journal->_filename);

  TRI_RemoveVectorPointer(vector, position);
  TRI_PushBackVectorPointer(&collection->base._datafiles, journal);

  return true;
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
/// @brief initialises a document collection
////////////////////////////////////////////////////////////////////////////////

void TRI_InitDocCollection (TRI_doc_collection_t* collection,
                            TRI_shaper_t* shaper) {
  collection->_shaper = shaper;

  collection->createLock = CreateLock;
  collection->createJson = CreateJson;

  collection->updateLock = UpdateLock;
  collection->updateJson = UpdateJson;

  collection->destroyLock = DestroyLock;

  collection->figures = Figures;

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
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief finds a datafile description
////////////////////////////////////////////////////////////////////////////////

TRI_doc_datafile_info_t* TRI_FindDatafileInfoDocCollection (TRI_doc_collection_t* collection,
                                                            TRI_voc_fid_t fid) {
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
///
/// Note that the caller must hold a lock protecting the _journals entry.
////////////////////////////////////////////////////////////////////////////////

TRI_datafile_t* TRI_CreateJournalDocCollection (TRI_doc_collection_t* collection) {
  return CreateJournalDocCollection(collection, false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief closes a journal
///
/// Note that the caller must hold a lock protecting the _datafiles and
/// _journals entry.
////////////////////////////////////////////////////////////////////////////////

bool TRI_CloseJournalDocCollection (TRI_doc_collection_t* collection,
                                    size_t position) {
  return CloseJournalDocCollection(collection, position, false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new compactor file
///
/// Note that the caller must hold a lock protecting the _journals entry.
////////////////////////////////////////////////////////////////////////////////

TRI_datafile_t* TRI_CreateCompactorDocCollection (TRI_doc_collection_t* collection) {
  return CreateJournalDocCollection(collection, true);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief closes an existing compactor file
///
/// Note that the caller must hold a lock protecting the _datafiles and
/// _journals entry.
////////////////////////////////////////////////////////////////////////////////

bool TRI_CloseCompactorDocCollection (TRI_doc_collection_t* collection,
                                      size_t position) {
  return CloseJournalDocCollection(collection, position, true);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:

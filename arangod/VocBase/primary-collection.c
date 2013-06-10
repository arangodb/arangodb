////////////////////////////////////////////////////////////////////////////////
/// @brief primary collection
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

#include "primary-collection.h"

#include "BasicsC/conversions.h"
#include "BasicsC/files.h"
#include "BasicsC/hashes.h"
#include "BasicsC/logging.h"
#include "BasicsC/tri-strings.h"

#include "VocBase/key-generator.h"
#include "VocBase/voc-shaper.h"

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
  return TRI_FnvHashString((char const*) key);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief hashs the document header
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashElementDocument (TRI_associative_pointer_t* array, void const* element) {
  TRI_doc_mptr_t const* e = element;
  return TRI_FnvHashString((char const*) e->_key);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compares a document id and a document
////////////////////////////////////////////////////////////////////////////////

static bool IsEqualKeyDocument (TRI_associative_pointer_t* array, void const* key, void const* element) {
  TRI_doc_mptr_t const* e = element;

  char const * k = key;
  return (strcmp(k, e->_key) == 0);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief hashs a datafile identifier
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashKeyDatafile (TRI_associative_pointer_t* array, void const* key) {
  TRI_voc_fid_t const* k = key;

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
  TRI_voc_fid_t const* k = key;
  TRI_doc_datafile_info_t const* e = element;

  return *k == e->_fid;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief debug output for datafile information
////////////////////////////////////////////////////////////////////////////////

static void DebugDatafileInfoDatafile (TRI_primary_collection_t* primary,
                                       TRI_datafile_t* datafile) {
  TRI_doc_datafile_info_t* dfi;
  
  printf("FILE '%s'\n", datafile->getName(datafile));

  dfi = TRI_FindDatafileInfoPrimaryCollection(primary, datafile->_fid, false);

  if (dfi == NULL) {
    printf(" no info\n\n");
    return;
  }

  printf("  number alive:        %ld\n", (long) dfi->_numberAlive);
  printf("  size alive:          %ld\n", (long) dfi->_sizeAlive);
  printf("  number dead:         %ld\n", (long) dfi->_numberDead);
  printf("  size dead:           %ld\n", (long) dfi->_sizeDead);
  printf("  deletion:            %ld\n\n", (long) dfi->_numberDeletion);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief debug output for datafile information
////////////////////////////////////////////////////////////////////////////////

static void DebugDatafileInfoPrimaryCollection (TRI_primary_collection_t* primary) {
  TRI_datafile_t* datafile;
  size_t i, n;

  // journals
  n = primary->base._journals._length;
  if (n > 0) {
    printf("JOURNALS (%d)\n-----------------------------\n", (int) n);

    for (i = 0;  i < n;  ++i) {
      datafile = primary->base._journals._buffer[i];
      DebugDatafileInfoDatafile(primary, datafile);
    }
  }

  // compactors
  n = primary->base._compactors._length;
  if (n > 0) {
    printf("COMPACTORS (%d)\n-----------------------------\n", (int) n);

    for (i = 0;  i < n;  ++i) {
      datafile = primary->base._compactors._buffer[i];
      DebugDatafileInfoDatafile(primary, datafile);
    }
  }

  // datafiles
  n = primary->base._datafiles._length;
  if (n > 0) {
    printf("DATAFILES (%d)\n-----------------------------\n", (int) n);

    for (i = 0;  i < n;  ++i) {
      datafile = primary->base._datafiles._buffer[i];
      DebugDatafileInfoDatafile(primary, datafile);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a compactor file
////////////////////////////////////////////////////////////////////////////////

static TRI_datafile_t* CreateCompactor (TRI_primary_collection_t* primary, 
                                        TRI_voc_fid_t fid,
                                        TRI_voc_size_t maximalSize) {
  TRI_col_header_marker_t cm;
  TRI_collection_t* collection;
  TRI_datafile_t* journal;
  TRI_df_marker_t* position;
  int res;

  collection = &primary->base;

  if (collection->_info._isVolatile) {
    // in-memory collection
    journal = TRI_CreateDatafile(NULL, fid, maximalSize);
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

    journal = TRI_CreateDatafile(filename, fid, maximalSize);
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


  TRI_InitMarker(&cm.base, TRI_COL_MARKER_HEADER, sizeof(TRI_col_header_marker_t), TRI_NewTickVocBase());
  cm._type = (TRI_col_type_t) collection->_info._type;
  cm._cid  = collection->_info._cid;

  res = TRI_WriteCrcElementDatafile(journal, position, &cm.base, sizeof(cm), true);

  if (res != TRI_ERROR_NO_ERROR) {
    collection->_lastError = journal->_lastError;
    LOG_ERROR("cannot create document header in compactor '%s': %s", journal->getName(journal), TRI_last_error());

    TRI_FreeDatafile(journal);

    return NULL;
  }

  assert(fid == journal->_fid);

  return journal;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a journal
////////////////////////////////////////////////////////////////////////////////

static TRI_datafile_t* CreateJournal (TRI_primary_collection_t* primary, 
                                      TRI_voc_size_t maximalSize) {
  TRI_col_header_marker_t cm;
  TRI_collection_t* collection;
  TRI_datafile_t* journal;
  TRI_df_marker_t* position;
  TRI_voc_fid_t fid;
  int res;

  collection = &primary->base;

  fid = TRI_NewTickVocBase();

  if (collection->_info._isVolatile) {
    // in-memory collection
    journal = TRI_CreateDatafile(NULL, fid, maximalSize);
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

    journal = TRI_CreateDatafile(filename, fid, maximalSize);
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
  res = TRI_ReserveElementDatafile(journal, sizeof(TRI_col_header_marker_t), &position, maximalSize);

  if (res != TRI_ERROR_NO_ERROR) {
    collection->_lastError = journal->_lastError;
    LOG_ERROR("cannot create document header in journal '%s': %s", journal->getName(journal), TRI_last_error());

    TRI_FreeDatafile(journal);

    return NULL;
  }


  TRI_InitMarker(&cm.base, TRI_COL_MARKER_HEADER, sizeof(TRI_col_header_marker_t), TRI_NewTickVocBase());
  cm._type = (TRI_col_type_t) collection->_info._type;
  cm._cid  = collection->_info._cid;

  res = TRI_WriteCrcElementDatafile(journal, position, &cm.base, sizeof(cm), true);

  if (res != TRI_ERROR_NO_ERROR) {
    collection->_lastError = journal->_lastError;
    LOG_ERROR("cannot create document header in journal '%s': %s", journal->getName(journal), TRI_last_error());

    TRI_FreeDatafile(journal);

    return NULL;
  }

  assert(fid == journal->_fid);


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
/// @brief closes a journal
///
/// Note that the caller must hold a lock protecting the _datafiles and
/// _journals entry.
////////////////////////////////////////////////////////////////////////////////

static bool CloseJournalPrimaryCollection (TRI_primary_collection_t* primary,
                                           size_t position,
                                           bool compactor) {
  TRI_datafile_t* journal;
  TRI_collection_t* collection;
  TRI_vector_pointer_t* vector;
  int res;

  collection = &primary->base;

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
  journal = vector->_buffer[position];
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
/// @brief returns information about the collection
/// note: the collection lock must be held when calling this function
////////////////////////////////////////////////////////////////////////////////

static TRI_doc_collection_info_t* Figures (TRI_primary_collection_t* primary) {
  TRI_doc_collection_info_t* info;
  TRI_collection_t* base;
  size_t i;

  // prefill with 0's to init counters
  info = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_doc_collection_info_t), true);

  if (info == NULL) {
    return NULL;
  }
    
  for (i = 0;  i < primary->_datafileInfo._nrAlloc;  ++i) {
    TRI_doc_datafile_info_t* d = primary->_datafileInfo._table[i];

    if (d != NULL) {
      info->_numberAlive += d->_numberAlive;
      info->_numberDead += d->_numberDead;
      info->_numberTransaction += d->_numberTransaction; // not used here (only in compaction)
      info->_numberDeletion += d->_numberDeletion;
      info->_sizeAlive += d->_sizeAlive;
      info->_sizeDead += d->_sizeDead;
      info->_sizeTransaction += d->_sizeTransaction; // not used here (only in compaction)
    }
  }

  // add the file sizes for datafiles and journals
  base = &primary->base;
  for (i = 0; i < base->_datafiles._length; ++i) {
    TRI_datafile_t* df = (TRI_datafile_t*) base->_datafiles._buffer[i];

    info->_datafileSize += (int64_t) df->_maximalSize;
    ++info->_numberDatafiles;
  }

  for (i = 0; i < base->_journals._length; ++i) {
    TRI_datafile_t* df = (TRI_datafile_t*) base->_journals._buffer[i];

    info->_journalfileSize += df->_maximalSize;
    ++info->_numberJournalfiles;
  }

  info->_numberShapes = (TRI_voc_ssize_t) primary->_shaper->numShapes(primary->_shaper);
  info->_numberAttributes = (TRI_voc_ssize_t) primary->_shaper->numAttributes(primary->_shaper);
  
  return info;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief size of a primary collection
///
/// the caller must have read-locked the collection!
////////////////////////////////////////////////////////////////////////////////

static TRI_voc_size_t Count (TRI_primary_collection_t* primary) {
  return (TRI_voc_size_t) primary->_numberDocuments;
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

int TRI_InitPrimaryCollection (TRI_primary_collection_t* primary,
                               TRI_shaper_t* shaper) {
  primary->_shaper             = shaper;
  primary->_capConstraint      = NULL;
  primary->_keyGenerator       = NULL;
  primary->_numberDocuments    = 0;

  primary->figures             = Figures;
  primary->size                = Count;


  TRI_InitBarrierList(&primary->_barrierList, primary);

  TRI_InitAssociativePointer(&primary->_datafileInfo,
                             TRI_UNKNOWN_MEM_ZONE,
                             HashKeyDatafile,
                             HashElementDatafile,
                             IsEqualKeyElementDatafile,
                             NULL);

  TRI_InitAssociativePointer(&primary->_primaryIndex,
                             TRI_UNKNOWN_MEM_ZONE,
                             HashKeyHeader,
                             HashElementDocument,
                             IsEqualKeyDocument,
                             NULL);

  TRI_InitReadWriteLock(&primary->_lock);
  TRI_InitReadWriteLock(&primary->_compactionLock);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a primary collection
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyPrimaryCollection (TRI_primary_collection_t* primary) {
  size_t i, n;

  if (primary->_keyGenerator != NULL) {
    TRI_FreeKeyGenerator(primary->_keyGenerator);
  }

  TRI_DestroyReadWriteLock(&primary->_compactionLock);
  TRI_DestroyReadWriteLock(&primary->_lock);
  TRI_DestroyAssociativePointer(&primary->_primaryIndex);

  if (primary->_shaper != NULL) {
    TRI_FreeVocShaper(primary->_shaper);
  }
  
  n = primary->_datafileInfo._nrAlloc;

  for (i = 0; i < n; ++i) {
    TRI_doc_datafile_info_t* dfi = primary->_datafileInfo._table[i];
    if (dfi != NULL) {
      FreeDatafileInfo(dfi);
    }
  }

  TRI_DestroyAssociativePointer(&primary->_datafileInfo);
  
  TRI_DestroyBarrierList(&primary->_barrierList);

  TRI_DestroyCollection(&primary->base);
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
/// @brief removes a datafile description
////////////////////////////////////////////////////////////////////////////////

void TRI_RemoveDatafileInfoPrimaryCollection (TRI_primary_collection_t* primary,
                                              TRI_voc_fid_t fid) {
  TRI_RemoveKeyAssociativePointer(&primary->_datafileInfo, &fid);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief finds a datafile description
////////////////////////////////////////////////////////////////////////////////

TRI_doc_datafile_info_t* TRI_FindDatafileInfoPrimaryCollection (TRI_primary_collection_t* primary,
                                                                TRI_voc_fid_t fid,
                                                                bool create) {
  TRI_doc_datafile_info_t const* found;
  TRI_doc_datafile_info_t* dfi;

  found = TRI_LookupByKeyAssociativePointer(&primary->_datafileInfo, &fid);

  if (found != NULL) {
    return CONST_CAST(found);
  }

  if (! create) {
    return NULL;
  }

  // allocate and set to 0
  dfi = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_doc_datafile_info_t), true);

  if (dfi == NULL) {
    return NULL;
  }

  dfi->_fid = fid;

  TRI_InsertKeyAssociativePointer(&primary->_datafileInfo, &fid, dfi, true);

  return dfi;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a journal
///
/// Note that the caller must hold a lock protecting the _journals entry.
////////////////////////////////////////////////////////////////////////////////

TRI_datafile_t* TRI_CreateJournalPrimaryCollection (TRI_primary_collection_t* primary) {
  return CreateJournal(primary, primary->base._info._maximalSize);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief closes a journal
///
/// Note that the caller must hold a lock protecting the _datafiles and
/// _journals entry.
////////////////////////////////////////////////////////////////////////////////

bool TRI_CloseJournalPrimaryCollection (TRI_primary_collection_t* primary,
                                        size_t position) {
  return CloseJournalPrimaryCollection(primary, position, false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new compactor file
///
/// Note that the caller must hold a lock protecting the _journals entry.
////////////////////////////////////////////////////////////////////////////////

TRI_datafile_t* TRI_CreateCompactorPrimaryCollection (TRI_primary_collection_t* primary,
                                                      TRI_voc_fid_t fid,
                                                      TRI_voc_size_t maximalSize) {
  return CreateCompactor(primary, fid, maximalSize);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief closes an existing compactor file
///
/// Note that the caller must hold a lock protecting the _datafiles and
/// _journals entry.
////////////////////////////////////////////////////////////////////////////////

bool TRI_CloseCompactorPrimaryCollection (TRI_primary_collection_t* primary,
                                          size_t position) {
  return CloseJournalPrimaryCollection(primary, position, true);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief dump information about all datafiles of a collection
////////////////////////////////////////////////////////////////////////////////

void TRI_DebugDatafileInfoPrimaryCollection (TRI_primary_collection_t* primary) {
  DebugDatafileInfoPrimaryCollection(primary);
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

size_t TRI_DocumentIteratorPrimaryCollection (TRI_primary_collection_t* primary,
                                              void* data,
                                              bool (*callback)(TRI_doc_mptr_t const*, void*)) {
  if (primary->_primaryIndex._nrUsed > 0) {
    void** ptr = primary->_primaryIndex._table;
    void** end = ptr + primary->_primaryIndex._nrAlloc;

    for (;  ptr < end;  ++ptr) {
      if (*ptr) {
        TRI_doc_mptr_t const* d = (TRI_doc_mptr_t const*) *ptr;

        if (! callback(d, data)) {
          break;
        }
      }
    }
  }

  return (size_t) primary->_primaryIndex._nrUsed;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:

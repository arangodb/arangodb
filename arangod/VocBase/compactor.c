////////////////////////////////////////////////////////////////////////////////
/// @brief compactor
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

#ifdef _WIN32
#include "BasicsC/win-utils.h"
#endif

#include "compactor.h"

#include "BasicsC/conversions.h"
#include "BasicsC/files.h"
#include "BasicsC/logging.h"
#include "BasicsC/tri-strings.h"
#include "VocBase/document-collection.h"
#include "VocBase/marker.h"
#include "VocBase/vocbase.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                 private constants
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief minimum size of dead data (in bytes) in a datafile that will make
/// the datafile eligible for compaction at all.
///
/// Any datafile with less dead data than the threshold will not become a 
/// candidate for compaction.
////////////////////////////////////////////////////////////////////////////////

#define COMPACTOR_DEAD_SIZE_THRESHOLD (1024 * 128)

////////////////////////////////////////////////////////////////////////////////
/// @brief percentage of dead documents in a datafile that will trigger the
/// compaction
///
/// for example, if the collection contains 800 bytes of alive and 400 bytes of
/// dead documents, the share of the dead documents is 400 / (400 + 800) = 33 %. 
/// if this value if higher than the threshold, the datafile will be compacted
////////////////////////////////////////////////////////////////////////////////

#define COMPACTOR_DEAD_SIZE_SHARE (0.1)

////////////////////////////////////////////////////////////////////////////////
/// @brief compactify interval in microseconds
////////////////////////////////////////////////////////////////////////////////

static int const COMPACTOR_INTERVAL = (1 * 1000 * 1000);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                     private types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief compaction state
////////////////////////////////////////////////////////////////////////////////
  
typedef struct compaction_context_s {
  TRI_document_collection_t* _document;
  TRI_datafile_t*            _compactor;
  TRI_doc_datafile_info_t    _dfi;
}
compaction_context_t;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a compactor file, based on a datafile
////////////////////////////////////////////////////////////////////////////////

static TRI_datafile_t* CreateCompactor (TRI_document_collection_t* document,
                                        TRI_datafile_t const* datafile) {
  TRI_collection_t* collection;
  TRI_datafile_t* compactor;
  TRI_voc_fid_t fid;

  collection = &document->base.base;
    
  TRI_LOCK_JOURNAL_ENTRIES_DOC_COLLECTION(document);

  // we are re-using the _fid of the datafile!
  fid = datafile->_fid;
  compactor = TRI_CreateCompactorPrimaryCollection(&document->base, fid, datafile->_maximalSize);

  if (compactor != NULL) {
    TRI_PushBackVectorPointer(&collection->_compactors, compactor);
  }
    
  // we still must wake up the other thread from time to time, otherwise we'll deadlock
  TRI_BROADCAST_JOURNAL_ENTRIES_DOC_COLLECTION(document);

  TRI_UNLOCK_JOURNAL_ENTRIES_DOC_COLLECTION(document);

  return compactor;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief write a copy of the marker into the datafile
////////////////////////////////////////////////////////////////////////////////

static int CopyMarker (TRI_document_collection_t* document,
                       TRI_datafile_t* compactor,
                       TRI_df_marker_t const* marker,
                       TRI_df_marker_t** result) {
  int res;

  res = TRI_ReserveElementDatafile(compactor, marker->_size, result, document->base.base._info._maximalSize);

  if (res != TRI_ERROR_NO_ERROR) {
    document->base.base._lastError = TRI_set_errno(TRI_ERROR_ARANGO_NO_JOURNAL);

    return TRI_ERROR_ARANGO_NO_JOURNAL;
  }

  return TRI_WriteElementDatafile(compactor, *result, marker, marker->_size, false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief locate a datafile, identified by fid, in a vector of datafiles
////////////////////////////////////////////////////////////////////////////////

static bool LocateDatafile (TRI_vector_pointer_t const* vector,
                            const TRI_voc_fid_t fid,
                            size_t* position) {
  size_t i, n;
  
  n = vector->_length;

  for (i = 0;  i < n;  ++i) {
    TRI_datafile_t* df = vector->_buffer[i];

    if (df->_fid == fid) {
      *position = i;
      return true; 
    }
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief callback to drop a datafile
////////////////////////////////////////////////////////////////////////////////

static void DropDatafileCallback (TRI_datafile_t* datafile, void* data) {
  TRI_primary_collection_t* primary;
  TRI_voc_fid_t fid;
#ifdef TRI_ENABLE_LOGGER
  char* old;
#endif
  char* filename;
  char* name;
  char* number;
  bool ok;

  primary = data;
  fid = datafile->_fid;

  number   = TRI_StringUInt64(fid);
  name     = TRI_Concatenate3String("deleted-", number, ".db");
  filename = TRI_Concatenate2File(primary->base._directory, name);

  TRI_FreeString(TRI_CORE_MEM_ZONE, number);
  TRI_FreeString(TRI_CORE_MEM_ZONE, name);

#ifdef TRI_ENABLE_LOGGER
  old = datafile->_filename;
#endif

  if (datafile->isPhysical(datafile)) {
    ok = TRI_RenameDatafile(datafile, filename);

    if (! ok) {
      LOG_ERROR("cannot rename obsolete datafile '%s' to '%s': %s",
                old,
                filename,
                TRI_last_error());
    }
  }

  LOG_DEBUG("finished compacting datafile '%s'", datafile->getName(datafile));

  ok = TRI_CloseDatafile(datafile);

  if (! ok) {
    LOG_ERROR("cannot close obsolete datafile '%s': %s",
              datafile->getName(datafile),
              TRI_last_error());
  }
  else if (datafile->isPhysical(datafile)) {
    if (primary->base._vocbase->_removeOnCompacted) {
      int res;

      LOG_DEBUG("wiping compacted datafile from disk");

      res = TRI_UnlinkFile(filename);

      if (res != TRI_ERROR_NO_ERROR) {
        LOG_ERROR("cannot wipe obsolete datafile '%s': %s",
                  datafile->getName(datafile),
                  TRI_last_error());
      }
    }
  }

  TRI_FreeDatafile(datafile);
  TRI_FreeString(TRI_CORE_MEM_ZONE, filename);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief callback to rename a datafile
////////////////////////////////////////////////////////////////////////////////

static void RenameDatafileCallback (TRI_datafile_t* datafile, void* data) {
  compaction_context_t* context;
  TRI_datafile_t* compactor;
  TRI_primary_collection_t* primary;
  bool ok;
   
  context = data;
  compactor = context->_compactor;
  primary = &context->_document->base;

  ok = false;
  assert(datafile->_fid == compactor->_fid);

  if (datafile->isPhysical(datafile)) {
    char* number;
    char* jname;
    char* tempFilename;
    char* realName;

    realName     = TRI_DuplicateString(datafile->_filename);
    
    // construct a suitable tempname
    number       = TRI_StringUInt64(datafile->_fid);
    jname        = TRI_Concatenate3String("temp-", number, ".db");
    tempFilename = TRI_Concatenate2File(primary->base._directory, jname);

    TRI_FreeString(TRI_CORE_MEM_ZONE, number);
    TRI_FreeString(TRI_CORE_MEM_ZONE, jname);

    if (! TRI_RenameDatafile(datafile, tempFilename)) {
      LOG_ERROR("unable to rename datafile '%s' to '%s'", datafile->getName(datafile), tempFilename);
    }
    else {
      if (! TRI_RenameDatafile(compactor, realName)) {
        LOG_ERROR("unable to rename compaction file '%s' to '%s'", compactor->getName(compactor), realName);
      }
      else {
        ok = true;
      }
    }

    TRI_FreeString(TRI_CORE_MEM_ZONE, tempFilename);
    TRI_FreeString(TRI_CORE_MEM_ZONE, realName);
  }
  else {
    ok = true;
  }

  if (ok) {
    TRI_doc_datafile_info_t* dfi;
    size_t i;
    
    TRI_WRITE_LOCK_DATAFILES_DOC_COLLECTION(primary);

    if (! LocateDatafile(&primary->base._datafiles, datafile->_fid, &i)) {
      TRI_WRITE_UNLOCK_DATAFILES_DOC_COLLECTION(primary);

      LOG_ERROR("logic error: could not locate datafile");

      return;
    }

    // put the compactor in place of the datafile
    primary->base._datafiles._buffer[i] = compactor;

    // update dfi
    dfi = TRI_FindDatafileInfoPrimaryCollection(primary, compactor->_fid, false);

    if (dfi != NULL) {
      memcpy(dfi, &context->_dfi, sizeof(TRI_doc_datafile_info_t));
    }
    else {
      LOG_ERROR("logic error: could not find compactor file information");
    }

    if (! LocateDatafile(&primary->base._compactors, compactor->_fid, &i)) {
      TRI_WRITE_UNLOCK_DATAFILES_DOC_COLLECTION(primary);
   
      LOG_ERROR("logic error: could not locate compactor");

      return;
    }

    // remove the compactor from the list of compactors
    TRI_RemoveVectorPointer(&primary->base._compactors, i);
          
    TRI_WRITE_UNLOCK_DATAFILES_DOC_COLLECTION(primary);

    DropDatafileCallback(datafile, primary);
  }

  TRI_Free(TRI_CORE_MEM_ZONE, context);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief datafile iterator
////////////////////////////////////////////////////////////////////////////////

static bool Compactifier (TRI_df_marker_t const* marker, 
                          void* data, 
                          TRI_datafile_t* datafile, 
                          bool journal) {
  TRI_df_marker_t* result;
  TRI_doc_mptr_t const* found;
  TRI_doc_mptr_t* found2;
  TRI_document_collection_t* document;
  TRI_primary_collection_t* primary;
  compaction_context_t* context;
  int res;

  context  = data;
  document = context->_document;
  primary  = &document->base;

  // new or updated document
  if (marker->_type == TRI_DOC_MARKER_KEY_DOCUMENT ||
      marker->_type == TRI_DOC_MARKER_KEY_EDGE) {
    TRI_doc_document_key_marker_t const* d;
    TRI_voc_key_t key;
    bool deleted;

    d = (TRI_doc_document_key_marker_t const*) marker;
    key = (char*) d + d->_offsetKey;

    // check if the document is still active
    TRI_READ_LOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(primary);

    found = TRI_LookupByKeyAssociativePointer(&primary->_primaryIndex, key);
    deleted = (found == NULL || found->_rid > marker->_tick);

    TRI_READ_UNLOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(primary);

    if (deleted) {
      LOG_TRACE("found a stale document: %s", key);
      return true;
    }

    // write to compactor files
    res = CopyMarker(document, context->_compactor, marker, &result);

    if (res != TRI_ERROR_NO_ERROR) {
      LOG_FATAL_AND_EXIT("cannot write compactor file: %s", TRI_last_error());
    }

    // check if the document is still active
    TRI_WRITE_LOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(primary);

    found = TRI_LookupByKeyAssociativePointer(&primary->_primaryIndex, key);
    deleted = found == NULL;

    if (deleted) {
      context->_dfi._numberDead += 1;
      context->_dfi._sizeDead += (int64_t) marker->_size;
      
      TRI_WRITE_UNLOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(primary);

      LOG_DEBUG("found a stale document after copying: %s", key);

      return true;
    }

    found2 = CONST_CAST(found);
    // the fid won't change

    // let marker point to the new position
    found2->_data = result;

    // let _key point to the new key position
    found2->_key = ((char*) result) + (((TRI_doc_document_key_marker_t*) result)->_offsetKey);

    // update datafile info
    context->_dfi._numberAlive += 1;
    context->_dfi._sizeAlive += (int64_t) marker->_size;

    TRI_WRITE_UNLOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(primary);
  }

  // deletion
  else if (marker->_type == TRI_DOC_MARKER_KEY_DELETION) {
    // write to compactor files
    res = CopyMarker(document, context->_compactor, marker, &result);

    if (res != TRI_ERROR_NO_ERROR) {
      LOG_FATAL_AND_EXIT("cannot write compactor file: %s", TRI_last_error());
    }

    // update datafile info
    context->_dfi._numberDeletion++;
  }
  else if (marker->_type == TRI_DOC_MARKER_BEGIN_TRANSACTION ||
           marker->_type == TRI_DOC_MARKER_COMMIT_TRANSACTION ||
           marker->_type == TRI_DOC_MARKER_ABORT_TRANSACTION ||
           marker->_type == TRI_DOC_MARKER_PREPARE_TRANSACTION) {
    // write to compactor files
    res = CopyMarker(document, context->_compactor, marker, &result);

    if (res != TRI_ERROR_NO_ERROR) {
      LOG_FATAL_AND_EXIT("cannot write compactor file: %s", TRI_last_error());
    }
    
    context->_dfi._numberTransaction += 1;
    context->_dfi._sizeTransaction += (int64_t) marker->_size;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief remove an empty compactor file
////////////////////////////////////////////////////////////////////////////////

static int RemoveEmpty (TRI_document_collection_t* document,
                        TRI_datafile_t* df,
                        TRI_datafile_t* compactor) {
  TRI_primary_collection_t* primary;
  TRI_doc_datafile_info_t* dfi;
  size_t i;

  primary = &document->base;

  LOG_TRACE("removing empty compaction file '%s'", compactor->getName(compactor));
  
  // remove the datafile from the list of datafiles
  TRI_WRITE_LOCK_DATAFILES_DOC_COLLECTION(primary);

  if (! LocateDatafile(&primary->base._datafiles, df->_fid, &i)) {
    TRI_WRITE_UNLOCK_DATAFILES_DOC_COLLECTION(primary);
    
    LOG_ERROR("logic error: could not locate datafile");

    return TRI_ERROR_INTERNAL;
  }
 
  TRI_RemoveVectorPointer(&primary->base._datafiles, i);
  
  // remove the compactor from the list of compactors
  if (! LocateDatafile(&primary->base._compactors, compactor->_fid, &i)) {
    TRI_WRITE_UNLOCK_DATAFILES_DOC_COLLECTION(primary);
    
    LOG_ERROR("logic error: could not locate compactor");

    return TRI_ERROR_INTERNAL;
  }
    
  TRI_RemoveVectorPointer(&primary->base._compactors, i);

  
  // update dfi
  dfi = TRI_FindDatafileInfoPrimaryCollection(primary, df->_fid, false);

  if (dfi != NULL) {
    TRI_RemoveDatafileInfoPrimaryCollection(primary, df->_fid);
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, dfi);
  }

  TRI_WRITE_UNLOCK_DATAFILES_DOC_COLLECTION(primary);


  // close the file & remove it
  if (compactor->isPhysical(compactor)) {
    char* filename;

    filename = TRI_DuplicateString(compactor->getName(compactor));
    TRI_CloseDatafile(compactor);
    TRI_FreeDatafile(compactor);

    TRI_UnlinkFile(filename);
    TRI_Free(TRI_CORE_MEM_ZONE, filename);
  }
  else {
    TRI_CloseDatafile(compactor);
    TRI_FreeDatafile(compactor);
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compact a datafile
////////////////////////////////////////////////////////////////////////////////

static void CompactifyDatafile (TRI_document_collection_t* document, 
                                TRI_voc_fid_t fid) {
  TRI_datafile_t* df;
  TRI_datafile_t* compactor;
  TRI_primary_collection_t* primary;
  compaction_context_t context;
  bool ok;
  size_t i;

  primary = &document->base;

  // locate the datafile
  TRI_READ_LOCK_DATAFILES_DOC_COLLECTION(primary);

  if (! LocateDatafile(&primary->base._datafiles, fid, &i)) {
    // not found
    TRI_READ_UNLOCK_DATAFILES_DOC_COLLECTION(primary);

    LOG_ERROR("logic error in CompactifyDatafile: could not find datafile");
    return;
  }

  // found the datafile
  df = primary->base._datafiles._buffer[i];
  TRI_READ_UNLOCK_DATAFILES_DOC_COLLECTION(primary);


  // now create a compactor file for it
  compactor = CreateCompactor(document, df);

  if (compactor == NULL) {
    // some error occurred
    LOG_ERROR("could not create compactor file");

    return;
  }
  
  LOG_DEBUG("created new compactor file '%s'", compactor->getName(compactor));
  
  // now compactify the datafile
  LOG_DEBUG("compacting datafile '%s' into '%s'", df->getName(df), compactor->getName(compactor));

  context._document  = document;
  context._compactor = compactor;

  memset(&context._dfi, 0, sizeof(TRI_doc_datafile_info_t));
  // set _fid
  context._dfi._fid  = df->_fid;

  ok = TRI_IterateDatafile(df, Compactifier, &context, false);

  if (! ok) {
    LOG_WARNING("failed to compact datafile '%s'", df->getName(df));
    // compactor file does not need to be removed now. will be removed on next startup
    return;
  }
  
  // locate the compactor
  TRI_READ_LOCK_DATAFILES_DOC_COLLECTION(primary);

  if (! LocateDatafile(&primary->base._compactors, compactor->_fid, &i)) {
    // not found
    TRI_READ_UNLOCK_DATAFILES_DOC_COLLECTION(primary);

    LOG_ERROR("logic error in CompactifyDatafile: could not find compactor");
    return;
  }

  TRI_READ_UNLOCK_DATAFILES_DOC_COLLECTION(primary);
  
  if (! TRI_CloseCompactorPrimaryCollection(primary, i)) {
    LOG_ERROR("could not close compactor file");
    // TODO: how do we recover from this state?
    return;
  }
  
  if (context._dfi._numberAlive == 0 &&
      context._dfi._numberDead == 0 &&
      context._dfi._numberDeletion == 0 &&
      context._dfi._numberTransaction == 0) {

    TRI_barrier_t* b;

    // datafile is empty after compaction and thus useless
    RemoveEmpty(document, df, compactor);
  
    // add a deletion marker to the result set container
    b = TRI_CreateBarrierDropDatafile(&primary->_barrierList, df, DropDatafileCallback, primary);

    if (b == NULL) {
      LOG_ERROR("out of memory when creating datafile-drop barrier");
    }
  }
  else {
    // add a rename marker 
    TRI_barrier_t* b;
    void* copy;
     
    copy = TRI_Allocate(TRI_CORE_MEM_ZONE, sizeof(compaction_context_t), false);

    memcpy(copy, &context, sizeof(compaction_context_t));
    
    b = TRI_CreateBarrierRenameDatafile(&primary->_barrierList, df, RenameDatafileCallback, copy);
    if (b == NULL) {
      LOG_ERROR("out of memory when creating datafile-rename barrier");
      TRI_Free(TRI_CORE_MEM_ZONE, copy);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks all datafiles of a collection
////////////////////////////////////////////////////////////////////////////////

static bool CompactifyDocumentCollection (TRI_document_collection_t* document) {
  TRI_primary_collection_t* primary;
  TRI_vector_t vector;
  size_t i, n;

  primary = &document->base;

  // if we cannot acquire the read lock instantly, we will exit directly.
  // otherwise we'll risk a multi-thread deadlock between synchroniser,
  // compactor and data-modification threads (e.g. POST /_api/document)
  if (! TRI_TRY_READ_LOCK_DATAFILES_DOC_COLLECTION(primary)) {
    return false;
  }

  n = primary->base._datafiles._length;
  
  if (primary->base._compactors._length > 0 || n == 0) {
    // we already have created a compactor file in progress.
    // if this happens, then a previous compaction attempt for this collection failed

    // additionally, if there are no datafiles, then there's no need to compact
    TRI_READ_UNLOCK_DATAFILES_DOC_COLLECTION(primary);
    return false;
  }

  // copy datafile information
  TRI_InitVector(&vector, TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_doc_datafile_info_t));

  for (i = 0;  i < n;  ++i) {
    TRI_datafile_t* df;
    TRI_doc_datafile_info_t* dfi;
    double share;

    df = primary->base._datafiles._buffer[i];
    dfi = TRI_FindDatafileInfoPrimaryCollection(primary, df->_fid, false);
    
    if (dfi == NULL || dfi->_numberDead == 0 || dfi->_sizeDead < (int64_t) COMPACTOR_DEAD_SIZE_THRESHOLD) {
      continue;
    }

    share = (double) dfi->_sizeDead / ((double) dfi->_sizeDead + (double) dfi->_sizeAlive);

    if (share < COMPACTOR_DEAD_SIZE_SHARE) {
      continue;
    }
    
    LOG_TRACE("found datafile eligible for compaction. fid: %llu, "
              "numberDead: %llu, numberAlive: %llu, numberTransaction: %llu, "
              "sizeDead: %llu, sizeAlive: %llu, sizeTransaction: %llu",
              (unsigned long long) df->_fid,
              (unsigned long long) dfi->_numberDead,
              (unsigned long long) dfi->_numberAlive,
              (unsigned long long) dfi->_numberTransaction,
              (unsigned long long) dfi->_sizeDead,
              (unsigned long long) dfi->_sizeAlive,
              (unsigned long long) dfi->_sizeTransaction);

    // only use those datafiles that contain dead objects
    TRI_PushBackVector(&vector, dfi);

    // we stop at the first datafile.
    // this is better than going over all datafiles in a collection in one go
    // because the compactor is single-threaded, and collecting all datafiles
    // might take a long time (it might even be that there is a request to
    // delete the collection in the middle of compaction, but the compactor
    // will not pick this up as it is read-locking the collection status)
    break;
  }

  // can now continue without the lock
  TRI_READ_UNLOCK_DATAFILES_DOC_COLLECTION(primary);


  if (vector._length == 0) {
    // cleanup local variables
    TRI_DestroyVector(&vector);
    return false;
  }

  // handle datafiles with dead objects
  n = vector._length;
  for (i = 0;  i < n;  ++i) {
    TRI_doc_datafile_info_t* dfi = TRI_AtVector(&vector, i);

    assert(dfi->_numberDead > 0);

    LOG_TRACE("compacting datafile. fid: %llu, "
              "numberDead: %llu, numberAlive: %llu, numberTransaction: %llu, "
              "sizeDead: %llu, sizeAlive: %llu, sizeTransaction: %llu",
              (unsigned long long) dfi->_fid,
              (unsigned long long) dfi->_numberDead,
              (unsigned long long) dfi->_numberAlive,
              (unsigned long long) dfi->_numberTransaction,
              (unsigned long long) dfi->_sizeDead,
              (unsigned long long) dfi->_sizeAlive,
              (unsigned long long) dfi->_sizeTransaction);

    CompactifyDatafile(document, dfi->_fid);
  }

  // cleanup local variables
  TRI_DestroyVector(&vector);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief compactor event loop
////////////////////////////////////////////////////////////////////////////////

void TRI_CompactorVocBase (void* data) {
  TRI_vocbase_t* vocbase = data;
  TRI_vector_pointer_t collections;

  assert(vocbase->_state == 1);

  TRI_InitVectorPointer(&collections, TRI_UNKNOWN_MEM_ZONE);

  while (true) {
    TRI_col_type_e type;
    size_t i, n;
    int state;
    bool worked;
    
    // keep initial _state value as vocbase->_state might change during compaction loop
    state = vocbase->_state;

    // copy all collections
    TRI_READ_LOCK_COLLECTIONS_VOCBASE(vocbase);

    TRI_CopyDataVectorPointer(&collections, &vocbase->_collections);

    TRI_READ_UNLOCK_COLLECTIONS_VOCBASE(vocbase);

    n = collections._length;

    for (i = 0;  i < n;  ++i) {
      TRI_vocbase_col_t* collection;
      TRI_primary_collection_t* primary;

      collection = collections._buffer[i];

      if (! TRI_TRY_READ_LOCK_STATUS_VOCBASE_COL(collection)) {
        // if we can't acquire the read lock instantly, we continue directly
        // we don't want to stall here for too long
        continue;
      }

      primary = collection->_collection;

      if (primary == NULL) {
        TRI_READ_UNLOCK_STATUS_VOCBASE_COL(collection);
        continue;
      }

      worked = false;
      type = primary->base._info._type;

      // for document collection, compactify datafiles
      if (TRI_IS_DOCUMENT_COLLECTION(type)) {
        if (collection->_status == TRI_VOC_COL_STATUS_LOADED) {
          TRI_barrier_t* ce;

          // check whether someone else holds a read-lock on the compaction lock
          if (! TRI_TryWriteLockReadWriteLock(&primary->_compactionLock)) {
            // someone else is holding the compactor lock, we'll not compact
            TRI_READ_UNLOCK_STATUS_VOCBASE_COL(collection);
            continue;
          }

          ce = TRI_CreateBarrierCompaction(&primary->_barrierList);

          if (ce == NULL) {
            // out of memory
            LOG_WARNING("out of memory when trying to create a barrier element");
          }
          else {
            worked = CompactifyDocumentCollection((TRI_document_collection_t*) primary);

            TRI_FreeBarrier(ce);
          }
        }
          
        // read-unlock the compaction lock
        TRI_WriteUnlockReadWriteLock(&primary->_compactionLock);
      }

      TRI_READ_UNLOCK_STATUS_VOCBASE_COL(collection);

      if (worked) {
        // signal the cleanup thread that we worked and that it can now wake up
        TRI_LockCondition(&vocbase->_cleanupCondition);
        TRI_SignalCondition(&vocbase->_cleanupCondition);
        TRI_UnlockCondition(&vocbase->_cleanupCondition);
      }
    }

    if (vocbase->_state == 1) {
      // only sleep while server is still running
      usleep(COMPACTOR_INTERVAL);
    }

    if (state == 2) {
      // server shutdown
      break;
    }
  }

  TRI_DestroyVectorPointer(&collections);

  LOG_TRACE("shutting down compactor thread");
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:

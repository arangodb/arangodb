////////////////////////////////////////////////////////////////////////////////
/// @brief compactor
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

#ifdef _WIN32
#include "Basics/win-utils.h"
#endif

#include "compactor.h"

#include "Basics/conversions.h"
#include "Basics/files.h"
#include "Basics/logging.h"
#include "Basics/tri-strings.h"
#include "Utils/transactions.h"
#include "VocBase/document-collection.h"
#include "VocBase/primary-index.h"
#include "VocBase/server.h"
#include "VocBase/vocbase.h"
#include "VocBase/voc-shaper.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                 private constants
// -----------------------------------------------------------------------------

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
/// @brief maximum number of datafiles to join together in one compaction run
////////////////////////////////////////////////////////////////////////////////

#define COMPACTOR_MAX_FILES 4

////////////////////////////////////////////////////////////////////////////////
/// @brief maximum multiple of journal filesize of a compacted file
/// a value of 3 means that the maximum filesize of the compacted file is
/// 3 x (collection->journalSize)
////////////////////////////////////////////////////////////////////////////////

#define COMPACTOR_MAX_SIZE_FACTOR (3)

////////////////////////////////////////////////////////////////////////////////
/// @brief maximum filesize of resulting compacted file
////////////////////////////////////////////////////////////////////////////////

#define COMPACTOR_MAX_RESULT_FILESIZE (128 * 1024 * 1024)

////////////////////////////////////////////////////////////////////////////////
/// @brief datafiles smaller than the following value will be merged with others
////////////////////////////////////////////////////////////////////////////////

#define COMPACTOR_MIN_SIZE (128 * 1024)

////////////////////////////////////////////////////////////////////////////////
/// @brief re-try compaction of a specific collection in this interval (in s)
////////////////////////////////////////////////////////////////////////////////

#define COMPACTOR_COLLECTION_INTERVAL (10.0)

////////////////////////////////////////////////////////////////////////////////
/// @brief compactify interval in microseconds
////////////////////////////////////////////////////////////////////////////////

static int const COMPACTOR_INTERVAL = (1 * 1000 * 1000);

// -----------------------------------------------------------------------------
// --SECTION--                                                     private types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief compaction blocker entry
////////////////////////////////////////////////////////////////////////////////

typedef struct compaction_blocker_s {
  TRI_voc_tick_t  _id;
  double          _expires;
}
compaction_blocker_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief auxiliary struct used when initialising compaction
////////////////////////////////////////////////////////////////////////////////

typedef struct compaction_intial_context_s {
  TRI_document_collection_t* _document;
  int64_t                    _targetSize;
  TRI_voc_fid_t              _fid;
  bool                       _keepDeletions;
  bool                       _failed;
}
compaction_initial_context_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief compaction state
////////////////////////////////////////////////////////////////////////////////

typedef struct compaction_context_s {
  TRI_document_collection_t* _document;
  TRI_datafile_t*            _compactor;
  TRI_doc_datafile_info_t    _dfi;
  bool                       _keepDeletions;
}
compaction_context_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief compaction instruction for a single datafile
////////////////////////////////////////////////////////////////////////////////

typedef struct compaction_info_s {
  TRI_datafile_t* _datafile;
  bool            _keepDeletions;
}
compaction_info_t;

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief return a marker's size
////////////////////////////////////////////////////////////////////////////////

static inline int64_t AlignedSize (TRI_df_marker_t const* marker) {
  return static_cast<int64_t>(TRI_DF_ALIGN_BLOCK(marker->_size));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a compactor file, based on a datafile
////////////////////////////////////////////////////////////////////////////////

static TRI_datafile_t* CreateCompactor (TRI_document_collection_t* document,
                                        TRI_voc_fid_t fid,
                                        int64_t maximalSize) {
  TRI_collection_t* collection = document;

  // reserve room for one additional entry
  if (TRI_ReserveVectorPointer(&collection->_compactors, 1) != TRI_ERROR_NO_ERROR) {
    // could not get memory, exit early
    return nullptr;
  }

  TRI_LOCK_JOURNAL_ENTRIES_DOC_COLLECTION(document);

  TRI_datafile_t* compactor = TRI_CreateDatafileDocumentCollection(document, fid, static_cast<TRI_voc_size_t>(maximalSize), true);

  if (compactor != nullptr) {
    int res TRI_UNUSED = TRI_PushBackVectorPointer(&collection->_compactors, compactor);

    // we have reserved space before, so we can be sure the push succeeds
    TRI_ASSERT(res == TRI_ERROR_NO_ERROR);
  }

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
  int res = TRI_ReserveElementDatafile(compactor, marker->_size, result, 0);

  if (res != TRI_ERROR_NO_ERROR) {
    document->_lastError = TRI_set_errno(TRI_ERROR_ARANGO_NO_JOURNAL);

    return TRI_ERROR_ARANGO_NO_JOURNAL;
  }

  return TRI_WriteElementDatafile(compactor, *result, marker, false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief locate a datafile, identified by fid, in a vector of datafiles
////////////////////////////////////////////////////////////////////////////////

static bool LocateDatafile (TRI_vector_pointer_t const* vector,
                            const TRI_voc_fid_t fid,
                            size_t* position) {
  size_t const n = vector->_length;

  for (size_t i = 0;  i < n;  ++i) {
    TRI_datafile_t* df = static_cast<TRI_datafile_t*>(vector->_buffer[i]);

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
  TRI_voc_fid_t fid;
  char* filename;
  char* name;
  char* number;
  char* copy;
  bool ok;

  TRI_document_collection_t* document = static_cast<TRI_document_collection_t*>(data);
  fid     = datafile->_fid;
  copy    = nullptr;

  number   = TRI_StringUInt64(fid);
  name     = TRI_Concatenate3String("deleted-", number, ".db");
  filename = TRI_Concatenate2File(document->_directory, name);

  TRI_FreeString(TRI_CORE_MEM_ZONE, number);
  TRI_FreeString(TRI_CORE_MEM_ZONE, name);

  if (datafile->isPhysical(datafile)) {
    // copy the current filename
    copy = TRI_DuplicateStringZ(TRI_CORE_MEM_ZONE, datafile->_filename);

    ok = TRI_RenameDatafile(datafile, filename);

    if (! ok) {
      LOG_ERROR("cannot rename obsolete datafile '%s' to '%s': %s",
                copy,
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
     int res;

     LOG_DEBUG("wiping compacted datafile from disk");

     res = TRI_UnlinkFile(filename);

     if (res != TRI_ERROR_NO_ERROR) {
      LOG_ERROR("cannot wipe obsolete datafile '%s': %s",
                datafile->getName(datafile),
                TRI_last_error());
     }

     // check for .dead files
     if (copy != nullptr) {
      // remove .dead file for datafile
      char* deadfile = TRI_Concatenate2String(copy, ".dead");

      if (deadfile != nullptr) {
        // check if .dead file exists, then remove it
        if (TRI_ExistsFile(deadfile)) {
          TRI_UnlinkFile(deadfile);
        }

        TRI_FreeString(TRI_CORE_MEM_ZONE, deadfile);
      }
    }
  }

  TRI_FreeDatafile(datafile);
  TRI_FreeString(TRI_CORE_MEM_ZONE, filename);

  if (copy != nullptr) {
    TRI_FreeString(TRI_CORE_MEM_ZONE, copy);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief callback to rename a datafile
///
/// The datafile will be renamed to "temp-abc.db" (where "abc" is the fid of
/// the datafile) first. If this rename operation fails, there will be a
/// compactor file and a datafile. On startup, the datafile will be preferred
/// in this case.
/// If renaming succeeds, the compactor will be named to the original datafile.
/// If that does not succeed, there is a compactor file and a renamed datafile.
/// On startup, the compactor file will be used, and the renamed datafile
/// will be treated as a temporary file and dropped.
////////////////////////////////////////////////////////////////////////////////

static void RenameDatafileCallback (TRI_datafile_t* datafile,
                                    void* data) {
  compaction_context_t* context;
  TRI_datafile_t* compactor;
  bool ok;

  context   = (compaction_context_t*) data;
  compactor = context->_compactor;

  TRI_document_collection_t* document = context->_document;

  ok = false;
  TRI_ASSERT(datafile->_fid == compactor->_fid);

  if (datafile->isPhysical(datafile)) {
    char* number;
    char* jname;
    char* tempFilename;
    char* realName;

    realName     = TRI_DuplicateString(datafile->_filename);

    // construct a suitable tempname
    number       = TRI_StringUInt64(datafile->_fid);
    jname        = TRI_Concatenate3String("temp-", number, ".db");
    tempFilename = TRI_Concatenate2File(document->_directory, jname);

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

    // must acquire a write-lock as we're about to change the datafiles vector
    TRI_WRITE_LOCK_DATAFILES_DOC_COLLECTION(document);

    if (! LocateDatafile(&document->_datafiles, datafile->_fid, &i)) {
      TRI_WRITE_UNLOCK_DATAFILES_DOC_COLLECTION(document);

      LOG_ERROR("logic error: could not locate datafile");

      return;
    }

    // put the compactor in place of the datafile
    document->_datafiles._buffer[i] = compactor;

    // update dfi
    dfi = TRI_FindDatafileInfoDocumentCollection(document, compactor->_fid, false);

    if (dfi != nullptr) {
      memcpy(dfi, &context->_dfi, sizeof(TRI_doc_datafile_info_t));
    }
    else {
      LOG_ERROR("logic error: could not find compactor file information");
    }

    if (! LocateDatafile(&document->_compactors, compactor->_fid, &i)) {
      TRI_WRITE_UNLOCK_DATAFILES_DOC_COLLECTION(document);

      LOG_ERROR("logic error: could not locate compactor");

      return;
    }

    // remove the compactor from the list of compactors
    TRI_RemoveVectorPointer(&document->_compactors, i);

    TRI_WRITE_UNLOCK_DATAFILES_DOC_COLLECTION(document);

    DropDatafileCallback(datafile, document);
  }

  TRI_Free(TRI_CORE_MEM_ZONE, context);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief datafile iterator, copies "live" data from datafile into compactor
///
/// this function is called for all markers in the collected datafiles. Its
//7 purpose is to find the still-alive markers and copy them into the compactor
/// file.
/// IMPORTANT: if the logic inside this function is adjusted, the total size
/// calculated by function CalculateSize might need adjustment, too!!
////////////////////////////////////////////////////////////////////////////////

static bool Compactifier (TRI_df_marker_t const* marker,
                          void* data,
                          TRI_datafile_t* datafile) {
  TRI_df_marker_t* result;
  int res;

  compaction_context_t* context = static_cast<compaction_context_t*>(data);
  TRI_document_collection_t* document = context->_document;

  // new or updated document
  if (marker->_type == TRI_DOC_MARKER_KEY_DOCUMENT ||
      marker->_type == TRI_DOC_MARKER_KEY_EDGE) {

    bool deleted;

    TRI_doc_document_key_marker_t const* d = reinterpret_cast<TRI_doc_document_key_marker_t const*>(marker);
    TRI_voc_key_t key = (char*) d + d->_offsetKey;

    // check if the document is still active
    TRI_doc_mptr_t const* found = static_cast<TRI_doc_mptr_t const*>(TRI_LookupByKeyPrimaryIndex(&document->_primaryIndex, key));
    deleted = (found == nullptr || found->_rid > d->_rid);

    if (deleted) {
      LOG_TRACE("found a stale document: %s", key);
      return true;
    }

    context->_keepDeletions = true;

    // write to compactor files
    res = CopyMarker(document, context->_compactor, marker, &result);

    if (res != TRI_ERROR_NO_ERROR) {
      // TODO: dont fail but recover from this state
      LOG_FATAL_AND_EXIT("cannot write compactor file: %s", TRI_last_error());
    }

    // check if the document is still active
    found = static_cast<TRI_doc_mptr_t const*>(TRI_LookupByKeyPrimaryIndex(&document->_primaryIndex, key));
    deleted = (found == nullptr);

    if (deleted) {
      context->_dfi._numberDead += 1;
      context->_dfi._sizeDead += AlignedSize(marker);

      LOG_DEBUG("found a stale document after copying: %s", key);

      return true;
    }

    TRI_doc_mptr_t* found2 = const_cast<TRI_doc_mptr_t*>(found);
    TRI_ASSERT(found2->getDataPtr() != nullptr);  // ONLY in COMPACTIFIER, PROTECTED by fake trx outside
    TRI_ASSERT(((TRI_df_marker_t*) found2->getDataPtr())->_size > 0);  // ONLY in COMPACTIFIER, PROTECTED by fake trx outside

    // the fid might change
    if (found->_fid != context->_compactor->_fid) {
      // update old datafile's info
      TRI_doc_datafile_info_t* dfi = TRI_FindDatafileInfoDocumentCollection(document, found->_fid, false);

      if (dfi != nullptr) {
        dfi->_numberDead += 1;
        dfi->_sizeDead += AlignedSize(marker);
      }

      found2->_fid = context->_compactor->_fid;
    }

    // let marker point to the new position
    found2->setDataPtr(result);

    // update datafile info
    context->_dfi._numberAlive += 1;
    context->_dfi._sizeAlive += AlignedSize(marker);
  }

  // deletions
  else if (marker->_type == TRI_DOC_MARKER_KEY_DELETION &&
           context->_keepDeletions) {
    // write to compactor files
    res = CopyMarker(document, context->_compactor, marker, &result);

    if (res != TRI_ERROR_NO_ERROR) {
      // TODO: dont fail but recover from this state
      LOG_FATAL_AND_EXIT("cannot write document marker to compactor file: %s", TRI_last_error());
    }

    // update datafile info
    context->_dfi._numberDeletion++;
  }

  // shapes
  else if (marker->_type == TRI_DF_MARKER_SHAPE) {
    // write to compactor files
    res = CopyMarker(document, context->_compactor, marker, &result);

    if (res != TRI_ERROR_NO_ERROR) {
      // TODO: dont fail but recover from this state
      LOG_FATAL_AND_EXIT("cannot write shape marker to compactor file: %s", TRI_last_error());
    }

    res = TRI_MoveMarkerVocShaper(document->getShaper(), result, nullptr);  // ONLY IN COMPACTOR, PROTECTED by fake trx in caller

    if (res != TRI_ERROR_NO_ERROR) {
      LOG_FATAL_AND_EXIT("cannot re-locate shape marker");
    }

    context->_dfi._numberShapes++;
    context->_dfi._sizeShapes += AlignedSize(marker);
  }

  // attributes
  else if (marker->_type == TRI_DF_MARKER_ATTRIBUTE) {
    // write to compactor files
    res = CopyMarker(document, context->_compactor, marker, &result);

    if (res != TRI_ERROR_NO_ERROR) {
      // TODO: dont fail but recover from this state
      LOG_FATAL_AND_EXIT("cannot write attribute marker to compactor file: %s", TRI_last_error());
    }

    res = TRI_MoveMarkerVocShaper(document->getShaper(), result, nullptr);  // ONLY IN COMPACTOR, PROTECTED by fake trx in caller

    if (res != TRI_ERROR_NO_ERROR) {
      LOG_FATAL_AND_EXIT("cannot re-locate shape marker");
    }

    context->_dfi._numberAttributes++;
    context->_dfi._sizeAttributes += AlignedSize(marker);
  }

  // transaction markers
  else if (marker->_type == TRI_DOC_MARKER_BEGIN_TRANSACTION ||
           marker->_type == TRI_DOC_MARKER_COMMIT_TRANSACTION ||
           marker->_type == TRI_DOC_MARKER_ABORT_TRANSACTION ||
           marker->_type == TRI_DOC_MARKER_PREPARE_TRANSACTION) {
    // these markers are used from ArangoDB 2.2 onwards
    // still, datafiles of older collections might contain these
    // markers and we need to copy them

    if (document->_failedTransactions != nullptr) {
      // write to compactor files
      res = CopyMarker(document, context->_compactor, marker, &result);

      if (res != TRI_ERROR_NO_ERROR) {
        // TODO: dont fail but recover from this state
        LOG_FATAL_AND_EXIT("cannot write transaction marker to compactor file: %s", TRI_last_error());
      }

      context->_dfi._numberTransactions++;
      context->_dfi._sizeTransactions += AlignedSize(marker);
    }
    // otherwise don't copy
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief remove an empty compactor file
////////////////////////////////////////////////////////////////////////////////

static int RemoveCompactor (TRI_document_collection_t* document,
                            TRI_datafile_t* compactor) {
  size_t i;

  LOG_TRACE("removing empty compaction file '%s'", compactor->getName(compactor));

  // remove the datafile from the list of datafiles
  TRI_WRITE_LOCK_DATAFILES_DOC_COLLECTION(document);

  // remove the compactor from the list of compactors
  if (! LocateDatafile(&document->_compactors, compactor->_fid, &i)) {
    TRI_WRITE_UNLOCK_DATAFILES_DOC_COLLECTION(document);

    LOG_ERROR("logic error: could not locate compactor");

    return TRI_ERROR_INTERNAL;
  }

  TRI_RemoveVectorPointer(&document->_compactors, i);

  TRI_WRITE_UNLOCK_DATAFILES_DOC_COLLECTION(document);

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
/// @brief remove an empty datafile
////////////////////////////////////////////////////////////////////////////////

static int RemoveDatafile (TRI_document_collection_t* document,
                           TRI_datafile_t* df) {
  TRI_doc_datafile_info_t* dfi;
  size_t i;

  LOG_TRACE("removing empty datafile '%s'", df->getName(df));

  // remove the datafile from the list of datafiles
  TRI_WRITE_LOCK_DATAFILES_DOC_COLLECTION(document);

  if (! LocateDatafile(&document->_datafiles, df->_fid, &i)) {
    TRI_WRITE_UNLOCK_DATAFILES_DOC_COLLECTION(document);

    LOG_ERROR("logic error: could not locate datafile");

    return TRI_ERROR_INTERNAL;
  }

  TRI_RemoveVectorPointer(&document->_datafiles, i);

  // update dfi
  dfi = TRI_FindDatafileInfoDocumentCollection(document, df->_fid, false);

  if (dfi != nullptr) {
    TRI_RemoveDatafileInfoDocumentCollection(document, df->_fid);
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, dfi);
  }

  TRI_WRITE_UNLOCK_DATAFILES_DOC_COLLECTION(document);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief datafile iterator, calculates necessary total size
////////////////////////////////////////////////////////////////////////////////

static bool CalculateSize (TRI_df_marker_t const* marker,
                           void* data,
                           TRI_datafile_t* datafile) {
  compaction_initial_context_t* context = static_cast<compaction_initial_context_t*>(data);
  TRI_document_collection_t* document = context->_document;

  // new or updated document
  if (marker->_type == TRI_DOC_MARKER_KEY_DOCUMENT ||
      marker->_type == TRI_DOC_MARKER_KEY_EDGE) {

    bool deleted;

    TRI_doc_document_key_marker_t const* d = reinterpret_cast<TRI_doc_document_key_marker_t const*>(marker);
    TRI_voc_key_t key = (char*) d + d->_offsetKey;

    // check if the document is still active
    TRI_doc_mptr_t const* found = static_cast<TRI_doc_mptr_t const*>(TRI_LookupByKeyPrimaryIndex(&document->_primaryIndex, key));
    deleted = (found == nullptr || found->_rid > d->_rid);

    if (deleted) {
      return true;
    }

    context->_keepDeletions = true;
    context->_targetSize += AlignedSize(marker);
  }

  // deletions
  else if (marker->_type == TRI_DOC_MARKER_KEY_DELETION &&
           context->_keepDeletions) {
    context->_targetSize += AlignedSize(marker);
  }

  // shapes, attributes
  else if (marker->_type == TRI_DF_MARKER_SHAPE ||
           marker->_type == TRI_DF_MARKER_ATTRIBUTE) {
    context->_targetSize += AlignedSize(marker);
  }

  // transaction markers
  else if (marker->_type == TRI_DOC_MARKER_BEGIN_TRANSACTION ||
           marker->_type == TRI_DOC_MARKER_COMMIT_TRANSACTION ||
           marker->_type == TRI_DOC_MARKER_ABORT_TRANSACTION ||
           marker->_type == TRI_DOC_MARKER_PREPARE_TRANSACTION) {
    if (document->_failedTransactions != nullptr) {
      // these markers only need to be copied if there are "old" failed transactions
      context->_targetSize += AlignedSize(marker);
    }
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief calculate the target size for the compactor to be created
////////////////////////////////////////////////////////////////////////////////

static compaction_initial_context_t InitCompaction (TRI_document_collection_t* document,
                                                    TRI_vector_t const* compactions) {
  compaction_initial_context_t context;

  memset(&context, 0, sizeof(compaction_initial_context_t));
  context._failed = false;
  context._document = document;

  // this is the minimum required size
  context._targetSize = sizeof(TRI_df_header_marker_t) +
                        sizeof(TRI_col_header_marker_t) +
                        sizeof(TRI_df_footer_marker_t) +
                        256; // allow for some overhead

  size_t const n = compactions->_length;
  for (size_t i = 0; i < n; ++i) {
    compaction_info_t* compaction = static_cast<compaction_info_t*>(TRI_AtVector(compactions, i));
    TRI_datafile_t* df = compaction->_datafile;

    if (i == 0) {
      // extract and store fid
      context._fid = compaction->_datafile->_fid;
    }

    context._keepDeletions = compaction->_keepDeletions;

    bool ok = TRI_IterateDatafile(df, CalculateSize, &context);

    if (! ok) {
      context._failed = true;
      break;
    }
  }

  return context;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compact a list of datafiles
////////////////////////////////////////////////////////////////////////////////

static void CompactifyDatafiles (TRI_document_collection_t* document,
                                 TRI_vector_t const* compactions) {
  TRI_datafile_t* compactor;
  compaction_initial_context_t initial;
  compaction_context_t context;
  size_t i, j, n;

  n = compactions->_length;
  TRI_ASSERT(n > 0);

  // create a fake transaction
  triagens::arango::TransactionBase trx(true);


  initial = InitCompaction(document, compactions);

  if (initial._failed) {
    LOG_ERROR("could not create initialise compaction");

    return;
  }

  LOG_TRACE("compactify called for collection '%llu' for %d datafiles of total size %llu",
            (unsigned long long) document->_info._cid,
            (int) n,
            (unsigned long long) initial._targetSize);

  // now create a new compactor file
  // we are re-using the _fid of the first original datafile!
  compactor = CreateCompactor(document, initial._fid, initial._targetSize);

  if (compactor == nullptr) {
    // some error occurred
    LOG_ERROR("could not create compactor file");

    return;
  }

  LOG_DEBUG("created new compactor file '%s'", compactor->getName(compactor));

  memset(&context._dfi, 0, sizeof(TRI_doc_datafile_info_t));
  // these attributes remain the same for all datafiles we collect
  context._document  = document;
  context._compactor = compactor;
  context._dfi._fid  = compactor->_fid;

  // now compact all datafiles
  for (i = 0; i < n; ++i) {
    compaction_info_t* compaction = static_cast<compaction_info_t*>(TRI_AtVector(compactions, i));
    TRI_datafile_t* df = compaction->_datafile;

    LOG_TRACE("compacting datafile '%s' into '%s', number: %d, keep deletions: %d",
               df->getName(df),
               compactor->getName(compactor),
               (int) i,
               (int) compaction->_keepDeletions);

    // if this is the first datafile in the list of datafiles, we can also collect
    // deletion markers
    context._keepDeletions = compaction->_keepDeletions;

    TRI_WRITE_LOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(document);
    
    // run the actual compaction of a single datafile
    bool ok = TRI_IterateDatafile(df, Compactifier, &context);
    
    TRI_WRITE_UNLOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(document);

    if (! ok) {
      LOG_WARNING("failed to compact datafile '%s'", df->getName(df));
      // compactor file does not need to be removed now. will be removed on next startup
      // TODO: Remove
      return;
    }
  } // next file


  // locate the compactor
  // must acquire a write-lock as we're about to change the datafiles vector
  TRI_WRITE_LOCK_DATAFILES_DOC_COLLECTION(document);

  if (! LocateDatafile(&document->_compactors, compactor->_fid, &j)) {
    // not found
    TRI_WRITE_UNLOCK_DATAFILES_DOC_COLLECTION(document);

    LOG_ERROR("logic error in CompactifyDatafiles: could not find compactor");
    return;
  }

  if (! TRI_CloseDatafileDocumentCollection(document, j, true)) {
    TRI_WRITE_UNLOCK_DATAFILES_DOC_COLLECTION(document);

    LOG_ERROR("could not close compactor file");
    // TODO: how do we recover from this state?
    return;
  }

  TRI_WRITE_UNLOCK_DATAFILES_DOC_COLLECTION(document);


  if (context._dfi._numberAlive == 0 &&
      context._dfi._numberDead == 0 &&
      context._dfi._numberDeletion == 0 &&
      context._dfi._numberShapes == 0 &&
      context._dfi._numberAttributes == 0 &&
      context._dfi._numberTransactions == 0) {
    if (n > 1) {
      // create .dead files for all collected files
      for (i = 0; i < n; ++i) {
        compaction_info_t* compaction = static_cast<compaction_info_t*>(TRI_AtVector(compactions, i));
        TRI_datafile_t* datafile = compaction->_datafile;

        if (datafile->isPhysical(datafile)) {
          char* filename = TRI_Concatenate2String(datafile->getName(datafile), ".dead");

          if (filename != nullptr) {
            TRI_WriteFile(filename, "", 0);
            TRI_FreeString(TRI_CORE_MEM_ZONE, filename);
          }
        }
      }
    }

    // compactor is fully empty. remove it
    RemoveCompactor(document, compactor);

    for (i = 0; i < n; ++i) {
      TRI_barrier_t* b;

      compaction_info_t* compaction = static_cast<compaction_info_t*>(TRI_AtVector(compactions, i));

      // datafile is also empty after compaction and thus useless
      RemoveDatafile(document, compaction->_datafile);

      // add a deletion marker to the result set container
      b = TRI_CreateBarrierDropDatafile(&document->_barrierList, compaction->_datafile, DropDatafileCallback, document);

      if (b == nullptr) {
        LOG_ERROR("out of memory when creating datafile-drop barrier");
      }
    }
  }
  else {
    if (n > 1) {
      // create .dead files for all collected files but the first
      for (i = 1; i < n; ++i) {
        compaction_info_t* compaction = static_cast<compaction_info_t*>(TRI_AtVector(compactions, i));
        TRI_datafile_t* datafile = compaction->_datafile;

        if (datafile->isPhysical(datafile)) {
          char* filename = TRI_Concatenate2String(datafile->getName(datafile), ".dead");

          if (filename != nullptr) {
            TRI_WriteFile(filename, "", 0);
            TRI_FreeString(TRI_CORE_MEM_ZONE, filename);
          }
        }
      }
    }

    for (i = 0; i < n; ++i) {
      TRI_barrier_t* b;
      compaction_info_t* compaction = static_cast<compaction_info_t*>(TRI_AtVector(compactions, i));

      if (i == 0) {
        // add a rename marker
        void* copy;

        copy = TRI_Allocate(TRI_CORE_MEM_ZONE, sizeof(compaction_context_t), false);

        memcpy(copy, &context, sizeof(compaction_context_t));

        b = TRI_CreateBarrierRenameDatafile(&document->_barrierList, compaction->_datafile, RenameDatafileCallback, copy);

        if (b == nullptr) {
          LOG_ERROR("out of memory when creating datafile-rename barrier");
          TRI_Free(TRI_CORE_MEM_ZONE, copy);
        }
      }
      else {
        // datafile is empty after compaction and thus useless
        RemoveDatafile(document, compaction->_datafile);

        // add a drop datafile marker
        b = TRI_CreateBarrierDropDatafile(&document->_barrierList, compaction->_datafile, DropDatafileCallback, document);

        if (b == nullptr) {
          LOG_ERROR("out of memory when creating datafile-drop barrier");
        }
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks all datafiles of a collection
////////////////////////////////////////////////////////////////////////////////

static bool CompactifyDocumentCollection (TRI_document_collection_t* document) {
  // we can hopefully get away without the lock here...
//  if (! TRI_IsFullyCollectedDocumentCollection(document)) {
//    return false;
//  }

  // if we cannot acquire the read lock instantly, we will exit directly.
  // otherwise we'll risk a multi-thread deadlock between synchroniser,
  // compactor and data-modification threads (e.g. POST /_api/document)
  if (! TRI_TRY_READ_LOCK_DATAFILES_DOC_COLLECTION(document)) {
    return false;
  }

  size_t const n = document->_datafiles._length;

  if (document->_compactors._length > 0 || n == 0) {
    // we already have created a compactor file in progress.
    // if this happens, then a previous compaction attempt for this collection failed

    // additionally, if there are no datafiles, then there's no need to compact
    TRI_READ_UNLOCK_DATAFILES_DOC_COLLECTION(document);
    return false;
  }

  // get maximum size of result file
  uint64_t maxSize = (uint64_t) COMPACTOR_MAX_SIZE_FACTOR * (uint64_t) document->_info._maximalSize;
  if (maxSize < 8 * 1024 * 1024) {
    maxSize = 8 * 1024 * 1024;
  }
  if (maxSize >= COMPACTOR_MAX_RESULT_FILESIZE) {
    maxSize = COMPACTOR_MAX_RESULT_FILESIZE;
  }

  // copy datafile information
  TRI_vector_t vector;
  TRI_InitVector(&vector, TRI_UNKNOWN_MEM_ZONE, sizeof(compaction_info_t));
  int64_t numAlive = 0;
  bool compactNext = false;

  for (size_t i = 0;  i < n;  ++i) {
    TRI_doc_datafile_info_t* dfi;
    compaction_info_t compaction;
    uint64_t totalSize = 0;
    bool shouldCompact;

    TRI_datafile_t* df = static_cast<TRI_datafile_t*>(document->_datafiles._buffer[i]);

    TRI_ASSERT(df != nullptr);

    dfi = TRI_FindDatafileInfoDocumentCollection(document, df->_fid, true);

    if (dfi == nullptr) {
      continue;
    }

    shouldCompact = false;
  
    if (! compactNext &&
        df->_maximalSize < COMPACTOR_MIN_SIZE &&
        i < n - 1) {
      // very small datafile. let's compact it so it's merged with others
      shouldCompact = true;
      compactNext = true;
    }
    else if (numAlive == 0 && dfi->_numberAlive == 0 && dfi->_numberDeletion > 0) {
      // compact first datafile(s) already if they have some deletions
      shouldCompact = true;
      compactNext = true;
    }
    else {
      // in all other cases, only check the number and size of "dead" objects
      if (dfi->_sizeDead >= (int64_t) COMPACTOR_DEAD_SIZE_THRESHOLD) {
        shouldCompact = true;
        compactNext = true;
      }
      else if (dfi->_sizeDead > 0) {
        // the size of dead objects is above some threshold
        double share = (double) dfi->_sizeDead / ((double) dfi->_sizeDead + (double) dfi->_sizeAlive);

        if (share >= COMPACTOR_DEAD_SIZE_SHARE) {
          // the size of dead objects is above some share
          shouldCompact = true;
          compactNext = true;
        }
      }
    }

    if (! shouldCompact) {
      // only use those datafiles that contain dead objects

      if (! compactNext) {
        numAlive += (int64_t) dfi->_numberAlive;
        continue;
      }
    }

    LOG_TRACE("found datafile eligible for compaction. fid: %llu, size: %llu "
              "numberDead: %llu, numberAlive: %llu, numberDeletion: %llu, "
              "numberShapes: %llu, numberAttributes: %llu, transactions: %llu, "
              "sizeDead: %llu, sizeAlive: %llu, sizeShapes %llu, sizeAttributes: %llu, "
              "sizeTransactions: %llu",
              (unsigned long long) df->_fid,
              (unsigned long long) df->_maximalSize,
              (unsigned long long) dfi->_numberDead,
              (unsigned long long) dfi->_numberAlive,
              (unsigned long long) dfi->_numberDeletion,
              (unsigned long long) dfi->_numberShapes,
              (unsigned long long) dfi->_numberAttributes,
              (unsigned long long) dfi->_numberTransactions,
              (unsigned long long) dfi->_sizeDead,
              (unsigned long long) dfi->_sizeAlive,
              (unsigned long long) dfi->_sizeShapes,
              (unsigned long long) dfi->_sizeAttributes,
              (unsigned long long) dfi->_sizeTransactions);
    totalSize += (uint64_t) df->_maximalSize;

    compaction._datafile = df;
    compaction._keepDeletions = (numAlive > 0 && i > 0);

    TRI_PushBackVector(&vector, &compaction);

    // we stop at the first few datafiles.
    // this is better than going over all datafiles in a collection in one go
    // because the compactor is single-threaded, and collecting all datafiles
    // might take a long time (it might even be that there is a request to
    // delete the collection in the middle of compaction, but the compactor
    // will not pick this up as it is read-locking the collection status)

    if (TRI_LengthVector(&vector) >= COMPACTOR_MAX_FILES ||
        totalSize >= maxSize) {
      // found enough to compact
      break;
    }

    numAlive += (int64_t) dfi->_numberAlive;
  }

  // can now continue without the lock
  TRI_READ_UNLOCK_DATAFILES_DOC_COLLECTION(document);
  
  if (vector._length == 0) {
    // cleanup local variables
    TRI_DestroyVector(&vector);
    return false;
  }

  // handle datafiles with dead objects
  TRI_ASSERT(vector._length >= 1);

  CompactifyDatafiles(document, &vector);

  // cleanup local variables
  TRI_DestroyVector(&vector);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief try to write-lock the compaction
/// returns true if lock acquisition was successful. the caller is responsible
/// to free the write lock eventually
////////////////////////////////////////////////////////////////////////////////

static bool TryLockCompaction (TRI_vocbase_t* vocbase) {
  return TRI_TryWriteLockReadWriteLock(&vocbase->_compactionBlockers._lock);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief write-lock the compaction
////////////////////////////////////////////////////////////////////////////////

static void LockCompaction (TRI_vocbase_t* vocbase) {
  while (! TryLockCompaction(vocbase)) {
    // cycle until we have acquired the write-lock
    usleep(1000);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief write-unlock the compaction
////////////////////////////////////////////////////////////////////////////////

static void UnlockCompaction (TRI_vocbase_t* vocbase) {
  TRI_WriteUnlockReadWriteLock(&vocbase->_compactionBlockers._lock);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief atomic check and lock for running the compaction
/// if this function returns true, it has acquired a write-lock on the
/// compactionBlockers structure, which the caller must free eventually
////////////////////////////////////////////////////////////////////////////////

static bool CheckAndLockCompaction (TRI_vocbase_t* vocbase) {
  double now = TRI_microtime();

  // check if we can acquire the write lock instantly
  if (! TryLockCompaction(vocbase)) {
    // couldn't acquire the write lock
    return false;
  }

  // we are now holding the write lock

  // check if we have a still-valid compaction blocker
  size_t const n = vocbase->_compactionBlockers._data._length;
  for (size_t i = 0; i < n; ++i) {
    compaction_blocker_t* blocker = static_cast<compaction_blocker_t*>(TRI_AtVector(&vocbase->_compactionBlockers._data, i));

    if (blocker->_expires > now) {
      // found a compaction blocker. unlock and return
      UnlockCompaction(vocbase);
      return false;
    }
  }

  return true;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief initialise the compaction blockers structure
////////////////////////////////////////////////////////////////////////////////

int TRI_InitCompactorVocBase (TRI_vocbase_t* vocbase) {
  TRI_InitReadWriteLock(&vocbase->_compactionBlockers._lock);
  TRI_InitVector(&vocbase->_compactionBlockers._data, TRI_UNKNOWN_MEM_ZONE, sizeof(compaction_blocker_t));

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the compaction blockers structure
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyCompactorVocBase (TRI_vocbase_t* vocbase) {
  TRI_DestroyVector(&vocbase->_compactionBlockers._data);
  TRI_DestroyReadWriteLock(&vocbase->_compactionBlockers._lock);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief remove data of expired compaction blockers
////////////////////////////////////////////////////////////////////////////////

bool TRI_CleanupCompactorVocBase (TRI_vocbase_t* vocbase) {
  double now;

  now = TRI_microtime();

  // check if we can instantly acquire the lock
  if (! TryLockCompaction(vocbase)) {
    // couldn't acquire lock
    return false;
  }

  // we are now holding the write lock

  size_t n = vocbase->_compactionBlockers._data._length;
  size_t i = 0;
  while (i < n) {
    compaction_blocker_t* blocker = static_cast<compaction_blocker_t*>(TRI_AtVector(&vocbase->_compactionBlockers._data, i));

    if (blocker->_expires < now) {
      TRI_RemoveVector(&vocbase->_compactionBlockers._data, i);
      n--;
    }
    else {
      i++;
    }
  }

  UnlockCompaction(vocbase);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief insert a compaction blocker
////////////////////////////////////////////////////////////////////////////////

int TRI_InsertBlockerCompactorVocBase (TRI_vocbase_t* vocbase,
                                       double lifetime,
                                       TRI_voc_tick_t* id) {
  compaction_blocker_t blocker;
  int res;

  if (lifetime <= 0.0) {
    return TRI_ERROR_BAD_PARAMETER;
  }

  blocker._id      = TRI_NewTickServer();
  blocker._expires = TRI_microtime() + lifetime;

  LockCompaction(vocbase);

  res = TRI_PushBackVector(&vocbase->_compactionBlockers._data, &blocker);

  UnlockCompaction(vocbase);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  *id = blocker._id;

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief touch an existing compaction blocker
////////////////////////////////////////////////////////////////////////////////

int TRI_TouchBlockerCompactorVocBase (TRI_vocbase_t* vocbase,
                                      TRI_voc_tick_t id,
                                      double lifetime) {
  bool found = false;

  if (lifetime <= 0.0) {
    return TRI_ERROR_BAD_PARAMETER;
  }

  LockCompaction(vocbase);

  size_t const n = vocbase->_compactionBlockers._data._length;

  for (size_t i = 0; i < n; ++i) {
    compaction_blocker_t* blocker = static_cast<compaction_blocker_t*>(TRI_AtVector(&vocbase->_compactionBlockers._data, i));

    if (blocker->_id == id) {
      blocker->_expires = TRI_microtime() + lifetime;
      found = true;
      break;
    }
  }

  UnlockCompaction(vocbase);

  if (! found) {
    return TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND;
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief atomically check-and-lock the compactor
/// if the function returns true, then a write-lock on the compactor was
/// acquired, which must eventually be freed by the caller
////////////////////////////////////////////////////////////////////////////////

bool TRI_CheckAndLockCompactorVocBase (TRI_vocbase_t* vocbase) {
  return TryLockCompaction(vocbase);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief unlock the compactor
////////////////////////////////////////////////////////////////////////////////

void TRI_UnlockCompactorVocBase (TRI_vocbase_t* vocbase) {
  UnlockCompaction(vocbase);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief remove an existing compaction blocker
////////////////////////////////////////////////////////////////////////////////

int TRI_RemoveBlockerCompactorVocBase (TRI_vocbase_t* vocbase,
                                       TRI_voc_tick_t id) {
  bool found = false;

  LockCompaction(vocbase);

  size_t const n = vocbase->_compactionBlockers._data._length;

  for (size_t i = 0; i < n; ++i) {
    compaction_blocker_t* blocker = static_cast<compaction_blocker_t*>(TRI_AtVector(&vocbase->_compactionBlockers._data, i));

    if (blocker->_id == id) {
      TRI_RemoveVector(&vocbase->_compactionBlockers._data, i);
      found = true;
      break;
    }
  }

  UnlockCompaction(vocbase);

  if (! found) {
    return TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND;
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compactor event loop
////////////////////////////////////////////////////////////////////////////////

void TRI_CompactorVocBase (void* data) {
  TRI_vocbase_t* vocbase;
  TRI_vector_pointer_t collections;
  int numCompacted = 0;

  vocbase = (TRI_vocbase_t*) data;
  TRI_ASSERT(vocbase->_state == 1);

  TRI_InitVectorPointer(&collections, TRI_UNKNOWN_MEM_ZONE);

  while (true) {
    // keep initial _state value as vocbase->_state might change during compaction loop
    int state = vocbase->_state;

    // check if compaction is currently disallowed
    if (CheckAndLockCompaction(vocbase)) {
      // compaction is currently allowed
      double now = TRI_microtime();
      numCompacted = 0;

      // copy all collections
      TRI_READ_LOCK_COLLECTIONS_VOCBASE(vocbase);
      TRI_CopyDataVectorPointer(&collections, &vocbase->_collections);
      TRI_READ_UNLOCK_COLLECTIONS_VOCBASE(vocbase);

      size_t const n = collections._length;

      for (size_t i = 0;  i < n;  ++i) {
        TRI_vocbase_col_t* collection = static_cast<TRI_vocbase_col_t*>(collections._buffer[i]);

        if (! TRI_TRY_READ_LOCK_STATUS_VOCBASE_COL(collection)) {
          // if we can't acquire the read lock instantly, we continue directly
          // we don't want to stall here for too long
          continue;
        }

        TRI_document_collection_t* document = collection->_collection;

        if (document == nullptr) {
          TRI_READ_UNLOCK_STATUS_VOCBASE_COL(collection);
          continue;
        }

        bool worked    = false;
        bool doCompact = document->_info._doCompact;

        // for document collection, compactify datafiles
        if (collection->_status == TRI_VOC_COL_STATUS_LOADED && doCompact) {
          // check whether someone else holds a read-lock on the compaction lock
          if (! TRI_TryWriteLockReadWriteLock(&document->_compactionLock)) {
            // someone else is holding the compactor lock, we'll not compact
            TRI_READ_UNLOCK_STATUS_VOCBASE_COL(collection);
            continue;
          }

          if (document->_lastCompaction + COMPACTOR_COLLECTION_INTERVAL <= now) {
            TRI_barrier_t* ce = TRI_CreateBarrierCompaction(&document->_barrierList);

            if (ce == nullptr) {
              // out of memory
              LOG_WARNING("out of memory when trying to create a barrier element");
            }
            else {
              worked = CompactifyDocumentCollection(document);

              if (! worked) {
                // set compaction stamp
                document->_lastCompaction = now;
              }
              // if we worked, then we don't set the compaction stamp to force another round of compaction

              TRI_FreeBarrier(ce);
            }
          }

          // read-unlock the compaction lock
          TRI_WriteUnlockReadWriteLock(&document->_compactionLock);
        }

        TRI_READ_UNLOCK_STATUS_VOCBASE_COL(collection);

        if (worked) {
          ++numCompacted;

          // signal the cleanup thread that we worked and that it can now wake up
          TRI_LockCondition(&vocbase->_cleanupCondition);
          TRI_SignalCondition(&vocbase->_cleanupCondition);
          TRI_UnlockCondition(&vocbase->_cleanupCondition);
        }
      }

      UnlockCompaction(vocbase);
    }

    if (numCompacted > 0) {
      // no need to sleep long or go into wait state if we worked.
      // maybe there's still work left
      usleep(1000);
    }
    else if (state != 2 && vocbase->_state == 1) {
      // only sleep while server is still running
      TRI_LockCondition(&vocbase->_compactorCondition);
      TRI_TimedWaitCondition(&vocbase->_compactorCondition, (uint64_t) COMPACTOR_INTERVAL);
      TRI_UnlockCondition(&vocbase->_compactorCondition);
    }

    if (state == 2) {
      // server shutdown
      break;
    }
  }

  TRI_DestroyVectorPointer(&collections);

  LOG_TRACE("shutting down compactor thread");
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#ifdef _WIN32
#include "Basics/win-utils.h"
#endif

#include "compactor.h"

#include "Basics/conversions.h"
#include "Basics/files.h"
#include "Basics/Logger.h"
#include "Basics/tri-strings.h"
#include "Basics/memory-map.h"
#include "Indexes/PrimaryIndex.h"
#include "Utils/StandaloneTransactionContext.h"
#include "Utils/transactions.h"
#include "VocBase/DatafileStatistics.h"
#include "VocBase/document-collection.h"
#include "VocBase/server.h"
#include "VocBase/vocbase.h"
#include "VocBase/VocShaper.h"

using namespace arangodb;

static char const* ReasonNoDatafiles =
    "skipped compaction because collection has no datafiles";
static char const* ReasonCompactionBlocked =
    "skipped compaction because existing compactor file is in the way and "
    "waits to be processed";
static char const* ReasonDatafileSmall =
    "compacting datafile because it's small and will be merged with next";
static char const* ReasonEmpty =
    "compacting datafile because collection is empty";
static char const* ReasonOnlyDeletions =
    "compacting datafile because it contains only deletion markers";
static char const* ReasonDeadSize =
    "compacting datafile because it contains much dead object space";
static char const* ReasonDeadSizeShare =
    "compacting datafile because it contains high share of dead objects";
static char const* ReasonDeadCount =
    "compacting datafile because it contains many dead objects";
static char const* ReasonNothingToCompact =
    "checked datafiles, but no compaction opportunity found";

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
/// @brief minimum number of deletion marker in file from which on we will
/// compact it if nothing else qualifies file for compaction
////////////////////////////////////////////////////////////////////////////////

#define COMPACTOR_DEAD_THRESHOLD (16384)

////////////////////////////////////////////////////////////////////////////////
/// @brief maximum number of datafiles to join together in one compaction run
////////////////////////////////////////////////////////////////////////////////

#define COMPACTOR_MAX_FILES 3

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

////////////////////////////////////////////////////////////////////////////////
/// @brief compaction blocker entry
////////////////////////////////////////////////////////////////////////////////

typedef struct compaction_blocker_s {
  TRI_voc_tick_t _id;
  double _expires;
} compaction_blocker_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief auxiliary struct used when initializing compaction
////////////////////////////////////////////////////////////////////////////////

struct compaction_initial_context_t {
  arangodb::Transaction* _trx;
  TRI_document_collection_t* _document;
  int64_t _targetSize;
  TRI_voc_fid_t _fid;
  bool _keepDeletions;
  bool _failed;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief compaction state
////////////////////////////////////////////////////////////////////////////////

struct compaction_context_t {
  arangodb::Transaction* _trx;
  TRI_document_collection_t* _document;
  TRI_datafile_t* _compactor;
  DatafileStatisticsContainer _dfi;
  bool _keepDeletions;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief compaction instruction for a single datafile
////////////////////////////////////////////////////////////////////////////////

struct compaction_info_t {
  TRI_datafile_t* _datafile;
  bool _keepDeletions;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief return a marker's size
////////////////////////////////////////////////////////////////////////////////

static inline int64_t AlignedSize(TRI_df_marker_t const* marker) {
  return static_cast<int64_t>(TRI_DF_ALIGN_BLOCK(marker->_size));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a compactor file, based on a datafile
////////////////////////////////////////////////////////////////////////////////

static TRI_datafile_t* CreateCompactor(TRI_document_collection_t* document,
                                       TRI_voc_fid_t fid, int64_t maximalSize) {
  TRI_collection_t* collection = document;

  // reserve room for one additional entry
  if (TRI_ReserveVectorPointer(&collection->_compactors, 1) !=
      TRI_ERROR_NO_ERROR) {
    // could not get memory, exit early
    return nullptr;
  }

  TRI_LOCK_JOURNAL_ENTRIES_DOC_COLLECTION(document);

  TRI_datafile_t* compactor = TRI_CreateDatafileDocumentCollection(
      document, fid, static_cast<TRI_voc_size_t>(maximalSize), true);

  if (compactor != nullptr) {
    int res TRI_UNUSED =
        TRI_PushBackVectorPointer(&collection->_compactors, compactor);

    // we have reserved space before, so we can be sure the push succeeds
    TRI_ASSERT(res == TRI_ERROR_NO_ERROR);
  }

  TRI_UNLOCK_JOURNAL_ENTRIES_DOC_COLLECTION(document);

  return compactor;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief write a copy of the marker into the datafile
////////////////////////////////////////////////////////////////////////////////

static int CopyMarker(TRI_document_collection_t* document,
                      TRI_datafile_t* compactor, TRI_df_marker_t const* marker,
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

static bool LocateDatafile(TRI_vector_pointer_t const* vector,
                           const TRI_voc_fid_t fid, size_t* position) {
  size_t const n = vector->_length;

  for (size_t i = 0; i < n; ++i) {
    auto df = static_cast<TRI_datafile_t const*>(vector->_buffer[i]);

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

static void DropDatafileCallback(TRI_datafile_t* datafile, void* data) {
  TRI_document_collection_t* document =
      static_cast<TRI_document_collection_t*>(data);
  TRI_voc_fid_t fid = datafile->_fid;
  char* copy = nullptr;

  char* number = TRI_StringUInt64(fid);
  char* name = TRI_Concatenate3String("deleted-", number, ".db");
  char* filename = TRI_Concatenate2File(document->_directory, name);

  TRI_FreeString(TRI_CORE_MEM_ZONE, number);
  TRI_FreeString(TRI_CORE_MEM_ZONE, name);

  bool ok;

  if (datafile->isPhysical(datafile)) {
    // copy the current filename
    copy = TRI_DuplicateString(TRI_CORE_MEM_ZONE, datafile->_filename);

    ok = TRI_RenameDatafile(datafile, filename);

    if (!ok) {
      LOG_TOPIC(ERR, Logger::COMPACTOR) << "cannot rename obsolete datafile '" << copy << "' to '" << filename << "': " << TRI_last_error();
    }
  }

  LOG_TOPIC(DEBUG, Logger::COMPACTOR) << "finished compacting datafile '" << datafile->getName(datafile) << "'";

  ok = TRI_CloseDatafile(datafile);

  if (!ok) {
    LOG_TOPIC(ERR, Logger::COMPACTOR) << "cannot close obsolete datafile '" << datafile->getName(datafile) << "': " << TRI_last_error();
  } else if (datafile->isPhysical(datafile)) {
    int res;

    LOG_TOPIC(DEBUG, Logger::COMPACTOR) << "wiping compacted datafile from disk";

    res = TRI_UnlinkFile(filename);

    if (res != TRI_ERROR_NO_ERROR) {
      LOG_TOPIC(ERR, Logger::COMPACTOR) << "cannot wipe obsolete datafile '" << datafile->getName(datafile) << "': " << TRI_last_error();
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

static void RenameDatafileCallback(TRI_datafile_t* datafile, void* data) {
  compaction_context_t* context = static_cast<compaction_context_t*>(data);
  TRI_datafile_t* compactor = context->_compactor;

  TRI_document_collection_t* document = context->_document;

  bool ok = false;
  TRI_ASSERT(datafile->_fid == compactor->_fid);

  if (datafile->isPhysical(datafile)) {
    char* realName = TRI_DuplicateString(datafile->_filename);

    // construct a suitable tempname
    char* number = TRI_StringUInt64(datafile->_fid);
    char* jname = TRI_Concatenate3String("temp-", number, ".db");
    char* tempFilename = TRI_Concatenate2File(document->_directory, jname);

    TRI_FreeString(TRI_CORE_MEM_ZONE, number);
    TRI_FreeString(TRI_CORE_MEM_ZONE, jname);

    if (!TRI_RenameDatafile(datafile, tempFilename)) {
      LOG_TOPIC(ERR, Logger::COMPACTOR) << "unable to rename datafile '" << datafile->getName(datafile) << "' to '" << tempFilename << "'";
    } else {
      if (!TRI_RenameDatafile(compactor, realName)) {
        LOG_TOPIC(ERR, Logger::COMPACTOR) << "unable to rename compaction file '" << compactor->getName(compactor) << "' to '" << realName << "'";
      } else {
        ok = true;
      }
    }

    TRI_FreeString(TRI_CORE_MEM_ZONE, tempFilename);
    TRI_FreeString(TRI_CORE_MEM_ZONE, realName);
  } else {
    ok = true;
  }

  if (ok) {
    size_t i;

    // must acquire a write-lock as we're about to change the datafiles vector
    TRI_WRITE_LOCK_DATAFILES_DOC_COLLECTION(document);

    if (!LocateDatafile(&document->_datafiles, datafile->_fid, &i)) {
      TRI_WRITE_UNLOCK_DATAFILES_DOC_COLLECTION(document);

      LOG_TOPIC(ERR, Logger::COMPACTOR) << "logic error: could not locate datafile";
      TRI_Free(TRI_CORE_MEM_ZONE, context);
      return;
    }

    // put the compactor in place of the datafile
    document->_datafiles._buffer[i] = compactor;

    if (!LocateDatafile(&document->_compactors, compactor->_fid, &i)) {
      TRI_WRITE_UNLOCK_DATAFILES_DOC_COLLECTION(document);

      LOG_TOPIC(ERR, Logger::COMPACTOR) << "logic error: could not locate compactor";
      TRI_Free(TRI_CORE_MEM_ZONE, context);
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
// 7 purpose is to find the still-alive markers and copy them into the compactor
/// file.
/// IMPORTANT: if the logic inside this function is adjusted, the total size
/// calculated by function CalculateSize might need adjustment, too!!
////////////////////////////////////////////////////////////////////////////////

static bool Compactifier(TRI_df_marker_t const* marker, void* data,
                         TRI_datafile_t* datafile) {
  TRI_df_marker_t* result;
  int res;

  compaction_context_t* context = static_cast<compaction_context_t*>(data);
  TRI_document_collection_t* document = context->_document;
  TRI_voc_fid_t const targetFid = context->_compactor->_fid;

  // new or updated document
  if (marker->_type == TRI_DOC_MARKER_KEY_DOCUMENT ||
      marker->_type == TRI_DOC_MARKER_KEY_EDGE) {
    TRI_doc_document_key_marker_t const* d =
        reinterpret_cast<TRI_doc_document_key_marker_t const*>(marker);
    TRI_voc_key_t key = (char*)d + d->_offsetKey;

    // check if the document is still active
    auto primaryIndex = document->primaryIndex();

    auto found = static_cast<TRI_doc_mptr_t const*>(
        primaryIndex->lookupKey(context->_trx, key));
    bool deleted = (found == nullptr || found->_rid > d->_rid);

    if (deleted) {
      // found a dead document
      context->_dfi.numberDead++;
      context->_dfi.sizeDead += AlignedSize(marker);
      LOG_TOPIC(TRACE, Logger::COMPACTOR) << "found a stale document: " << key;
      return true;
    }

    context->_keepDeletions = true;

    // write to compactor files
    res = CopyMarker(document, context->_compactor, marker, &result);

    if (res != TRI_ERROR_NO_ERROR) {
      // TODO: dont fail but recover from this state
      LOG_TOPIC(FATAL, Logger::COMPACTOR) << "cannot write compactor file: " << TRI_last_error(); FATAL_ERROR_EXIT();
    }

    TRI_doc_mptr_t* found2 = const_cast<TRI_doc_mptr_t*>(found);
    TRI_ASSERT(found2->getDataPtr() !=
               nullptr);  // ONLY in COMPACTIFIER, PROTECTED by fake trx outside
    TRI_ASSERT(((TRI_df_marker_t*)found2->getDataPtr())->_size >
               0);  // ONLY in COMPACTIFIER, PROTECTED by fake trx outside

    // let marker point to the new position
    found2->setDataPtr(result);
    // update fid in case it changes
    if (found2->_fid != targetFid) {
      found2->_fid = targetFid;
    }

    context->_dfi.numberAlive++;
    context->_dfi.sizeAlive += AlignedSize(marker);
  }

  // deletions
  else if (marker->_type == TRI_DOC_MARKER_KEY_DELETION) {
    if (context->_keepDeletions) {
      // write to compactor files
      res = CopyMarker(document, context->_compactor, marker, &result);

      if (res != TRI_ERROR_NO_ERROR) {
        // TODO: dont fail but recover from this state
        LOG_TOPIC(FATAL, Logger::COMPACTOR) << "cannot write document marker to compactor file: " << TRI_last_error(); FATAL_ERROR_EXIT();
      }

      // update datafile info
      context->_dfi.numberDeletions++;
    }
  }

  // shapes
  else if (marker->_type == TRI_DF_MARKER_SHAPE) {
    // write to compactor files
    res = CopyMarker(document, context->_compactor, marker, &result);

    if (res != TRI_ERROR_NO_ERROR) {
      // TODO: dont fail but recover from this state
      LOG_TOPIC(FATAL, Logger::COMPACTOR) << "cannot write shape marker to compactor file: " << TRI_last_error(); FATAL_ERROR_EXIT();
    }

    res = document->getShaper()->moveMarker(
        result, nullptr);  // ONLY IN COMPACTOR, PROTECTED by fake trx in caller

    if (res != TRI_ERROR_NO_ERROR) {
      LOG_TOPIC(FATAL, Logger::COMPACTOR) << "cannot re-locate shape marker"; FATAL_ERROR_EXIT();
    }

    context->_dfi.numberShapes++;
    context->_dfi.sizeShapes += AlignedSize(marker);
  }

  // attributes
  else if (marker->_type == TRI_DF_MARKER_ATTRIBUTE) {
    // write to compactor files
    res = CopyMarker(document, context->_compactor, marker, &result);

    if (res != TRI_ERROR_NO_ERROR) {
      // TODO: dont fail but recover from this state
      LOG_TOPIC(FATAL, Logger::COMPACTOR) << "cannot write attribute marker to compactor file: " << TRI_last_error(); FATAL_ERROR_EXIT();
    }

    res = document->getShaper()->moveMarker(
        result, nullptr);  // ONLY IN COMPACTOR, PROTECTED by fake trx in caller

    if (res != TRI_ERROR_NO_ERROR) {
      LOG_TOPIC(FATAL, Logger::COMPACTOR) << "cannot re-locate attribute marker"; FATAL_ERROR_EXIT();
    }

    context->_dfi.numberAttributes++;
    context->_dfi.sizeAttributes += AlignedSize(marker);
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief remove an empty compactor file
////////////////////////////////////////////////////////////////////////////////

static int RemoveCompactor(TRI_document_collection_t* document,
                           TRI_datafile_t* compactor) {
  size_t i;

  LOG_TOPIC(TRACE, Logger::COMPACTOR) << "removing empty compaction file '" << compactor->getName(compactor) << "'";

  // remove the datafile from the list of datafiles
  TRI_WRITE_LOCK_DATAFILES_DOC_COLLECTION(document);

  // remove the compactor from the list of compactors
  if (!LocateDatafile(&document->_compactors, compactor->_fid, &i)) {
    TRI_WRITE_UNLOCK_DATAFILES_DOC_COLLECTION(document);

    LOG_TOPIC(ERR, Logger::COMPACTOR) << "logic error: could not locate compactor";

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
  } else {
    TRI_CloseDatafile(compactor);
    TRI_FreeDatafile(compactor);
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief remove an empty datafile
////////////////////////////////////////////////////////////////////////////////

static int RemoveDatafile(TRI_document_collection_t* document,
                          TRI_datafile_t* df) {
  LOG_TOPIC(TRACE, Logger::COMPACTOR) << "removing empty datafile '" << df->getName(df) << "'";

  // remove the datafile from the list of datafiles
  TRI_WRITE_LOCK_DATAFILES_DOC_COLLECTION(document);

  size_t i;
  if (!LocateDatafile(&document->_datafiles, df->_fid, &i)) {
    TRI_WRITE_UNLOCK_DATAFILES_DOC_COLLECTION(document);

    LOG_TOPIC(ERR, Logger::COMPACTOR) << "logic error: could not locate datafile";

    return TRI_ERROR_INTERNAL;
  }

  TRI_RemoveVectorPointer(&document->_datafiles, i);
  TRI_WRITE_UNLOCK_DATAFILES_DOC_COLLECTION(document);

  // update dfi
  document->_datafileStatistics.remove(df->_fid);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief datafile iterator, calculates necessary total size
////////////////////////////////////////////////////////////////////////////////

static bool CalculateSize(TRI_df_marker_t const* marker, void* data,
                          TRI_datafile_t* datafile) {
  compaction_initial_context_t* context =
      static_cast<compaction_initial_context_t*>(data);
  TRI_document_collection_t* document = context->_document;

  // new or updated document
  if (marker->_type == TRI_DOC_MARKER_KEY_DOCUMENT ||
      marker->_type == TRI_DOC_MARKER_KEY_EDGE) {
    TRI_doc_document_key_marker_t const* d =
        reinterpret_cast<TRI_doc_document_key_marker_t const*>(marker);
    TRI_voc_key_t key = (char*)d + d->_offsetKey;

    // check if the document is still active
    auto primaryIndex = document->primaryIndex();
    auto found = static_cast<TRI_doc_mptr_t const*>(
        primaryIndex->lookupKey(context->_trx, key));
    bool deleted = (found == nullptr || found->_rid > d->_rid);

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
      // these markers only need to be copied if there are "old" failed
      // transactions
      context->_targetSize += AlignedSize(marker);
    }
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief calculate the target size for the compactor to be created
////////////////////////////////////////////////////////////////////////////////

static compaction_initial_context_t InitCompaction(
    arangodb::Transaction* trx, TRI_document_collection_t* document,
    std::vector<compaction_info_t> const& toCompact) {
  compaction_initial_context_t context;

  memset(&context, 0, sizeof(compaction_initial_context_t));
  context._trx = trx;
  context._failed = false;
  context._document = document;

  // this is the minimum required size
  context._targetSize =
      sizeof(TRI_df_header_marker_t) + sizeof(TRI_col_header_marker_t) +
      sizeof(TRI_df_footer_marker_t) + 256;  // allow for some overhead

  size_t const n = toCompact.size();

  for (size_t i = 0; i < n; ++i) {
    auto compaction = toCompact[i];
    TRI_datafile_t* df = compaction._datafile;

    // We will sequentially scan the logfile for collection:
    if (df->isPhysical(df)) {
      TRI_MMFileAdvise(df->_data, df->_maximalSize, TRI_MADVISE_SEQUENTIAL);
      TRI_MMFileAdvise(df->_data, df->_maximalSize, TRI_MADVISE_WILLNEED);
    }

    if (i == 0) {
      // extract and store fid
      context._fid = compaction._datafile->_fid;
    }

    context._keepDeletions = compaction._keepDeletions;

    TRI_READ_LOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(document);
    bool ok;
    try {
      ok = TRI_IterateDatafile(df, CalculateSize, &context);
    } catch (...) {
      ok = false;
    }
    TRI_READ_UNLOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(document);

    if (df->isPhysical(df)) {
      TRI_MMFileAdvise(df->_data, df->_maximalSize, TRI_MADVISE_RANDOM);
    }

    if (!ok) {
      context._failed = true;
      break;
    }
  }

  return context;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compact a list of datafiles
////////////////////////////////////////////////////////////////////////////////

static void CompactifyDatafiles(
    TRI_document_collection_t* document,
    std::vector<compaction_info_t> const& toCompact) {
  TRI_datafile_t* compactor;
  compaction_context_t context;
  size_t i, j;

  size_t const n = toCompact.size();
  TRI_ASSERT(n > 0);

  arangodb::SingleCollectionWriteTransaction<UINT64_MAX> trx(
      new arangodb::StandaloneTransactionContext(), document->_vocbase,
      document->_info.id());
  trx.addHint(TRI_TRANSACTION_HINT_NO_BEGIN_MARKER, true);
  trx.addHint(TRI_TRANSACTION_HINT_NO_ABORT_MARKER, true);
  trx.addHint(TRI_TRANSACTION_HINT_NO_COMPACTION_LOCK, true);

  compaction_initial_context_t initial =
      InitCompaction(&trx, document, toCompact);

  if (initial._failed) {
    LOG_TOPIC(ERR, Logger::COMPACTOR) << "could not create initialize compaction";

    return;
  }

  LOG_TOPIC(TRACE, Logger::COMPACTOR) << "compactify called for collection '" << document->_info.id() << "' for " << n << " datafiles of total size " << initial._targetSize;

  // now create a new compactor file
  // we are re-using the _fid of the first original datafile!
  compactor = CreateCompactor(document, initial._fid, initial._targetSize);

  if (compactor == nullptr) {
    // some error occurred
    LOG_TOPIC(ERR, Logger::COMPACTOR) << "could not create compactor file";

    return;
  }

  LOG_TOPIC(DEBUG, Logger::COMPACTOR) << "created new compactor file '" << compactor->getName(compactor) << "'";

  // these attributes remain the same for all datafiles we collect
  context._document = document;
  context._compactor = compactor;
  context._trx = &trx;

  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    LOG_TOPIC(ERR, Logger::COMPACTOR) << "error during compaction: " << TRI_errno_string(res);
    return;
  }

  // now compact all datafiles
  for (i = 0; i < n; ++i) {
    auto compaction = toCompact[i];
    TRI_datafile_t* df = compaction._datafile;

    LOG_TOPIC(TRACE, Logger::COMPACTOR) << "compacting datafile '" << df->getName(df) << "' into '" << compactor->getName(compactor) << "', number: " << i << ", keep deletions: " << compaction._keepDeletions;

    // if this is the first datafile in the list of datafiles, we can also
    // collect
    // deletion markers
    context._keepDeletions = compaction._keepDeletions;

    // run the actual compaction of a single datafile
    bool ok = TRI_IterateDatafile(df, Compactifier, &context);

    if (!ok) {
      LOG_TOPIC(WARN, Logger::COMPACTOR) << "failed to compact datafile '" << df->getName(df) << "'";
      // compactor file does not need to be removed now. will be removed on next
      // startup
      // TODO: Remove file
      return;
    }

  }  // next file

  document->_datafileStatistics.replace(compactor->_fid, context._dfi);

  trx.commit();

  // remove all datafile statistics that we don't need anymore
  for (i = 1; i < n; ++i) {
    auto compaction = toCompact[i];
    document->_datafileStatistics.remove(compaction._datafile->_fid);
  }

  // locate the compactor
  // must acquire a write-lock as we're about to change the datafiles vector
  TRI_WRITE_LOCK_DATAFILES_DOC_COLLECTION(document);

  if (!LocateDatafile(&document->_compactors, compactor->_fid, &j)) {
    // not found
    TRI_WRITE_UNLOCK_DATAFILES_DOC_COLLECTION(document);

    LOG_TOPIC(ERR, Logger::COMPACTOR) << "logic error in CompactifyDatafiles: could not find compactor";
    return;
  }

  if (!TRI_CloseDatafileDocumentCollection(document, j, true)) {
    TRI_WRITE_UNLOCK_DATAFILES_DOC_COLLECTION(document);

    LOG_TOPIC(ERR, Logger::COMPACTOR) << "could not close compactor file";
    // TODO: how do we recover from this state?
    return;
  }

  TRI_WRITE_UNLOCK_DATAFILES_DOC_COLLECTION(document);

  if (context._dfi.numberAlive == 0 && context._dfi.numberDead == 0 &&
      context._dfi.numberDeletions == 0 && context._dfi.numberShapes == 0 &&
      context._dfi.numberAttributes == 0) {
    if (n > 1) {
      // create .dead files for all collected files
      for (i = 0; i < n; ++i) {
        auto compaction = toCompact[i];
        TRI_datafile_t* datafile = compaction._datafile;

        if (datafile->isPhysical(datafile)) {
          char* filename =
              TRI_Concatenate2String(datafile->getName(datafile), ".dead");

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
      auto compaction = toCompact[i];

      // datafile is also empty after compaction and thus useless
      RemoveDatafile(document, compaction._datafile);

      // add a deletion ditch to the collection
      auto b = document->ditches()->createDropDatafileDitch(
          compaction._datafile, document, DropDatafileCallback, __FILE__,
          __LINE__);

      if (b == nullptr) {
        LOG_TOPIC(ERR, Logger::COMPACTOR) << "out of memory when creating datafile-drop ditch";
      }
    }
  } else {
    if (n > 1) {
      // create .dead files for all collected files but the first
      for (i = 1; i < n; ++i) {
        auto compaction = toCompact[i];
        TRI_datafile_t* datafile = compaction._datafile;

        if (datafile->isPhysical(datafile)) {
          char* filename =
              TRI_Concatenate2String(datafile->getName(datafile), ".dead");

          if (filename != nullptr) {
            TRI_WriteFile(filename, "", 0);
            TRI_FreeString(TRI_CORE_MEM_ZONE, filename);
          }
        }
      }
    }

    for (i = 0; i < n; ++i) {
      auto compaction = toCompact[i];

      if (i == 0) {
        // add a rename marker
        void* copy = TRI_Allocate(TRI_CORE_MEM_ZONE,
                                  sizeof(compaction_context_t), false);

        memcpy(copy, &context, sizeof(compaction_context_t));

        auto b = document->ditches()->createRenameDatafileDitch(
            compaction._datafile, copy, RenameDatafileCallback, __FILE__,
            __LINE__);

        if (b == nullptr) {
          LOG_TOPIC(ERR, Logger::COMPACTOR) << "out of memory when creating datafile-rename ditch";
          TRI_Free(TRI_CORE_MEM_ZONE, copy);
        }
      } else {
        // datafile is empty after compaction and thus useless
        RemoveDatafile(document, compaction._datafile);

        // add a drop datafile marker
        auto b = document->ditches()->createDropDatafileDitch(
            compaction._datafile, document, DropDatafileCallback, __FILE__,
            __LINE__);

        if (b == nullptr) {
          LOG_TOPIC(ERR, Logger::COMPACTOR) << "out of memory when creating datafile-drop ditch";
        }
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks all datafiles of a collection
////////////////////////////////////////////////////////////////////////////////

static bool CompactifyDocumentCollection(TRI_document_collection_t* document) {
  // we can hopefully get away without the lock here...
  //  if (! TRI_IsFullyCollectedDocumentCollection(document)) {
  //    return false;
  //  }

  std::vector<compaction_info_t> toCompact;
  toCompact.reserve(COMPACTOR_MAX_FILES);

  // if we cannot acquire the read lock instantly, we will exit directly.
  // otherwise we'll risk a multi-thread deadlock between synchronizer,
  // compactor and data-modification threads (e.g. POST /_api/document)
  if (!TRI_TRY_READ_LOCK_DATAFILES_DOC_COLLECTION(document)) {
    return false;
  }

  size_t const n = document->_datafiles._length;

  if (n == 0 || document->_compactors._length > 0) {
    // we already have created a compactor file in progress.
    // if this happens, then a previous compaction attempt for this collection
    // failed

    // additionally, if there are no datafiles, then there's no need to compact
    TRI_READ_UNLOCK_DATAFILES_DOC_COLLECTION(document);

    if (n == 0) {
      document->setCompactionStatus(ReasonNoDatafiles);
    } else {
      document->setCompactionStatus(ReasonCompactionBlocked);
    }
    return false;
  }

  LOG_TOPIC(TRACE, Logger::COMPACTOR) << "inspecting datafiles of collection '" << document->_info.namec_str() << "' for compaction opportunities";

  size_t start = document->getNextCompactionStartIndex();

  // number of documents is protected by the same lock
  uint64_t const numDocuments = document->size();

  // get maximum size of result file
  uint64_t maxSize = (uint64_t)COMPACTOR_MAX_SIZE_FACTOR *
                     (uint64_t)document->_info.maximalSize();
  if (maxSize < 8 * 1024 * 1024) {
    maxSize = 8 * 1024 * 1024;
  }
  if (maxSize >= COMPACTOR_MAX_RESULT_FILESIZE) {
    maxSize = COMPACTOR_MAX_RESULT_FILESIZE;
  }

  if (start >= n || numDocuments == 0) {
    start = 0;
  }

  int64_t numAlive = 0;
  if (start > 0) {
    // we don't know for sure if there are alive documents in the first
    // datafile,
    // so let's assume there are some
    numAlive = 16384;
  }

  bool doCompact = false;
  uint64_t totalSize = 0;
  char const* reason = nullptr;

  for (size_t i = start; i < n; ++i) {
    TRI_datafile_t* df =
        static_cast<TRI_datafile_t*>(document->_datafiles._buffer[i]);

    TRI_ASSERT(df != nullptr);

    DatafileStatisticsContainer dfi =
        document->_datafileStatistics.get(df->_fid);

    if (dfi.numberUncollected > 0) {
      LOG_TOPIC(TRACE, Logger::COMPACTOR) << "cannot compact datafile " << df->_fid << " of collection '" << document->_info.namec_str() << "' because it still has uncollected entries";
      start = i + 1;
      break;
    }

    if (!doCompact && df->_maximalSize < COMPACTOR_MIN_SIZE && i < n - 1) {
      // very small datafile and not the last one. let's compact it so it's
      // merged with others
      doCompact = true;
      reason = ReasonDatafileSmall;
    } else if (numDocuments == 0 &&
               (dfi.numberAlive > 0 || dfi.numberDead > 0 ||
                dfi.numberDeletions > 0)) {
      // collection is empty, but datafile statistics indicate there is
      // something in this datafile
      doCompact = true;
      reason = ReasonEmpty;
    } else if (numAlive == 0 && dfi.numberAlive == 0 &&
               dfi.numberDeletions > 0) {
      // compact first datafile(s) if they contain only deletions
      doCompact = true;
      reason = ReasonOnlyDeletions;
    } else if (dfi.sizeDead >= (int64_t)COMPACTOR_DEAD_SIZE_THRESHOLD) {
      // the size of dead objects is above some threshold
      doCompact = true;
      reason = ReasonDeadSize;
    } else if (dfi.sizeDead > 0 &&
               (((double)dfi.sizeDead /
                     ((double)dfi.sizeDead + (double)dfi.sizeAlive) >=
                 COMPACTOR_DEAD_SIZE_SHARE) ||
                ((double)dfi.sizeDead / (double)df->_maximalSize >=
                 COMPACTOR_DEAD_SIZE_SHARE))) {
      // the size of dead objects is above some share
      doCompact = true;
      reason = ReasonDeadSizeShare;
    } else if (dfi.numberDead >= (int64_t)COMPACTOR_DEAD_THRESHOLD) {
      // the number of dead objects is above some threshold
      doCompact = true;
      reason = ReasonDeadCount;
    }

    if (!doCompact) {
      numAlive += (int64_t)dfi.numberAlive;
      continue;
    }

    // remember for next compaction
    start = i + 1;

    if (totalSize + (uint64_t)df->_maximalSize >= maxSize &&
        !toCompact.empty()) {
      // found enough files to compact
      break;
    }

    TRI_ASSERT(reason != nullptr);

    LOG_TOPIC(TRACE, Logger::COMPACTOR) << "found datafile eligible for compaction. fid: " << df->_fid << ", size: " << df->_maximalSize << ", reason: " << reason << ", numberDead: " << dfi.numberDead << ", numberAlive: " << dfi.numberAlive << ", numberDeletions: " << dfi.numberDeletions << ", numberShapes: " << dfi.numberShapes << ", numberAttributes: " << dfi.numberAttributes << ", numberUncollected: " << dfi.numberUncollected << ", sizeDead: " << dfi.sizeDead << ", sizeAlive: " << dfi.sizeAlive << ", sizeShapes " << dfi.sizeShapes << ", sizeAttributes: " << dfi.sizeAttributes;
    totalSize += (uint64_t)df->_maximalSize;

    compaction_info_t compaction;
    compaction._datafile = df;
    compaction._keepDeletions = (numAlive > 0 && i > 0);
    // TODO: verify that keepDeletions actually works with wrong numAlive stats

    try {
      toCompact.push_back(compaction);
    } catch (...) {
      // silently fail. either we had found something to compact or not
      // if not, then we can try again next time. if yes, then we'll simply
      // forget
      // about it and also try again next time
      break;
    }

    // we stop at the first few datafiles.
    // this is better than going over all datafiles in a collection in one go
    // because the compactor is single-threaded, and collecting all datafiles
    // might take a long time (it might even be that there is a request to
    // delete the collection in the middle of compaction, but the compactor
    // will not pick this up as it is read-locking the collection status)

    if (totalSize >= maxSize) {
      // result file will be big enough
      break;
    }

    if (totalSize >= COMPACTOR_MIN_SIZE &&
        toCompact.size() >= COMPACTOR_MAX_FILES) {
      // found enough files to compact
      break;
    }

    numAlive += (int64_t)dfi.numberAlive;
  }

  // can now continue without the lock
  TRI_READ_UNLOCK_DATAFILES_DOC_COLLECTION(document);

  if (toCompact.empty()) {
    // nothing to compact. now reset start index
    document->setNextCompactionStartIndex(0);

    // cleanup local variables
    document->setCompactionStatus(ReasonNothingToCompact);
    return false;
  }

  // handle datafiles with dead objects
  TRI_ASSERT(toCompact.size() >= 1);
  TRI_ASSERT(reason != nullptr);
  document->setCompactionStatus(reason);

  document->setNextCompactionStartIndex(start);
  CompactifyDatafiles(document, toCompact);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief try to write-lock the compaction
/// returns true if lock acquisition was successful. the caller is responsible
/// to free the write lock eventually
////////////////////////////////////////////////////////////////////////////////

static bool TryLockCompaction(TRI_vocbase_t* vocbase) {
  return TRI_TryWriteLockReadWriteLock(&vocbase->_compactionBlockers._lock);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief write-lock the compaction
////////////////////////////////////////////////////////////////////////////////

static void LockCompaction(TRI_vocbase_t* vocbase) {
  while (!TryLockCompaction(vocbase)) {
    // cycle until we have acquired the write-lock
    usleep(1000);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief write-unlock the compaction
////////////////////////////////////////////////////////////////////////////////

static void UnlockCompaction(TRI_vocbase_t* vocbase) {
  TRI_WriteUnlockReadWriteLock(&vocbase->_compactionBlockers._lock);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief atomic check and lock for running the compaction
/// if this function returns true, it has acquired a write-lock on the
/// compactionBlockers structure, which the caller must free eventually
////////////////////////////////////////////////////////////////////////////////

static bool CheckAndLockCompaction(TRI_vocbase_t* vocbase) {
  // check if we can acquire the write lock instantly
  if (!TryLockCompaction(vocbase)) {
    // couldn't acquire the write lock
    return false;
  }

  // we are now holding the write lock
  double now = TRI_microtime();

  // check if we have a still-valid compaction blocker
  size_t const n = TRI_LengthVector(&vocbase->_compactionBlockers._data);
  for (size_t i = 0; i < n; ++i) {
    compaction_blocker_t* blocker = static_cast<compaction_blocker_t*>(
        TRI_AtVector(&vocbase->_compactionBlockers._data, i));

    if (blocker->_expires > now) {
      // found a compaction blocker. unlock and return
      UnlockCompaction(vocbase);
      return false;
    }
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initialize the compaction blockers structure
////////////////////////////////////////////////////////////////////////////////

int TRI_InitCompactorVocBase(TRI_vocbase_t* vocbase) {
  TRI_InitReadWriteLock(&vocbase->_compactionBlockers._lock);
  TRI_InitVector(&vocbase->_compactionBlockers._data, TRI_UNKNOWN_MEM_ZONE,
                 sizeof(compaction_blocker_t));

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the compaction blockers structure
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyCompactorVocBase(TRI_vocbase_t* vocbase) {
  TRI_DestroyVector(&vocbase->_compactionBlockers._data);
  TRI_DestroyReadWriteLock(&vocbase->_compactionBlockers._lock);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief remove data of expired compaction blockers
////////////////////////////////////////////////////////////////////////////////

bool TRI_CleanupCompactorVocBase(TRI_vocbase_t* vocbase) {
  // check if we can instantly acquire the lock
  if (!TryLockCompaction(vocbase)) {
    // couldn't acquire lock
    return false;
  }

  // we are now holding the write lock
  double now = TRI_microtime();

  size_t n = TRI_LengthVector(&vocbase->_compactionBlockers._data);
  size_t i = 0;
  while (i < n) {
    compaction_blocker_t* blocker = static_cast<compaction_blocker_t*>(
        TRI_AtVector(&vocbase->_compactionBlockers._data, i));

    if (blocker->_expires < now) {
      TRI_RemoveVector(&vocbase->_compactionBlockers._data, i);
      n--;
    } else {
      i++;
    }
  }

  UnlockCompaction(vocbase);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief insert a compaction blocker
////////////////////////////////////////////////////////////////////////////////

int TRI_InsertBlockerCompactorVocBase(TRI_vocbase_t* vocbase, double lifetime,
                                      TRI_voc_tick_t* id) {
  if (lifetime <= 0.0) {
    return TRI_ERROR_BAD_PARAMETER;
  }

  compaction_blocker_t blocker;
  blocker._id = TRI_NewTickServer();
  blocker._expires = TRI_microtime() + lifetime;

  LockCompaction(vocbase);

  int res = TRI_PushBackVector(&vocbase->_compactionBlockers._data, &blocker);

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

int TRI_TouchBlockerCompactorVocBase(TRI_vocbase_t* vocbase, TRI_voc_tick_t id,
                                     double lifetime) {
  bool found = false;

  if (lifetime <= 0.0) {
    return TRI_ERROR_BAD_PARAMETER;
  }

  LockCompaction(vocbase);

  size_t const n = TRI_LengthVector(&vocbase->_compactionBlockers._data);

  for (size_t i = 0; i < n; ++i) {
    compaction_blocker_t* blocker = static_cast<compaction_blocker_t*>(
        TRI_AtVector(&vocbase->_compactionBlockers._data, i));

    if (blocker->_id == id) {
      blocker->_expires = TRI_microtime() + lifetime;
      found = true;
      break;
    }
  }

  UnlockCompaction(vocbase);

  if (!found) {
    return TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND;
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief atomically check-and-lock the compactor
/// if the function returns true, then a write-lock on the compactor was
/// acquired, which must eventually be freed by the caller
////////////////////////////////////////////////////////////////////////////////

bool TRI_CheckAndLockCompactorVocBase(TRI_vocbase_t* vocbase) {
  return TryLockCompaction(vocbase);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief unlock the compactor
////////////////////////////////////////////////////////////////////////////////

void TRI_UnlockCompactorVocBase(TRI_vocbase_t* vocbase) {
  UnlockCompaction(vocbase);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief remove an existing compaction blocker
////////////////////////////////////////////////////////////////////////////////

int TRI_RemoveBlockerCompactorVocBase(TRI_vocbase_t* vocbase,
                                      TRI_voc_tick_t id) {
  bool found = false;

  LockCompaction(vocbase);

  size_t const n = TRI_LengthVector(&vocbase->_compactionBlockers._data);

  for (size_t i = 0; i < n; ++i) {
    compaction_blocker_t* blocker = static_cast<compaction_blocker_t*>(
        TRI_AtVector(&vocbase->_compactionBlockers._data, i));

    if (blocker->_id == id) {
      TRI_RemoveVector(&vocbase->_compactionBlockers._data, i);
      found = true;
      break;
    }
  }

  UnlockCompaction(vocbase);

  if (!found) {
    return TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND;
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compactor event loop
////////////////////////////////////////////////////////////////////////////////

void TRI_CompactorVocBase(void* data) {
  TRI_vocbase_t* vocbase = static_cast<TRI_vocbase_t*>(data);

  int numCompacted = 0;
  TRI_ASSERT(vocbase->_state == 1);

  std::vector<TRI_vocbase_col_t*> collections;

  while (true) {
    // keep initial _state value as vocbase->_state might change during
    // compaction loop
    int state = vocbase->_state;

    // check if compaction is currently disallowed
    if (CheckAndLockCompaction(vocbase)) {
      // compaction is currently allowed
      double now = TRI_microtime();
      numCompacted = 0;

      try {
        READ_LOCKER(readLocker, vocbase->_collectionsLock);
        // copy all collections
        collections = vocbase->_collections;
      } catch (...) {
        collections.clear();
      }

      bool worked;

      for (auto& collection : collections) {
        {
          TRY_READ_LOCKER(readLocker, collection->_lock);

          if (!readLocker.isLocked()) {
            // if we can't acquire the read lock instantly, we continue directly
            // we don't want to stall here for too long
            continue;
          }

          TRI_document_collection_t* document = collection->_collection;

          if (document == nullptr) {
            continue;
          }

          worked = false;
          bool doCompact = document->_info.doCompact();

          // for document collection, compactify datafiles
          if (collection->_status == TRI_VOC_COL_STATUS_LOADED && doCompact) {
            // check whether someone else holds a read-lock on the compaction
            // lock
            if (!TRI_TryWriteLockReadWriteLock(&document->_compactionLock)) {
              // someone else is holding the compactor lock, we'll not compact
              continue;
            }

            // read-unlock the compaction lock later
            TRI_DEFER(TRI_WriteUnlockReadWriteLock(&document->_compactionLock));

            try {
              if (document->_lastCompaction + COMPACTOR_COLLECTION_INTERVAL <=
                  now) {
                auto ce = document->ditches()->createCompactionDitch(__FILE__,
                                                                     __LINE__);

                if (ce == nullptr) {
                  // out of memory
                  LOG_TOPIC(WARN, Logger::COMPACTOR) << "out of memory when trying to create compaction ditch";
                } else {
                  try {
                    worked = CompactifyDocumentCollection(document);

                    if (!worked) {
                      // set compaction stamp
                      document->_lastCompaction = now;
                    }
                    // if we worked, then we don't set the compaction stamp to
                    // force
                    // another round of compaction
                  } catch (...) {
                    LOG_TOPIC(ERR, Logger::COMPACTOR) << "an unknown exception occurred during compaction";
                    // in case an error occurs, we must still free this ditch
                  }

                  document->ditches()->freeDitch(ce);
                }
              }
            } catch (...) {
              // in case an error occurs, we must still relase the lock
              LOG_TOPIC(ERR, Logger::COMPACTOR) << "an unknown exception occurred during compaction";
            }
          }

        }  // end of lock

        if (worked) {
          ++numCompacted;

          // signal the cleanup thread that we worked and that it can now wake
          // up
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
    } else if (state != 2 && vocbase->_state == 1) {
      // only sleep while server is still running
      TRI_LockCondition(&vocbase->_compactorCondition);
      TRI_TimedWaitCondition(&vocbase->_compactorCondition,
                             (uint64_t)COMPACTOR_INTERVAL);
      TRI_UnlockCondition(&vocbase->_compactorCondition);
    }

    if (state == 2) {
      // server shutdown
      break;
    }
  }

  LOG_TOPIC(TRACE, Logger::COMPACTOR) << "shutting down compactor thread";
}

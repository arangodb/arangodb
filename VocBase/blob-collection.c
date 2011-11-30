////////////////////////////////////////////////////////////////////////////////
/// @brief blob collection
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

#include "blob-collection.h"

#include <Basics/conversions.h>
#include <Basics/files.h>
#include <Basics/logging.h>
#include <Basics/strings.h>

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a journal
////////////////////////////////////////////////////////////////////////////////

static bool CreateJournal (TRI_blob_collection_t* collection) {
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

  // check that a journal was created
  if (journal == NULL) {
    collection->base._lastError = TRI_set_errno(TRI_VOC_ERROR_NO_JOURNAL);
    collection->base._state = TRI_COL_STATE_WRITE_ERROR;

    LOG_ERROR("cannot create new journal '%s': %s", filename, TRI_last_error());

    TRI_FreeString(filename);
    return false;
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
    TRI_FreeDatafile(journal);

    LOG_ERROR("cannot create document header in journal '%s': %s",
              journal->_filename,
              TRI_last_error());

    return false;
  }

  // create a header
  memset(&cm, 0, sizeof(cm));

  cm.base._size = sizeof(TRI_col_header_marker_t);
  cm.base._type = TRI_COL_MARKER_HEADER;
  cm.base._tick = TRI_NewTickVocBase();

  cm._cid = collection->base._cid;

  TRI_FillCrcMarkerDatafile(&cm.base, sizeof(cm), 0, 0);
  ok = TRI_WriteElementDatafile(journal, position, &cm.base, sizeof(cm), 0, 0, true);

  if (! ok) {
    TRI_FreeDatafile(journal);

    LOG_ERROR("cannot create document header in journal '%s': %s",
              journal->_filename,
              TRI_last_error());

    return false;
  }

  // that's it
  TRI_PushBackVectorPointer(&collection->base._journals, journal);
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief closes a journal
////////////////////////////////////////////////////////////////////////////////

static bool CloseJournal (TRI_blob_collection_t* collection, TRI_datafile_t* journal) {
  bool ok;
  char* dname;
  char* filename;
  char* number;
  size_t i;
  size_t n;

  // remove datafile from list of journals
  n = collection->base._journals._length;

  for (i = 0;  i < n;  ++i) {
    TRI_datafile_t* df;

    df = collection->base._journals._buffer[i];

    if (journal == df) {
      break;
    }
  }

  if (i == n) {
    TRI_set_errno(TRI_VOC_ERROR_NO_JOURNAL);
    return false;
  }

  // seal and rename datafile
  ok = TRI_SealDatafile(journal);

  if (! ok) {
    collection->base._state = TRI_COL_STATE_WRITE_ERROR;
    return false;
  }

  number = TRI_StringUInt32(journal->_fid);
  dname = TRI_Concatenate3String("datafile-", number, ".db");
  filename = TRI_Concatenate2File(collection->base._directory, dname);
  TRI_FreeString(dname);
  TRI_FreeString(number);

  ok = TRI_RenameDatafile(journal, filename);

  if (! ok) {
    collection->base._state = TRI_COL_STATE_WRITE_ERROR;
    return false;
  }

  LOG_TRACE("closed journal '%s'", journal->_filename);

  TRI_RemoveVectorPointer(&collection->base._journals, i);
  TRI_PushBackVectorPointer(&collection->base._datafiles, journal);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief selects a journal
////////////////////////////////////////////////////////////////////////////////

static TRI_datafile_t* SelectJournal (TRI_blob_collection_t* collection,
                                      TRI_voc_size_t size,
                                      TRI_df_marker_t** result) {
  TRI_datafile_t* datafile;
  bool ok;
  size_t n;

  // need to create a new journal?
  n = collection->base._journals._length;

  if (n == 0) {
    ok = CreateJournal(collection);
    n = collection->base._journals._length;

    if (! ok || n == 0) {
      return NULL;
    }
  }

  // select first datafile
  datafile = collection->base._journals._buffer[0];

  // try to reserve space
  ok = TRI_ReserveElementDatafile(datafile, size, result);

  while (! ok && TRI_errno() == TRI_VOC_ERROR_DATAFILE_FULL) {
    ok = CloseJournal(collection, datafile);

    if (! ok) {
      return NULL;
    }

    ok = CreateJournal(collection);
    n = collection->base._journals._length;

    if (! ok || n == 0) {
      return NULL;
    }

    datafile = collection->base._journals._buffer[0];

    ok = TRI_ReserveElementDatafile(datafile, size, result);
  }

  if (! ok) {
    collection->base._state = TRI_COL_STATE_WRITE_ERROR;
    return NULL;
  }

  // we got enough space
  return datafile;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief writes an element to a given position
////////////////////////////////////////////////////////////////////////////////

static bool WriteElement (TRI_blob_collection_t* collection,
                          TRI_datafile_t* journal,
                          TRI_df_marker_t* position,
                          TRI_df_marker_t* marker,
                          TRI_voc_size_t markerSize,
                          void const* body,
                          size_t bodySize) {
  bool ok;

  ok = TRI_WriteElementDatafile(journal, position, marker, markerSize, body, bodySize, true);

  if (! ok) {
    collection->base._state = TRI_COL_STATE_WRITE_ERROR;
  }

  return ok;
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
/// @brief creates a new collection
////////////////////////////////////////////////////////////////////////////////

TRI_blob_collection_t* TRI_CreateBlobCollection (char const* path, TRI_col_parameter_t* parameter) {
  TRI_col_info_t info;
  TRI_blob_collection_t* blob;
  TRI_collection_t* collection;

  blob = TRI_Allocate(sizeof(TRI_blob_collection_t));

  if (blob == NULL) {
    return NULL;
  }

  memset(&info, 0, sizeof(info));
  info._version = TRI_COL_VERSION;
  info._type = TRI_COL_TYPE_BLOB;
  info._cid = TRI_NewTickVocBase();
  TRI_CopyString(info._name, parameter->_name, sizeof(info._name));
  info._maximalSize = parameter->_maximalSize;
  info._size = sizeof(TRI_col_info_t);

  collection = TRI_CreateCollection(&blob->base, path, &info);

  if (collection == NULL) {
    TRI_Free(blob);
    return NULL;
  }

  TRI_InitMutex(&blob->_lock);

  return blob;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the memory allocated, but does not free the pointer
///
/// Note that the collection must be closed first.
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyBlobCollection (TRI_blob_collection_t* collection) {
  TRI_DestroyMutex(&collection->_lock);
  TRI_DestroyCollection(&collection->base);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the memory allocated and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeBlobCollection (TRI_blob_collection_t* collection) {
  TRI_DestroyBlobCollection(collection);
  TRI_Free(collection);
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
/// @brief writes an element splitted into marker and body to file
////////////////////////////////////////////////////////////////////////////////

bool TRI_WriteBlobCollection (TRI_blob_collection_t* collection,
                              TRI_df_marker_t* marker,
                              TRI_voc_size_t markerSize,
                              void const* body,
                              TRI_voc_size_t bodySize,
                              TRI_df_marker_t** result) {
  TRI_datafile_t* journal;
  bool ok;

  // generate a new tick
  marker->_tick = TRI_NewTickVocBase();

  // generate crc
  TRI_FillCrcMarkerDatafile(marker, markerSize, body, bodySize);

  // lock the collection
  TRI_LockMutex(&collection->_lock);

  if (collection->base._state != TRI_COL_STATE_WRITE) {
    if (collection->base._state == TRI_COL_STATE_READ) {
      TRI_UnlockMutex(&collection->_lock);
      return TRI_VOC_ERROR_READ_ONLY;
    }

    TRI_UnlockMutex(&collection->_lock);
    return TRI_VOC_ERROR_ILLEGAL_STATE;
  }

  // find and select a journal
  journal = SelectJournal(collection, markerSize + bodySize, result);

  if (journal == NULL) {
    TRI_UnlockMutex(&collection->_lock);
    return TRI_VOC_ERROR_NO_JOURNAL;
  }

  // and write marker and blob
  ok = WriteElement(collection, journal, *result, marker, markerSize, body, bodySize);

  // release lock on collection
  TRI_UnlockMutex(&collection->_lock);

  return ok;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief opens an existing collection
////////////////////////////////////////////////////////////////////////////////

TRI_blob_collection_t* TRI_OpenBlobCollection (char const* path) {
  TRI_blob_collection_t* blob;
  TRI_collection_t* collection;

  blob = TRI_Allocate(sizeof(TRI_blob_collection_t));

  if (blob == NULL) {
    return NULL;
  }

  collection = TRI_OpenCollection(&blob->base, path);

  if (collection == NULL) {
    TRI_Free(blob);
    return NULL;
  }

  TRI_InitMutex(&blob->_lock);

  return blob;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief closes a collection
////////////////////////////////////////////////////////////////////////////////

bool TRI_CloseBlobCollection (TRI_blob_collection_t* collection) {
  return TRI_CloseCollection(&collection->base);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\)"
// End:

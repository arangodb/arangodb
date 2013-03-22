////////////////////////////////////////////////////////////////////////////////
/// @brief (binary) shape collection
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

#include "shape-collection.h"

#include "BasicsC/conversions.h"
#include "BasicsC/files.h"
#include "BasicsC/logging.h"
#include "BasicsC/strings.h"

#include "VocBase/marker.h"

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

static bool CreateJournal (TRI_shape_collection_t* collection) {
  TRI_col_header_marker_t cm;
  TRI_datafile_t* journal;
  TRI_df_marker_t* position;
  TRI_voc_fid_t fid;
  char* filename;
  int res;
  bool isVolatile;

  fid = TRI_NewTickVocBase();
  isVolatile = collection->base._info._isVolatile;

  if (isVolatile) {
    // in memory collection
    filename = NULL;
  }
  else {
    char* jname;
    char* number;

    number = TRI_StringUInt64(fid);
    if (number == NULL) {
      return false;
    }

    jname = TRI_Concatenate3String("temp-", number, ".db");
    TRI_FreeString(TRI_CORE_MEM_ZONE, number);

    filename = TRI_Concatenate2File(collection->base._directory, jname);
    TRI_FreeString(TRI_CORE_MEM_ZONE, jname);
  }

  journal = TRI_CreateDatafile(filename, fid, collection->base._info._maximalSize);

  if (filename != NULL) {
    TRI_FreeString(TRI_CORE_MEM_ZONE, filename);
  }

  // check that a journal was created
  if (journal == NULL) {
    if (TRI_errno() == TRI_ERROR_OUT_OF_MEMORY_MMAP) {
      collection->base._lastError = TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY_MMAP);
      collection->base._state = TRI_COL_STATE_READ;
    }
    else {
      collection->base._lastError = TRI_set_errno(TRI_ERROR_ARANGO_NO_JOURNAL);
      collection->base._state = TRI_COL_STATE_WRITE_ERROR;
    }

    return false;
  }

  LOG_TRACE("created a new shape journal '%s'", journal->getName(journal));

  TRI_ASSERT_DEBUG(fid == journal->_fid);

  if (journal->isPhysical(journal)) {
    char* jname;
    char* number;
    bool ok;

    // and use the correct name
    number = TRI_StringUInt64(fid);
    jname = TRI_Concatenate3String("journal-", number, ".db");
    filename = TRI_Concatenate2File(collection->base._directory, jname);

    TRI_FreeString(TRI_CORE_MEM_ZONE, number);
    TRI_FreeString(TRI_CORE_MEM_ZONE, jname);

    ok = TRI_RenameDatafile(journal, filename);

    if (! ok) {
      LOG_FATAL_AND_EXIT("failed to rename the journal to '%s': %s", filename, TRI_last_error());
      TRI_FreeDatafile(journal);
      TRI_FreeString(TRI_CORE_MEM_ZONE, filename);

      return false;
    }
    LOG_TRACE("renamed journal to '%s'", filename);

    TRI_FreeString(TRI_CORE_MEM_ZONE, filename);
  }


  // create a collection header
  res = TRI_ReserveElementDatafile(journal, sizeof(TRI_col_header_marker_t), &position);

  if (res != TRI_ERROR_NO_ERROR) {
    LOG_ERROR("cannot create document header in journal '%s': %s",
              journal->getName(journal),
              TRI_last_error());

    TRI_FreeDatafile(journal);

    return false;
  }

  // create the header marker
  TRI_InitMarker(&cm.base, TRI_COL_MARKER_HEADER, sizeof(TRI_col_header_marker_t), TRI_NewTickVocBase());

  cm._cid  = collection->base._info._cid;
  cm._type = TRI_COL_TYPE_SHAPE;

  // on journal creation, always use waitForSync = true
  res = TRI_WriteCrcElementDatafile(journal, position, &cm.base, sizeof(cm), true);

  if (res != TRI_ERROR_NO_ERROR) {
    LOG_ERROR("cannot create document header in journal '%s': %s",
              journal->getName(journal),
              TRI_last_error());

    TRI_FreeDatafile(journal);

    return false;
  }

  TRI_PushBackVectorPointer(&collection->base._journals, journal);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief closes a journal
////////////////////////////////////////////////////////////////////////////////

static bool CloseJournal (TRI_shape_collection_t* collection, TRI_datafile_t* journal) {
  int res;
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
    TRI_set_errno(TRI_ERROR_ARANGO_NO_JOURNAL);
    return false;
  }

  // seal datafile
  res = TRI_SealDatafile(journal);

  if (res != TRI_ERROR_NO_ERROR) {
    collection->base._state = TRI_COL_STATE_WRITE_ERROR;
    return false;
  }

  // rename datafile
  if (journal->isPhysical(journal)) {
    char* dname;
    char* filename;
    char* number;
    bool ok;

    number = TRI_StringUInt64(journal->_fid);
    dname = TRI_Concatenate3String("datafile-", number, ".db");
    filename = TRI_Concatenate2File(collection->base._directory, dname);

    TRI_FreeString(TRI_CORE_MEM_ZONE, dname);
    TRI_FreeString(TRI_CORE_MEM_ZONE, number);

    ok = TRI_RenameDatafile(journal, filename);
    TRI_FreeString(TRI_CORE_MEM_ZONE, filename);

    if (! ok) {
      collection->base._state = TRI_COL_STATE_WRITE_ERROR;
      return false;
    }
  }

  LOG_TRACE("closed journal '%s'", journal->getName(journal));

  TRI_RemoveVectorPointer(&collection->base._journals, i);
  TRI_PushBackVectorPointer(&collection->base._datafiles, journal);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief selects a journal
////////////////////////////////////////////////////////////////////////////////

static TRI_datafile_t* SelectJournal (TRI_shape_collection_t* collection,
                                      TRI_voc_size_t size,
                                      TRI_df_marker_t** result) {
  TRI_datafile_t* datafile;
  bool ok;
  int res;
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
  res = TRI_ReserveElementDatafile(datafile, size, result);

  while (res == TRI_ERROR_ARANGO_DATAFILE_FULL) {
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

    res = TRI_ReserveElementDatafile(datafile, size, result);
  }

  if (res != TRI_ERROR_NO_ERROR) {
    collection->base._state = TRI_COL_STATE_WRITE_ERROR;
    return NULL;
  }

  // we got enough space
  return datafile;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief writes an element to a given position
////////////////////////////////////////////////////////////////////////////////

static int WriteElement (TRI_shape_collection_t* collection,
                         TRI_datafile_t* journal,
                         TRI_df_marker_t* position,
                         TRI_df_marker_t* marker,
                         TRI_voc_size_t markerSize) {
  int res;
  bool waitForSync;

  if (marker->_type == TRI_DF_MARKER_SHAPE) {
    // this is a shape. we will honor the collection's waitForSync attribute
    // which is determined by the global forceSyncShape flag and the actual collection's
    // waitForSync flag
    waitForSync = collection->base._info._waitForSync;
  }
  else {
    // this is an attribute. we will not sync the data now, but when the shape information
    // containing the attribute is written (that means we defer the sync until the TRI_DF_MARKER_SHAPE
    // marker is written)
    waitForSync = false;
  }

  res = TRI_WriteCrcElementDatafile(journal, position, marker, markerSize, waitForSync);

  if (res != TRI_ERROR_NO_ERROR) {
    collection->base._state = TRI_COL_STATE_WRITE_ERROR;
  }

  return res;
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

TRI_shape_collection_t* TRI_CreateShapeCollection (TRI_vocbase_t* vocbase,
                                                   char const* path,
                                                   TRI_col_info_t* parameter) {
  TRI_shape_collection_t* shape;
  TRI_collection_t* collection;

  shape = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_shape_collection_t), false);

  if (shape == NULL) {
    return NULL;
  }

  parameter->_type = TRI_COL_TYPE_SHAPE;
  parameter->_cid  = TRI_NewTickVocBase();
  parameter->_waitForSync = (vocbase->_forceSyncShapes || parameter->_waitForSync);

  collection = TRI_CreateCollection(vocbase, &shape->base, path, parameter);

  if (collection == NULL) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, shape);
    return NULL;
  }

  TRI_InitMutex(&shape->_lock);

  return shape;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the memory allocated, but does not free the pointer
///
/// Note that the collection must be closed first.
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyShapeCollection (TRI_shape_collection_t* collection) {
  assert(collection);

  TRI_DestroyMutex(&collection->_lock);
  TRI_DestroyCollection(&collection->base);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the memory allocated and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeShapeCollection (TRI_shape_collection_t* collection) {
  assert(collection);

  TRI_DestroyShapeCollection(collection);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, collection);
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

int TRI_WriteShapeCollection (TRI_shape_collection_t* collection,
                              TRI_df_marker_t* marker,
                              TRI_voc_size_t markerSize,
                              TRI_df_marker_t** result) {
  TRI_datafile_t* journal;
  int res;

  // lock the collection
  TRI_LockMutex(&collection->_lock);

  if (collection->base._state != TRI_COL_STATE_WRITE) {
    if (collection->base._state == TRI_COL_STATE_READ) {
      TRI_UnlockMutex(&collection->_lock);

      return TRI_ERROR_ARANGO_READ_ONLY;
    }

    TRI_UnlockMutex(&collection->_lock);
    return TRI_ERROR_ARANGO_ILLEGAL_STATE;
  }

  // find and select a journal
  journal = SelectJournal(collection, markerSize, result);

  if (journal == NULL) {
    TRI_UnlockMutex(&collection->_lock);

    return TRI_ERROR_ARANGO_NO_JOURNAL;
  }

  // write marker and shape
  res = WriteElement(collection, journal, *result, marker, markerSize);

  // release lock on collection
  TRI_UnlockMutex(&collection->_lock);

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief opens an existing collection
////////////////////////////////////////////////////////////////////////////////

TRI_shape_collection_t* TRI_OpenShapeCollection (TRI_vocbase_t* vocbase,
                                                 char const* path) {
  TRI_shape_collection_t* shape;
  TRI_collection_t* collection;

  assert(path != NULL);

  shape = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_shape_collection_t), false);

  if (shape == NULL) {
    return NULL;
  }

  collection = TRI_OpenCollection(vocbase, &shape->base, path);

  if (collection == NULL) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, shape);
    return NULL;
  }

  TRI_InitMutex(&shape->_lock);

  return shape;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief closes a collection
////////////////////////////////////////////////////////////////////////////////

bool TRI_CloseShapeCollection (TRI_shape_collection_t* collection) {
  return TRI_CloseCollection(&collection->base);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:

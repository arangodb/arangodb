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
#include "BasicsC/tri-strings.h"

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

static int CreateJournal (TRI_shape_collection_t* collection,
                          TRI_voc_size_t size) {
  TRI_col_header_marker_t cm;
  TRI_datafile_t* journal;
  TRI_df_marker_t* position;
  TRI_voc_tick_t tick;
  char* filename;
  int res;
  bool isVolatile;
  
  // sanity check for minimum marker size
  if (size >= (TRI_voc_size_t) TRI_MARKER_MAXIMAL_SIZE) {
    LOG_ERROR("too big journal requested for shape-collection. "
              "requested size: %llu. "
              "maximum allowed size: %llu.", 
              (unsigned long long) size,
              (unsigned long long) TRI_MARKER_MAXIMAL_SIZE);

    return TRI_ERROR_ARANGO_DOCUMENT_TOO_LARGE;
  }

  tick = (TRI_voc_tick_t) TRI_NewTickVocBase();
  isVolatile = collection->base._info._isVolatile;

  if (isVolatile) {
    // in memory collection
    filename = NULL;
  }
  else {
    char* jname;
    char* number;

    number = TRI_StringUInt64(tick);
    if (number == NULL) {
      return TRI_ERROR_OUT_OF_MEMORY;
    }

    jname = TRI_Concatenate3String("temp-", number, ".db");
    TRI_FreeString(TRI_CORE_MEM_ZONE, number);

    filename = TRI_Concatenate2File(collection->base._directory, jname);
    TRI_FreeString(TRI_CORE_MEM_ZONE, jname);
  }

  journal = TRI_CreateDatafile(filename, (TRI_voc_fid_t) tick, size);

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

    return collection->base._lastError;
  }

  LOG_TRACE("created a new shape journal '%s'", journal->getName(journal));

  assert((TRI_voc_fid_t) tick == journal->_fid);

  if (journal->isPhysical(journal)) {
    char* jname;
    char* number;
    bool ok;

    // and use the correct name
    number = TRI_StringUInt64(tick);
    jname = TRI_Concatenate3String("journal-", number, ".db");
    filename = TRI_Concatenate2File(collection->base._directory, jname);

    TRI_FreeString(TRI_CORE_MEM_ZONE, number);
    TRI_FreeString(TRI_CORE_MEM_ZONE, jname);

    ok = TRI_RenameDatafile(journal, filename);

    if (! ok) {
      LOG_FATAL_AND_EXIT("failed to rename the journal to '%s': %s", filename, TRI_last_error());
      TRI_FreeDatafile(journal);
      TRI_FreeString(TRI_CORE_MEM_ZONE, filename);

      return journal->_lastError;
    }
    LOG_TRACE("renamed journal to '%s'", filename);

    TRI_FreeString(TRI_CORE_MEM_ZONE, filename);
  }


  // create a collection header
  res = TRI_ReserveElementDatafile(journal, sizeof(TRI_col_header_marker_t), &position, TRI_SHAPER_DATAFILE_SIZE);

  if (res != TRI_ERROR_NO_ERROR) {
    LOG_ERROR("cannot create document header in journal '%s': %s",
              journal->getName(journal),
              TRI_last_error());

    TRI_FreeDatafile(journal);

    return res;
  }

  // create the header marker
  TRI_InitMarker(&cm.base, TRI_COL_MARKER_HEADER, sizeof(TRI_col_header_marker_t));
  cm.base._tick = tick;
  cm._cid  = collection->base._info._cid;
  cm._type = TRI_COL_TYPE_SHAPE;

  // on journal creation, always use waitForSync = false, as there will
  // always be basic shapes inserted directly afterwards
  res = TRI_WriteCrcElementDatafile(journal, position, &cm.base, sizeof(cm), false);

  if (res != TRI_ERROR_NO_ERROR) {
    LOG_ERROR("cannot create document header in journal '%s': %s",
              journal->getName(journal),
              TRI_last_error());

    TRI_FreeDatafile(journal);

    return res;
  }

  TRI_PushBackVectorPointer(&collection->base._journals, journal);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief closes a journal
////////////////////////////////////////////////////////////////////////////////

static int CloseJournal (TRI_shape_collection_t* collection, 
                         TRI_datafile_t* journal) {
  size_t i, n;
  int res;

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
    return TRI_set_errno(TRI_ERROR_ARANGO_NO_JOURNAL);
  }

  // seal datafile
  res = TRI_SealDatafile(journal);

  if (res != TRI_ERROR_NO_ERROR) {
    collection->base._state = TRI_COL_STATE_WRITE_ERROR;
    return res;
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
      return journal->_lastError;
    }
  }

  LOG_TRACE("closed journal '%s'", journal->getName(journal));

  TRI_RemoveVectorPointer(&collection->base._journals, i);
  TRI_PushBackVectorPointer(&collection->base._datafiles, journal);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief selects a journal
////////////////////////////////////////////////////////////////////////////////

static int SelectJournal (TRI_shape_collection_t* collection,
                          TRI_voc_size_t size,
                          TRI_datafile_t** journal,
                          TRI_df_marker_t** result) {
  TRI_datafile_t* datafile;
  TRI_voc_size_t targetSize;
  size_t n;
  int res;

  *journal = NULL;

  // calculate the journal size
  // we'll use the default size first, and double it until the requested size fits in
  targetSize = collection->base._info._maximalSize;
  // add some overhead for datafile header/footer
  while (size + 2048 > targetSize) {
    targetSize *= 2;
  }

  assert(targetSize >= size);

  // need to create a new journal?
  n = collection->base._journals._length;

  if (n == 0) {
    int res2;

    res2 = CreateJournal(collection, targetSize);
    if (res2 != TRI_ERROR_NO_ERROR) {
      return res2;
    }

    n = collection->base._journals._length;
    if (n == 0) {
      return TRI_ERROR_INTERNAL;
    }
  }

  // select first datafile
  datafile = collection->base._journals._buffer[0];

  // try to reserve space
  res = TRI_ReserveElementDatafile(datafile, size, result, targetSize);

  while (res == TRI_ERROR_ARANGO_DATAFILE_FULL) {
    int res2;

    res2 = CloseJournal(collection, datafile);
    if (res2 != TRI_ERROR_NO_ERROR) {
      return res2;
     }

    res2 = CreateJournal(collection, targetSize);
    if (res2 != TRI_ERROR_NO_ERROR) {
      return res2;
    }

    n = collection->base._journals._length;
    if (n == 0) {
      return TRI_ERROR_INTERNAL;
    }

    datafile = collection->base._journals._buffer[0];

    res = TRI_ReserveElementDatafile(datafile, size, result, targetSize);
  }

  if (res != TRI_ERROR_NO_ERROR) {
    // check if we can reject just the one too large document, 
    // but do not need to render the complete collection unusable
    if (res != TRI_ERROR_ARANGO_DOCUMENT_TOO_LARGE) {
      collection->base._state = TRI_COL_STATE_WRITE_ERROR;
    }

    return res;
  }

  // we got enough space
  *journal = datafile;

  return TRI_ERROR_NO_ERROR;
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

  if (marker->_type == TRI_DF_MARKER_SHAPE && collection->_initialised) {
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
  shape->_initialised = false;

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

  assert(marker->_size == markerSize);

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
  journal = NULL;
  res = SelectJournal(collection, markerSize, &journal, result);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_UnlockMutex(&collection->_lock);

    return res;
  }

  assert(journal != NULL);

  // write marker and shape
  res = WriteElement(collection, journal, *result, marker, markerSize);

  if (res == TRI_ERROR_NO_ERROR) {
    journal->_written = ((char*) *result) + marker->_size;
  }

  // release lock on collection
  TRI_UnlockMutex(&collection->_lock);

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief syncs the active journal of a shape collection
////////////////////////////////////////////////////////////////////////////////

int TRI_SyncShapeCollection (TRI_shape_collection_t* collection) {
  int res;

  // lock the collection
  TRI_LockMutex(&collection->_lock);

  res = TRI_SyncCollection(&collection->base);
  
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

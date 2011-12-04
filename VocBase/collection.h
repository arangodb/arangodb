////////////////////////////////////////////////////////////////////////////////
/// @brief collections
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

#ifndef TRIAGENS_VOC_BASE_COLLECTION_H
#define TRIAGENS_VOC_BASE_COLLECTION_H 1

#include <Basics/Common.h>

#include <Basics/vector.h>

#include <VocBase/datafile.h>
#include <VocBase/vocbase.h>

#ifdef __cplusplus
extern "C" {
#endif

////////////////////////////////////////////////////////////////////////////////
/// @page DurhamCollections Collections
///
/// Data is stored in datafiles. A set of datafiles forms a collection. A
/// datafile can be read-only and sealed or read-write. All datafiles of a
/// collections are stored in a directory. This directory contains the following
/// files:
///
/// - parameter.json: The parameter of a collection.
///
/// - datafile-NNN.db: A read-only datafile. The number NNN is the datafile
///     identifier, see @ref TRI_datafile_t.
///
/// - journal-NNN.db: A read-write datafile used as journal. All new entries
///     of a collection are appended to a journal. The number NNN is the
///     datafile identifier, see @ref TRI_datafile_t.
///
/// - index-NNN.json: An index description. The number NNN is the index
///     identifier, see @ref TRI_index_t.
///
/// The structure @ref TRI_collection_t is abstract. Currently, there are
/// two concrete sub-classes @ref TRI_sim_collection_t and
/// @ref TRI_blob_collection_t.
///
/// @section BlobCollection Blob Collection
///
/// @copydetails TRI_blob_collection_t
///
/// @section DocCollection Document Collection
///
/// @copydetails TRI_doc_collection_t
///
/// @section SimCollection Simple Document Collection
///
/// @copydetails TRI_sim_collection_t
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public constants
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief collection version
////////////////////////////////////////////////////////////////////////////////

#define TRI_COL_VERSION         (1)

////////////////////////////////////////////////////////////////////////////////
/// @brief maximal path length
////////////////////////////////////////////////////////////////////////////////

#define TRI_COL_PARAMETER_FILE  "parameter.json"

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief state of the datafile
////////////////////////////////////////////////////////////////////////////////

typedef enum {
  TRI_COL_STATE_CLOSED = 1,        // collection is closed
  TRI_COL_STATE_READ = 2,          // collection is opened read only
  TRI_COL_STATE_WRITE = 3,         // collection is opened read/append
  TRI_COL_STATE_OPEN_ERROR = 4,    // an error has occurred while opening
  TRI_COL_STATE_WRITE_ERROR = 5    // an error has occurred while writing
}
TRI_col_state_e;

////////////////////////////////////////////////////////////////////////////////
/// @brief collection version
////////////////////////////////////////////////////////////////////////////////

typedef uint32_t TRI_col_version_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief collection type
////////////////////////////////////////////////////////////////////////////////

typedef enum {
  TRI_COL_TYPE_BLOB = 1,
  TRI_COL_TYPE_SIMPLE_DOCUMENT = 2
}
TRI_col_type_e;

////////////////////////////////////////////////////////////////////////////////
/// @brief document datafile header marker
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_col_header_marker_s {
  TRI_df_marker_t base;

  TRI_col_type_t _type;
  TRI_voc_cid_t _cid;
}
TRI_col_header_marker_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief collection parameter for create
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_col_parameter_s {
  TRI_col_type_e _type;              // collection type

  char _name[TRI_COL_PATH_LENGTH];   // name of the collection
  TRI_voc_size_t _maximalSize;       // maximal size of memory mapped file

  TRI_voc_size_t _syncAfterObjects;  // 0 = ignore, 1 = always, n = at most n non-synced
  TRI_voc_size_t _syncAfterBytes;    // 0 = ignore, n = at most n bytes
  TRI_voc_ms_t _syncAfterTime;       // 0 = ignore, n = at most n milli-seconds
}
TRI_col_parameter_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief collection info block saved to disk as json
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_col_info_s {
  TRI_col_version_t _version;        // collection version
  TRI_col_type_t _type;              // collection type
  TRI_voc_cid_t _cid;                // collection identifier

  char _name[TRI_COL_PATH_LENGTH];   // name of the collection
  TRI_voc_size_t _maximalSize;       // maximal size of memory mapped file
  TRI_voc_size_t _syncAfterObjects;  // 0 = ignore, 1 = always, n = at most n non-synced
  TRI_voc_size_t _syncAfterBytes;    // 0 = ignore, n = at most n bytes
  double _syncAfterTime;             // 0 = ignore, n = at most n seconds

  TRI_voc_size_t _size;              // total size of the parameter info block
}
TRI_col_info_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief collection
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_collection_s {
  TRI_col_version_t _version;        // collection version, will be set
  TRI_col_type_t _type;              // collection type, will be set

  TRI_col_state_e _state;            // state of the collection
  int _lastError;                    // last (critical) error

  TRI_voc_cid_t _cid;                // collection identifier, will we generated
  char _name[TRI_COL_PATH_LENGTH];   // name of the collection

  TRI_voc_size_t _maximalSize;       // maximal size of memory mapped file
  TRI_voc_size_t _syncAfterObjects;  // 0 = ignore, 1 = always, n = at most n non-synced
  TRI_voc_size_t _syncAfterBytes;    // 0 = ignore, n = at most n bytes
  double _syncAfterTime;             // 0 = ignore, n = at most n seconds

  char* _directory;                  // directory of the collection

  TRI_vector_pointer_t _datafiles;   // all datafiles
  TRI_vector_pointer_t _journals;    // all journals
  TRI_vector_pointer_t _compactors;  // all compactor files
  TRI_vector_string_t _indexFiles;   // all index filenames
}
TRI_collection_t;

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
/// @brief initializes a collection parameter block
////////////////////////////////////////////////////////////////////////////////

void TRI_InitParameterCollection (TRI_col_parameter_t*,
                                  char const* name,
                                  TRI_voc_size_t maximalSize);

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new collection
////////////////////////////////////////////////////////////////////////////////

TRI_collection_t* TRI_CreateCollection (TRI_collection_t*,
                                        char const* path,
                                        TRI_col_info_t* parameter);

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the memory allocated, but does not free the pointer
///
/// Note that the collection must be closed first.
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyCollection (TRI_collection_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the memory allocated and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeCollection (TRI_collection_t*);

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
/// @brief creates a parameter info block from file
////////////////////////////////////////////////////////////////////////////////

bool TRI_LoadParameterInfo (char const* filename,
                            TRI_col_info_t* parameter);

////////////////////////////////////////////////////////////////////////////////
/// @brief saves a parameter info block to file
////////////////////////////////////////////////////////////////////////////////

bool TRI_SaveParameterInfo (char const* filename,
                            TRI_col_info_t* parameter);

////////////////////////////////////////////////////////////////////////////////
/// @brief updates the parameter info block
////////////////////////////////////////////////////////////////////////////////

bool TRI_UpdateParameterInfoCollection (TRI_collection_t*);

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
/// @brief iterates over a collection
////////////////////////////////////////////////////////////////////////////////

bool TRI_IterateCollection (TRI_collection_t*,
                            bool (*iterator)(TRI_df_marker_t const*, void*, TRI_datafile_t*, bool),
                            void* data);

////////////////////////////////////////////////////////////////////////////////
/// @brief iterates over all index files of a collection
////////////////////////////////////////////////////////////////////////////////

void TRI_IterateIndexCollection (TRI_collection_t* collection,
                                 void (*iterator)(char const* filename, void*),
                                 void* data);

////////////////////////////////////////////////////////////////////////////////
/// @brief opens an existing collection
////////////////////////////////////////////////////////////////////////////////

TRI_collection_t* TRI_OpenCollection (TRI_collection_t*,
                                      char const* path);

////////////////////////////////////////////////////////////////////////////////
/// @brief closes an open collection
////////////////////////////////////////////////////////////////////////////////

bool TRI_CloseCollection (TRI_collection_t*);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:

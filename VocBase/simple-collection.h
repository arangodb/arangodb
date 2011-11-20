////////////////////////////////////////////////////////////////////////////////
/// @brief document collection with global read-write lock
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

#ifndef TRIAGENS_DURHAM_VOCBASE_SIMPLE_COLLECTION_H
#define TRIAGENS_DURHAM_VOCBASE_SIMPLE_COLLECTION_H 1

#include <VocBase/document-collection.h>

#include <VocBase/headers.h>
#include <VocBase/index.h>

#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief document collection with global read-write lock
///
/// A simple document collection is a document collection with a single
/// read-write lock. This lock is used to coordinate the read and write
/// transactions.
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_sim_collection_s {
  TRI_doc_collection_t base;

  // .............................................................................
  // this lock protects the _next pointer, _headers, _indexes, and _primaryIndex
  // .............................................................................

  TRI_read_write_lock_t _lock;

  TRI_headers_t* _headers;
  TRI_associative_pointer_t _primaryIndex;
  TRI_vector_pointer_t _indexes;
  struct TRI_geo_index_s* _geoIndex;

  // .............................................................................
  // this condition variable protects the _journals
  // .............................................................................

  TRI_condition_t _journalsCondition;
}
TRI_sim_collection_t;

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

TRI_sim_collection_t* TRI_CreateSimCollection (char const* path,
                                               TRI_col_parameter_t* parameter);

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the memory allocated, but does not free the pointer
///
/// Note that the collection must be closed first.
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroySimCollection (TRI_sim_collection_t* collection);

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the memory allocated and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeSimCollection (TRI_sim_collection_t* collection);

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
/// @brief creates a new journal
////////////////////////////////////////////////////////////////////////////////

TRI_datafile_t* TRI_CreateJournalSimCollection (TRI_sim_collection_t* collection);

////////////////////////////////////////////////////////////////////////////////
/// @brief closes an existing journal
////////////////////////////////////////////////////////////////////////////////

bool TRI_CloseJournalSimCollection (TRI_sim_collection_t* collection,
                                    size_t position);

////////////////////////////////////////////////////////////////////////////////
/// @brief opens an existing collection
////////////////////////////////////////////////////////////////////////////////

TRI_sim_collection_t* TRI_OpenSimCollection (char const* path);

////////////////////////////////////////////////////////////////////////////////
/// @brief closes an open collection
////////////////////////////////////////////////////////////////////////////////

bool TRI_CloseSimCollection (TRI_sim_collection_t* collection);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                           INDEXES
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a description of all indexes
////////////////////////////////////////////////////////////////////////////////

TRI_vector_pointer_t* TRI_IndexesSimCollection (TRI_sim_collection_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief drops an index
////////////////////////////////////////////////////////////////////////////////

bool TRI_DropIndexSimCollection (TRI_sim_collection_t* collection, TRI_idx_iid_t iid);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                       GEO INDEXES
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief finds a geo index
///
/// Note that the caller must hold at least a read-lock.
////////////////////////////////////////////////////////////////////////////////

struct TRI_index_s* TRI_LookupGeoIndexSimCollection (TRI_sim_collection_t* collection,
                                                     TRI_shape_pid_t location,
                                                     bool geoJson);

////////////////////////////////////////////////////////////////////////////////
/// @brief finds a geo index
///
/// Note that the caller must hold at least a read-lock.
////////////////////////////////////////////////////////////////////////////////

struct TRI_index_s* TRI_LookupGeoIndex2SimCollection (TRI_sim_collection_t* collection,
                                                      TRI_shape_pid_t latitude,
                                                      TRI_shape_pid_t longitude);

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures that a geo index exists
////////////////////////////////////////////////////////////////////////////////

TRI_idx_iid_t TRI_EnsureGeoIndexSimCollection (TRI_sim_collection_t* collection,
                                               char const* location,
                                               bool geoJson);

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a geo index to a collection
////////////////////////////////////////////////////////////////////////////////

TRI_idx_iid_t TRI_EnsureGeoIndex2SimCollection (TRI_sim_collection_t* collection,
                                                char const* latitude,
                                                char const* longitude);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\)"
// End:

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

#ifndef TRIAGENS_DURHAM_VOC_BASE_SIMPLE_COLLECTION_H
#define TRIAGENS_DURHAM_VOC_BASE_SIMPLE_COLLECTION_H 1

#include <VocBase/document-collection.h>

#include <BasicsC/associative-multi.h>

#include <VocBase/headers.h>
#include <VocBase/index.h>

#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                 SIMPLE COLLECTION
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                     public macros
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief read locks the journal files and the parameter file
////////////////////////////////////////////////////////////////////////////////

#define TRI_READ_LOCK_DATAFILES_SIM_COLLECTION(a) \
  TRI_ReadLockReadWriteLock(&(a)->_lock)

////////////////////////////////////////////////////////////////////////////////
/// @brief read unlocks the journal files and the parameter file
////////////////////////////////////////////////////////////////////////////////

#define TRI_READ_UNLOCK_DATAFILES_SIM_COLLECTION(a) \
  TRI_ReadUnlockReadWriteLock(&(a)->_lock)

////////////////////////////////////////////////////////////////////////////////
/// @brief write locks the journal files and the parameter file
////////////////////////////////////////////////////////////////////////////////

#define TRI_WRITE_LOCK_DATAFILES_SIM_COLLECTION(a) \
  TRI_WriteLockReadWriteLock(&(a)->_lock)

////////////////////////////////////////////////////////////////////////////////
/// @brief write unlocks the journal files and the parameter file
////////////////////////////////////////////////////////////////////////////////

#define TRI_WRITE_UNLOCK_DATAFILES_SIM_COLLECTION(a) \
  TRI_WriteUnlockReadWriteLock(&(a)->_lock)

////////////////////////////////////////////////////////////////////////////////
/// @brief read locks the documents and indexes
////////////////////////////////////////////////////////////////////////////////

#define TRI_READ_LOCK_DOCUMENTS_INDEXES_SIM_COLLECTION(a) \
  TRI_ReadLockReadWriteLock(&(a)->_lock)

////////////////////////////////////////////////////////////////////////////////
/// @brief read unlocks the documents and indexes
////////////////////////////////////////////////////////////////////////////////

#define TRI_READ_UNLOCK_DOCUMENTS_INDEXES_SIM_COLLECTION(a) \
  TRI_ReadUnlockReadWriteLock(&(a)->_lock)

////////////////////////////////////////////////////////////////////////////////
/// @brief write locks the documents and indexes
////////////////////////////////////////////////////////////////////////////////

#define TRI_WRITE_LOCK_DOCUMENTS_INDEXES_SIM_COLLECTION(a) \
  TRI_WriteLockReadWriteLock(&(a)->_lock)

////////////////////////////////////////////////////////////////////////////////
/// @brief write unlocks the documents and indexes
////////////////////////////////////////////////////////////////////////////////

#define TRI_WRITE_UNLOCK_DOCUMENTS_INDEXES_SIM_COLLECTION(a) \
  TRI_WriteUnlockReadWriteLock(&(a)->_lock)

////////////////////////////////////////////////////////////////////////////////
/// @brief locks the journal entries
////////////////////////////////////////////////////////////////////////////////

#define TRI_LOCK_JOURNAL_ENTRIES_SIM_COLLECTION(a) \
  TRI_LockCondition(&(a)->_journalsCondition)

////////////////////////////////////////////////////////////////////////////////
/// @brief unlocks the journal entries
////////////////////////////////////////////////////////////////////////////////

#define TRI_UNLOCK_JOURNAL_ENTRIES_SIM_COLLECTION(a) \
  TRI_UnlockCondition(&(a)->_journalsCondition)

////////////////////////////////////////////////////////////////////////////////
/// @brief waits for the journal entries
////////////////////////////////////////////////////////////////////////////////

#define TRI_WAIT_JOURNAL_ENTRIES_SIM_COLLECTION(a) \
  TRI_WaitCondition(&(a)->_journalsCondition)

////////////////////////////////////////////////////////////////////////////////
/// @brief signals the journal entries
////////////////////////////////////////////////////////////////////////////////

#define TRI_BROADCAST_JOURNAL_ENTRIES_SIM_COLLECTION(a) \
  TRI_BroadcastCondition(&(a)->_journalsCondition)

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts the shape identifier pointer from a marker
////////////////////////////////////////////////////////////////////////////////

#define TRI_EXTRACT_SHAPE_IDENTIFIER_MARKER(dst, src)                                     \
  do {                                                                                    \
    if (((TRI_df_marker_t const*) (src))->_type == TRI_DOC_MARKER_DOCUMENT) {             \
      (dst) = ((TRI_doc_document_marker_t*) (src))->_shape;                               \
    }                                                                                     \
    else if (((TRI_df_marker_t const*) (src))->_type == TRI_DOC_MARKER_EDGE) {            \
      (dst) = ((TRI_doc_edge_marker_t*) (src))->base._shape;                              \
    }                                                                                     \
    else {                                                                                \
      (dst) = 0;                                                                          \
    }                                                                                     \
  } while (false)

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts the shaped JSON pointer from a marker
////////////////////////////////////////////////////////////////////////////////

#define TRI_EXTRACT_SHAPED_JSON_MARKER(dst, src)                                                     \
  do {                                                                                               \
    if (((TRI_df_marker_t const*) (src))->_type == TRI_DOC_MARKER_DOCUMENT) {                        \
      (dst)._sid = ((TRI_doc_document_marker_t*) (src))->_shape;                                     \
      (dst)._data.length = ((TRI_df_marker_t*) (src))->_size - sizeof(TRI_doc_document_marker_t);    \
      (dst)._data.data = (((char*) (src)) + sizeof(TRI_doc_document_marker_t));                      \
    }                                                                                                \
    else if (((TRI_df_marker_t const*) (src))->_type == TRI_DOC_MARKER_EDGE) {                       \
      (dst)._sid = ((TRI_doc_document_marker_t*) (src))->_shape;                                     \
      (dst)._data.length = ((TRI_df_marker_t*) (src))->_size - sizeof(TRI_doc_edge_marker_t);        \
      (dst)._data.data = (((char*) (src)) + sizeof(TRI_doc_edge_marker_t));                          \
    }                                                                                                \
    else {                                                                                           \
      (dst)._sid = 0;                                                                                \
    }                                                                                                \
  } while (false)

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
  TRI_multi_pointer_t _edgesIndex;
  TRI_vector_pointer_t _indexes;

  // .............................................................................
  // this condition variable protects the _journals
  // .............................................................................

  TRI_condition_t _journalsCondition;
}
TRI_sim_collection_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief edge from and to
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_sim_edge_s {
  TRI_voc_cid_t _fromCid;
  TRI_voc_did_t _fromDid;

  TRI_voc_cid_t _toCid;
  TRI_voc_did_t _toDid;
}
TRI_sim_edge_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief edge direction
////////////////////////////////////////////////////////////////////////////////

typedef enum {
  TRI_EDGE_UNUSED = 0,
  TRI_EDGE_IN = 1,
  TRI_EDGE_OUT = 2,
  TRI_EDGE_ANY = 3
}
TRI_edge_direction_e;

////////////////////////////////////////////////////////////////////////////////
/// @brief index entry for edges
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_edge_header_s {
  TRI_doc_mptr_t const* _mptr;
  TRI_edge_direction_e _direction;
  TRI_voc_cid_t _cid; // from or to, depending on the direction
  TRI_voc_did_t _did; // from or to, depending on the direction
}
TRI_edge_header_t;

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

TRI_sim_collection_t* TRI_CreateSimCollection (TRI_vocbase_t*,
                                               char const* path,
                                               TRI_col_parameter_t* parameter,
                                               TRI_voc_cid_t);

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

TRI_sim_collection_t* TRI_OpenSimCollection (TRI_vocbase_t*, char const* path);

////////////////////////////////////////////////////////////////////////////////
/// @brief closes an open collection
////////////////////////////////////////////////////////////////////////////////

int TRI_CloseSimCollection (TRI_sim_collection_t* collection);

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
/// @brief returns a description of an index
////////////////////////////////////////////////////////////////////////////////

TRI_index_t* TRI_IndexSimCollection (TRI_sim_collection_t*, TRI_idx_iid_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief drops an index
////////////////////////////////////////////////////////////////////////////////

bool TRI_DropIndexSimCollection (TRI_sim_collection_t* collection, TRI_idx_iid_t iid);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                       EDGES INDEX
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up edges
////////////////////////////////////////////////////////////////////////////////

TRI_vector_pointer_t TRI_LookupEdgesSimCollection (TRI_sim_collection_t* edges,
                                                   TRI_edge_direction_e direction,
                                                   TRI_voc_cid_t cid,
                                                   TRI_voc_did_t did);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    CAP CONSTRAINT
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures that a cap constraint exists
////////////////////////////////////////////////////////////////////////////////

TRI_index_t* TRI_EnsureCapConstraintSimCollection (TRI_sim_collection_t* sim,
                                                   size_t size,
                                                   bool* created);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                         GEO INDEX
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

struct TRI_index_s* TRI_LookupGeoIndex1SimCollection (TRI_sim_collection_t* collection,
                                                      TRI_shape_pid_t location,
                                                      bool geoJson,
                                                      bool constraint,
                                                      bool ignoreNull);

////////////////////////////////////////////////////////////////////////////////
/// @brief finds a geo index
///
/// Note that the caller must hold at least a read-lock.
////////////////////////////////////////////////////////////////////////////////

struct TRI_index_s* TRI_LookupGeoIndex2SimCollection (TRI_sim_collection_t* collection,
                                                      TRI_shape_pid_t latitude,
                                                      TRI_shape_pid_t longitude,
                                                      bool constraint,
                                                      bool ignoreNull);

////////////////////////////////////////////////////////////////////////////////
/// @brief finds a hash index
///
/// Note that the caller must hold at least a read-lock.
////////////////////////////////////////////////////////////////////////////////

struct TRI_index_s* TRI_LookupHashIndexSimCollection (TRI_sim_collection_t*,
                                                      TRI_vector_t const*);


////////////////////////////////////////////////////////////////////////////////
/// @brief finds a priority queue index
///
/// Note that the caller must hold at least a read-lock.
////////////////////////////////////////////////////////////////////////////////

struct TRI_index_s* TRI_LookupPriorityQueueIndexSimCollection (TRI_sim_collection_t*,
                                                               TRI_vector_t const*);
                                                               
                                                               
////////////////////////////////////////////////////////////////////////////////
/// @brief finds a skiplist index
///
/// Note that the caller must hold at least a read-lock.
////////////////////////////////////////////////////////////////////////////////

struct TRI_index_s* TRI_LookupSkiplistIndexSimCollection (TRI_sim_collection_t*,
                                                          TRI_vector_t const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures that a cap constraint exists
////////////////////////////////////////////////////////////////////////////////

TRI_index_t* TRI_EnsureCapConstraintSimCollection (TRI_sim_collection_t* sim,
                                                   size_t size,
                                                   bool* created);

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures that a geo index exists
////////////////////////////////////////////////////////////////////////////////

struct TRI_index_s* TRI_EnsureGeoIndex1SimCollection (TRI_sim_collection_t* collection,
                                                      char const* location,
                                                      bool geoJson,
                                                      bool constraint,
                                                      bool ignoreNull,
                                                      bool* created);

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a geo index to a collection
////////////////////////////////////////////////////////////////////////////////

struct TRI_index_s* TRI_EnsureGeoIndex2SimCollection (TRI_sim_collection_t* collection,
                                                      char const* latitude,
                                                      char const* longitude,
                                                      bool constraint,
                                                      bool ignoreNull,
                                                      bool* created);
                                                
////////////////////////////////////////////////////////////////////////////////
/// @brief adds or returns an existing hash index to a collection
////////////////////////////////////////////////////////////////////////////////

struct TRI_index_s* TRI_EnsureHashIndexSimCollection (TRI_sim_collection_t* collection,
                                                      const TRI_vector_t* attributes,
                                                      bool unique,
                                                      bool* created);
                                                
                                                
////////////////////////////////////////////////////////////////////////////////
/// @brief adds or returns an existing priority queue index to a collection
////////////////////////////////////////////////////////////////////////////////

struct TRI_index_s* TRI_EnsurePriorityQueueIndexSimCollection (TRI_sim_collection_t* collection,
                                                    const TRI_vector_t* attributes,
                                                    bool unique,
                                                    bool* created);
                                                

////////////////////////////////////////////////////////////////////////////////
/// @brief adds or returns an existing skiplist index to a collection
////////////////////////////////////////////////////////////////////////////////

struct TRI_index_s* TRI_EnsureSkiplistIndexSimCollection (TRI_sim_collection_t* collection,
                                                          const TRI_vector_t* attributes,
                                                          bool unique,
                                                          bool* created);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                           SELECT BY EXAMPLE QUERY
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a select-by-example query
////////////////////////////////////////////////////////////////////////////////

TRI_vector_t TRI_SelectByExample (TRI_sim_collection_t* sim,
                                  size_t length,
                                  TRI_shape_pid_t* pids,
                                  TRI_shaped_json_t** values);

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

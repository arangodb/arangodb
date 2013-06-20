////////////////////////////////////////////////////////////////////////////////
/// @brief document collection with global read-write lock, derived from
/// TRI_primary_collection_t
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

#ifndef TRIAGENS_VOC_BASE_DOCUMENT_COLLECTION_H
#define TRIAGENS_VOC_BASE_DOCUMENT_COLLECTION_H 1

#include "VocBase/primary-collection.h"

#include "VocBase/headers.h"
#include "VocBase/index.h"

#include <regex.h>

#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                              forward declarations
// -----------------------------------------------------------------------------

struct TRI_df_marker_s;

// -----------------------------------------------------------------------------
// --SECTION--                                               DOCUMENT COLLECTION
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                     public macros
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief tries to read lock the journal files and the parameter file
///
/// note: the return value of the call to TRI_TryReadLockReadWriteLock is
/// is checked so we cannot add logging here
////////////////////////////////////////////////////////////////////////////////

#define TRI_TRY_READ_LOCK_DATAFILES_DOC_COLLECTION(a) \
  TRI_TryReadLockReadWriteLock(&(a)->_lock)

////////////////////////////////////////////////////////////////////////////////
/// @brief read locks the journal files and the parameter file
////////////////////////////////////////////////////////////////////////////////

#define TRI_READ_LOCK_DATAFILES_DOC_COLLECTION(a) \
  TRI_ReadLockReadWriteLock(&(a)->_lock)

////////////////////////////////////////////////////////////////////////////////
/// @brief read unlocks the journal files and the parameter file
////////////////////////////////////////////////////////////////////////////////

#define TRI_READ_UNLOCK_DATAFILES_DOC_COLLECTION(a) \
  TRI_ReadUnlockReadWriteLock(&(a)->_lock)

////////////////////////////////////////////////////////////////////////////////
/// @brief write locks the journal files and the parameter file
////////////////////////////////////////////////////////////////////////////////

#define TRI_WRITE_LOCK_DATAFILES_DOC_COLLECTION(a) \
  TRI_WriteLockReadWriteLock(&(a)->_lock)

////////////////////////////////////////////////////////////////////////////////
/// @brief write unlocks the journal files and the parameter file
////////////////////////////////////////////////////////////////////////////////

#define TRI_WRITE_UNLOCK_DATAFILES_DOC_COLLECTION(a) \
  TRI_WriteUnlockReadWriteLock(&(a)->_lock)

////////////////////////////////////////////////////////////////////////////////
/// @brief locks the journal entries
////////////////////////////////////////////////////////////////////////////////

#define TRI_LOCK_JOURNAL_ENTRIES_DOC_COLLECTION(a) \
  TRI_LockCondition(&(a)->_journalsCondition)

////////////////////////////////////////////////////////////////////////////////
/// @brief unlocks the journal entries
////////////////////////////////////////////////////////////////////////////////

#define TRI_UNLOCK_JOURNAL_ENTRIES_DOC_COLLECTION(a) \
  TRI_UnlockCondition(&(a)->_journalsCondition)

////////////////////////////////////////////////////////////////////////////////
/// @brief waits for the journal entries
////////////////////////////////////////////////////////////////////////////////

#define TRI_WAIT_JOURNAL_ENTRIES_DOC_COLLECTION(a) \
  TRI_WaitCondition(&(a)->_journalsCondition)

////////////////////////////////////////////////////////////////////////////////
/// @brief signals the journal entries
////////////////////////////////////////////////////////////////////////////////

#define TRI_BROADCAST_JOURNAL_ENTRIES_DOC_COLLECTION(a) \
  TRI_BroadcastCondition(&(a)->_journalsCondition)

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts the shape identifier pointer from a marker
////////////////////////////////////////////////////////////////////////////////

#define TRI_EXTRACT_SHAPE_IDENTIFIER_MARKER(dst, src)                                     \
  do {                                                                                    \
    if (((TRI_df_marker_t const*) (src))->_type == TRI_DOC_MARKER_KEY_DOCUMENT) {         \
      (dst) = ((TRI_doc_document_key_marker_t*) (src))->_shape;                           \
    }                                                                                     \
    else if (((TRI_df_marker_t const*) (src))->_type == TRI_DOC_MARKER_KEY_EDGE) {        \
      (dst) = ((TRI_doc_edge_key_marker_t*) (src))->base._shape;                          \
    }                                                                                     \
    else {                                                                                \
      (dst) = 0;                                                                          \
    }                                                                                     \
  } while (0)

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts the shaped JSON pointer from a marker
////////////////////////////////////////////////////////////////////////////////

#define TRI_EXTRACT_SHAPED_JSON_MARKER(dst, src)                                                                       \
  do {                                                                                                                 \
    if (((TRI_df_marker_t const*) (src))->_type == TRI_DOC_MARKER_KEY_DOCUMENT) {                                      \
      (dst)._sid = ((TRI_doc_document_key_marker_t*) (src))->_shape;                                                   \
      (dst)._data.length = ((TRI_df_marker_t*) (src))->_size - ((TRI_doc_document_key_marker_t*) (src))->_offsetJson;  \
      (dst)._data.data = (((char*) (src)) + ((TRI_doc_document_key_marker_t*) (src))->_offsetJson);                    \
    }                                                                                                                  \
    else if (((TRI_df_marker_t const*) (src))->_type == TRI_DOC_MARKER_KEY_EDGE) {                                     \
      (dst)._sid = ((TRI_doc_document_key_marker_t*) (src))->_shape;                                                   \
      (dst)._data.length = ((TRI_df_marker_t*) (src))->_size - ((TRI_doc_document_key_marker_t*) (src))->_offsetJson;  \
      (dst)._data.data = (((char*) (src)) + ((TRI_doc_document_key_marker_t*) (src))->_offsetJson);                    \
    }                                                                                                                  \
    else {                                                                                                             \
      (dst)._sid = 0;                                                                                                  \
    }                                                                                                                  \
  } while (0)

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
/// @brief primary collection with global read-write lock
///
/// A primary collection is a collection with a single read-write lock. This
/// lock is used to coordinate the read and write transactions.
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_document_collection_s {
  TRI_primary_collection_t base;

  // .............................................................................
  // the collection does not have a lock of its own. it is protected by the
  // _lock of its base type, TRI_primary_collection_t.
  // .............................................................................

  TRI_headers_t* _headers;

  TRI_vector_pointer_t _allIndexes;

  // whether or not any of the indexes may need to be garbage-collected
  // this flag may be modifying when an index is added to a collection
  // if true, the cleanup thread will periodically call the cleanup functions of
  // the collection's indexes that support cleanup
  bool _cleanupIndexes;

  // .............................................................................
  // this condition variable protects the _journalsCondition
  // .............................................................................

  TRI_condition_t _journalsCondition;

  // function that is called to garbage-collect the collection's indexes
  int (*cleanupIndexes)(struct TRI_document_collection_s*);
}
TRI_document_collection_t;

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

TRI_document_collection_t* TRI_CreateDocumentCollection (TRI_vocbase_t*,
                                                         char const*,
                                                         TRI_col_info_t*,
                                                         TRI_voc_cid_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the memory allocated, but does not free the pointer
///
/// Note that the collection must be closed first.
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyDocumentCollection (TRI_document_collection_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the memory allocated and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeDocumentCollection (TRI_document_collection_t*);

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
/// @brief rolls back a document operation
////////////////////////////////////////////////////////////////////////////////

int TRI_RollbackOperationDocumentCollection (TRI_document_collection_t*,
                                             TRI_voc_document_operation_e,
                                             TRI_doc_mptr_t*,
                                             TRI_doc_mptr_t*,
                                             TRI_doc_mptr_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief writes a marker into the datafile
////////////////////////////////////////////////////////////////////////////////

int TRI_WriteMarkerDocumentCollection (TRI_document_collection_t*,
                                       struct TRI_df_marker_s*,
                                       const TRI_voc_size_t,
                                       TRI_voc_fid_t*,
                                       struct TRI_df_marker_s**,
                                       const bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief writes a document operation marker into the datafile
////////////////////////////////////////////////////////////////////////////////

int TRI_WriteOperationDocumentCollection (TRI_document_collection_t*,
                                          TRI_voc_document_operation_e,
                                          TRI_doc_mptr_t*,
                                          TRI_doc_mptr_t*,
                                          TRI_doc_mptr_t*,
                                          TRI_df_marker_t*,
                                          TRI_voc_size_t,
                                          bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new journal
////////////////////////////////////////////////////////////////////////////////

TRI_datafile_t* TRI_CreateJournalDocumentCollection (TRI_document_collection_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief closes an existing journal
////////////////////////////////////////////////////////////////////////////////

bool TRI_CloseJournalDocumentCollection (TRI_document_collection_t*, size_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief opens an existing collection
////////////////////////////////////////////////////////////////////////////////

TRI_document_collection_t* TRI_OpenDocumentCollection (TRI_vocbase_t*, char const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief closes an open collection
////////////////////////////////////////////////////////////////////////////////

int TRI_CloseDocumentCollection (TRI_document_collection_t*);

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
///
/// the caller must have read-locked the underyling collection!
////////////////////////////////////////////////////////////////////////////////

TRI_vector_pointer_t* TRI_IndexesDocumentCollection (TRI_document_collection_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief drops an index
////////////////////////////////////////////////////////////////////////////////

bool TRI_DropIndexDocumentCollection (TRI_document_collection_t*, TRI_idx_iid_t);

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

TRI_index_t* TRI_EnsureCapConstraintDocumentCollection (TRI_document_collection_t*,
                                                        size_t,
                                                        bool*);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////


// -----------------------------------------------------------------------------
// --SECTION--                                                    BITARRAY INDEX
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief finds a bitarray index
///
/// Note that the caller must hold at least a read-lock.
/// Also note that the only the set of attributes are used to distinguish
/// a bitarray index -- that is, a bitarray is considered to be the same if
/// the attributes match irrespective of the possible values for an attribute.
/// Finally observe that there is no notion of uniqueness for a bitarray index.
/// TODO: allow changes to possible values to be made at run-time.
////////////////////////////////////////////////////////////////////////////////

struct TRI_index_s* TRI_LookupBitarrayIndexDocumentCollection (TRI_document_collection_t*,
                                                               const TRI_vector_pointer_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures that a bitarray index exists
////////////////////////////////////////////////////////////////////////////////

struct TRI_index_s* TRI_EnsureBitarrayIndexDocumentCollection (TRI_document_collection_t*,
                                                               const TRI_vector_pointer_t*,
                                                               const TRI_vector_pointer_t*,
                                                               bool,
                                                               bool*,
                                                               int*,
                                                               char**);

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
/// @brief finds a geo index, list style
///
/// Note that the caller must hold at least a read-lock.
////////////////////////////////////////////////////////////////////////////////

struct TRI_index_s* TRI_LookupGeoIndex1DocumentCollection (TRI_document_collection_t*,
                                                           TRI_shape_pid_t,
                                                           bool,
                                                           bool,
                                                           bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief finds a geo index, attribute style
///
/// Note that the caller must hold at least a read-lock.
////////////////////////////////////////////////////////////////////////////////

struct TRI_index_s* TRI_LookupGeoIndex2DocumentCollection (TRI_document_collection_t*,
                                                           TRI_shape_pid_t,
                                                           TRI_shape_pid_t,
                                                           bool,
                                                           bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures that a geo index exists, list style
////////////////////////////////////////////////////////////////////////////////

struct TRI_index_s* TRI_EnsureGeoIndex1DocumentCollection (TRI_document_collection_t*,
                                                           char const*,
                                                           bool,
                                                           bool,
                                                           bool,
                                                           bool*);

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures that a geo index exists, attribute style
////////////////////////////////////////////////////////////////////////////////

struct TRI_index_s* TRI_EnsureGeoIndex2DocumentCollection (TRI_document_collection_t*,
                                                           char const*,
                                                           char const*,
                                                           bool,
                                                           bool,
                                                           bool*);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                        HASH INDEX
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief converts attribute names to sorted lists of pids and names
////////////////////////////////////////////////////////////////////////////////

int TRI_PidNamesByAttributeNames (TRI_vector_pointer_t const*,
                                  TRI_shaper_t*,
                                  TRI_vector_t*,
                                  TRI_vector_pointer_t*,
                                  bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief finds a hash index
///
/// @note The caller must hold at least a read-lock.
///
/// @note The @FA{paths} must be sorted.
////////////////////////////////////////////////////////////////////////////////

struct TRI_index_s* TRI_LookupHashIndexDocumentCollection (TRI_document_collection_t*,
                                                           TRI_vector_pointer_t const*,
                                                           bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures that a hash index exists
////////////////////////////////////////////////////////////////////////////////

struct TRI_index_s* TRI_EnsureHashIndexDocumentCollection (TRI_document_collection_t*,
                                                           TRI_vector_pointer_t const*,
                                                           bool,
                                                           bool*);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    SKIPLIST INDEX
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief finds a skiplist index
///
/// Note that the caller must hold at least a read-lock.
////////////////////////////////////////////////////////////////////////////////

struct TRI_index_s* TRI_LookupSkiplistIndexDocumentCollection (TRI_document_collection_t*,
                                                               TRI_vector_pointer_t const*,
                                                               bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures that a skiplist index exists
////////////////////////////////////////////////////////////////////////////////

struct TRI_index_s* TRI_EnsureSkiplistIndexDocumentCollection (TRI_document_collection_t*,
                                                               TRI_vector_pointer_t const*,
                                                               bool,
                                                               bool*);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    FULLTEXT INDEX
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief finds a fulltext index
///
/// Note that the caller must hold at least a read-lock.
////////////////////////////////////////////////////////////////////////////////

struct TRI_index_s* TRI_LookupFulltextIndexDocumentCollection (TRI_document_collection_t*,
                                                               const char*,
                                                               const bool,
                                                               int);

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures that a fulltext index exists
////////////////////////////////////////////////////////////////////////////////

struct TRI_index_s* TRI_EnsureFulltextIndexDocumentCollection (TRI_document_collection_t*,
                                                               const char*,
                                                               const bool,
                                                               int,
                                                               bool*);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                              PRIORITY QUEUE INDEX
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief finds a priority queue index
///
/// Note that the caller must hold at least a read-lock.
////////////////////////////////////////////////////////////////////////////////

struct TRI_index_s* TRI_LookupPriorityQueueIndexDocumentCollection (TRI_document_collection_t*,
                                                                    TRI_vector_t const*);


////////////////////////////////////////////////////////////////////////////////
/// @brief ensures that a priority queue index exists
////////////////////////////////////////////////////////////////////////////////

struct TRI_index_s* TRI_EnsurePriorityQueueIndexDocumentCollection (TRI_document_collection_t*,
                                                                    TRI_vector_pointer_t const*,
                                                                    bool,
                                                                    bool*);

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
/// @brief executes a select-by-example query
////////////////////////////////////////////////////////////////////////////////

TRI_vector_t TRI_SelectByExample (struct TRI_transaction_collection_s*,
                                  size_t,
                                  TRI_shape_pid_t*,
                                  TRI_shaped_json_t**);

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes a documet given by a master pointer
////////////////////////////////////////////////////////////////////////////////

int TRI_DeleteDocumentDocumentCollection (struct TRI_transaction_collection_s*,
                                          struct TRI_doc_update_policy_s const*,
                                          TRI_doc_mptr_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief set the collection revision id
////////////////////////////////////////////////////////////////////////////////

void TRI_SetRevisionDocumentCollection (TRI_document_collection_t*,
                                        TRI_voc_tick_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief rotate the current journal of the collection
/// use this for testing only
////////////////////////////////////////////////////////////////////////////////

int TRI_RotateJournalDocumentCollection (TRI_document_collection_t*);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:

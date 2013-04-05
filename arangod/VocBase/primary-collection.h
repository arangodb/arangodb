////////////////////////////////////////////////////////////////////////////////
/// @brief primary collection with global read-write lock
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

#ifndef TRIAGENS_VOC_BASE_PRIMARY_COLLECTION_H
#define TRIAGENS_VOC_BASE_PRIMARY_COLLECTION_H 1

#include "VocBase/collection.h"

#include "BasicsC/json.h"
#include "ShapedJson/json-shaper.h"
#include "VocBase/barrier.h"
#include "VocBase/marker.h"
#include "VocBase/transaction.h"
#include "VocBase/update-policy.h"
#include "VocBase/voc-types.h"

#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                              forward declarations
// -----------------------------------------------------------------------------

struct TRI_cap_constraint_s;
struct TRI_doc_deletion_key_marker_s;
struct TRI_doc_document_key_marker_s;
struct TRI_doc_update_policy_s;
struct TRI_key_generator_s;
struct TRI_primary_collection_s;

// -----------------------------------------------------------------------------
// --SECTION--                                                     public macros
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief read locks the documents and indexes
////////////////////////////////////////////////////////////////////////////////

#define TRI_READ_LOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(a) \
  TRI_ReadLockReadWriteLock(&(a)->_lock)

////////////////////////////////////////////////////////////////////////////////
/// @brief read unlocks the documents and indexes
////////////////////////////////////////////////////////////////////////////////

#define TRI_READ_UNLOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(a) \
  TRI_ReadUnlockReadWriteLock(&(a)->_lock)

////////////////////////////////////////////////////////////////////////////////
/// @brief write locks the documents and indexes
////////////////////////////////////////////////////////////////////////////////

#define TRI_WRITE_LOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(a) \
  TRI_WriteLockReadWriteLock(&(a)->_lock)

////////////////////////////////////////////////////////////////////////////////
/// @brief write unlocks the documents and indexes
////////////////////////////////////////////////////////////////////////////////

#define TRI_WRITE_UNLOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(a) \
  TRI_WriteUnlockReadWriteLock(&(a)->_lock)

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
/// @brief master pointer
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_doc_mptr_s {
  TRI_voc_rid_t  _rid;        // this is the revision identifier
  TRI_voc_fid_t  _fid;        // this is the datafile identifier
  TRI_voc_tick_t _validTo;    // this is the deletion time (0 if document is not yet deleted)
  void const*    _data;       // this is the pointer to the beginning of the raw marker
  char*          _key;        // this is the document identifier (string)
}
TRI_doc_mptr_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief datafile info
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_doc_datafile_info_s {
  TRI_voc_fid_t _fid;

  TRI_voc_ssize_t _numberAlive;
  TRI_voc_ssize_t _numberDead;
  TRI_voc_ssize_t _sizeAlive;
  TRI_voc_ssize_t _sizeDead;
  TRI_voc_ssize_t _numberDeletion;
}
TRI_doc_datafile_info_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief collection info
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_doc_collection_info_s {
  TRI_voc_ssize_t _numberDatafiles;
  TRI_voc_ssize_t _numberJournalfiles;

  TRI_voc_ssize_t _numberAlive;
  TRI_voc_ssize_t _numberDead;
  TRI_voc_ssize_t _sizeAlive;
  TRI_voc_ssize_t _sizeDead;
  TRI_voc_ssize_t _numberDeletion;
  TRI_voc_ssize_t _datafileSize;
  TRI_voc_ssize_t _journalfileSize;

  TRI_voc_ssize_t _numberShapes;
}
TRI_doc_collection_info_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief primary collection
///
/// A primary collection is a collection of documents. These documents are
/// represented as @ref ShapedJson "shaped JSON objects". Each document has a
/// place in memory which is determined by the position in the memory mapped
/// file. As datafiles are compacted during garbage collection, this position
/// can change over time. Each active document also has a master pointer of type
/// @ref TRI_doc_mptr_t. This master pointer never changes and is valid as long
/// as the object is not deleted.
///
/// It is important to use locks for create, read, update, and delete.  The
/// functions @FN{create}, @FN{update}, and
/// @FN{destroy} are only allowed within a @FN{beginWrite} and
/// @FN{endWrite}. The function @FN{read} is only allowed within a
/// @FN{beginRead} and @FN{endRead}. Note that @FN{read} returns a copy of the
/// master pointer.
///
/// If a document is deleted, it's master pointer becomes invalid. However, the
/// document itself still exists. Executing a query and constructing its result
/// set, must be done inside a "beginRead" and "endRead".
///
/// @FUN{int beginRead (TRI_primary_collection_t*)}
///////////////////////////////////////////////
///
/// Starts a read transaction. Query and calls to @FN{read} are allowed within a
/// read transaction, but not calls to @FN{create}, @FN{update}, or
/// @FN{destroy}.  Returns @ref TRI_ERROR_NO_ERROR if the transaction could be
/// started. This call might block until a running write transaction is
/// finished.
///
/// @FUN{int endRead (TRI_primary_collection_t*)}
/////////////////////////////////////////////
///
/// Ends a read transaction. Should only be called after a successful
/// "beginRead".
///
/// @FUN{int beginWrite (TRI_primary_collection_t*)}
////////////////////////////////////////////////
///
/// Starts a write transaction. Query and calls to @FN{create}, @FN{read},
/// @FN{update}, and @FN{destroy} are allowed within a write
/// transaction. Returns @ref TRI_ERROR_NO_ERROR if the transaction could be
/// started. This call might block until a running write transaction is
/// finished.
///
/// @FUN{int endWrite (TRI_primary_collection_t*)}
//////////////////////////////////////////////
///
/// Ends a write transaction. Should only be called after a successful
/// @LIT{beginWrite}.
///
/// @FUN{TRI_doc_mptr_t const create (TRI_primary_collection_t*, TRI_df_marker_type_e, TRI_shaped_json_t const*, bool @FA{release})}
///
/// Adds a new document to the collection and returns the master pointer of the
/// newly created entry. In case of an error, the attribute @LIT{_did} of the
/// result is @LIT{0} and "TRI_errno()" is set accordingly. The function DOES
/// NOT acquire a write lock. This must be done by the caller. If @FA{release}
/// is true, it will release the write lock as soon as possible.
///
/// @FUN{TRI_doc_mptr_t const read (TRI_primary_collection_t*, TRI_voc_key_t)}
//////////////////////////////////////////////////////////////////////////
///
/// Returns the master pointer of the document with the given identifier. If the
/// document does not exist or is deleted, then the identifier @LIT{_did} of
/// the result is @LIT{0}. The function DOES NOT acquire or release a read
/// lock. This must be done by the caller.
///
/// @FUN{TRI_doc_mptr_t const update (TRI_primary_collection_t*, TRI_shaped_json_t const*, TRI_voc_key_t, TRI_voc_rid_t @FA{rid}, TRI_voc_rid_t* @FA{current}, TRI_doc_update_policy_e, bool @FA{release})}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///
/// Updates an existing document of the collection and returns copy of a valid
/// master pointer in case of success. Otherwise, the attribute @LIT{_did} of
/// the result is @LIT{0} and the "TRI_errno()" is set accordingly. The function
/// DOES NOT acquire a write lock. This must be done by the caller. However, if
/// @FA{release} is true, it will release the write lock as soon as possible.
///
/// If the policy is @ref TRI_doc_update_policy_e "TRI_DOC_UPDATE_LAST_WRITE",
/// than the revision @FA{rid} is ignored and the update is always performed. If
/// the policy is @ref TRI_doc_update_policy_e "TRI_DOC_UPDATE_ERROR" and the
/// revision @FA{rid} is given (i. e. not equal 0), then the update is only
/// performed if the current revision matches the given. In any case the current
/// revision after the updated of the document is returned in @FA{current}.
///
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///
/// @FUN{int destroy (TRI_primary_collection_t*, TRI_voc_key_t, TRI_voc_rid_t, TRI_voc_rid_t @FA{rid}, TRI_voc_rid_t* @FA{current}, TRI_doc_update_policy_e, bool @FA{release})}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///
/// Deletes an existing document from the given collection and returns @ref
/// TRI_ERROR_NO_ERROR in case of success. Otherwise, an error is returned and
/// the "TRI_errno()" is set accordingly. The function DOES NOT acquire a write
/// lock.  However, if @FA{release} is true, it will release the write lock as
/// soon as possible.
///
/// If the policy is @ref TRI_doc_update_policy_e "TRI_DOC_UPDATE_ERROR" and the
/// revision is given, then it must match the current revision of the
/// document. If the delete was executed, than @FA{current} contains the last
/// valid revision of the document. If the delete was aborted, than @FA{current}
/// contains the revision of the still alive document.
///
/// @FUN{TRI_doc_collection_info_t* figures (TRI_primary_collection_t*)}
////////////////////////////////////////////////////////////////////
///
/// Returns informatiom about the collection. You must hold a read lock and must
/// destroy the result after usage.
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_primary_collection_s {
  TRI_collection_t base;

  // .............................................................................
  // this lock protects the _primaryIndex plus the _allIndexes
  // and _headers attributes in derived types
  // .............................................................................

  TRI_read_write_lock_t _lock;

  TRI_shaper_t* _shaper;
  TRI_barrier_list_t _barrierList;
  TRI_associative_pointer_t _datafileInfo;

  TRI_associative_pointer_t _primaryIndex;
  struct TRI_key_generator_s* _keyGenerator;
  struct TRI_cap_constraint_s* _capConstraint;

  int64_t _numberDocuments;

  int (*beginRead) (struct TRI_primary_collection_s*);
  int (*endRead) (struct TRI_primary_collection_s*);

  int (*beginWrite) (struct TRI_primary_collection_s*);
  int (*endWrite) (struct TRI_primary_collection_s*);

  int (*notifyTransaction) (struct TRI_primary_collection_s*, TRI_transaction_status_e);

  int (*insert) (struct TRI_transaction_collection_s*, const TRI_voc_key_t, TRI_doc_mptr_t*, TRI_df_marker_type_e, TRI_shaped_json_t const*, void const*, const bool, const bool);

  int (*read) (struct TRI_transaction_collection_s*, const TRI_voc_key_t, TRI_doc_mptr_t*, const bool);

  int (*update) (struct TRI_transaction_collection_s*, const TRI_voc_key_t, TRI_doc_mptr_t*, TRI_shaped_json_t const*, struct TRI_doc_update_policy_s const*, const bool, const bool);
  int (*remove) (struct TRI_transaction_collection_s*, const TRI_voc_key_t, struct TRI_doc_update_policy_s const*, const bool, const bool);

  TRI_doc_collection_info_t* (*figures) (struct TRI_primary_collection_s* collection);
  TRI_voc_size_t (*size) (struct TRI_primary_collection_s* collection);
}
TRI_primary_collection_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief document datafile marker with key
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_doc_document_key_marker_s {
  TRI_df_marker_t base;

  TRI_voc_rid_t   _rid;        // this is the tick for a create and update
  TRI_voc_tid_t   _tid;

  TRI_shape_sid_t _shape;

  uint16_t        _offsetKey;
  uint16_t        _offsetJson;

#ifdef TRI_PADDING_32
  char            _padding_df_marker[4];
#endif
}
TRI_doc_document_key_marker_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief edge datafile marker with key
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_doc_edge_key_marker_s {
  TRI_doc_document_key_marker_t base;

  TRI_voc_cid_t   _toCid;
  TRI_voc_cid_t   _fromCid;

  uint16_t        _offsetToKey;
  uint16_t        _offsetFromKey;

#ifdef TRI_PADDING_32
  char            _padding_df_marker[4];
#endif
}
TRI_doc_edge_key_marker_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief document datafile deletion marker
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_doc_deletion_key_marker_s {
  TRI_df_marker_t base;

  TRI_voc_rid_t   _rid;        // this is the tick for the deletion
  TRI_voc_tid_t   _tid;

  uint16_t        _offsetKey;
}
TRI_doc_deletion_key_marker_t;

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
/// @brief initializes a primary collection structure
////////////////////////////////////////////////////////////////////////////////

int TRI_InitPrimaryCollection (TRI_primary_collection_t*, TRI_shaper_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a primary collection
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyPrimaryCollection (TRI_primary_collection_t*);

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
/// @brief finds a datafile description
////////////////////////////////////////////////////////////////////////////////

TRI_doc_datafile_info_t* TRI_FindDatafileInfoPrimaryCollection (TRI_primary_collection_t*,
                                                                TRI_voc_fid_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new journal
////////////////////////////////////////////////////////////////////////////////

TRI_datafile_t* TRI_CreateJournalPrimaryCollection (TRI_primary_collection_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief closes an existing journal
////////////////////////////////////////////////////////////////////////////////

bool TRI_CloseJournalPrimaryCollection (TRI_primary_collection_t*,
                                        size_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new compactor file
////////////////////////////////////////////////////////////////////////////////

TRI_datafile_t* TRI_CreateCompactorPrimaryCollection (TRI_primary_collection_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief closes an existing compactor file
////////////////////////////////////////////////////////////////////////////////

bool TRI_CloseCompactorPrimaryCollection (TRI_primary_collection_t*,
                                          size_t);

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

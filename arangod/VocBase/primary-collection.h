////////////////////////////////////////////////////////////////////////////////
/// @brief primary collection with global read-write lock
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

#ifndef TRIAGENS_DURHAM_VOC_BASE_PRIMARY_COLLECTION_H
#define TRIAGENS_DURHAM_VOC_BASE_PRIMARY_COLLECTION_H 1

#include "VocBase/collection.h"

#include "BasicsC/json.h"
#include "ShapedJson/json-shaper.h"
#include "VocBase/barrier.h"

#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                              forward declarations
// -----------------------------------------------------------------------------

struct TRI_cap_constraint_s;

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
/// @brief update and delete policy
////////////////////////////////////////////////////////////////////////////////

typedef enum {
  TRI_DOC_UPDATE_ERROR,
  TRI_DOC_UPDATE_LAST_WRITE,
  TRI_DOC_UPDATE_CONFLICT,
  TRI_DOC_UPDATE_ILLEGAL
}
TRI_doc_update_policy_e;

////////////////////////////////////////////////////////////////////////////////
/// @brief master pointer
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_doc_mptr_s {
  //TRI_voc_did_t _did; // this is the document identifier
  TRI_voc_rid_t _rid; // this is the revision identifier
  TRI_voc_eid_t _eid; // this is the step identifier

  TRI_voc_fid_t _fid; // this is the datafile identifier

  TRI_voc_tick_t _deletion; // this is the deletion time

  void const* _data; // this is the pointer to the raw marker
  
  char* _key; // this is the document identifier (string)
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
/// functions @FN{create}, @FN{createJson}, @FN{update}, @FN{updateJson}, and
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
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///
/// Adds a new document to the collection and returns the master pointer of the
/// newly created entry. In case of an error, the attribute @LIT{_did} of the
/// result is @LIT{0} and "TRI_errno()" is set accordingly. The function DOES
/// NOT acquire a write lock. This must be done by the caller. If @FA{release}
/// is true, it will release the write lock as soon as possible.
///
/// @FUN{TRI_doc_mptr_t const createJson (TRI_primary_collection_t*, TRI_df_marker_type_e, TRI_json_t const*, bool @FA{release})}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///
/// As before, but instead of a shaped json a json object must be given.
///
/// @FUN{TRI_voc_did_t createLock (TRI_primary_collection_t*, TRI_df_marker_type_e, TRI_shaped_json_t const*)}
//////////////////////////////////////////////////////////////////////////////////////////////////////////
///
/// As before, but the function will acquire and release the write lock.
///
/// @FUN{TRI_doc_mptr_t const read (TRI_primary_collection_t*, TRI_voc_did_t)}
//////////////////////////////////////////////////////////////////////////
///
/// Returns the master pointer of the document with the given identifier. If the
/// document does not exist or is deleted, then the identifier @LIT{_did} of
/// the result is @LIT{0}. The function DOES NOT acquire or release a read
/// lock. This must be done by the caller.
///
/// @FUN{TRI_doc_mptr_t const update (TRI_primary_collection_t*, TRI_shaped_json_t const*, TRI_voc_did_t, TRI_voc_rid_t @FA{rid}, TRI_voc_rid_t* @FA{current}, TRI_doc_update_policy_e, bool @FA{release})}
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
/// @FUN{TRI_doc_mptr_t const updateJson (TRI_primary_collection_t*, TRI_json_t const*, TRI_voc_did_t, TRI_voc_rid_t @FA{rid}, TRI_voc_rid_t* @FA{current}, TRI_doc_update_policy_e, bool @FA{release})}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///
/// As before, but instead of a shaped json a json object must be given.
///
/// @FUN{int updateLock (TRI_primary_collection_t*, TRI_shaped_json_t const*, TRI_voc_did_t, TRI_voc_rid_t @FA{rid}, TRI_voc_rid_t* @FA{current}, TRI_doc_update_policy_e)}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///
/// As before, but the function will acquire and release the write lock.
///
/// @FUN{int destroy (TRI_primary_collection_t*, TRI_voc_did_t, TRI_voc_rid_t, TRI_voc_rid_t @FA{rid}, TRI_voc_rid_t* @FA{current}, TRI_doc_update_policy_e, bool @FA{release})}
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
/// @FUN{int destroyLock (TRI_primary_collection_t*, TRI_voc_did_t, TRI_voc_rid_t @FA{rid}, TRI_voc_rid_t* @FA{current}, TRI_doc_update_policy_e)}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///
/// As before, but the function will acquire and release the write lock.
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
  // this lock protects the _primaryIndex plus the _allIndexes, _edgesIndex,
  // and _headers attributes in derived types
  // .............................................................................

  TRI_read_write_lock_t _lock;

  TRI_shaper_t* _shaper;
  TRI_barrier_list_t _barrierList;
  TRI_associative_pointer_t _datafileInfo;
  
  TRI_associative_pointer_t _primaryIndex;

  struct TRI_cap_constraint_s* _capConstraint;

  int (*beginRead) (struct TRI_primary_collection_s*);
  int (*endRead) (struct TRI_primary_collection_s*);

  int (*beginWrite) (struct TRI_primary_collection_s*);
  int (*endWrite) (struct TRI_primary_collection_s*);

  void (*createHeader) (struct TRI_primary_collection_s*, TRI_datafile_t*, TRI_df_marker_t const*, size_t, TRI_doc_mptr_t*, void const* data);
  void (*updateHeader) (struct TRI_primary_collection_s*, TRI_datafile_t*, TRI_df_marker_t const*, size_t, TRI_doc_mptr_t const*, TRI_doc_mptr_t*);

  TRI_doc_mptr_t (*create) (struct TRI_primary_collection_s*, TRI_df_marker_type_e, TRI_shaped_json_t const*, void const*, TRI_voc_key_t key, bool, bool);
  TRI_doc_mptr_t (*createJson) (struct TRI_primary_collection_s*, TRI_df_marker_type_e, TRI_json_t const*, void const*, bool, bool, bool);
  TRI_voc_key_t (*createLock) (struct TRI_primary_collection_s*, TRI_df_marker_type_e, TRI_shaped_json_t const*, void const*, bool);
  TRI_doc_mptr_t (*read) (struct TRI_primary_collection_s*, TRI_voc_key_t);

  TRI_doc_mptr_t (*update) (struct TRI_primary_collection_s*, TRI_shaped_json_t const*, TRI_voc_key_t, TRI_voc_rid_t, TRI_voc_rid_t*, TRI_doc_update_policy_e, bool, bool);
  TRI_doc_mptr_t (*updateJson) (struct TRI_primary_collection_s*, TRI_json_t const*, TRI_voc_key_t, TRI_voc_rid_t, TRI_voc_rid_t*, TRI_doc_update_policy_e, bool, bool);
  int (*updateLock) (struct TRI_primary_collection_s*, TRI_shaped_json_t const*, TRI_voc_key_t, TRI_voc_rid_t, TRI_voc_rid_t*, TRI_doc_update_policy_e, bool);

  int (*destroy) (struct TRI_primary_collection_s* collection, TRI_voc_key_t, TRI_voc_rid_t, TRI_voc_rid_t*, TRI_doc_update_policy_e, bool, bool);
  int (*destroyLock) (struct TRI_primary_collection_s* collection, TRI_voc_key_t, TRI_voc_rid_t, TRI_voc_rid_t*, TRI_doc_update_policy_e, bool);

  TRI_doc_collection_info_t* (*figures) (struct TRI_primary_collection_s* collection);
  TRI_voc_size_t (*size) (struct TRI_primary_collection_s* collection);
}
TRI_primary_collection_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief document datafile marker
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_doc_document_marker_s_deprecated {
  TRI_df_marker_t base;

  TRI_voc_did_t _did;        // this is the tick for a create, but not an update
  TRI_voc_rid_t _rid;        // this is the tick for an create and update
  TRI_voc_eid_t _sid;

  TRI_shape_sid_t _shape;
}
TRI_doc_document_marker_t_deprecated;

////////////////////////////////////////////////////////////////////////////////
/// @brief edge datafile marker
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_doc_edge_marker_s_deprecated {
  TRI_doc_document_marker_t_deprecated base;

  TRI_voc_cid_t _toCid;
  TRI_voc_did_t _toDid;

  TRI_voc_cid_t _fromCid;
  TRI_voc_did_t _fromDid;
}
TRI_doc_edge_marker_t_deprecated;

////////////////////////////////////////////////////////////////////////////////
/// @brief document datafile marker with key 
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_doc_document_key_marker_s {
  TRI_df_marker_t base;

  TRI_voc_rid_t _rid;        // this is the tick for an create and update
  TRI_voc_eid_t _sid;

  TRI_shape_sid_t _shape; 
 
  uint16_t _offsetKey; 
  uint16_t _offsetJson; 
}
TRI_doc_document_key_marker_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief edge datafile marker with key
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_doc_edge_key_marker_s {
  TRI_doc_document_key_marker_t base;

  TRI_voc_cid_t _toCid;  
  TRI_voc_cid_t _fromCid; 

  uint16_t _offsetToKey;
  uint16_t _offsetFromKey;
  uint8_t _isBidirectional;
}
TRI_doc_edge_key_marker_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief document datafile deletion marker
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_doc_deletion_marker_s_deprecated {
  TRI_df_marker_t base;

  TRI_voc_did_t _did;        // this is the tick for a create, but not an update
  TRI_voc_rid_t _rid;        // this is the tick for an create and update
  TRI_voc_eid_t _sid;
}
TRI_doc_deletion_marker_t_deprecated;

////////////////////////////////////////////////////////////////////////////////
/// @brief document datafile deletion marker
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_doc_deletion_key_marker_s {
  TRI_df_marker_t base;
  
  TRI_voc_rid_t _rid;        // this is the tick for an create and update
  TRI_voc_eid_t _sid;

  uint16_t _offsetKey;
}
TRI_doc_deletion_key_marker_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief document datafile begin transaction marker
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_doc_begin_transaction_marker_s {
  TRI_df_marker_t base;

  TRI_voc_tid_t _tid;
}
TRI_doc_begin_transaction_marker_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief document datafile commit transaction marker
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_doc_commit_transaction_marker_s {
  TRI_df_marker_t base;

  TRI_voc_tid_t _tid;
}
TRI_doc_commit_transaction_marker_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief document datafile abort transaction marker
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_doc_abort_transaction_marker_s {
  TRI_df_marker_t base;

  TRI_voc_tid_t _tid;
}
TRI_doc_abort_transaction_marker_t;

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

void TRI_InitPrimaryCollection (TRI_primary_collection_t*, TRI_shaper_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a primary collection
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyPrimaryCollection (TRI_primary_collection_t* collection);

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

TRI_doc_datafile_info_t* TRI_FindDatafileInfoPrimaryCollection (TRI_primary_collection_t* collection,
                                                                TRI_voc_fid_t fid);

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new journal
////////////////////////////////////////////////////////////////////////////////

TRI_datafile_t* TRI_CreateJournalPrimaryCollection (TRI_primary_collection_t* collection);

////////////////////////////////////////////////////////////////////////////////
/// @brief closes an existing journal
////////////////////////////////////////////////////////////////////////////////

bool TRI_CloseJournalPrimaryCollection (TRI_primary_collection_t* collection,
                                        size_t position);

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new compactor file
////////////////////////////////////////////////////////////////////////////////

TRI_datafile_t* TRI_CreateCompactorPrimaryCollection (TRI_primary_collection_t* collection);

////////////////////////////////////////////////////////////////////////////////
/// @brief closes an existing compactor file
////////////////////////////////////////////////////////////////////////////////

bool TRI_CloseCompactorPrimaryCollection (TRI_primary_collection_t* collection,
                                          size_t position);

////////////////////////////////////////////////////////////////////////////////
/// @brief Convert a raw data document pointer into a master pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_MarkerMasterPointer (void const*, TRI_doc_mptr_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts the data length from a master pointer
////////////////////////////////////////////////////////////////////////////////

size_t TRI_LengthDataMasterPointer (const TRI_doc_mptr_t* const);

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

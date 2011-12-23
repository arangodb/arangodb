////////////////////////////////////////////////////////////////////////////////
/// @brief document collection
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

#ifndef TRIAGENS_DURHAM_VOC_BASE_DOCUMENT_COLLECTION_H
#define TRIAGENS_DURHAM_VOC_BASE_DOCUMENT_COLLECTION_H 1

#include <VocBase/collection.h>

#include <BasicsC/json.h>
#include <VocBase/result-set.h>

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
  TRI_voc_did_t _did; // this is the document identifier
  TRI_voc_rid_t _rid; // this is the revision identifier
  TRI_voc_eid_t _eid; // this is the step identifier

  TRI_voc_fid_t _fid; // this is the datafile identifier

  TRI_voc_tick_t _deletion; // this is the deletion time

  void const* _data;                    // this is the pointer to the raw marker
  struct TRI_shaped_json_s _document;   // this is the pointer to the json document
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

  TRI_voc_ssize_t _numberAlive;
  TRI_voc_ssize_t _numberDead;
  TRI_voc_ssize_t _sizeAlive;
  TRI_voc_ssize_t _sizeDead;
  TRI_voc_ssize_t _numberDeletion;
}
TRI_doc_collection_info_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief document collection
///
/// A document collection is a collection of documents. These documents are
/// represented as @ref ShapedJson "shaped JSON objects". Each document has a
/// place in memory which is determined by the position in the memory mapped
/// file. As datafiles are compacted during garbage collection, this position
/// can change over time. Each active document also has a master pointer of
/// type @ref TRI_doc_mptr_t. This master pointer never changes and is
/// valid as long as the object is not deleted.
///
/// It is important to use transaction for create, read, update, and delete.
/// The functions "create", "createJson", "update", "updateJson", and "destroy"
/// are only allowed within a "beginWrite" and "endWrite". The function read is
/// only allowed with a "beginRead" and "endRead". Note that "read" returns
/// a master pointer. After the "endRead" this master pointer might no longer
/// be valid, because the document could be deleted by another thread after
/// "endRead" has been executed.
///
/// If a document is deleted, it's master pointer becomes invalid. However, the
/// document itself exists. Executing a query and constructing its result set,
/// see @ref TRI_result_set_t, must be done inside a "beginRead" and
/// "endRead". However, the result set itself does not contain any master
/// pointers. Therefore, it stays valid after the "endRead" call.
///
/// <b><tt>bool beginRead (TRI_doc_collection_t*)</tt></b>
///
/// Starts a read transaction. Query and calls to "read" are allowed within a
/// read transaction, but not calls to "create", "update", or "destroy".
/// Returns @c true if the transaction could be started. This call might block
/// until a running write transaction is finished.
///
/// <b><tt>bool endRead (TRI_doc_collection_t*)</tt></b>
///
/// Ends a read transaction. Should only be called after a successful
/// "beginRead".
///
/// <b><tt>bool beginWrite (TRI_doc_collection_t*)</tt></b>
///
/// Starts a write transaction. Query and calls to "create", "read", "update",
/// and "destroy" are allowed within a write transaction. Returns @c true if the
/// transaction could be started. This call might block until a running write
/// transaction is finished.
///
/// <b><tt>bool endWrite (TRI_doc_collection_t*)</tt></b>
///
/// Ends a write transaction. Should only be called after a successful
/// "beginWrite".
///
/// @FUN{void createHeader (TRI_doc_collection_t* @FA{collection}, TRI_datafile_t* @FA{datafile}, TRI_df_marker_t const* @FA{marker}, size_t @FA{markerSize}, TRI_doc_mptr_t* {mptr}, void const* @FA{data})}
///
/// Creates a new header.
///
/// @FUN{void updateHeader (TRI_doc_collection_t* @FA{collection}, TRI_datafile_t* @FA{datafile}, TRI_df_marker_t const* @FA{marker}, size_t @FA{markerSize}, TRI_doc_mptr_t const* {mptr}, TRI_doc_mptr_t* @FA{update})}
///
/// Updates an existing header.
///
/// <b><tt>TRI_doc_mptr_t const* create (TRI_doc_collection_t*, TRI_df_marker_type_e, TRI_shaped_json_t const*)</tt></b>
///
/// Adds a new document to the collection and returns the master pointer of the
/// newly created entry. In case of an error, NULL is returned and "TRI_errno()" is
/// set accordingly. The function DOES NOT acquire or release a write lock. This
/// must be done by the caller.
///
/// <b><tt>TRI_doc_mptr_t const* createJson (TRI_doc_collection_t*, TRI_df_marker_type_e, TRI_json_t const*)</tt></b>
///
/// Adds a new document to the collection and returns the master pointer of the
/// newly created entry. In case of an error, NULL is returned and "TRI_errno()"
/// is set accordingly. The function will acquire and release a write lock.
///
/// <b><tt>TRI_voc_did_t createLock (TRI_doc_collection_t*, TRI_df_marker_type_e, TRI_shaped_json_t const*)</tt></b>
///
/// Adds a new document to the collection and returns document identifier of the
/// newly created entry. In case of an error, NULL is returned and "TRI_errno()"
/// is set accordingly. The function will acquire and release a write lock.
///
/// <b><tt>TRI_doc_mptr_t const* read (TRI_doc_collection_t*, TRI_voc_did_t did)</tt></b>
///
/// Returns the master pointer of the document with identifier @c did. If the
/// document does not exists or is deleted, then @c NULL is returned. The function
/// DOES NOT acquire or release a read lock. This must be done by the caller.
///
/// <b><tt>TRI_doc_mptr_t const* update (TRI_doc_collection_t*, TRI_shaped_json_t const*, TRI_voc_did_t, TRI_voc_rid_t, TRI_doc_update_policy_e)</tt></b>
///
/// Updates an existing document of the collection and returns the master
/// pointer in case of success. Otherwise, @c false is returned and the
/// "TRI_errno()" is accordingly. The function DOES NOT acquire or release a
/// write lock. This must be done by the caller.
///
/// If the policy is @c TRI_DOC_UPDATE_LAST_WRITE, than the revision is ignored
/// and the update is always performed. If the policy is @c TRI_DOC_UPDATE_ERROR
/// and the revision is given (i. e. not equal 0), then the update is only
/// performed if the current revision matches the given.
///
/// <b><tt>TRI_doc_mptr_t const* updateJson (TRI_doc_collection_t*, TRI_json_t const*, TRI_voc_did_t, TRI_voc_rid_t, TRI_doc_update_policy_e)</tt></b>
///
/// Updates an existing document of the collection and returns the master
/// pointer in case of success. Otherwise, @c false is returned and the
/// "TRI_errno()" is accordingly. The function will acquire and release a write
/// lock.
///
/// <b><tt>bool updateLock (TRI_doc_collection_t*, TRI_shaped_json_t const*, TRI_voc_did_t, TRI_voc_rid_t, TRI_doc_update_policy_e)</tt></b>
///
/// Updates an existing document of the collection and returns @c true in case
/// of success. Otherwise, @c false is returned and the "TRI_errno()" is
/// accordingly. The function will acquire and release a write lock.
///
/// <b><tt>bool destroy (TRI_doc_collection_t*, TRI_voc_did_t, TRI_voc_rid_t, TRI_doc_update_policy_e)</tt></b>
///
/// Deletes an existing document of the collection and returns @c true in case
/// of success. Otherwise, @c false is returned and the "TRI_errno()" is
/// accordingly. The function DOES NOT acquire or release a write lock.
///
/// If the policy is @c TRI_DOC_UPDATE_ERROR and the reivision is given, than
/// it must match the current revision of the document.
///
/// <b><tt>bool destroyLock (TRI_doc_collection_t*, TRI_voc_did_t, TRI_voc_rid_t, TRI_doc_update_policy_e)</tt></b>
///
/// Deletes an existing document of the collection and returns @c true in case
/// of success. Otherwise, @c false is returned and the "TRI_errno()" is
/// accordingly. The function will acquire and release a write lock.
///
/// <b><tt>TRI_doc_collection_info_t* figures (TRI_doc_collection_t*)</tt></b>
///
/// Returns informatiom about the collection. You must hold a read lock and
/// must destroy the result after usage.
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_doc_collection_s {
  TRI_collection_t base;

  TRI_shaper_t* _shaper;
  TRI_rs_container_t _resultSets;
  TRI_associative_pointer_t _datafileInfo;

  bool (*beginRead) (struct TRI_doc_collection_s*);
  bool (*endRead) (struct TRI_doc_collection_s*);

  bool (*beginWrite) (struct TRI_doc_collection_s*);
  bool (*endWrite) (struct TRI_doc_collection_s*);

  void (*createHeader) (struct TRI_doc_collection_s*, TRI_datafile_t*, TRI_df_marker_t const*, size_t, TRI_doc_mptr_t*, void const* data);
  void (*updateHeader) (struct TRI_doc_collection_s*, TRI_datafile_t*, TRI_df_marker_t const*, size_t, TRI_doc_mptr_t const*, TRI_doc_mptr_t*);

  TRI_doc_mptr_t const* (*create) (struct TRI_doc_collection_s*, TRI_df_marker_type_e, TRI_shaped_json_t const*, void const*);
  TRI_doc_mptr_t const* (*createJson) (struct TRI_doc_collection_s*, TRI_df_marker_type_e, TRI_json_t const*, void const*);
  TRI_voc_did_t (*createLock) (struct TRI_doc_collection_s*, TRI_df_marker_type_e, TRI_shaped_json_t const*, void const*);

  TRI_doc_mptr_t const* (*read) (struct TRI_doc_collection_s*, TRI_voc_did_t);

  TRI_doc_mptr_t const* (*update) (struct TRI_doc_collection_s*, TRI_shaped_json_t const*, TRI_voc_did_t, TRI_voc_rid_t, TRI_doc_update_policy_e);
  TRI_doc_mptr_t const* (*updateJson) (struct TRI_doc_collection_s*, TRI_json_t const*, TRI_voc_did_t, TRI_voc_rid_t, TRI_doc_update_policy_e);
  bool (*updateLock) (struct TRI_doc_collection_s*, TRI_shaped_json_t const*, TRI_voc_did_t, TRI_voc_rid_t, TRI_doc_update_policy_e);

  bool (*destroy) (struct TRI_doc_collection_s* collection, TRI_voc_did_t, TRI_voc_rid_t, TRI_doc_update_policy_e);
  bool (*destroyLock) (struct TRI_doc_collection_s* collection, TRI_voc_did_t, TRI_voc_rid_t, TRI_doc_update_policy_e);

  TRI_doc_collection_info_t* (*figures) (struct TRI_doc_collection_s* collection);
}
TRI_doc_collection_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief document datafile marker
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_doc_document_marker_s {
  TRI_df_marker_t base;

  TRI_voc_did_t _did;        // this is the tick for a create, but not an update
  TRI_voc_rid_t _rid;        // this is the tick for an create and update
  TRI_voc_eid_t _sid;

  TRI_shape_sid_t _shape;

  // char data[]
}
TRI_doc_document_marker_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief edge datafile marker
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_doc_edge_marker_s {
  TRI_doc_document_marker_t base;

  TRI_voc_cid_t _toCid;
  TRI_voc_did_t _toDid;

  TRI_voc_cid_t _fromCid;
  TRI_voc_did_t _fromDid;

  // char data[]
}
TRI_doc_edge_marker_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief document datafile deletion marker
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_doc_deletion_marker_s {
  TRI_df_marker_t base;

  TRI_voc_did_t _did;        // this is the tick for a create, but not an update
  TRI_voc_rid_t _rid;        // this is the tick for an create and update
  TRI_voc_eid_t _sid;
}
TRI_doc_deletion_marker_t;

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
/// @brief initializes a document collection structure
////////////////////////////////////////////////////////////////////////////////

void TRI_InitDocCollection (TRI_doc_collection_t*, TRI_shaper_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a document collection
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyDocCollection (TRI_doc_collection_t* collection);

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

TRI_doc_datafile_info_t* TRI_FindDatafileInfoDocCollection (TRI_doc_collection_t* collection,
                                                            TRI_voc_fid_t fid);

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new journal
////////////////////////////////////////////////////////////////////////////////

TRI_datafile_t* TRI_CreateJournalDocCollection (TRI_doc_collection_t* collection);

////////////////////////////////////////////////////////////////////////////////
/// @brief closes an existing journal
////////////////////////////////////////////////////////////////////////////////

bool TRI_CloseJournalDocCollection (TRI_doc_collection_t* collection,
                                    size_t position);

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new compactor file
////////////////////////////////////////////////////////////////////////////////

TRI_datafile_t* TRI_CreateCompactorDocCollection (TRI_doc_collection_t* collection);

////////////////////////////////////////////////////////////////////////////////
/// @brief closes an existing compactor file
////////////////////////////////////////////////////////////////////////////////

bool TRI_CloseCompactorDocCollection (TRI_doc_collection_t* collection,
                                      size_t position);

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

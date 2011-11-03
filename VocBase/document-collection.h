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

#ifndef TRIAGENS_DURHAM_VOCBASE_DOCUMENT_COLLECTION_H
#define TRIAGENS_DURHAM_VOCBASE_DOCUMENT_COLLECTION_H 1

#include <VocBase/collection.h>

#include <Basics/json.h>
#include <VocBase/result-set.h>

#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief master pointer
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_doc_mptr_s {
  TRI_voc_did_t _did; // this is the document identifier
  TRI_voc_rid_t _rid; // this is the revision identifier
  TRI_voc_eid_t _eid; // this is the step identifier

  TRI_voc_tick_t _deletion; // this is the deletion time

  void const* _data;                    // this is the pointer to the raw marker
  struct TRI_shaped_json_s _document;   // this is the pointer to the json document
}
TRI_doc_mptr_t;

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
/// <tt>bool beginRead (TRI_doc_collection_t*)</tt>
///
/// Starts a read transaction. Query and calls to "read" are allowed within a
/// read transaction, but not calls to "create", "update", or "destroy".
/// Returns @c true if the transaction could be started. This call might block
/// until a running write transaction is finished.
///
/// <tt>bool endRead (TRI_doc_collection_t*)</tt>
///
/// Ends a read transaction. Should only be called after a successful
/// "beginRead".
///
/// <tt>bool beginWrite (TRI_doc_collection_t*)</tt>
///
/// Starts a write transaction. Query and calls to "create", "read", "update",
/// and "destroy" are allowed within a write transaction. Returns @c true if the
/// transaction could be started. This call might block until a running write
/// transaction is finished.
///
/// <tt>bool endWrite (TRI_doc_collection_t*)</tt>
///
/// Ends a write transaction. Should only be called after a successful
/// "beginWrite".
///
/// <tt>TRI_voc_did_t create (TRI_doc_collection_t*, TRI_shaped_json_t const*)</tt>
///
/// Adds a new document to the collection and returns the document identifier of
/// the newly created entry. In case of an error, 0 is returned and
/// "TRI_errno()" is set accordingly.
///
/// <tt>TRI_voc_did_t createJson (TRI_doc_collection_t*, TRI_json_t const*)</tt>
///
/// Adds a new document to the collection and returns the document identifier of
/// the newly created entry. In case of an error, 0 is returned and
/// "TRI_errno()" is set accordingly.
///
/// <tt>TRI_doc_mptr_t const* read (TRI_doc_collection_t*, TRI_voc_did_t did)</tt>
///
/// Returns the master pointer of the document with identifier @c did. If the
/// document does not exists or is deleted, then @c NULL is returned.
///
/// <tt>bool update (TRI_doc_collection_t*, TRI_shaped_json_t const*, TRI_voc_did_t)</tt>
///
/// Updates an existing document of the collection and returns @c true in case
/// of success. Otherwise, @c false is returned and the "TRI_errno()" is
/// accordingly.
///
/// <tt>bool updateJson (TRI_doc_collection_t*, TRI_json_t const*, TRI_voc_did_t)</tt>
///
/// Updates an existing document of the collection and returns @c true in case
/// of success. Otherwise, @c false is returned and the "TRI_errno()" is
/// accordingly.
///
/// <tt>bool destroy (TRI_doc_collection_t*, TRI_voc_did_t)</tt>
///
/// Deletes an existing document of the collection and returns @c true in case
/// of success. Otherwise, @c false is returned and the "TRI_errno()" is
/// accordingly.
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_doc_collection_s {
  TRI_collection_t base;

  TRI_shaper_t* _shaper;
  TRI_rs_container_t _resultSets;

  bool (*beginRead) (struct TRI_doc_collection_s*);
  bool (*endRead) (struct TRI_doc_collection_s*);

  bool (*beginWrite) (struct TRI_doc_collection_s*);
  bool (*endWrite) (struct TRI_doc_collection_s*);

  TRI_voc_did_t (*create) (struct TRI_doc_collection_s*, TRI_shaped_json_t const*);
  TRI_voc_did_t (*createJson) (struct TRI_doc_collection_s*, TRI_json_t const*);

  TRI_doc_mptr_t const* (*read) (struct TRI_doc_collection_s*, TRI_voc_did_t);

  bool (*update) (struct TRI_doc_collection_s*, TRI_shaped_json_t const*, TRI_voc_did_t);
  bool (*updateJson) (struct TRI_doc_collection_s*, TRI_json_t const*, TRI_voc_did_t);

  bool (*destroy) (struct TRI_doc_collection_s* collection, TRI_voc_did_t);
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
/// @addtogroup VocBase VocBase
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
/// @addtogroup VocBase VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

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

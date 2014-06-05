///////////////////////////////////////////////////////////////////////////////
/// @brief document collection with global read-write lock, derived from
/// TRI_document_collection_t
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

#include "Basics/Common.h"

#include "VocBase/barrier.h"
#include "VocBase/collection.h"
#include "VocBase/headers.h"
#include "VocBase/index.h"
#include "VocBase/primary-index.h"
#include "VocBase/transaction.h"
#include "VocBase/update-policy.h"
#include "VocBase/voc-types.h"
#include "Wal/Marker.h"

#include <regex.h>

// -----------------------------------------------------------------------------
// --SECTION--                                              forward declarations
// -----------------------------------------------------------------------------

struct TRI_cap_constraint_s;
struct TRI_df_marker_s;
struct TRI_doc_deletion_key_marker_s;
struct TRI_doc_document_key_marker_s;
struct TRI_doc_update_policy_s;
struct TRI_document_edge_s;
struct TRI_index_s;
struct TRI_json_s;
struct TRI_key_generator_s;

namespace triagens {
  namespace arango {
    class TransactionBase;
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                     public macros
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief read locks the documents and indexes
////////////////////////////////////////////////////////////////////////////////

#define TRI_READ_LOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(a) \
  TRI_ReadLockReadWriteLock(&(a)->_lock)

////////////////////////////////////////////////////////////////////////////////
/// @brief tries to read lock the documents and indexes
////////////////////////////////////////////////////////////////////////////////

#define TRI_TRY_READ_LOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(a) \
  TRI_TryReadLockReadWriteLock(&(a)->_lock)

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
/// @brief tries to write lock the documents and indexes
////////////////////////////////////////////////////////////////////////////////

#define TRI_TRY_WRITE_LOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(a) \
  TRI_TryWriteLockReadWriteLock(&(a)->_lock)

////////////////////////////////////////////////////////////////////////////////
/// @brief write unlocks the documents and indexes
////////////////////////////////////////////////////////////////////////////////

#define TRI_WRITE_UNLOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(a) \
  TRI_WriteUnlockReadWriteLock(&(a)->_lock)

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief master pointer
////////////////////////////////////////////////////////////////////////////////

struct TRI_doc_mptr_t {
    TRI_voc_rid_t          _rid;     // this is the revision identifier
    TRI_voc_fid_t          _fid;     // this is the datafile identifier
  private:
    void const*            _dataptr; // this is the pointer to the beginning of the raw marker
  public:
    uint64_t               _hash;    // the pre-calculated hash value of the key
    TRI_doc_mptr_t*        _prev;    // previous master pointer
    TRI_doc_mptr_t*        _next;    // next master pointer

    TRI_doc_mptr_t () : _rid(0), _fid(0), _hash(0), _prev(nullptr), 
                        _next(nullptr) {
    }

    void clear () {
      _rid = 0;
      _fid = 0;
      setDataPtr(nullptr);
      _hash = 0;
      _prev = nullptr;
      _next = nullptr;
    }

    void const* getDataPtr () const {
      return _dataptr;
    }

    void setDataPtr (void const* d) {
      _dataptr = d;
    }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief datafile info
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_doc_datafile_info_s {
  TRI_voc_fid_t   _fid;

  TRI_voc_ssize_t _numberAlive;
  TRI_voc_ssize_t _numberDead;
  TRI_voc_ssize_t _numberDeletion;
  TRI_voc_ssize_t _numberTransaction; // populated only during compaction
  TRI_voc_ssize_t _numberShapes;
  TRI_voc_ssize_t _numberAttributes;

  int64_t         _sizeAlive;
  int64_t         _sizeDead;
  int64_t         _sizeTransaction;   // populated only during compaction
  int64_t         _sizeShapes;
  int64_t         _sizeAttributes;
}
TRI_doc_datafile_info_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief collection info
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_doc_collection_info_s {
  TRI_voc_ssize_t _numberDatafiles;
  TRI_voc_ssize_t _numberJournalfiles;
  TRI_voc_ssize_t _numberCompactorfiles;
  TRI_voc_ssize_t _numberShapefiles;

  TRI_voc_ssize_t _numberAlive;
  TRI_voc_ssize_t _numberDead;
  TRI_voc_ssize_t _numberDeletion;
  TRI_voc_ssize_t _numberTransaction; // populated only during compaction
  TRI_voc_ssize_t _numberShapes;
  TRI_voc_ssize_t _numberAttributes;
  TRI_voc_ssize_t _numberIndexes;

  int64_t         _sizeAlive;
  int64_t         _sizeDead;
  int64_t         _sizeTransaction;   // populated only during compaction
  int64_t         _sizeShapes;  
  int64_t         _sizeAttributes; 
  int64_t         _sizeIndexes; 

  int64_t         _datafileSize;
  int64_t         _journalfileSize;
  int64_t         _compactorfileSize;
  int64_t         _shapefileSize;
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
/// @FUN{int beginRead (TRI_document_collection_t*)}
///////////////////////////////////////////////
///
/// Starts a read transaction. Query and calls to @FN{read} are allowed within a
/// read transaction, but not calls to @FN{create}, @FN{update}, or
/// @FN{destroy}.  Returns @ref TRI_ERROR_NO_ERROR if the transaction could be
/// started. This call might block until a running write transaction is
/// finished.
///
/// @FUN{int endRead (TRI_document_collection_t*)}
/////////////////////////////////////////////
///
/// Ends a read transaction. Should only be called after a successful
/// "beginRead".
///
/// @FUN{int beginWrite (TRI_document_collection_t*)}
////////////////////////////////////////////////
///
/// Starts a write transaction. Query and calls to @FN{create}, @FN{read},
/// @FN{update}, and @FN{destroy} are allowed within a write
/// transaction. Returns @ref TRI_ERROR_NO_ERROR if the transaction could be
/// started. This call might block until a running write transaction is
/// finished.
///
/// @FUN{int endWrite (TRI_document_collection_t*)}
//////////////////////////////////////////////
///
/// Ends a write transaction. Should only be called after a successful
/// @LIT{beginWrite}.
///
/// @FUN{TRI_doc_mptr_t const create (TRI_document_collection_t*, TRI_df_marker_type_e, TRI_shaped_json_t const*, bool @FA{release})}
///
/// Adds a new document to the collection and returns the master pointer of the
/// newly created entry. In case of an error, the attribute @LIT{_did} of the
/// result is @LIT{0} and "TRI_errno()" is set accordingly. The function DOES
/// NOT acquire a write lock. This must be done by the caller. If @FA{release}
/// is true, it will release the write lock as soon as possible.
///
/// @FUN{TRI_doc_mptr_t const read (TRI_document_collection_t*, TRI_voc_key_t)}
//////////////////////////////////////////////////////////////////////////
///
/// Returns the master pointer of the document with the given identifier. If the
/// document does not exist or is deleted, then the identifier @LIT{_did} of
/// the result is @LIT{0}. The function DOES NOT acquire or release a read
/// lock. This must be done by the caller.
///
/// @FUN{TRI_doc_mptr_t const update (TRI_document_collection_t*, TRI_shaped_json_t const*, TRI_voc_key_t, TRI_voc_rid_t @FA{rid}, TRI_voc_rid_t* @FA{current}, TRI_doc_update_policy_e, bool @FA{release})}
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
/// @FUN{int destroy (TRI_document_collection_t*, TRI_voc_key_t, TRI_voc_rid_t, TRI_voc_rid_t @FA{rid}, TRI_voc_rid_t* @FA{current}, TRI_doc_update_policy_e, bool @FA{release})}
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
/// @FUN{TRI_doc_collection_info_t* figures (TRI_document_collection_t*)}
////////////////////////////////////////////////////////////////////
///
/// Returns informatiom about the collection. You must hold a read lock and must
/// destroy the result after usage.
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief primary collection with global read-write lock
///
/// A primary collection is a collection with a single read-write lock. This
/// lock is used to coordinate the read and write transactions.
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_document_collection_s {
  TRI_collection_t             base;

  // .............................................................................
  // this lock protects the _primaryIndex plus the _allIndexes
  // and _headers attributes in derived types
  // .............................................................................

  TRI_read_write_lock_t        _lock;

  TRI_shaper_t*                _shaper;
  TRI_barrier_list_t           _barrierList;
  TRI_associative_pointer_t    _datafileInfo;

  TRI_primary_index_t          _primaryIndex;
  TRI_headers_t*               _headersPtr;
  struct TRI_key_generator_s*  _keyGenerator;
  struct TRI_cap_constraint_s* _capConstraint;
  
  TRI_vector_pointer_t         _allIndexes;
  TRI_vector_t                 _failedTransactions;
  
  int64_t                      _numberDocuments;
  TRI_read_write_lock_t        _compactionLock;
  double                       _lastCompaction;

  // this lock protected _lastWrittenId and _lastCollectedId
  TRI_spin_t                   _idLock;
  TRI_voc_tick_t               _lastWrittenId;
  TRI_voc_tick_t               _lastCollectedId;

  // .............................................................................
  // this condition variable protects the _journalsCondition
  // .............................................................................

  TRI_condition_t          _journalsCondition;

  // whether or not any of the indexes may need to be garbage-collected
  // this flag may be modifying when an index is added to a collection
  // if true, the cleanup thread will periodically call the cleanup functions of
  // the collection's indexes that support cleanup
  bool                     _cleanupIndexes;

  int (*beginRead) (struct TRI_document_collection_s*);
  int (*endRead) (struct TRI_document_collection_s*);

  int (*beginWrite) (struct TRI_document_collection_s*);
  int (*endWrite) (struct TRI_document_collection_s*);
  
  int (*beginReadTimed) (struct TRI_document_collection_s*, uint64_t, uint64_t);
  int (*beginWriteTimed) (struct TRI_document_collection_s*, uint64_t, uint64_t);

#ifdef TRI_ENABLE_MAINTAINER_MODE
  void (*dump) (struct TRI_document_collection_s*);
#endif 
 
  TRI_doc_collection_info_t* (*figures) (struct TRI_document_collection_s* collection);
  TRI_voc_size_t (*size) (struct TRI_document_collection_s* collection);

  
  // WAL-based CRUD methods
  int (*updateDocument) (struct TRI_transaction_collection_s*, TRI_voc_key_t, TRI_voc_rid_t, TRI_doc_mptr_t*, TRI_shaped_json_t const*, struct TRI_doc_update_policy_s const*, bool, bool);
  int (*removeDocument) (struct TRI_transaction_collection_s*, TRI_voc_key_t, TRI_voc_rid_t, struct TRI_doc_update_policy_s const*, bool, bool);
  int (*insertDocument) (struct TRI_transaction_collection_s*, TRI_voc_key_t, TRI_voc_rid_t, TRI_doc_mptr_t*, TRI_df_marker_type_e, TRI_shaped_json_t const*, struct TRI_document_edge_s const*, bool, bool, bool);
  int (*readDocument) (struct TRI_transaction_collection_s*, const TRI_voc_key_t, TRI_doc_mptr_t*, bool);

  // function that is called to garbage-collect the collection's indexes
  int (*cleanupIndexes)(struct TRI_document_collection_s*);
}
TRI_document_collection_t;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief initializes a primary collection structure
////////////////////////////////////////////////////////////////////////////////

int TRI_InitPrimaryCollection (TRI_document_collection_t*, TRI_shaper_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a primary collection
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyPrimaryCollection (TRI_document_collection_t*);

// -----------------------------------------------------------------------------
// --SECTION--                                               protected functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief removes a datafile description
////////////////////////////////////////////////////////////////////////////////

void TRI_RemoveDatafileInfoPrimaryCollection (TRI_document_collection_t*,
                                              TRI_voc_fid_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief finds a datafile description
////////////////////////////////////////////////////////////////////////////////

TRI_doc_datafile_info_t* TRI_FindDatafileInfoPrimaryCollection (TRI_document_collection_t*,
                                                                TRI_voc_fid_t,
                                                                bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new journal
////////////////////////////////////////////////////////////////////////////////

TRI_datafile_t* TRI_CreateJournalPrimaryCollection (TRI_document_collection_t*,
                                                    TRI_voc_size_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief closes an existing journal
////////////////////////////////////////////////////////////////////////////////

bool TRI_CloseJournalPrimaryCollection (TRI_document_collection_t*,
                                        size_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new compactor file
////////////////////////////////////////////////////////////////////////////////

TRI_datafile_t* TRI_CreateCompactorPrimaryCollection (TRI_document_collection_t*,
                                                      TRI_voc_fid_t,
                                                      TRI_voc_size_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief closes an existing compactor file
////////////////////////////////////////////////////////////////////////////////

bool TRI_CloseCompactorPrimaryCollection (TRI_document_collection_t*,
                                          size_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief dump information about all datafiles of a collection
////////////////////////////////////////////////////////////////////////////////

void TRI_DebugDatafileInfoPrimaryCollection (TRI_document_collection_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief iterate over all documents in the collection, using a user-defined
/// callback function. Returns the total number of documents in the collection
///
/// The user can abort the iteration by return "false" from the callback
/// function.
///
/// Note: the function will not acquire any locks. It is the task of the caller
/// to ensure the collection is properly locked
////////////////////////////////////////////////////////////////////////////////

size_t TRI_DocumentIteratorPrimaryCollection (triagens::arango::TransactionBase const*,
                                              TRI_document_collection_t*,
                                              void*,
                                              bool (*callback)(TRI_doc_mptr_t const*, 
                                              TRI_document_collection_t*, void*));



// -----------------------------------------------------------------------------
// --SECTION--                                               DOCUMENT COLLECTION
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                     public macros
// -----------------------------------------------------------------------------

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
/// @brief extracts the pointer to the key from a marker
////////////////////////////////////////////////////////////////////////////////

static inline char const* TRI_EXTRACT_MARKER_KEY (TRI_doc_mptr_t const* mptr) {
  TRI_df_marker_t const* marker = static_cast<TRI_df_marker_t const*>(mptr->getDataPtr());  // PROTECTED by TRI_EXTRACT_MARKER_KEY search
  if (marker->_type == TRI_DOC_MARKER_KEY_DOCUMENT || marker->_type == TRI_DOC_MARKER_KEY_EDGE) {
    return ((char const*) mptr->getDataPtr()) + ((TRI_doc_document_key_marker_t const*) mptr->getDataPtr())->_offsetKey;  // PROTECTED by TRI_EXTRACT_MARKER_KEY search
  }
  else if (marker->_type == TRI_WAL_MARKER_DOCUMENT || marker->_type == TRI_WAL_MARKER_EDGE) {
    return ((char const*) mptr->getDataPtr()) + ((triagens::wal::document_marker_t const*) mptr->getDataPtr())->_offsetKey;  // PROTECTED by TRI_EXTRACT_MARKER_KEY search
  }
  return nullptr;
}

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

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

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief update the "last written" value for a collection
////////////////////////////////////////////////////////////////////////////////

void TRI_SetLastWrittenDocumentCollection (TRI_document_collection_t*, 
                                           TRI_voc_tick_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief update the "last collected" value for a collection
////////////////////////////////////////////////////////////////////////////////

void TRI_SetLastCollectedDocumentCollection (TRI_document_collection_t*, 
                                             TRI_voc_tick_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not markers of a collection were fully collected
////////////////////////////////////////////////////////////////////////////////

bool TRI_IsFullyCollectedDocumentCollection (TRI_document_collection_t*); 

////////////////////////////////////////////////////////////////////////////////
/// @brief create an index, based on a JSON description
////////////////////////////////////////////////////////////////////////////////
  
int TRI_FromJsonIndexDocumentCollection (TRI_document_collection_t*,
                                         struct TRI_json_s const*,
                                         struct TRI_index_s**);

////////////////////////////////////////////////////////////////////////////////
/// @brief rolls back a document operation
////////////////////////////////////////////////////////////////////////////////

int TRI_RollbackOperationDocumentCollection (TRI_document_collection_t*,
                                             TRI_voc_document_operation_e,
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
/// @brief creates a new journal
////////////////////////////////////////////////////////////////////////////////

TRI_datafile_t* TRI_CreateJournalDocumentCollection (TRI_document_collection_t*,
                                                     TRI_voc_size_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief closes an existing journal
////////////////////////////////////////////////////////////////////////////////

bool TRI_CloseJournalDocumentCollection (TRI_document_collection_t*, 
                                         size_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief opens an existing collection
////////////////////////////////////////////////////////////////////////////////

TRI_document_collection_t* TRI_OpenDocumentCollection (TRI_vocbase_t*, char const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief closes an open collection
////////////////////////////////////////////////////////////////////////////////

int TRI_CloseDocumentCollection (TRI_document_collection_t*);

// -----------------------------------------------------------------------------
// --SECTION--                                                           INDEXES
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a description of all indexes
///
/// the caller must have read-locked the underyling collection!
////////////////////////////////////////////////////////////////////////////////

TRI_vector_pointer_t* TRI_IndexesDocumentCollection (TRI_document_collection_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief drops an index, including index file removal and replication
////////////////////////////////////////////////////////////////////////////////

bool TRI_DropIndexDocumentCollection (TRI_document_collection_t*, 
                                      TRI_idx_iid_t,
                                      TRI_server_id_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief drops an index, without index file removal and replication
////////////////////////////////////////////////////////////////////////////////

bool TRI_DropIndex2DocumentCollection (TRI_document_collection_t*, 
                                       TRI_idx_iid_t,
                                       TRI_server_id_t);

// -----------------------------------------------------------------------------
// --SECTION--                                                    CAP CONSTRAINT
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up a cap constraint
////////////////////////////////////////////////////////////////////////////////

TRI_index_t* TRI_LookupCapConstraintDocumentCollection (TRI_document_collection_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures that a cap constraint exists
////////////////////////////////////////////////////////////////////////////////

TRI_index_t* TRI_EnsureCapConstraintDocumentCollection (TRI_document_collection_t*,
                                                        TRI_idx_iid_t,
                                                        size_t,
                                                        int64_t,
                                                        bool*,
                                                        TRI_server_id_t);

// -----------------------------------------------------------------------------
// --SECTION--                                                    BITARRAY INDEX
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

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
                                                               TRI_idx_iid_t,
                                                               const TRI_vector_pointer_t*,
                                                               const TRI_vector_pointer_t*,
                                                               bool,
                                                               bool*,
                                                               int*,
                                                               char**,
                                                               TRI_server_id_t);

// -----------------------------------------------------------------------------
// --SECTION--                                                         GEO INDEX
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief finds a geo index, list style
///
/// Note that the caller must hold at least a read-lock.
////////////////////////////////////////////////////////////////////////////////

struct TRI_index_s* TRI_LookupGeoIndex1DocumentCollection (TRI_document_collection_t*,
                                                           char const*,
                                                           bool,
                                                           bool,
                                                           bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief finds a geo index, attribute style
///
/// Note that the caller must hold at least a read-lock.
////////////////////////////////////////////////////////////////////////////////

struct TRI_index_s* TRI_LookupGeoIndex2DocumentCollection (TRI_document_collection_t*,
                                                           char const*,
                                                           char const*,
                                                           bool,
                                                           bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures that a geo index exists, list style
////////////////////////////////////////////////////////////////////////////////

struct TRI_index_s* TRI_EnsureGeoIndex1DocumentCollection (TRI_document_collection_t*,
                                                           TRI_idx_iid_t,
                                                           char const*,
                                                           bool,
                                                           bool,
                                                           bool,
                                                           bool*,
                                                           TRI_server_id_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures that a geo index exists, attribute style
////////////////////////////////////////////////////////////////////////////////

struct TRI_index_s* TRI_EnsureGeoIndex2DocumentCollection (TRI_document_collection_t*,
                                                           TRI_idx_iid_t,
                                                           char const*,
                                                           char const*,
                                                           bool,
                                                           bool,
                                                           bool*,
                                                           TRI_server_id_t);

// -----------------------------------------------------------------------------
// --SECTION--                                                        HASH INDEX
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

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
                                                           TRI_idx_iid_t,
                                                           TRI_vector_pointer_t const*,
                                                           bool,
                                                           bool*,
                                                           TRI_server_id_t);

// -----------------------------------------------------------------------------
// --SECTION--                                                    SKIPLIST INDEX
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

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
                                                               TRI_idx_iid_t,
                                                               TRI_vector_pointer_t const*,
                                                               bool,
                                                               bool*,
                                                               TRI_server_id_t);

// -----------------------------------------------------------------------------
// --SECTION--                                                    FULLTEXT INDEX
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

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
                                                               TRI_idx_iid_t,
                                                               const char*,
                                                               const bool,
                                                               int,
                                                               bool*,
                                                               TRI_server_id_t);

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a select-by-example query
////////////////////////////////////////////////////////////////////////////////

void TRI_SelectByExample (struct TRI_transaction_collection_s*,
                          size_t,
                          TRI_shape_pid_t*,
                          TRI_shaped_json_t**,
                          std::vector<TRI_doc_mptr_t>&);

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes a documet given by a master pointer
////////////////////////////////////////////////////////////////////////////////

int TRI_DeleteDocumentDocumentCollection (struct TRI_transaction_collection_s*,
                                          struct TRI_doc_update_policy_s const*,
                                          TRI_doc_mptr_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief set the collection revision
////////////////////////////////////////////////////////////////////////////////

void TRI_SetRevisionDocumentCollection (TRI_document_collection_t*,
                                        TRI_voc_rid_t, 
                                        bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief rotate the current journal of the collection
/// use this for testing only
////////////////////////////////////////////////////////////////////////////////

int TRI_RotateJournalDocumentCollection (TRI_document_collection_t*);

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:

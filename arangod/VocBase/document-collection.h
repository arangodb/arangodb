////////////////////////////////////////////////////////////////////////////////
/// @brief document collection with global read-write lock, derived from
/// TRI_document_collection_t
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_VOC_BASE_DOCUMENT__COLLECTION_H
#define ARANGODB_VOC_BASE_DOCUMENT__COLLECTION_H 1

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
struct TRI_document_edge_s;
struct TRI_index_s;
struct TRI_json_t;

class KeyGenerator;

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
    uint64_t               _hash;    // the pre-calculated hash value of the key
    TRI_doc_mptr_t*        _prev;    // previous master pointer
    TRI_doc_mptr_t*        _next;    // next master pointer
  protected:
    void const*            _dataptr; // this is the pointer to the beginning of the raw marker

  public:
    TRI_doc_mptr_t () : _rid(0), 
                        _fid(0), 
                        _hash(0),
                        _prev(nullptr),
                        _next(nullptr),
                        _dataptr(nullptr) {
    }

    virtual ~TRI_doc_mptr_t () {
    }

    void clear () {
      _rid = 0;
      _fid = 0;
      setDataPtr(nullptr);
      _hash = 0;
      _prev = nullptr;
      _next = nullptr;
    }

    void copy (TRI_doc_mptr_t const& that) {
      // This is for cases where we explicitly have to copy originals!
      _rid = that._rid;
      _fid = that._fid;
      _dataptr = that._dataptr;
      _hash = that._hash;
      _prev = that._prev;
      _next = that._next;
    }

////////////////////////////////////////////////////////////////////////////////
/// @brief return a pointer to the beginning of the marker
////////////////////////////////////////////////////////////////////////////////

#ifndef TRI_ENABLE_MAINTAINER_MODE
    inline void const* getDataPtr () const {
      return _dataptr;
    }
#else
    // The actual code has an assertion about transactions!
    virtual void const* getDataPtr () const;
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief set the pointer to the beginning of the memory for the marker
////////////////////////////////////////////////////////////////////////////////

#ifndef TRI_ENABLE_MAINTAINER_MODE
    inline void setDataPtr (void const* d) {
      _dataptr = d;
    }
#else
    // The actual code has an assertion about transactions!
    virtual void setDataPtr (void const* d);
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief return a pointer to the beginning of the shaped json stored in the
/// marker
////////////////////////////////////////////////////////////////////////////////

    char const* getShapedJsonPtr () const {
      TRI_df_marker_t const* marker = static_cast<TRI_df_marker_t const*>(_dataptr);

      TRI_ASSERT(marker != nullptr);

      if (marker->_type == TRI_DOC_MARKER_KEY_DOCUMENT ||
          marker->_type == TRI_DOC_MARKER_KEY_EDGE) {
        auto offset = (reinterpret_cast<TRI_doc_document_key_marker_t const*>(marker))->_offsetJson;
        return static_cast<char const*>(_dataptr) + offset;
      }
      else if (marker->_type == TRI_WAL_MARKER_DOCUMENT ||
               marker->_type == TRI_WAL_MARKER_EDGE) {
        auto offset = (reinterpret_cast<triagens::wal::document_marker_t const*>(marker))->_offsetJson;
        return static_cast<char const*>(_dataptr) + offset;
      }

      TRI_ASSERT(false);

      return nullptr;
    }

    TRI_doc_mptr_t& operator= (TRI_doc_mptr_t const&) = delete;
    TRI_doc_mptr_t(TRI_doc_mptr_t const&) = delete;

};

////////////////////////////////////////////////////////////////////////////////
/// @brief A derived class for copies of master pointers, they
////////////////////////////////////////////////////////////////////////////////

struct TRI_doc_mptr_copy_t : public TRI_doc_mptr_t {
    TRI_doc_mptr_copy_t () : TRI_doc_mptr_t() {
    }

    TRI_doc_mptr_copy_t (TRI_doc_mptr_copy_t const& that)
      : TRI_doc_mptr_t() {
      copy(that);
    }

    TRI_doc_mptr_copy_t (TRI_doc_mptr_t const& that)
      : TRI_doc_mptr_t() {
      copy(that);
    }

    TRI_doc_mptr_copy_t& operator= (TRI_doc_mptr_copy_t const& that) {
      copy(that);
      return *this;
    }

    TRI_doc_mptr_copy_t const& operator= (TRI_doc_mptr_t const& that) {
#ifdef TRI_ENABLE_MAINTAINER_MODE
      triagens::arango::TransactionBase::assertCurrentTrxActive();
#endif
      copy(that);
      return *this;
    }

#ifndef TRI_ENABLE_MAINTAINER_MODE
    inline void const* getDataPtr () const {
      return _dataptr;
    }
#else
    // The actual code has an assertion about transactions!
    virtual void const* getDataPtr () const;
#endif

#ifndef TRI_ENABLE_MAINTAINER_MODE
    inline void setDataPtr (void const* d) {
      _dataptr = d;
    }
#else
    // The actual code has an assertion about transactions!
    virtual void setDataPtr (void const* d);
#endif

};

////////////////////////////////////////////////////////////////////////////////
/// @brief datafile info
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_doc_datafile_info_s {
  TRI_voc_fid_t   _fid;

  TRI_voc_ssize_t _numberAlive;
  TRI_voc_ssize_t _numberDead;
  TRI_voc_ssize_t _numberDeletion;
  TRI_voc_ssize_t _numberShapes;
  TRI_voc_ssize_t _numberAttributes;
  TRI_voc_ssize_t _numberTransactions; // used only during compaction

  int64_t         _sizeAlive;
  int64_t         _sizeDead;
  int64_t         _sizeShapes;
  int64_t         _sizeAttributes;
  int64_t         _sizeTransactions; // used only during compaction
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
  TRI_voc_ssize_t _numberShapes;
  TRI_voc_ssize_t _numberAttributes;
  TRI_voc_ssize_t _numberTransactions;
  TRI_voc_ssize_t _numberIndexes;

  int64_t         _sizeAlive;
  int64_t         _sizeDead;
  int64_t         _sizeShapes;
  int64_t         _sizeAttributes;
  int64_t         _sizeTransactions;
  int64_t         _sizeIndexes;

  int64_t         _datafileSize;
  int64_t         _journalfileSize;
  int64_t         _compactorfileSize;
  int64_t         _shapefileSize;

  TRI_voc_tick_t  _tickMax;
  uint64_t        _uncollectedLogfileEntries;
}
TRI_doc_collection_info_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief document collection with global read-write lock
///
/// A document collection is a collection with a single read-write lock. This
/// lock is used to coordinate the read and write transactions.
////////////////////////////////////////////////////////////////////////////////

struct TRI_document_collection_t : public TRI_collection_t {
  // ...........................................................................
  // this lock protects the _primaryIndex plus the _allIndexes
  // and _headers attributes in derived types
  // ...........................................................................

  TRI_read_write_lock_t        _lock;

private:
  TRI_shaper_t*                _shaper;

  // whether or not secondary indexes are filled
  bool                         _useSecondaryIndexes;

public:
  // We do some assertions with barriers and transactions in maintainer mode:
#ifndef TRI_ENABLE_MAINTAINER_MODE
  TRI_shaper_t* getShaper () const {
    return _shaper;
  }
#else
  TRI_shaper_t* getShaper () const;
#endif

  inline bool useSecondaryIndexes () const {
    return _useSecondaryIndexes;
  }

  void useSecondaryIndexes (bool value) {
    _useSecondaryIndexes = value;
  }

  void setShaper (TRI_shaper_t* s) {
    _shaper = s;
  }

  mutable TRI_barrier_list_t   _barrierList;
  TRI_associative_pointer_t    _datafileInfo;

  TRI_primary_index_t          _primaryIndex;
  TRI_headers_t*               _headersPtr;
  KeyGenerator*                _keyGenerator;
  struct TRI_cap_constraint_s* _capConstraint;

  TRI_vector_pointer_t         _allIndexes;
  std::set<TRI_voc_tid_t>*     _failedTransactions;

  int64_t                      _uncollectedLogfileEntries;
  int64_t                      _numberDocuments;
  TRI_read_write_lock_t        _compactionLock;
  double                       _lastCompaction;

  // ...........................................................................
  // this condition variable protects the _journalsCondition
  // ...........................................................................

  TRI_condition_t              _journalsCondition;

  // whether or not any of the indexes may need to be garbage-collected
  // this flag may be modifying when an index is added to a collection
  // if true, the cleanup thread will periodically call the cleanup functions of
  // the collection's indexes that support cleanup
  bool                         _cleanupIndexes;

  int (*beginRead) (struct TRI_document_collection_t*);
  int (*endRead) (struct TRI_document_collection_t*);

  int (*beginWrite) (struct TRI_document_collection_t*);
  int (*endWrite) (struct TRI_document_collection_t*);

  int (*beginReadTimed) (struct TRI_document_collection_t*, uint64_t, uint64_t);
  int (*beginWriteTimed) (struct TRI_document_collection_t*, uint64_t, uint64_t);

#ifdef TRI_ENABLE_MAINTAINER_MODE
  void (*dump) (struct TRI_document_collection_t*);
#endif

  TRI_doc_collection_info_t* (*figures) (struct TRI_document_collection_t* collection);
  TRI_voc_size_t (*size) (struct TRI_document_collection_t* collection);

  // function that is called to garbage-collect the collection's indexes
  int (*cleanupIndexes)(struct TRI_document_collection_t*);

  TRI_document_collection_t ();

  ~TRI_document_collection_t ();
};

// -----------------------------------------------------------------------------
// --SECTION--                                               protected functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief removes a datafile description
////////////////////////////////////////////////////////////////////////////////

void TRI_RemoveDatafileInfoDocumentCollection (TRI_document_collection_t*,
                                              TRI_voc_fid_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief finds a datafile description
////////////////////////////////////////////////////////////////////////////////

TRI_doc_datafile_info_t* TRI_FindDatafileInfoDocumentCollection (TRI_document_collection_t*,
                                                                TRI_voc_fid_t,
                                                                bool);

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

size_t TRI_DocumentIteratorDocumentCollection (triagens::arango::TransactionBase const*,
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
/// @brief whether or not the marker is an edge marker
////////////////////////////////////////////////////////////////////////////////

static inline bool TRI_IS_EDGE_MARKER (TRI_df_marker_t const* marker) {
  return (marker->_type == TRI_DOC_MARKER_KEY_EDGE ||
          marker->_type == TRI_WAL_MARKER_EDGE);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts the pointer to the _from key from a marker
////////////////////////////////////////////////////////////////////////////////

static inline char const* TRI_EXTRACT_MARKER_FROM_KEY (TRI_df_marker_t const* marker) {
  if (marker->_type == TRI_DOC_MARKER_KEY_EDGE) {
    return ((char const*) marker) + ((TRI_doc_edge_key_marker_t const*) marker)->_offsetFromKey;  
  }
  else if (marker->_type == TRI_WAL_MARKER_EDGE) {
    return ((char const*) marker) + ((triagens::wal::edge_marker_t const*) marker)->_offsetFromKey; 
  }

#ifdef TRI_ENABLE_MAINTAINER_MODE
  // invalid marker type
  TRI_ASSERT(false);
#endif

  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts the pointer to the _to key from a marker
////////////////////////////////////////////////////////////////////////////////

static inline char const* TRI_EXTRACT_MARKER_TO_KEY (TRI_df_marker_t const* marker) {
  if (marker->_type == TRI_DOC_MARKER_KEY_EDGE) {
    return ((char const*) marker) + ((TRI_doc_edge_key_marker_t const*) marker)->_offsetToKey;  
  }
  else if (marker->_type == TRI_WAL_MARKER_EDGE) {
    return ((char const*) marker) + ((triagens::wal::edge_marker_t const*) marker)->_offsetToKey; 
  }

#ifdef TRI_ENABLE_MAINTAINER_MODE
  // invalid marker type
  TRI_ASSERT(false);
#endif

  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts the _from cid from a marker
////////////////////////////////////////////////////////////////////////////////

static inline TRI_voc_cid_t TRI_EXTRACT_MARKER_FROM_CID (TRI_df_marker_t const* marker) {
  if (marker->_type == TRI_DOC_MARKER_KEY_EDGE) {
    return ((TRI_doc_edge_key_marker_t const*) marker)->_fromCid;
  }
  else if (marker->_type == TRI_WAL_MARKER_EDGE) {
    return ((triagens::wal::edge_marker_t const*) marker)->_fromCid;
  }

#ifdef TRI_ENABLE_MAINTAINER_MODE
  // invalid marker type
  TRI_ASSERT(false);
#endif

  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts the _to cid from a marker
////////////////////////////////////////////////////////////////////////////////

static inline TRI_voc_cid_t TRI_EXTRACT_MARKER_TO_CID (TRI_df_marker_t const* marker) {
  if (marker->_type == TRI_DOC_MARKER_KEY_EDGE) {
    return ((TRI_doc_edge_key_marker_t const*) marker)->_toCid;  
  }
  else if (marker->_type == TRI_WAL_MARKER_EDGE) {
    return ((triagens::wal::edge_marker_t const*) marker)->_toCid;
  }

#ifdef TRI_ENABLE_MAINTAINER_MODE
  // invalid marker type
  TRI_ASSERT(false);
#endif

  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts the revision id from a marker
////////////////////////////////////////////////////////////////////////////////

static inline TRI_voc_rid_t TRI_EXTRACT_MARKER_RID (TRI_df_marker_t const* marker) {
  if (marker->_type == TRI_DOC_MARKER_KEY_DOCUMENT || marker->_type == TRI_DOC_MARKER_KEY_EDGE) {
    return ((TRI_doc_document_key_marker_t const*) marker)->_rid;  
  }
  else if (marker->_type == TRI_WAL_MARKER_DOCUMENT || marker->_type == TRI_WAL_MARKER_EDGE) {
    return ((triagens::wal::document_marker_t const*) marker)->_revisionId; 
  }

#ifdef TRI_ENABLE_MAINTAINER_MODE
  // invalid marker type
  TRI_ASSERT(false);
#endif

  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts the pointer to the key from a marker
////////////////////////////////////////////////////////////////////////////////

static inline char const* TRI_EXTRACT_MARKER_KEY (TRI_df_marker_t const* marker) {
  if (marker->_type == TRI_DOC_MARKER_KEY_DOCUMENT || marker->_type == TRI_DOC_MARKER_KEY_EDGE) {
    return ((char const*) marker) + ((TRI_doc_document_key_marker_t const*) marker)->_offsetKey;  
  }
  else if (marker->_type == TRI_WAL_MARKER_DOCUMENT || marker->_type == TRI_WAL_MARKER_EDGE) {
    return ((char const*) marker) + ((triagens::wal::document_marker_t const*) marker)->_offsetKey; 
  }

#ifdef TRI_ENABLE_MAINTAINER_MODE
  // invalid marker type
  TRI_ASSERT(false);
#endif

  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts the pointer to the key from a marker
////////////////////////////////////////////////////////////////////////////////

static inline char const* TRI_EXTRACT_MARKER_KEY (TRI_doc_mptr_t const* mptr) {
  TRI_df_marker_t const* marker = static_cast<TRI_df_marker_t const*>(mptr->getDataPtr());  // PROTECTED by TRI_EXTRACT_MARKER_KEY search
  return TRI_EXTRACT_MARKER_KEY(marker);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts the pointer to the key from a marker
////////////////////////////////////////////////////////////////////////////////

static inline char const* TRI_EXTRACT_MARKER_KEY (TRI_doc_mptr_copy_t const* mptr) {
  TRI_df_marker_t const* marker = static_cast<TRI_df_marker_t const*>(mptr->getDataPtr());  // PROTECTED by TRI_EXTRACT_MARKER_KEY search
  return TRI_EXTRACT_MARKER_KEY(marker);
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
/// @brief update statistics for a collection
/// note: the write-lock for the collection must be held to call this
////////////////////////////////////////////////////////////////////////////////

void TRI_UpdateRevisionDocumentCollection (TRI_document_collection_t*,
                                           TRI_voc_rid_t,
                                           bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not a collection is fully collected
////////////////////////////////////////////////////////////////////////////////

bool TRI_IsFullyCollectedDocumentCollection (TRI_document_collection_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief create an index, based on a JSON description
////////////////////////////////////////////////////////////////////////////////

int TRI_FromJsonIndexDocumentCollection (TRI_document_collection_t*,
                                         struct TRI_json_t const*,
                                         struct TRI_index_s**);

////////////////////////////////////////////////////////////////////////////////
/// @brief rolls back a document operation
////////////////////////////////////////////////////////////////////////////////

int TRI_RollbackOperationDocumentCollection (TRI_document_collection_t*,
                                             TRI_voc_document_operation_e,
                                             TRI_doc_mptr_t*,
                                             TRI_doc_mptr_copy_t const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new journal
////////////////////////////////////////////////////////////////////////////////

TRI_datafile_t* TRI_CreateDatafileDocumentCollection (TRI_document_collection_t*,
                                                      TRI_voc_fid_t,
                                                      TRI_voc_size_t,
                                                      bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief closes an existing journal
////////////////////////////////////////////////////////////////////////////////

bool TRI_CloseDatafileDocumentCollection (TRI_document_collection_t*,
                                          size_t,
                                          bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief fill the additional (non-primary) indexes
////////////////////////////////////////////////////////////////////////////////

int TRI_FillIndexesDocumentCollection (TRI_document_collection_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief opens an existing collection
////////////////////////////////////////////////////////////////////////////////

TRI_document_collection_t* TRI_OpenDocumentCollection (TRI_vocbase_t*,
                                                       TRI_vocbase_col_t*);

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
                                      bool);

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
                                                        bool*);

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
                                                           bool*);

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures that a geo index exists, attribute style
////////////////////////////////////////////////////////////////////////////////

struct TRI_index_s* TRI_EnsureGeoIndex2DocumentCollection (TRI_document_collection_t*,
                                                           TRI_idx_iid_t,
                                                           char const*,
                                                           char const*,
                                                           bool,
                                                           bool,
                                                           bool*);

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
                                                           bool*);

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
                                                               bool*);

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
                                                               char const*,
                                                               bool,
                                                               int);

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures that a fulltext index exists
////////////////////////////////////////////////////////////////////////////////

struct TRI_index_s* TRI_EnsureFulltextIndexDocumentCollection (TRI_document_collection_t*,
                                                               TRI_idx_iid_t,
                                                               char const*,
                                                               bool,
                                                               int,
                                                               bool*);

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a select-by-example query
////////////////////////////////////////////////////////////////////////////////

std::vector<TRI_doc_mptr_copy_t> TRI_SelectByExample (
                          struct TRI_transaction_collection_s*,
                          size_t,
                          TRI_shape_pid_t*,
                          TRI_shaped_json_t**);

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes a documet given by a master pointer
////////////////////////////////////////////////////////////////////////////////

int TRI_DeleteDocumentDocumentCollection (struct TRI_transaction_collection_s*,
                                          TRI_doc_update_policy_t const*,
                                          TRI_doc_mptr_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief rotate the current journal of the collection
/// use this for testing only
////////////////////////////////////////////////////////////////////////////////

int TRI_RotateJournalDocumentCollection (TRI_document_collection_t*);

// -----------------------------------------------------------------------------
// --SECTION--                                                      CRUD methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief reads an element from the document collection
////////////////////////////////////////////////////////////////////////////////

int TRI_ReadShapedJsonDocumentCollection (TRI_transaction_collection_t*,
                                          const TRI_voc_key_t,
                                          TRI_doc_mptr_copy_t*,
                                          bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief removes a shaped-json document (or edge)
////////////////////////////////////////////////////////////////////////////////

int TRI_RemoveShapedJsonDocumentCollection (TRI_transaction_collection_t*,
                                            const TRI_voc_key_t,
                                            TRI_voc_rid_t,
                                            triagens::wal::Marker*,
                                            TRI_doc_update_policy_t const*,
                                            bool,
                                            bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief insert a shaped-json document (or edge)
/// note: key might be NULL. in this case, a key is auto-generated
////////////////////////////////////////////////////////////////////////////////

int TRI_InsertShapedJsonDocumentCollection (TRI_transaction_collection_t*,
                                            const TRI_voc_key_t,
                                            TRI_voc_rid_t,
                                            triagens::wal::Marker*,
                                            TRI_doc_mptr_copy_t*,
                                            TRI_shaped_json_t const*,
                                            TRI_document_edge_t const*,
                                            bool,
                                            bool,
                                            bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief updates a document in the collection from shaped json
////////////////////////////////////////////////////////////////////////////////

int TRI_UpdateShapedJsonDocumentCollection (TRI_transaction_collection_t*,
                                            const TRI_voc_key_t,
                                            TRI_voc_rid_t,
                                            triagens::wal::Marker*,
                                            TRI_doc_mptr_copy_t*,
                                            TRI_shaped_json_t const*,
                                            TRI_doc_update_policy_t const*,
                                            bool,
                                            bool);

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_VOC_BASE_DOCUMENT_COLLECTION_H
#define ARANGOD_VOC_BASE_DOCUMENT_COLLECTION_H 1

#include "Basics/Common.h"
#include "Basics/fasthash.h"
#include "Basics/ReadWriteLock.h"
#include "Cluster/ClusterInfo.h"
#include "VocBase/collection.h"
#include "VocBase/DatafileStatistics.h"
#include "VocBase/Ditch.h"
#include "VocBase/transaction.h"
#include "VocBase/update-policy.h"
#include "VocBase/voc-types.h"
#include "Wal/Marker.h"

class TRI_headers_t;

class VocShaper;

namespace arangodb {
class CapConstraint;
class EdgeIndex;
class ExampleMatcher;
class Index;
class PrimaryIndex;
class Transaction;
namespace velocypack {
class Builder;
class Slice;
}
}

class KeyGenerator;

////////////////////////////////////////////////////////////////////////////////
/// @brief master pointer
////////////////////////////////////////////////////////////////////////////////

struct TRI_doc_mptr_t {
  TRI_voc_rid_t _rid;     // this is the revision identifier
  TRI_voc_fid_t _fid;     // this is the datafile identifier
  uint64_t _hash;         // the pre-calculated hash value of the key
  TRI_doc_mptr_t* _prev;  // previous master pointer
  TRI_doc_mptr_t* _next;  // next master pointer
 protected:
  void const*
      _dataptr;  // this is the pointer to the beginning of the raw marker

 public:
  TRI_doc_mptr_t()
      : _rid(0),
        _fid(0),
        _hash(0),
        _prev(nullptr),
        _next(nullptr),
        _dataptr(nullptr) {}

  virtual ~TRI_doc_mptr_t() {}

  void clear() {
    _rid = 0;
    _fid = 0;
    setDataPtr(nullptr);
    _hash = 0;
    _prev = nullptr;
    _next = nullptr;
  }

  void copy(TRI_doc_mptr_t const& that) {
    // This is for cases where we explicitly have to copy originals!
    _rid = that._rid;
    _fid = that._fid;
    _dataptr = that._dataptr;
    _hash = that._hash;
    _prev = that._prev;
    _next = that._next;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief return a pointer to the beginning of the marker
  //////////////////////////////////////////////////////////////////////////////

  inline void const* getDataPtr() const { return _dataptr; }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief set the pointer to the beginning of the memory for the marker
  //////////////////////////////////////////////////////////////////////////////

  inline void setDataPtr(void const* d) { _dataptr = d; }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief return a pointer to the beginning of the shaped json stored in the
  /// marker
  //////////////////////////////////////////////////////////////////////////////

  char const* getShapedJsonPtr() const {
    TRI_df_marker_t const* marker =
        static_cast<TRI_df_marker_t const*>(_dataptr);

    TRI_ASSERT(marker != nullptr);

    if (marker->_type == TRI_DOC_MARKER_KEY_DOCUMENT ||
        marker->_type == TRI_DOC_MARKER_KEY_EDGE) {
      auto offset =
          (reinterpret_cast<TRI_doc_document_key_marker_t const*>(marker))
              ->_offsetJson;
      return reinterpret_cast<char const*>(marker) + offset;
    } else if (marker->_type == TRI_WAL_MARKER_DOCUMENT ||
               marker->_type == TRI_WAL_MARKER_EDGE) {
      auto offset =
          (reinterpret_cast<arangodb::wal::document_marker_t const*>(marker))
              ->_offsetJson;
      return reinterpret_cast<char const*>(marker) + offset;
    }

    TRI_ASSERT(false);

    return nullptr;
  }

  TRI_doc_mptr_t& operator=(TRI_doc_mptr_t const&) = delete;
  TRI_doc_mptr_t(TRI_doc_mptr_t const&) = delete;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief A derived class for copies of master pointers, they
////////////////////////////////////////////////////////////////////////////////

struct TRI_doc_mptr_copy_t final : public TRI_doc_mptr_t {
  TRI_doc_mptr_copy_t() : TRI_doc_mptr_t() {}

  TRI_doc_mptr_copy_t(TRI_doc_mptr_copy_t const& that) : TRI_doc_mptr_t() {
    copy(that);
  }

  TRI_doc_mptr_copy_t(TRI_doc_mptr_t const& that) : TRI_doc_mptr_t() {
    copy(that);
  }

  TRI_doc_mptr_copy_t& operator=(TRI_doc_mptr_copy_t const& that) {
    copy(that);
    return *this;
  }

  TRI_doc_mptr_copy_t const& operator=(TRI_doc_mptr_t const& that) {
    copy(that);
    return *this;
  }

  // Move semantics:

  TRI_doc_mptr_copy_t(TRI_doc_mptr_copy_t&& that) { copy(that); }

  TRI_doc_mptr_copy_t&& operator=(TRI_doc_mptr_copy_t&& that) {
    copy(that);
    return std::move(*this);
  }

  inline void const* getDataPtr() const { return _dataptr; }

  inline void setDataPtr(void const* d) { _dataptr = d; }
};

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
  TRI_voc_ssize_t _numberDeletions;
  TRI_voc_ssize_t _numberShapes;
  TRI_voc_ssize_t _numberAttributes;
  TRI_voc_ssize_t _numberTransactions;
  TRI_voc_ssize_t _numberIndexes;

  int64_t _sizeAlive;
  int64_t _sizeDead;
  int64_t _sizeShapes;
  int64_t _sizeAttributes;
  int64_t _sizeTransactions;
  int64_t _sizeIndexes;

  int64_t _datafileSize;
  int64_t _journalfileSize;
  int64_t _compactorfileSize;
  int64_t _shapefileSize;

  TRI_voc_tick_t _tickMax;
  uint64_t _uncollectedLogfileEntries;
  uint64_t _numberDocumentDitches;
  char const* _waitingForDitch;
  char const* _lastCompactionStatus;
  char _lastCompactionStamp[21];
} TRI_doc_collection_info_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief document collection with global read-write lock
///
/// A document collection is a collection with a single read-write lock. This
/// lock is used to coordinate the read and write transactions.
////////////////////////////////////////////////////////////////////////////////

struct TRI_document_collection_t : public TRI_collection_t {
  TRI_document_collection_t();

  ~TRI_document_collection_t();

  std::string label() const;

  // ...........................................................................
  // this lock protects the indexes and _headers attributes
  // ...........................................................................

  // TRI_read_write_lock_t        _lock;
  arangodb::basics::ReadWriteLock _lock;

 private:
  VocShaper* _shaper;

  arangodb::Mutex _compactionStatusLock;
  size_t _nextCompactionStartIndex;
  char const* _lastCompactionStatus;
  char _lastCompactionStamp[21];

  // whether or not secondary indexes are filled
  bool _useSecondaryIndexes;

  // the following contains in the cluster/DBserver case the information
  // which other servers are in sync with this shard. It is unset in all
  // other cases.
  std::unique_ptr<arangodb::FollowerInfo> _followers;

 public:
  arangodb::DatafileStatistics _datafileStatistics;

// We do some assertions with barriers and transactions in maintainer mode:
#ifndef TRI_ENABLE_MAINTAINER_MODE
  VocShaper* getShaper() const { return _shaper; }
#else
  VocShaper* getShaper() const;
#endif

  void setNextCompactionStartIndex(size_t);
  size_t getNextCompactionStartIndex();
  void setCompactionStatus(char const*);
  void getCompactionStatus(char const*&, char*, size_t);

  inline bool useSecondaryIndexes() const { return _useSecondaryIndexes; }

  void useSecondaryIndexes(bool value) { _useSecondaryIndexes = value; }

  void setShaper(VocShaper* s) { _shaper = s; }

  void addIndex(arangodb::Index*);
  arangodb::Index* removeIndex(TRI_idx_iid_t);
  std::vector<arangodb::Index*> allIndexes() const;
  arangodb::Index* lookupIndex(TRI_idx_iid_t) const;
  arangodb::PrimaryIndex* primaryIndex();
  arangodb::EdgeIndex* edgeIndex();
  arangodb::CapConstraint* capConstraint();

  arangodb::CapConstraint* _capConstraint;

  arangodb::Ditches* ditches() { return &_ditches; }

  mutable arangodb::Ditches _ditches;

  class TRI_headers_t* _headersPtr;
  KeyGenerator* _keyGenerator;

  std::vector<arangodb::Index*> _indexes;

  std::set<TRI_voc_tid_t>* _failedTransactions;

  std::atomic<int64_t> _uncollectedLogfileEntries;
  int64_t _numberDocuments;
  TRI_read_write_lock_t _compactionLock;
  double _lastCompaction;

  // ...........................................................................
  // this condition variable protects the _journalsCondition
  // ...........................................................................

  TRI_condition_t _journalsCondition;

  // whether or not any of the indexes may need to be garbage-collected
  // this flag may be modifying when an index is added to a collection
  // if true, the cleanup thread will periodically call the cleanup functions of
  // the collection's indexes that support cleanup
  size_t _cleanupIndexes;

  int beginRead();
  int endRead();
  int beginWrite();
  int endWrite();
  int beginReadTimed(uint64_t, uint64_t);
  int beginWriteTimed(uint64_t, uint64_t);

  TRI_doc_collection_info_t* figures();

  uint64_t size();

  // function that is called to garbage-collect the collection's indexes
  int (*cleanupIndexes)(struct TRI_document_collection_t*);

  int insert(arangodb::Transaction*, arangodb::velocypack::Slice const*,
             TRI_doc_mptr_copy_t*, bool, bool);
  int remove(arangodb::Transaction*, arangodb::velocypack::Slice const*,
             TRI_doc_update_policy_t const*, bool, bool);

 private:
  arangodb::wal::Marker* createVPackInsertMarker(
      arangodb::Transaction*, arangodb::velocypack::Slice const*);
  arangodb::wal::Marker* createVPackRemoveMarker(
      arangodb::Transaction*, arangodb::velocypack::Slice const*);
  int lookupDocument(arangodb::Transaction*, arangodb::velocypack::Slice const*,
                     TRI_doc_update_policy_t const*, TRI_doc_mptr_t*&);
  int insertDocument(arangodb::Transaction*, TRI_doc_mptr_t*,
                     arangodb::wal::DocumentOperation&, TRI_doc_mptr_copy_t*,
                     bool&);
  int insertPrimaryIndex(arangodb::Transaction*, TRI_doc_mptr_t*);
  int insertSecondaryIndexes(arangodb::Transaction*, TRI_doc_mptr_t const*,
                             bool);
  int deletePrimaryIndex(arangodb::Transaction*, TRI_doc_mptr_t const*);
  int deleteSecondaryIndexes(arangodb::Transaction*, TRI_doc_mptr_t const*,
                             bool);
  int postInsertIndexes(arangodb::Transaction*, TRI_doc_mptr_t*);
};

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

size_t TRI_DocumentIteratorDocumentCollection(
    arangodb::Transaction*, TRI_document_collection_t*, void*,
    bool (*callback)(TRI_doc_mptr_t const*, TRI_document_collection_t*, void*));

////////////////////////////////////////////////////////////////////////////////
/// @brief tries to read lock the journal files and the parameter file
///
/// note: the return value of the call to TRI_TryReadLockReadWriteLock is
/// is checked so we cannot add logging here
////////////////////////////////////////////////////////////////////////////////

#define TRI_TRY_READ_LOCK_DATAFILES_DOC_COLLECTION(a) a->_lock.tryReadLock()

////////////////////////////////////////////////////////////////////////////////
/// @brief read locks the journal files and the parameter file
////////////////////////////////////////////////////////////////////////////////

#define TRI_READ_LOCK_DATAFILES_DOC_COLLECTION(a) a->_lock.readLock()

////////////////////////////////////////////////////////////////////////////////
/// @brief read unlocks the journal files and the parameter file
////////////////////////////////////////////////////////////////////////////////

#define TRI_READ_UNLOCK_DATAFILES_DOC_COLLECTION(a) a->_lock.unlock()

////////////////////////////////////////////////////////////////////////////////
/// @brief write locks the journal files and the parameter file
////////////////////////////////////////////////////////////////////////////////

#define TRI_WRITE_LOCK_DATAFILES_DOC_COLLECTION(a) a->_lock.writeLock()

////////////////////////////////////////////////////////////////////////////////
/// @brief write unlocks the journal files and the parameter file
////////////////////////////////////////////////////////////////////////////////

#define TRI_WRITE_UNLOCK_DATAFILES_DOC_COLLECTION(a) a->_lock.unlock()

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
/// @brief read locks the documents and indexes
////////////////////////////////////////////////////////////////////////////////

#define TRI_READ_LOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(a) \
  a->_lock.readLock()

////////////////////////////////////////////////////////////////////////////////
/// @brief tries to read lock the documents and indexes
////////////////////////////////////////////////////////////////////////////////

#define TRI_TRY_READ_LOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(a) \
  a->_lock.tryReadLock()

////////////////////////////////////////////////////////////////////////////////
/// @brief read unlocks the documents and indexes
////////////////////////////////////////////////////////////////////////////////

#define TRI_READ_UNLOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(a) \
  a->_lock.unlock()

////////////////////////////////////////////////////////////////////////////////
/// @brief write locks the documents and indexes
////////////////////////////////////////////////////////////////////////////////

#define TRI_WRITE_LOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(a) \
  a->_lock.writeLock()

////////////////////////////////////////////////////////////////////////////////
/// @brief tries to write lock the documents and indexes
////////////////////////////////////////////////////////////////////////////////

#define TRI_TRY_WRITE_LOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(a) \
  a->_lock.tryWriteLock()

////////////////////////////////////////////////////////////////////////////////
/// @brief write unlocks the documents and indexes
////////////////////////////////////////////////////////////////////////////////

#define TRI_WRITE_UNLOCK_DOCUMENTS_INDEXES_PRIMARY_COLLECTION(a) \
  a->_lock.unlock()

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the marker is an edge marker
////////////////////////////////////////////////////////////////////////////////

static inline bool TRI_IS_EDGE_MARKER(TRI_df_marker_t const* marker) {
  return (marker->_type == TRI_DOC_MARKER_KEY_EDGE ||
          marker->_type == TRI_WAL_MARKER_EDGE);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the master pointer points to an edge marker
////////////////////////////////////////////////////////////////////////////////

static inline bool TRI_IS_EDGE_MARKER(TRI_doc_mptr_t const* mptr) {
  TRI_df_marker_t const* marker =
      static_cast<TRI_df_marker_t const*>(mptr->getDataPtr());
  return TRI_IS_EDGE_MARKER(marker);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts the pointer to the _from key from a marker
////////////////////////////////////////////////////////////////////////////////

static inline char const* TRI_EXTRACT_MARKER_FROM_KEY(
    TRI_df_marker_t const* marker) {
  if (marker->_type == TRI_DOC_MARKER_KEY_EDGE) {
    return ((char const*)marker) +
           ((TRI_doc_edge_key_marker_t const*)marker)->_offsetFromKey;
  } else if (marker->_type == TRI_WAL_MARKER_EDGE) {
    return ((char const*)marker) +
           ((arangodb::wal::edge_marker_t const*)marker)->_offsetFromKey;
  }

#ifdef TRI_ENABLE_MAINTAINER_MODE
  // invalid marker type
  TRI_ASSERT(false);
#endif

  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts the pointer to the _from key from a master pointer
////////////////////////////////////////////////////////////////////////////////

static inline char const* TRI_EXTRACT_MARKER_FROM_KEY(
    TRI_doc_mptr_t const* mptr) {
  TRI_df_marker_t const* marker =
      static_cast<TRI_df_marker_t const*>(mptr->getDataPtr());
  return TRI_EXTRACT_MARKER_FROM_KEY(marker);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts the pointer to the _to key from a marker
////////////////////////////////////////////////////////////////////////////////

static inline char const* TRI_EXTRACT_MARKER_TO_KEY(
    TRI_df_marker_t const* marker) {
  if (marker->_type == TRI_DOC_MARKER_KEY_EDGE) {
    return ((char const*)marker) +
           ((TRI_doc_edge_key_marker_t const*)marker)->_offsetToKey;
  } else if (marker->_type == TRI_WAL_MARKER_EDGE) {
    return ((char const*)marker) +
           ((arangodb::wal::edge_marker_t const*)marker)->_offsetToKey;
  }

#ifdef TRI_ENABLE_MAINTAINER_MODE
  // invalid marker type
  TRI_ASSERT(false);
#endif

  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts the pointer to the _to key from a master pointer
////////////////////////////////////////////////////////////////////////////////

static inline char const* TRI_EXTRACT_MARKER_TO_KEY(
    TRI_doc_mptr_t const* mptr) {
  TRI_df_marker_t const* marker =
      static_cast<TRI_df_marker_t const*>(mptr->getDataPtr());
  return TRI_EXTRACT_MARKER_TO_KEY(marker);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts the _from cid from a marker
////////////////////////////////////////////////////////////////////////////////

static inline TRI_voc_cid_t TRI_EXTRACT_MARKER_FROM_CID(
    TRI_df_marker_t const* marker) {
  if (marker->_type == TRI_DOC_MARKER_KEY_EDGE) {
    return ((TRI_doc_edge_key_marker_t const*)marker)->_fromCid;
  } else if (marker->_type == TRI_WAL_MARKER_EDGE) {
    return ((arangodb::wal::edge_marker_t const*)marker)->_fromCid;
  }

#ifdef TRI_ENABLE_MAINTAINER_MODE
  // invalid marker type
  TRI_ASSERT(false);
#endif

  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts the _from cid from a master pointer
////////////////////////////////////////////////////////////////////////////////

static inline TRI_voc_cid_t TRI_EXTRACT_MARKER_FROM_CID(
    TRI_doc_mptr_t const* mptr) {
  TRI_df_marker_t const* marker =
      static_cast<TRI_df_marker_t const*>(mptr->getDataPtr());
  return TRI_EXTRACT_MARKER_FROM_CID(marker);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts the _to cid from a marker
////////////////////////////////////////////////////////////////////////////////

static inline TRI_voc_cid_t TRI_EXTRACT_MARKER_TO_CID(
    TRI_df_marker_t const* marker) {
  if (marker->_type == TRI_DOC_MARKER_KEY_EDGE) {
    return ((TRI_doc_edge_key_marker_t const*)marker)->_toCid;
  } else if (marker->_type == TRI_WAL_MARKER_EDGE) {
    return ((arangodb::wal::edge_marker_t const*)marker)->_toCid;
  }

#ifdef TRI_ENABLE_MAINTAINER_MODE
  // invalid marker type
  TRI_ASSERT(false);
#endif

  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts the _to cid from a master pointer
////////////////////////////////////////////////////////////////////////////////

static inline TRI_voc_cid_t TRI_EXTRACT_MARKER_TO_CID(
    TRI_doc_mptr_t const* mptr) {
  TRI_df_marker_t const* marker =
      static_cast<TRI_df_marker_t const*>(mptr->getDataPtr());
  return TRI_EXTRACT_MARKER_TO_CID(marker);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts the revision id from a marker
////////////////////////////////////////////////////////////////////////////////

static inline TRI_voc_rid_t TRI_EXTRACT_MARKER_RID(
    TRI_df_marker_t const* marker) {
  if (marker->_type == TRI_DOC_MARKER_KEY_DOCUMENT ||
      marker->_type == TRI_DOC_MARKER_KEY_EDGE) {
    return ((TRI_doc_document_key_marker_t const*)marker)->_rid;
  } else if (marker->_type == TRI_WAL_MARKER_DOCUMENT ||
             marker->_type == TRI_WAL_MARKER_EDGE) {
    return ((arangodb::wal::document_marker_t const*)marker)->_revisionId;
  }

#ifdef TRI_ENABLE_MAINTAINER_MODE
  // invalid marker type
  TRI_ASSERT(false);
#endif

  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts the revision id from a master pointer
////////////////////////////////////////////////////////////////////////////////

static inline TRI_voc_rid_t TRI_EXTRACT_MARKER_RID(TRI_doc_mptr_t const* mptr) {
  TRI_df_marker_t const* marker =
      static_cast<TRI_df_marker_t const*>(mptr->getDataPtr());
  return TRI_EXTRACT_MARKER_RID(marker);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts the pointer to the key from a marker
////////////////////////////////////////////////////////////////////////////////

static inline char const* TRI_EXTRACT_MARKER_KEY(
    TRI_df_marker_t const* marker) {
  if (marker->_type == TRI_DOC_MARKER_KEY_DOCUMENT ||
      marker->_type == TRI_DOC_MARKER_KEY_EDGE) {
    return ((char const*)marker) +
           ((TRI_doc_document_key_marker_t const*)marker)->_offsetKey;
  } else if (marker->_type == TRI_WAL_MARKER_DOCUMENT ||
             marker->_type == TRI_WAL_MARKER_EDGE) {
    return ((char const*)marker) +
           ((arangodb::wal::document_marker_t const*)marker)->_offsetKey;
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

static inline char const* TRI_EXTRACT_MARKER_KEY(TRI_doc_mptr_t const* mptr) {
  TRI_df_marker_t const* marker = static_cast<TRI_df_marker_t const*>(
      mptr->getDataPtr());  // PROTECTED by TRI_EXTRACT_MARKER_KEY search
  return TRI_EXTRACT_MARKER_KEY(marker);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts the pointer to the key from a marker
////////////////////////////////////////////////////////////////////////////////

static inline char const* TRI_EXTRACT_MARKER_KEY(
    TRI_doc_mptr_copy_t const* mptr) {
  TRI_df_marker_t const* marker = static_cast<TRI_df_marker_t const*>(
      mptr->getDataPtr());  // PROTECTED by TRI_EXTRACT_MARKER_KEY search
  return TRI_EXTRACT_MARKER_KEY(marker);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new collection
////////////////////////////////////////////////////////////////////////////////

TRI_document_collection_t* TRI_CreateDocumentCollection(
    TRI_vocbase_t*, char const*, arangodb::VocbaseCollectionInfo&,
    TRI_voc_cid_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the memory allocated, but does not free the pointer
///
/// Note that the collection must be closed first.
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyDocumentCollection(TRI_document_collection_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the memory allocated and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeDocumentCollection(TRI_document_collection_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief update statistics for a collection
/// note: the write-lock for the collection must be held to call this
////////////////////////////////////////////////////////////////////////////////

void TRI_UpdateRevisionDocumentCollection(TRI_document_collection_t*,
                                          TRI_voc_rid_t, bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not a collection is fully collected
////////////////////////////////////////////////////////////////////////////////

bool TRI_IsFullyCollectedDocumentCollection(TRI_document_collection_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief create an index, based on a VelocyPack description
////////////////////////////////////////////////////////////////////////////////

int TRI_FromVelocyPackIndexDocumentCollection(
    arangodb::Transaction*, TRI_document_collection_t*,
    arangodb::velocypack::Slice const&, arangodb::Index**);

////////////////////////////////////////////////////////////////////////////////
/// @brief rolls back a document operation
////////////////////////////////////////////////////////////////////////////////

int TRI_RollbackOperationDocumentCollection(arangodb::Transaction*,
                                            TRI_document_collection_t*,
                                            TRI_voc_document_operation_e,
                                            TRI_doc_mptr_t*,
                                            TRI_doc_mptr_copy_t const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new journal
////////////////////////////////////////////////////////////////////////////////

TRI_datafile_t* TRI_CreateDatafileDocumentCollection(TRI_document_collection_t*,
                                                     TRI_voc_fid_t,
                                                     TRI_voc_size_t, bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief closes an existing journal
////////////////////////////////////////////////////////////////////////////////

bool TRI_CloseDatafileDocumentCollection(TRI_document_collection_t*, size_t,
                                         bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief fill the additional (non-primary) indexes
////////////////////////////////////////////////////////////////////////////////

int TRI_FillIndexesDocumentCollection(arangodb::Transaction*,
                                      TRI_vocbase_col_t*,
                                      TRI_document_collection_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief opens an existing collection
////////////////////////////////////////////////////////////////////////////////

TRI_document_collection_t* TRI_OpenDocumentCollection(TRI_vocbase_t*,
                                                      TRI_vocbase_col_t*, bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief closes an open collection
////////////////////////////////////////////////////////////////////////////////

int TRI_CloseDocumentCollection(TRI_document_collection_t*, bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief saves an index
////////////////////////////////////////////////////////////////////////////////

int TRI_SaveIndex(TRI_document_collection_t*, arangodb::Index*,
                  bool writeMarker);

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a description of all indexes
///
/// the caller must have read-locked the underyling collection!
////////////////////////////////////////////////////////////////////////////////

std::vector<std::shared_ptr<arangodb::velocypack::Builder>>
TRI_IndexesDocumentCollection(TRI_document_collection_t*, bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief drops an index, including index file removal and replication
////////////////////////////////////////////////////////////////////////////////

bool TRI_DropIndexDocumentCollection(TRI_document_collection_t*, TRI_idx_iid_t,
                                     bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up a cap constraint
////////////////////////////////////////////////////////////////////////////////

arangodb::Index* TRI_LookupCapConstraintDocumentCollection(
    TRI_document_collection_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures that a cap constraint exists
////////////////////////////////////////////////////////////////////////////////

arangodb::Index* TRI_EnsureCapConstraintDocumentCollection(
    arangodb::Transaction*, TRI_document_collection_t*, TRI_idx_iid_t, size_t,
    int64_t, bool&);

////////////////////////////////////////////////////////////////////////////////
/// @brief finds a geo index, list style
///
/// Note that the caller must hold at least a read-lock.
////////////////////////////////////////////////////////////////////////////////

arangodb::Index* TRI_LookupGeoIndex1DocumentCollection(
    TRI_document_collection_t*, std::string const&, bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief finds a geo index, attribute style
///
/// Note that the caller must hold at least a read-lock.
////////////////////////////////////////////////////////////////////////////////

arangodb::Index* TRI_LookupGeoIndex2DocumentCollection(
    TRI_document_collection_t*, std::string const&, std::string const&);

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures that a geo index exists, list style
////////////////////////////////////////////////////////////////////////////////

arangodb::Index* TRI_EnsureGeoIndex1DocumentCollection(
    arangodb::Transaction*, TRI_document_collection_t*, TRI_idx_iid_t,
    std::string const&, bool, bool&);

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures that a geo index exists, attribute style
////////////////////////////////////////////////////////////////////////////////

arangodb::Index* TRI_EnsureGeoIndex2DocumentCollection(
    arangodb::Transaction*, TRI_document_collection_t*, TRI_idx_iid_t,
    std::string const&, std::string const&, bool&);

////////////////////////////////////////////////////////////////////////////////
/// @brief finds a hash index
///
/// @note The caller must hold at least a read-lock.
///
/// @note The @FA{paths} must be sorted.
////////////////////////////////////////////////////////////////////////////////

arangodb::Index* TRI_LookupHashIndexDocumentCollection(
    TRI_document_collection_t*, std::vector<std::string> const&, int, bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures that a hash index exists
////////////////////////////////////////////////////////////////////////////////

arangodb::Index* TRI_EnsureHashIndexDocumentCollection(
    arangodb::Transaction* trx, TRI_document_collection_t*, TRI_idx_iid_t,
    std::vector<std::string> const&, bool, bool, bool&);

////////////////////////////////////////////////////////////////////////////////
/// @brief finds a skiplist index
///
/// Note that the caller must hold at least a read-lock.
////////////////////////////////////////////////////////////////////////////////

arangodb::Index* TRI_LookupSkiplistIndexDocumentCollection(
    TRI_document_collection_t*, std::vector<std::string> const&, int, bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures that a skiplist index exists
////////////////////////////////////////////////////////////////////////////////

arangodb::Index* TRI_EnsureSkiplistIndexDocumentCollection(
    arangodb::Transaction* trx, TRI_document_collection_t*, TRI_idx_iid_t,
    std::vector<std::string> const&, bool, bool, bool&);

////////////////////////////////////////////////////////////////////////////////
/// @brief finds a fulltext index
///
/// Note that the caller must hold at least a read-lock.
////////////////////////////////////////////////////////////////////////////////

arangodb::Index* TRI_LookupFulltextIndexDocumentCollection(
    TRI_document_collection_t*, std::string const&, int);

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures that a fulltext index exists
////////////////////////////////////////////////////////////////////////////////

arangodb::Index* TRI_EnsureFulltextIndexDocumentCollection(
    arangodb::Transaction* trx, TRI_document_collection_t*, TRI_idx_iid_t,
    std::string const&, int, bool&);

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a select-by-example query
////////////////////////////////////////////////////////////////////////////////

std::vector<TRI_doc_mptr_copy_t> TRI_SelectByExample(
    struct TRI_transaction_collection_s*,
    arangodb::ExampleMatcher const& matcher);

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a select-by-example query
////////////////////////////////////////////////////////////////////////////////

struct ShapedJsonHash {
  size_t operator()(TRI_shaped_json_t const& shap) const {
    uint64_t hash = 0x12345678;
    hash = fasthash64(&shap._sid, sizeof(shap._sid), hash);
    return static_cast<size_t>(
        fasthash64(shap._data.data, shap._data.length, hash));
  }
};

struct ShapedJsonEq {
  bool operator()(TRI_shaped_json_t const& left,
                  TRI_shaped_json_t const& right) const {
    if (left._sid != right._sid) {
      return false;
    }
    if (left._data.length != right._data.length) {
      return false;
    }
    return (memcmp(left._data.data, right._data.data, left._data.length) == 0);
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes a documet given by a master pointer
////////////////////////////////////////////////////////////////////////////////

int TRI_DeleteDocumentDocumentCollection(arangodb::Transaction*,
                                         struct TRI_transaction_collection_s*,
                                         TRI_doc_update_policy_t const*,
                                         TRI_doc_mptr_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief rotate the current journal of the collection
/// use this for testing only
////////////////////////////////////////////////////////////////////////////////

int TRI_RotateJournalDocumentCollection(TRI_document_collection_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief reads an element from the document collection
////////////////////////////////////////////////////////////////////////////////

int TRI_ReadShapedJsonDocumentCollection(arangodb::Transaction*,
                                         TRI_transaction_collection_t*,
                                         const TRI_voc_key_t,
                                         TRI_doc_mptr_copy_t*, bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief removes a shaped-json document (or edge)
////////////////////////////////////////////////////////////////////////////////

int TRI_RemoveShapedJsonDocumentCollection(arangodb::Transaction*,
                                           TRI_transaction_collection_t*,
                                           const TRI_voc_key_t, TRI_voc_rid_t,
                                           arangodb::wal::Marker*,
                                           TRI_doc_update_policy_t const*, bool,
                                           bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief insert a shaped-json document (or edge)
/// note: key might be NULL. in this case, a key is auto-generated
////////////////////////////////////////////////////////////////////////////////

int TRI_InsertShapedJsonDocumentCollection(
    arangodb::Transaction*, TRI_transaction_collection_t*, const TRI_voc_key_t,
    TRI_voc_rid_t, arangodb::wal::Marker*, TRI_doc_mptr_copy_t*,
    TRI_shaped_json_t const*, TRI_document_edge_t const*, bool, bool, bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief updates a document in the collection from shaped json
////////////////////////////////////////////////////////////////////////////////

int TRI_UpdateShapedJsonDocumentCollection(
    arangodb::Transaction*, TRI_transaction_collection_t*, const TRI_voc_key_t,
    TRI_voc_rid_t, arangodb::wal::Marker*, TRI_doc_mptr_copy_t*,
    TRI_shaped_json_t const*, TRI_doc_update_policy_t const*, bool, bool);

#endif

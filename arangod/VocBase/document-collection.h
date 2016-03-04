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
#include "Utils/OperationOptions.h"
#include "VocBase/collection.h"
#include "VocBase/DatafileHelper.h"
#include "VocBase/DatafileStatistics.h"
#include "VocBase/Ditch.h"
#include "VocBase/MasterPointers.h"
#include "VocBase/shaped-json.h"
#include "VocBase/update-policy.h"
#include "VocBase/voc-types.h"
#include "Wal/Marker.h"

class VocShaper;

namespace arangodb {
class EdgeIndex;
class ExampleMatcher;
class Index;
class KeyGenerator;
class PrimaryIndex;
class Transaction;
namespace velocypack {
class Builder;
class Slice;
}
namespace wal {
struct DocumentOperation;
}
}

////////////////////////////////////////////////////////////////////////////////
/// @brief master pointer
////////////////////////////////////////////////////////////////////////////////

struct TRI_doc_mptr_t {
 private:
  // this is the datafile identifier
  TRI_voc_fid_t _fid;   
  // the pre-calculated hash value of the key
  uint64_t _hash;       
  // this is the pointer to the beginning of the raw marker
  void const* _dataptr; 
    
  static_assert(sizeof(TRI_voc_fid_t) == sizeof(uint64_t), "invalid fid size");

 public:
  TRI_doc_mptr_t()
      : _fid(0),
        _hash(0),
        _dataptr(nullptr) {}

  // do NOT add virtual methods
  ~TRI_doc_mptr_t() {}

  void clear() {
    _fid = 0;
    _hash = 0;
    setDataPtr(nullptr);
  }

  // This is for cases where we explicitly have to copy originals!
  void copy(TRI_doc_mptr_t const& that) {
    _fid = that._fid;
    _dataptr = that._dataptr;
    _hash = that._hash;
  }
  
  // return the hash value for the primary key encapsulated by this
  // master pointer
  inline uint64_t getHash() const { return _hash; }
  
  // sets the hash value for the primary key encapsulated by this
  // master pointer
  inline void setHash(uint64_t hash) { _hash = hash; }

  // return the datafile id.
  inline TRI_voc_fid_t getFid() const { 
    // unmask the WAL bit
    return (_fid & ~arangodb::DatafileHelper::WalFileBitmask());
  }

  // sets datafile id. note that the highest bit of the file id must
  // not be set. the high bit will be used internally to distinguish
  // between WAL files and datafiles. if the highest bit is set, the
  // master pointer points into the WAL, and if not, it points into
  // a datafile
  inline void setFid(TRI_voc_fid_t fid, bool isWal) {
    // set the WAL bit if required
    _fid = fid;
    if (isWal) {
      _fid |= arangodb::DatafileHelper::WalFileBitmask();
    }
  }

  // return a pointer to the beginning of the marker 
  inline struct TRI_df_marker_t const* getMarkerPtr() const { 
    return static_cast<TRI_df_marker_t const*>(_dataptr); 
  }

  // return a pointer to the beginning of the marker
  inline void const* getDataPtr() const { return _dataptr; }

  // set the pointer to the beginning of the memory for the marker
  inline void setDataPtr(void const* value) { _dataptr = value; }

  // return a pointer to the beginning of the vpack  
  inline uint8_t const* vpack() const { 
    return reinterpret_cast<uint8_t const*>(_dataptr) + arangodb::DatafileHelper::VPackOffset(TRI_DF_MARKER_VPACK_DOCUMENT);
  }
  
  // whether or not the master pointer points into the WAL
  // the master pointer points into the WAL if the highest bit of
  // the _fid value is set, and to a datafile otherwise
  inline bool pointsToWal() const {
    // check whether the WAL bit is set
    return ((_fid & arangodb::DatafileHelper::WalFileBitmask()) == 1);
  }

  // return the marker's revision id
  TRI_voc_rid_t revisionId() const;
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
#ifndef ARANGODB_ENABLE_MAINTAINER_MODE
  VocShaper* getShaper() const { return _shaper; }
#else
  VocShaper* getShaper() const;
#endif

  std::unique_ptr<arangodb::FollowerInfo> const& followers() const {
    return _followers;
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief update statistics for a collection
  /// note: the write-lock for the collection must be held to call this
  ////////////////////////////////////////////////////////////////////////////////

  void setLastRevision(TRI_voc_rid_t, bool force);

  bool isFullyCollected();

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

  arangodb::Ditches* ditches() { return &_ditches; }

  mutable arangodb::Ditches _ditches;

  arangodb::MasterPointers _masterPointers;
  arangodb::KeyGenerator* _keyGenerator;

  std::vector<arangodb::Index*> _indexes;

  std::atomic<int64_t> _uncollectedLogfileEntries;
  int64_t _numberDocuments;
  arangodb::basics::ReadWriteLock _compactionLock;
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

  int read(arangodb::Transaction*, std::string const&,
           TRI_doc_mptr_t*, bool);
  int insert(arangodb::Transaction*, arangodb::velocypack::Slice const*,
             TRI_doc_mptr_t*, arangodb::OperationOptions&, bool);
  int update(arangodb::Transaction*, arangodb::velocypack::Slice const,
             TRI_doc_mptr_t*, arangodb::OperationOptions&, bool,
             TRI_voc_rid_t&);
  int replace(arangodb::Transaction*, arangodb::velocypack::Slice const,
             TRI_doc_mptr_t*, arangodb::OperationOptions&, bool,
             TRI_voc_rid_t&);
  int remove(arangodb::Transaction*, arangodb::velocypack::Slice const*,
             TRI_doc_update_policy_t const*, arangodb::OperationOptions&, bool);

  int rollbackOperation(arangodb::Transaction*, TRI_voc_document_operation_e, 
                        TRI_doc_mptr_t*, TRI_doc_mptr_t const*);

 private:
  arangodb::wal::Marker* createVPackInsertMarker(
      arangodb::Transaction*, arangodb::velocypack::Slice const*);
  arangodb::wal::Marker* createVPackInsertMarker(
      arangodb::Transaction*, arangodb::velocypack::Slice const&);
  arangodb::wal::Marker* createVPackRemoveMarker(
      arangodb::Transaction*, arangodb::velocypack::Slice const*);
  int lookupDocument(arangodb::Transaction*, arangodb::velocypack::Slice const*,
                     TRI_doc_update_policy_t const*, TRI_doc_mptr_t*&);
  int lookupDocument(arangodb::Transaction*, arangodb::velocypack::Slice const,
                     TRI_doc_mptr_t*&);
  int checkRevision(arangodb::Transaction*, arangodb::velocypack::Slice const,
                    TRI_voc_rid_t);
  int updateDocument(arangodb::Transaction*, TRI_voc_rid_t, TRI_doc_mptr_t*,
                     arangodb::wal::DocumentOperation&,
                     TRI_doc_mptr_t*, bool&);
  int insertDocument(arangodb::Transaction*, TRI_doc_mptr_t*,
                     arangodb::wal::DocumentOperation&, TRI_doc_mptr_t*,
                     bool&);
  int insertPrimaryIndex(arangodb::Transaction*, TRI_doc_mptr_t*);
  int insertSecondaryIndexes(arangodb::Transaction*, TRI_doc_mptr_t const*,
                             bool);
 public:
  int deletePrimaryIndex(arangodb::Transaction*, TRI_doc_mptr_t const*);
 private:
  int deleteSecondaryIndexes(arangodb::Transaction*, TRI_doc_mptr_t const*,
                             bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief new object for Replace
////////////////////////////////////////////////////////////////////////////////
    
  arangodb::velocypack::Builder newObjectForReplace(
      arangodb::Transaction* trx,
      arangodb::velocypack::Slice const oldValue,
      arangodb::velocypack::Slice const newValue,
      std::string const& rev);

////////////////////////////////////////////////////////////////////////////////
/// @brief merge two objects for update
////////////////////////////////////////////////////////////////////////////////
    
  arangodb::velocypack::Builder mergeObjectsForUpdate(
      arangodb::Transaction* trx,
      arangodb::velocypack::Slice const oldValue,
      arangodb::velocypack::Slice const newValue,
      std::string const& rev,
      bool mergeObjects, bool keepNull);
};

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
/// @brief extracts the pointer to the _from key from a marker
////////////////////////////////////////////////////////////////////////////////

static inline char const* TRI_EXTRACT_MARKER_FROM_KEY(
    TRI_df_marker_t const* marker) {
#if 0
  if (marker->_type == TRI_DOC_MARKER_KEY_EDGE) {
    return ((char const*)marker) +
           ((TRI_doc_edge_key_marker_t const*)marker)->_offsetFromKey;
  } else if (marker->_type == TRI_WAL_MARKER_EDGE) {
    return ((char const*)marker) +
           ((arangodb::wal::edge_marker_t const*)marker)->_offsetFromKey;
  }
#endif
  // invalid marker type
  TRI_ASSERT(false);

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
#if 0
  if (marker->_type == TRI_DOC_MARKER_KEY_EDGE) {
    return ((char const*)marker) +
           ((TRI_doc_edge_key_marker_t const*)marker)->_offsetToKey;
  } else if (marker->_type == TRI_WAL_MARKER_EDGE) {
    return ((char const*)marker) +
           ((arangodb::wal::edge_marker_t const*)marker)->_offsetToKey;
  }
#endif
  // invalid marker type
  TRI_ASSERT(false);

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
#if 0  
  if (marker->_type == TRI_DOC_MARKER_KEY_EDGE) {
    return ((TRI_doc_edge_key_marker_t const*)marker)->_fromCid;
  } else if (marker->_type == TRI_WAL_MARKER_EDGE) {
    return ((arangodb::wal::edge_marker_t const*)marker)->_fromCid;
  }
#endif

  // invalid marker type
  TRI_ASSERT(false);

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
#if 0  
  if (marker->_type == TRI_DOC_MARKER_KEY_EDGE) {
    return ((TRI_doc_edge_key_marker_t const*)marker)->_toCid;
  } else if (marker->_type == TRI_WAL_MARKER_EDGE) {
    return ((arangodb::wal::edge_marker_t const*)marker)->_toCid;
  }
#endif

  // invalid marker type
  TRI_ASSERT(false);

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
#if 0
  if (marker->_type == TRI_DOC_MARKER_KEY_DOCUMENT ||
      marker->_type == TRI_DOC_MARKER_KEY_EDGE) {
    return ((TRI_doc_document_key_marker_t const*)marker)->_rid;
  } else if (marker->_type == TRI_WAL_MARKER_DOCUMENT ||
             marker->_type == TRI_WAL_MARKER_EDGE) {
    return ((arangodb::wal::document_marker_t const*)marker)->_revisionId;
  }
#endif

  // invalid marker type
  TRI_ASSERT(false);

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
#if 0
  if (marker->_type == TRI_DOC_MARKER_KEY_DOCUMENT ||
      marker->_type == TRI_DOC_MARKER_KEY_EDGE) {
    return ((char const*)marker) +
           ((TRI_doc_document_key_marker_t const*)marker)->_offsetKey;
  } else if (marker->_type == TRI_WAL_MARKER_DOCUMENT ||
             marker->_type == TRI_WAL_MARKER_EDGE) {
    return ((char const*)marker) +
           ((arangodb::wal::document_marker_t const*)marker)->_offsetKey;
  }
#endif

  // invalid marker type
  TRI_ASSERT(false);

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
/// @brief create an index, based on a VelocyPack description
////////////////////////////////////////////////////////////////////////////////

int TRI_FromVelocyPackIndexDocumentCollection(
    arangodb::Transaction*, TRI_document_collection_t*,
    arangodb::velocypack::Slice const&, arangodb::Index**);

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
/// @brief finds a geo index, list style
///
/// Note that the caller must hold at least a read-lock.
////////////////////////////////////////////////////////////////////////////////

arangodb::Index* TRI_LookupGeoIndex1DocumentCollection(
    TRI_document_collection_t*, std::vector<std::string> const&, bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief finds a geo index, attribute style
///
/// Note that the caller must hold at least a read-lock.
////////////////////////////////////////////////////////////////////////////////

arangodb::Index* TRI_LookupGeoIndex2DocumentCollection(
    TRI_document_collection_t*, std::vector<std::string> const&,
    std::vector<std::string> const&);

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

std::vector<TRI_doc_mptr_t> TRI_SelectByExample(
    struct TRI_transaction_collection_t*,
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
                                         TRI_doc_mptr_t*, bool);

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
    TRI_voc_rid_t, arangodb::wal::Marker*, TRI_doc_mptr_t*,
    TRI_shaped_json_t const*, TRI_document_edge_t const*, bool, bool, bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief updates a document in the collection from shaped json
////////////////////////////////////////////////////////////////////////////////

int TRI_UpdateShapedJsonDocumentCollection(
    arangodb::Transaction*, TRI_transaction_collection_t*, const TRI_voc_key_t,
    TRI_voc_rid_t, arangodb::wal::Marker*, TRI_doc_mptr_t*,
    TRI_shaped_json_t const*, TRI_doc_update_policy_t const*, bool, bool);

#endif

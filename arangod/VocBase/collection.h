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

#ifndef ARANGOD_VOC_BASE_COLLECTION_H
#define ARANGOD_VOC_BASE_COLLECTION_H 1

#include "Basics/Common.h"
#include "Basics/ReadWriteLock.h"
#include "Cluster/ClusterInfo.h"
#include "VocBase/DatafileStatistics.h"
#include "VocBase/Ditch.h"
#include "VocBase/MasterPointer.h"
#include "VocBase/MasterPointers.h"
#include "VocBase/vocbase.h"

////////////////////////////////////////////////////////////////////////////////
/// Data is stored in datafiles. A set of datafiles forms a collection. A
/// datafile can be read-only and sealed or read-write. All datafiles of a
/// collection are stored in a directory. This directory contains the following
/// files:
///
/// - parameter.json: The parameters of a collection.
///
/// - datafile-NNN.db: A read-only datafile. The number NNN is the datafile
///     identifier, see @ref TRI_datafile_t.
///
/// - journal-NNN.db: A read-write datafile used as journal. All new entries
///     of a collection are appended to a journal. The number NNN is the
///     datafile identifier, see @ref TRI_datafile_t.
///
/// - index-NNN.json: An index description. The number NNN is the index
///     identifier, see @ref TRI_index_t.
////////////////////////////////////////////////////////////////////////////////

namespace arangodb {
class EdgeIndex;
class Index;
class KeyGenerator;
struct OperationOptions;
class PrimaryIndex;
class StringRef;
class Transaction;
namespace velocypack {
template <typename T>
class Buffer;
class Slice;
}
namespace wal {
struct DocumentOperation;
}
}

/// @brief current collection version
#define TRI_COL_VERSION 5

/// @brief predefined collection name for users
#define TRI_COL_NAME_USERS "_users"

/// @brief predefined collection name for statistics
#define TRI_COL_NAME_STATISTICS "_statistics"

/// @brief collection info
struct TRI_doc_collection_info_t {
  TRI_voc_ssize_t _numberDatafiles;
  TRI_voc_ssize_t _numberJournalfiles;
  TRI_voc_ssize_t _numberCompactorfiles;

  TRI_voc_ssize_t _numberAlive;
  TRI_voc_ssize_t _numberDead;
  TRI_voc_ssize_t _numberDeletions;
  TRI_voc_ssize_t _numberIndexes;

  int64_t _sizeAlive;
  int64_t _sizeDead;
  int64_t _sizeIndexes;

  int64_t _datafileSize;
  int64_t _journalfileSize;
  int64_t _compactorfileSize;

  TRI_voc_tick_t _tickMax;
  uint64_t _uncollectedLogfileEntries;
  uint64_t _numberDocumentDitches;
  char const* _waitingForDitch;
  char const* _lastCompactionStatus;
  char _lastCompactionStamp[21];
};

/// @brief state of the datafile
enum TRI_col_state_e {
  TRI_COL_STATE_CLOSED = 1,      // collection is closed
  TRI_COL_STATE_READ = 2,        // collection is opened read only
  TRI_COL_STATE_WRITE = 3,       // collection is opened read/append
  TRI_COL_STATE_OPEN_ERROR = 4,  // an error has occurred while opening
  TRI_COL_STATE_WRITE_ERROR = 5  // an error has occurred while writing
};

/// @brief collection version
typedef uint32_t TRI_col_version_t;

namespace arangodb {

/// @brief collection info block saved to disk as json
class VocbaseCollectionInfo {
 private:
  TRI_col_version_t _version;   // collection version
  TRI_col_type_e _type;         // collection type
  TRI_voc_rid_t _revision;      // last revision id written
  TRI_voc_cid_t _cid;           // local collection identifier
  TRI_voc_cid_t _planId;        // cluster-wide collection identifier
  TRI_voc_size_t _maximalSize;  // maximal size of memory mapped file
  int64_t _initialCount;        // initial count, used when loading a collection
  uint32_t _indexBuckets;  // number of buckets used in hash tables for indexes

  char _name[TRI_COL_PATH_LENGTH];  // name of the collection
  std::shared_ptr<arangodb::velocypack::Buffer<uint8_t> const>
      _keyOptions;  // options for key creation

  // flags
  bool _isSystem;     // if true, this is a system collection
  bool _deleted;      // if true, collection has been deleted
  bool _doCompact;    // if true, collection will be compacted
  bool _isVolatile;   // if true, collection is memory-only
  bool _waitForSync;  // if true, wait for msync

 public:
  VocbaseCollectionInfo() = default;
  ~VocbaseCollectionInfo() = default;

  VocbaseCollectionInfo(TRI_vocbase_t*, std::string const&, TRI_col_type_e,
                        TRI_voc_size_t, arangodb::velocypack::Slice const&);

  VocbaseCollectionInfo(TRI_vocbase_t*, std::string const&,
                        arangodb::velocypack::Slice const&,
                        bool forceIsSystem);

  VocbaseCollectionInfo(TRI_vocbase_t*, std::string const&, TRI_col_type_e,
                        arangodb::velocypack::Slice const&,
                        bool forceIsSystem);

  std::shared_ptr<VPackBuilder> toVelocyPack() const;
  void toVelocyPack(VPackBuilder& builder) const;

  /// @brief Creates a new VocbaseCollectionInfo from the json content of a file
  /// This function throws if the file cannot be parsed.
  static VocbaseCollectionInfo fromFile(std::string const& path, TRI_vocbase_t* vocbase,
                                        std::string const& collectionName, bool versionWarning);

  // collection version
  TRI_col_version_t version() const;

  // collection type
  TRI_col_type_e type() const;

  // local collection identifier
  TRI_voc_cid_t id() const;

  // cluster-wide collection identifier
  TRI_voc_cid_t planId() const;

  // last revision id written
  TRI_voc_rid_t revision() const;

  // maximal size of memory mapped file
  TRI_voc_size_t maximalSize() const;

  // initial count, used when loading a collection
  int64_t initialCount() const;

  // number of buckets used in hash tables for indexes
  uint32_t indexBuckets() const;

  // name of the collection
  std::string name() const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief returns a copy of the key options
  /// the caller is responsible for freeing it
  //////////////////////////////////////////////////////////////////////////////

  std::shared_ptr<arangodb::velocypack::Buffer<uint8_t> const> keyOptions()
      const;

  // If true, collection has been deleted
  bool deleted() const;

  // If true, collection will be compacted
  bool doCompact() const;

  // If true, collection is a system collection
  bool isSystem() const;

  // If true, collection is memory-only
  bool isVolatile() const;

  // If true waits for mysnc
  bool waitForSync() const;

  // Changes the name. Should only be called by TRI_collection_t::rename()
  // Use with caution!
  void rename(std::string const&);

  void setRevision(TRI_voc_rid_t, bool);

  void setCollectionId(TRI_voc_cid_t);

  void setPlanId(TRI_voc_cid_t);

  void updateCount(size_t);


  void setDeleted(bool);

  void clearKeyOptions();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief saves a parameter info block to file
  //////////////////////////////////////////////////////////////////////////////

  int saveToFile(std::string const&, bool) const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief updates settings for this collection info.
  ///        If the second parameter is false it will only
  ///        update the values explicitly contained in the slice.
  ///        If the second parameter is true and the third is a nullptr,
  ///        it will use global default values for all missing options in the
  ///        slice.
  ///        If the third parameter is not nullptr and the second is true, it
  ///        will
  ///        use the defaults stored in the vocbase.
  //////////////////////////////////////////////////////////////////////////////

  void update(arangodb::velocypack::Slice const&, bool, TRI_vocbase_t const*);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief updates settings for this collection info with the content of the
  /// other
  //////////////////////////////////////////////////////////////////////////////

  void update(VocbaseCollectionInfo const&);
};

}  // namespace arangodb

struct TRI_collection_t {
 public:
  TRI_collection_t(TRI_collection_t const&) = delete;
  TRI_collection_t& operator=(TRI_collection_t const&) = delete;
  TRI_collection_t() = delete;
  
  TRI_collection_t(TRI_vocbase_t* vocbase, arangodb::VocbaseCollectionInfo const& parameters);
  ~TRI_collection_t();

 public:
  /// @brief create a new collection
  static TRI_collection_t* create(TRI_vocbase_t*, arangodb::VocbaseCollectionInfo&, TRI_voc_cid_t);

  /// @brief opens an existing collection
  static TRI_collection_t* open(TRI_vocbase_t*, TRI_vocbase_col_t*, bool);

  /// @brief determine whether a collection name is a system collection name
  static inline bool IsSystemName(std::string const& name) {
    if (name.empty()) {
      return false;
    }
    return name[0] == '_';
  }

  /// @brief checks if a collection name is allowed
  /// returns true if the name is allowed and false otherwise
  static bool IsAllowedName(bool isSystem, std::string const& name);
  
  void setLastRevision(TRI_voc_rid_t, bool force);

  bool isFullyCollected();

  void setNextCompactionStartIndex(size_t);
  size_t getNextCompactionStartIndex();
  void setCompactionStatus(char const*);
  void getCompactionStatus(char const*&, char*, size_t);
  
  void addIndex(arangodb::Index*);
  std::vector<arangodb::Index*> const& allIndexes() const;
  arangodb::Index* lookupIndex(TRI_idx_iid_t) const;
  arangodb::PrimaryIndex* primaryIndex();
  arangodb::Index* removeIndex(TRI_idx_iid_t);
 
  /// @brief enumerate all indexes of the collection, but don't fill them yet
  int detectIndexes(arangodb::Transaction*);
 
  void iterateIndexes(std::function<bool(std::string const&, void*)> const&, void*);
  
  TRI_doc_collection_info_t* figures();

  int beginRead();
  int endRead();
  int beginWrite();
  int endWrite();
  int beginReadTimed(uint64_t, uint64_t);
  int beginWriteTimed(uint64_t, uint64_t);

  /// @brief updates the parameter info block
  int updateCollectionInfo(TRI_vocbase_t* vocbase,
                           arangodb::velocypack::Slice const& slice, bool doSync);

  // datafile management
  
  /// @brief rotate the active journal - will do nothing if there is no journal
  int rotateActiveJournal();

  /// @brief sync the active journal - will do nothing if there is no journal
  /// or if the journal is volatile
  int syncActiveJournal();
  int reserveJournalSpace(TRI_voc_tick_t tick, TRI_voc_size_t size,
                          char*& resultPosition, TRI_datafile_t*& resultDatafile);

  /// @brief create compactor file
  TRI_datafile_t* createCompactor(TRI_voc_fid_t fid, TRI_voc_size_t maximalSize);
  /// @brief close an existing compactor
  int closeCompactor(TRI_datafile_t* datafile);
  /// @brief replace a datafile with a compactor
  int replaceDatafileWithCompactor(TRI_datafile_t* datafile, TRI_datafile_t* compactor);

  bool removeCompactor(TRI_datafile_t*);
  bool removeDatafile(TRI_datafile_t*);
  void addIndexFile(std::string const&);
  bool removeIndexFile(TRI_idx_iid_t id);
  bool removeIndexFileFromVector(TRI_idx_iid_t id);
  std::string const& path() const { return _path; }
  std::string label() const;
  
  std::unique_ptr<arangodb::FollowerInfo> const& followers() const {
    return _followers;
  }
  
  arangodb::Ditches* ditches() { return &_ditches; }
  
  inline bool useSecondaryIndexes() const { return _useSecondaryIndexes; }

  void useSecondaryIndexes(bool value) { _useSecondaryIndexes = value; }

  /// @brief renames a collection
  int rename(std::string const& name);

  /// @brief iterates over a collection
  bool iterateDatafiles(std::function<bool(TRI_df_marker_t const*, TRI_datafile_t*)> const&);

  /// @brief opens an existing collection
  int open(bool ignoreErrors);

  /// @brief closes an open collection
  int close();
  
  int deletePrimaryIndex(arangodb::Transaction*, TRI_doc_mptr_t const*);
  
  // function that is called to garbage-collect the collection's indexes
  int cleanupIndexes();

  int read(arangodb::Transaction*, std::string const&, TRI_doc_mptr_t*, bool);
  int read(arangodb::Transaction*, arangodb::StringRef const&, TRI_doc_mptr_t*, bool);
  int insert(arangodb::Transaction*, arangodb::velocypack::Slice const,
             TRI_doc_mptr_t*, arangodb::OperationOptions&, TRI_voc_tick_t&, bool);
  int update(arangodb::Transaction*, arangodb::velocypack::Slice const,
             TRI_doc_mptr_t*, arangodb::OperationOptions&, TRI_voc_tick_t&, bool,
             VPackSlice&, TRI_doc_mptr_t&);
  int replace(arangodb::Transaction*, arangodb::velocypack::Slice const,
             TRI_doc_mptr_t*, arangodb::OperationOptions&, TRI_voc_tick_t&, bool,
             VPackSlice&, TRI_doc_mptr_t&);
  int remove(arangodb::Transaction*, arangodb::velocypack::Slice const,
             arangodb::OperationOptions&, TRI_voc_tick_t&, bool, 
             VPackSlice&, TRI_doc_mptr_t&);

  int rollbackOperation(arangodb::Transaction*, TRI_voc_document_operation_e, 
                        TRI_doc_mptr_t*, TRI_doc_mptr_t const*);

  /// @brief fill the additional (non-primary) indexes
  int fillIndexes(arangodb::Transaction* trx,
                  TRI_vocbase_col_t* collection);

  /// @brief initializes an index with all existing documents
  int fillIndex(arangodb::Transaction* trx,
                arangodb::Index* idx,
                bool skipPersistent = true);

  /// @brief saves an index
  int saveIndex(arangodb::Index* idx, bool writeMarker);

  /// @brief returns a description of all indexes
  /// the caller must have read-locked the underyling collection!
  std::vector<std::shared_ptr<arangodb::velocypack::Builder>>
  indexesToVelocyPack(bool withFigures);

  /// @brief drops an index, including index file removal and replication
  bool dropIndex(TRI_idx_iid_t iid, bool writeMarker);

  /// @brief finds a geo index, list style
  /// Note that the caller must hold at least a read-lock.
  arangodb::Index* lookupGeoIndex1(std::vector<std::string> const&, bool);

  /// @brief finds a geo index, attribute style
  arangodb::Index* lookupGeoIndex2(std::vector<std::string> const&,
    std::vector<std::string> const&);

  /// @brief ensures that a geo index exists, list style
  arangodb::Index* ensureGeoIndex1(arangodb::Transaction*, TRI_idx_iid_t,
    std::string const&, bool, bool&);

  /// @brief ensures that a geo index exists, attribute style
  arangodb::Index* ensureGeoIndex2(arangodb::Transaction*, TRI_idx_iid_t,
    std::string const&, std::string const&, bool&);

  /// @brief finds a hash index
  arangodb::Index* lookupHashIndex(std::vector<std::string> const&, int, bool);

  /// @brief ensures that a hash index exists
  arangodb::Index* ensureHashIndex(arangodb::Transaction* trx, TRI_idx_iid_t,
    std::vector<std::string> const&, bool, bool, bool&);

  /// @brief finds a skiplist index
  arangodb::Index* lookupSkiplistIndex(std::vector<std::string> const&, int, bool);

  /// @brief ensures that a skiplist index exists
  arangodb::Index* ensureSkiplistIndex(arangodb::Transaction* trx, TRI_idx_iid_t,
    std::vector<std::string> const&, bool, bool, bool&);

  /// @brief finds a RocksDB index
  arangodb::Index* lookupRocksDBIndex(std::vector<std::string> const&, int, bool);

  /// @brief ensures that a RocksDB index exists
  arangodb::Index* ensureRocksDBIndex(arangodb::Transaction* trx, TRI_idx_iid_t,
    std::vector<std::string> const&, bool, bool, bool&);
  
  /// @brief finds a fulltext index
  arangodb::Index* lookupFulltextIndex(std::string const&, int);

  /// @brief ensures that a fulltext index exists
  arangodb::Index* ensureFulltextIndex(arangodb::Transaction* trx, TRI_idx_iid_t,
    std::string const&, int, bool&);

  /// @brief create an index, based on a VelocyPack description
  int indexFromVelocyPack(arangodb::Transaction* trx, 
      VPackSlice const& slice, arangodb::Index** idx);

  /// @brief closes an open collection
  int unload(bool updateStatus);

 private:
  int lookupDocument(arangodb::Transaction*, arangodb::velocypack::Slice const,
                     TRI_doc_mptr_t*&);
  int checkRevision(arangodb::Transaction*, arangodb::velocypack::Slice const,
                    arangodb::velocypack::Slice const);
  int updateDocument(arangodb::Transaction*, TRI_voc_rid_t, TRI_doc_mptr_t*,
                     arangodb::wal::DocumentOperation&,
                     TRI_doc_mptr_t*, bool&);
  int insertDocument(arangodb::Transaction*, TRI_doc_mptr_t*,
                     arangodb::wal::DocumentOperation&, TRI_doc_mptr_t*,
                     bool&);
  int insertPrimaryIndex(arangodb::Transaction*, TRI_doc_mptr_t*);
  int insertSecondaryIndexes(arangodb::Transaction*, TRI_doc_mptr_t const*,
                             bool);
  /// @brief seal a datafile
  int sealDatafile(TRI_datafile_t* datafile, bool isCompactor);
  /// @brief creates a datafile
  TRI_datafile_t* createDatafile(TRI_voc_fid_t fid,
                                 TRI_voc_size_t journalSize, 
                                 bool isCompactor);

  // worker function for creating a collection
  int createWorker(); 

  /// @brief creates the initial indexes for the collection
  int createInitialIndexes();

  /// @brief closes the datafiles passed in the vector
  bool closeDataFiles(std::vector<TRI_datafile_t*> const& files);
  
  bool iterateDatafilesVector(std::vector<TRI_datafile_t*> const& files,
                              std::function<bool(TRI_df_marker_t const*, TRI_datafile_t*)> const& cb);
 
  int deleteSecondaryIndexes(arangodb::Transaction*, TRI_doc_mptr_t const*,
                             bool);

  /// @brief new object for insert, value must have _key set correctly.
  int newObjectForInsert(
      arangodb::Transaction* trx,
      arangodb::velocypack::Slice const& value,
      arangodb::velocypack::Slice const& fromSlice,
      arangodb::velocypack::Slice const& toSlice,
      bool isEdgeCollection,
      uint64_t& hash,
      arangodb::velocypack::Builder& builder,
      bool isRestore);

  /// @brief new object for replace
  void newObjectForReplace(
      arangodb::Transaction* trx,
      arangodb::velocypack::Slice const& oldValue,
      arangodb::velocypack::Slice const& newValue,
      arangodb::velocypack::Slice const& fromSlice,
      arangodb::velocypack::Slice const& toSlice,
      bool isEdgeCollection,
      std::string const& rev,
      arangodb::velocypack::Builder& builder);

  /// @brief merge two objects for update
  void mergeObjectsForUpdate(
      arangodb::Transaction* trx,
      arangodb::velocypack::Slice const& oldValue,
      arangodb::velocypack::Slice const& newValue,
      bool isEdgeCollection,
      std::string const& rev,
      bool mergeObjects, bool keepNull,
      arangodb::velocypack::Builder& b);

  /// @brief new object for remove, must have _key set
  void newObjectForRemove(
      arangodb::Transaction* trx,
      arangodb::velocypack::Slice const& oldValue,
      std::string const& rev,
      arangodb::velocypack::Builder& builder);

  /// @brief fill an index in batches
  int fillIndexBatch(arangodb::Transaction* trx, arangodb::Index* idx);

  /// @brief fill an index sequentially
  int fillIndexSequential(arangodb::Transaction* trx, arangodb::Index* idx);

 public:
  TRI_vocbase_t* _vocbase;
  TRI_voc_tick_t _tickMax;
 
  /// @brief a lock protecting the _info structure
  arangodb::basics::ReadWriteLock _infoLock;
  arangodb::VocbaseCollectionInfo _info;

  TRI_col_state_e _state;  // state of the collection
  int _lastError;          // last (critical) error
 
 private: 
  std::string _path;

  // the following contains in the cluster/DBserver case the information
  // which other servers are in sync with this shard. It is unset in all
  // other cases.
  std::unique_ptr<arangodb::FollowerInfo> _followers;

 public:
  arangodb::basics::ReadWriteLock _filesLock;
  std::vector<TRI_datafile_t*> _datafiles;   // all datafiles
  std::vector<TRI_datafile_t*> _journals;    // all journals
  std::vector<TRI_datafile_t*> _compactors;  // all compactor files
  
  arangodb::DatafileStatistics _datafileStatistics;
  
  mutable arangodb::Ditches _ditches;

  arangodb::MasterPointers _masterPointers;

  std::unique_ptr<arangodb::KeyGenerator> _keyGenerator;

  std::atomic<int64_t> _uncollectedLogfileEntries;
  int64_t _numberDocuments;
  arangodb::basics::ReadWriteLock _compactionLock;
  double _lastCompaction;
  
 private:
  std::vector<arangodb::Index*> _indexes;

  // whether or not any of the indexes may need to be garbage-collected
  // this flag may be modifying when an index is added to a collection
  // if true, the cleanup thread will periodically call the cleanup functions of
  // the collection's indexes that support cleanup
  size_t _cleanupIndexes;

  // number of persistent indexes
  size_t _persistentIndexes;

  arangodb::Mutex _compactionStatusLock;
  size_t _nextCompactionStartIndex;
  char const* _lastCompactionStatus;
  char _lastCompactionStamp[21];

  // whether or not secondary indexes should be filled
  bool _useSecondaryIndexes;
  
  // lock for indexes
  arangodb::basics::ReadWriteLock _lock;

  std::vector<std::string> _indexFiles;   // all index filenames
};

#endif

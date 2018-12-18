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

#ifndef ARANGOD_MMFILES_MMFILES_COLLECTION_H
#define ARANGOD_MMFILES_MMFILES_COLLECTION_H 1

#include "Basics/Common.h"
#include "Basics/Mutex.h"
#include "Basics/ReadWriteLock.h"
#include "Indexes/IndexIterator.h"
#include "MMFiles/MMFilesIndexLookupContext.h"
#include "MMFiles/MMFilesDatafileStatistics.h"
#include "MMFiles/MMFilesDatafileStatisticsContainer.h"
#include "MMFiles/MMFilesDitch.h"
#include "MMFiles/MMFilesDocumentPosition.h"
#include "MMFiles/MMFilesRevisionsCache.h"
#include "StorageEngine/PhysicalCollection.h"
#include "VocBase/KeyGenerator.h"
#include "VocBase/LocalDocumentId.h"
#include "VocBase/LogicalCollection.h"

struct MMFilesDatafile;
struct MMFilesMarker;

namespace arangodb {
class LogicalCollection;
class ManagedDocumentResult;
struct MMFilesDocumentOperation;
class MMFilesPrimaryIndex;
class MMFilesWalMarker;
struct OpenIteratorState;
class Result;
class TransactionState;

class MMFilesCollection final : public PhysicalCollection {
  friend class MMFilesCompactorThread;
  friend class MMFilesEngine;

 public:
  static inline MMFilesCollection* toMMFilesCollection(
      PhysicalCollection* physical) {
    auto rv = static_cast<MMFilesCollection*>(physical);
    TRI_ASSERT(rv != nullptr);
    return rv;
  }

  static inline MMFilesCollection* toMMFilesCollection(
      LogicalCollection* logical) {
    auto phys = logical->getPhysical();
    TRI_ASSERT(phys != nullptr);
    return toMMFilesCollection(phys);
  }
  
  struct DatafileDescription {
    MMFilesDatafile const* _data;
    TRI_voc_tick_t _dataMin;
    TRI_voc_tick_t _dataMax;
    TRI_voc_tick_t _tickMax;
    bool _isJournal;
  };

  explicit MMFilesCollection(
    LogicalCollection& collection,
    arangodb::velocypack::Slice const& info
  );
  MMFilesCollection(
    LogicalCollection& collection,
    PhysicalCollection const* physical
  );  // use in cluster only!!!!!

  ~MMFilesCollection();

  static constexpr uint32_t defaultIndexBuckets = 8;

  static constexpr double defaultLockTimeout = 10.0 * 60.0;

  std::string const& path() const override { return _path; };

  void setPath(std::string const& path) override { _path = path; };

  arangodb::Result updateProperties(VPackSlice const& slice,
                                    bool doSync) override;
  virtual arangodb::Result persistProperties() override;

  virtual PhysicalCollection* clone(LogicalCollection& logical) const override;

  TRI_voc_rid_t revision(arangodb::transaction::Methods* trx) const override;
  TRI_voc_rid_t revision() const;

  void setRevision(TRI_voc_rid_t revision, bool force);

  size_t journalSize() const;
  bool isVolatile() const;

  TRI_voc_tick_t maxTick() const { return _maxTick; }
  void maxTick(TRI_voc_tick_t value) { _maxTick = value; }

  /// @brief export properties
  void getPropertiesVPack(velocypack::Builder&) const override;

  // datafile management
  bool applyForTickRange(
      TRI_voc_tick_t dataMin, TRI_voc_tick_t dataMax,
      std::function<bool(TRI_voc_tick_t foundTick,
                         MMFilesMarker const* marker)> const& callback);

  /// @brief closes an open collection
  int close() override;
  void load() override {}
  void unload() override {}

  /// @brief set the initial datafiles for the collection
  void setInitialFiles(std::vector<MMFilesDatafile*>&& datafiles,
                       std::vector<MMFilesDatafile*>&& journals,
                       std::vector<MMFilesDatafile*>&& compactors);

  /// @brief rotate the active journal - will do nothing if there is no journal
  int rotateActiveJournal();

  /// @brief sync the active journal - will do nothing if there is no journal
  /// or if the journal is volatile
  int syncActiveJournal();

  int reserveJournalSpace(TRI_voc_tick_t tick, uint32_t size,
                          char*& resultPosition,
                          MMFilesDatafile*& resultDatafile);

  /// @brief create compactor file
  MMFilesDatafile* createCompactor(TRI_voc_fid_t fid,
                                   uint32_t maximalSize);

  /// @brief close an existing compactor
  int closeCompactor(MMFilesDatafile* datafile);

  /// @brief replace a datafile with a compactor
  int replaceDatafileWithCompactor(MMFilesDatafile* datafile,
                                   MMFilesDatafile* compactor);

  bool removeCompactor(MMFilesDatafile*);
  bool removeDatafile(MMFilesDatafile*);

  /// @brief seal a datafile
  int sealDatafile(MMFilesDatafile* datafile, bool isCompactor);

  /// @brief increase dead stats for a datafile, if it exists
  void updateStats(TRI_voc_fid_t fid,
                   MMFilesDatafileStatisticsContainer const& values,
                   bool warn) {
    _datafileStatistics.update(fid, values, warn);
  }

  uint64_t numberDocuments(transaction::Methods* trx) const override;

  /// @brief report extra memory used by indexes etc.
  size_t memory() const override;

  void preventCompaction();
  bool tryPreventCompaction();
  void allowCompaction();
  void lockForCompaction();
  bool tryLockForCompaction();
  void finishCompaction();

  void setNextCompactionStartIndex(size_t index) {
    MUTEX_LOCKER(mutexLocker, _compactionStatusLock);
    _nextCompactionStartIndex = index;
  }

  size_t getNextCompactionStartIndex() {
    MUTEX_LOCKER(mutexLocker, _compactionStatusLock);
    return _nextCompactionStartIndex;
  }

  void setCompactionStatus(char const* reason) {
    TRI_ASSERT(reason != nullptr);
    MUTEX_LOCKER(mutexLocker, _compactionStatusLock);
    _lastCompactionStatus = reason;
  }
  double lastCompactionStamp() const { return _lastCompactionStamp; }
  void lastCompactionStamp(double value) { _lastCompactionStamp = value; }

  MMFilesDitches* ditches() const { return &_ditches; }

  void open(bool ignoreErrors) override;

  /// @brief iterate all markers of a collection on load
  int iterateMarkersOnLoad(arangodb::transaction::Methods* trx);

  bool isFullyCollected() const;

  bool doCompact() const { return _doCompact; }

  int64_t uncollectedLogfileEntries() const {
    return _uncollectedLogfileEntries.load();
  }

  void increaseUncollectedLogfileEntries(int64_t value) {
    _uncollectedLogfileEntries += value;
  }

  void decreaseUncollectedLogfileEntries(int64_t value) {
    _uncollectedLogfileEntries -= value;
    if (_uncollectedLogfileEntries < 0) {
      _uncollectedLogfileEntries = 0;
    }
  }

  ////////////////////////////////////
  // -- SECTION Indexes --
  ///////////////////////////////////

  uint32_t indexBuckets() const;

  // WARNING: Make sure that this Collection Instance
  // is somehow protected. If it goes out of all scopes
  // or it's indexes are freed the pointer returned will get invalidated.
  MMFilesPrimaryIndex* primaryIndex() const {
    TRI_ASSERT(_primaryIndex != nullptr);
    return _primaryIndex;
  }

  inline bool useSecondaryIndexes() const { return _useSecondaryIndexes; }

  void useSecondaryIndexes(bool value) { _useSecondaryIndexes = value; }

  int fillAllIndexes(transaction::Methods& trx);

  void prepareIndexes(arangodb::velocypack::Slice indexesSlice) override;

  /// @brief Find index by definition
  std::shared_ptr<Index> lookupIndex(velocypack::Slice const&) const override;

  std::unique_ptr<IndexIterator> getAllIterator(transaction::Methods* trx) const override;
  std::unique_ptr<IndexIterator> getAnyIterator(transaction::Methods* trx) const override;
  void invokeOnAllElements(
      transaction::Methods* trx,
      std::function<bool(LocalDocumentId const&)> callback) override;

  std::shared_ptr<Index> createIndex(arangodb::velocypack::Slice const& info,
                                     bool restore, bool& created) override;

  std::shared_ptr<Index> createIndex(
    transaction::Methods& trx,
    velocypack::Slice const& info,
    bool restore,
    bool& created
  );

  /// @brief Drop an index with the given iid.
  bool dropIndex(TRI_idx_iid_t iid) override;

  ////////////////////////////////////
  // -- SECTION Locking --
  ///////////////////////////////////

  int lockRead(bool useDeadlockDetector, TransactionState const* state, double timeout = 0.0);

  int lockWrite(bool useDeadlockDetector, TransactionState const* state, double timeout = 0.0);

  int unlockRead(bool useDeadlockDetector, TransactionState const* state);

  int unlockWrite(bool useDeadlockDetector, TransactionState const* state);

  ////////////////////////////////////
  // -- SECTION DML Operations --
  ///////////////////////////////////

  Result truncate(
    transaction::Methods& trx,
    OperationOptions& options
  ) override;

  /// @brief Defer a callback to be executed when the collection
  ///        can be dropped. The callback is supposed to drop
  ///        the collection and it is guaranteed that no one is using
  ///        it at that moment.
  void deferDropCollection(
    std::function<bool(LogicalCollection&)> const& callback
  ) override;

  LocalDocumentId lookupKey(transaction::Methods* trx,
                            velocypack::Slice const& key) const override;

  Result read(transaction::Methods*, arangodb::StringRef const& key,
              ManagedDocumentResult& result, bool) override;

  Result read(transaction::Methods*, arangodb::velocypack::Slice const& key,
              ManagedDocumentResult& result, bool) override;

  bool readDocument(transaction::Methods* trx,
                    LocalDocumentId const& documentId,
                    ManagedDocumentResult& result) const override;

  bool readDocumentWithCallback(transaction::Methods* trx,
                                LocalDocumentId const& documentId,
                                IndexIterator::DocumentCallback const& cb) const override;

  size_t readDocumentWithCallback(transaction::Methods* trx,
                                  std::vector<std::pair<LocalDocumentId, uint8_t const*>>& documentIds,
                                  IndexIterator::DocumentCallback const& cb);

  bool readDocumentConditional(transaction::Methods* trx,
                               LocalDocumentId const& documentId,
                               TRI_voc_tick_t maxTick,
                               ManagedDocumentResult& result);

  Result insert(arangodb::transaction::Methods* trx,
                arangodb::velocypack::Slice newSlice,
                arangodb::ManagedDocumentResult& result,
                OperationOptions& options, TRI_voc_tick_t& resultMarkerTick,
                bool lock, TRI_voc_tick_t& revisionId,
                KeyLockInfo* keyLockInfo,
                std::function<Result(void)> callbackDuringLock) override;

  Result update(arangodb::transaction::Methods* trx,
                arangodb::velocypack::Slice newSlice,
                ManagedDocumentResult& result, OperationOptions& options,
                TRI_voc_tick_t& resultMarkerTick, bool lock,
                TRI_voc_rid_t& prevRev, ManagedDocumentResult& previous,
                arangodb::velocypack::Slice key,
                std::function<Result(void)> callbackDuringLock) override;

  Result replace(transaction::Methods* trx,
                 arangodb::velocypack::Slice newSlice,
                 ManagedDocumentResult& result, OperationOptions& options,
                 TRI_voc_tick_t& resultMarkerTick, bool lock,
                 TRI_voc_rid_t& prevRev, ManagedDocumentResult& previous,
                 std::function<Result(void)> callbackDuringLock) override;

  Result remove(
    transaction::Methods& trx,
    velocypack::Slice slice,
    ManagedDocumentResult& previous,
    OperationOptions& options,
    TRI_voc_tick_t& resultMarkerTick,
    bool lock,
    TRI_voc_rid_t& prevRev,
    TRI_voc_rid_t& revisionId,
    KeyLockInfo* keyLockInfo,
    std::function<Result(void)> callbackDuringLock
  ) override;

  Result rollbackOperation(
    transaction::Methods& trx,
    TRI_voc_document_operation_e type,
    LocalDocumentId const& oldDocumentId,
    velocypack::Slice const& oldDoc,
    LocalDocumentId const& newDocumentId,
    velocypack::Slice const& newDoc
  );

  MMFilesDocumentPosition insertLocalDocumentId(LocalDocumentId const& documentId,
                                                uint8_t const* dataptr,
                                                TRI_voc_fid_t fid, bool isInWal,
                                                bool shouldLock);

  void insertLocalDocumentId(MMFilesDocumentPosition const& position, bool shouldLock);

  void updateLocalDocumentId(LocalDocumentId const& documentId, uint8_t const* dataptr,
                             TRI_voc_fid_t fid, bool isInWal);

  bool updateLocalDocumentIdConditional(LocalDocumentId const& documentId,
                                        MMFilesMarker const* oldPosition,
                                        MMFilesMarker const* newPosition,
                                        TRI_voc_fid_t newFid, bool isInWal);

  void removeLocalDocumentId(LocalDocumentId const& documentId, bool updateStats);

  Result persistLocalDocumentIds();
  
  bool hasAllPersistentLocalIds() const;

 private:
  void sizeHint(transaction::Methods* trx, int64_t hint);

  bool openIndex(
    velocypack::Slice const& description,
    transaction::Methods& trx
  );

  /// @brief initializes an index with all existing documents
  void fillIndex(
    std::shared_ptr<basics::LocalTaskQueue> queue,
    transaction::Methods& trx,
    Index* index,
    std::shared_ptr<std::vector<std::pair<LocalDocumentId, velocypack::Slice>>> docs,
    bool skipPersistent
  );

  /// @brief Fill indexes used in recovery
  int fillIndexes(
    transaction::Methods& trx,
    std::vector<std::shared_ptr<Index>> const& indexes,
    bool skipPersistent = true
  );

  int openWorker(bool ignoreErrors);

  Result removeFastPath(
    transaction::Methods& trx,
    TRI_voc_rid_t revisionId,
    LocalDocumentId const& oldDocumentId,
    velocypack::Slice const oldDoc,
    OperationOptions& options,
    LocalDocumentId const& documentId,
    velocypack::Slice const toRemove
  );

  static int OpenIteratorHandleDocumentMarker(MMFilesMarker const* marker,
                                              MMFilesDatafile* datafile,
                                              OpenIteratorState* state);
  static int OpenIteratorHandleDeletionMarker(MMFilesMarker const* marker,
                                              MMFilesDatafile* datafile,
                                              OpenIteratorState* state);
  static bool OpenIterator(MMFilesMarker const* marker, OpenIteratorState* data,
                           MMFilesDatafile* datafile);

  /// @brief create statistics for a datafile, using the stats provided
  void createStats(TRI_voc_fid_t fid,
                   MMFilesDatafileStatisticsContainer const& values) {
    _datafileStatistics.create(fid, values);
  }

  /// @brief iterates over a collection
  bool iterateDatafiles(
      std::function<bool(MMFilesMarker const*, MMFilesDatafile*)> const& cb);

  /// @brief creates a datafile
  MMFilesDatafile* createDatafile(TRI_voc_fid_t fid, uint32_t journalSize,
                                  bool isCompactor);

  /// @brief iterate over a vector of datafiles and pick those with a specific
  /// data range
  std::vector<DatafileDescription> datafilesInRange(TRI_voc_tick_t dataMin,
                                                    TRI_voc_tick_t dataMax);

  /// @brief closes the datafiles passed in the vector
  bool closeDatafiles(std::vector<MMFilesDatafile*> const& files);

  bool iterateDatafilesVector(
      std::vector<MMFilesDatafile*> const& files,
      std::function<bool(MMFilesMarker const*, MMFilesDatafile*)> const& cb);

  MMFilesDocumentPosition lookupDocument(LocalDocumentId const& documentId) const;

  Result insertDocument(
    transaction::Methods& trx,
    LocalDocumentId const& documentId,
    TRI_voc_rid_t revisionId,
    velocypack::Slice const& doc,
    MMFilesDocumentOperation& operation,
    MMFilesWalMarker const* marker,
    OperationOptions& options,
    bool& waitForSync
  );

  uint8_t const* lookupDocumentVPack(LocalDocumentId const& documentId) const;
  uint8_t const* lookupDocumentVPackConditional(LocalDocumentId const& documentId,
                                                TRI_voc_tick_t maxTick,
                                                bool excludeWal) const;
  void batchLookupRevisionVPack(std::vector<std::pair<LocalDocumentId, uint8_t const*>>& documentIds) const;

  void createInitialIndexes();
  bool addIndex(std::shared_ptr<arangodb::Index> idx);
  void addIndexLocal(std::shared_ptr<arangodb::Index> idx);

  bool removeIndex(TRI_idx_iid_t iid);

  /// @brief return engine-specific figures
  void figuresSpecific(std::shared_ptr<arangodb::velocypack::Builder>&) override;

  // SECTION: Index storage

  int saveIndex(transaction::Methods& trx, std::shared_ptr<Index> idx);

  /// @brief Detect all indexes form file
  int detectIndexes(transaction::Methods& trx);

  Result insertIndexes(
    transaction::Methods& trx,
    LocalDocumentId const& documentId,
    velocypack::Slice const& doc,
    OperationOptions& options
  );

  Result insertPrimaryIndex(transaction::Methods*, LocalDocumentId const& documentId, velocypack::Slice const&, OperationOptions& options);

  Result deletePrimaryIndex(transaction::Methods*, LocalDocumentId const& documentId, velocypack::Slice const&, OperationOptions& options);

  Result insertSecondaryIndexes(
    transaction::Methods& trx,
    LocalDocumentId const& documentId,
    velocypack::Slice const& doc,
    Index::OperationMode mode
  );

  Result deleteSecondaryIndexes(
    transaction::Methods& trx,
    LocalDocumentId const& documentId,
    velocypack::Slice const& doc,
    Index::OperationMode mode
  );

  Result lookupDocument(transaction::Methods*, velocypack::Slice,
                        ManagedDocumentResult& result);

  Result updateDocument(
    transaction::Methods& trx,
    TRI_voc_rid_t revisionId,
    LocalDocumentId const& oldDocumentId,
    velocypack::Slice const& oldDoc,
    LocalDocumentId const& newDocumentId,
    velocypack::Slice const& newDoc,
    MMFilesDocumentOperation& operation,
    MMFilesWalMarker const* marker,
    OperationOptions& options,
    bool& waitForSync
  );

  LocalDocumentId reuseOrCreateLocalDocumentId(OperationOptions const& options) const;

  static Result persistLocalDocumentIdsForDatafile(
      MMFilesCollection& collection, MMFilesDatafile& file);

  void setCurrentVersion();
  
  // key locking
  struct KeyLockShard;
  void lockKey(KeyLockInfo& keyLockInfo, arangodb::velocypack::Slice const& key);
  void unlockKey(KeyLockInfo& keyLockInfo) noexcept;
  KeyLockShard& getShardForKey(std::string const& key) noexcept;

 private:
  mutable arangodb::MMFilesDitches _ditches;

  // lock protecting the indexes and data
  mutable basics::ReadWriteLock _dataLock;

  arangodb::basics::ReadWriteLock _filesLock;
  std::vector<MMFilesDatafile*> _datafiles;   // all datafiles
  std::vector<MMFilesDatafile*> _journals;    // all journals
  std::vector<MMFilesDatafile*> _compactors;  // all compactor files

  arangodb::basics::ReadWriteLock _compactionLock;

  int64_t _initialCount;

  MMFilesDatafileStatistics _datafileStatistics;

  TRI_voc_rid_t _lastRevision;

  MMFilesRevisionsCache _revisionsCache;

  std::atomic<int64_t> _uncollectedLogfileEntries;

  Mutex _compactionStatusLock;
  size_t _nextCompactionStartIndex;
  char const* _lastCompactionStatus;
  double _lastCompactionStamp;
  std::string _path;
  uint32_t _journalSize;

  bool const _isVolatile;

  // SECTION: Indexes

  size_t _persistentIndexes;
  MMFilesPrimaryIndex* _primaryIndex;
  uint32_t _indexBuckets;

  // whether or not secondary indexes should be filled
  bool _useSecondaryIndexes;

  bool _doCompact;
  TRI_voc_tick_t _maxTick;

  // currently locked keys
  struct KeyLockShard {
    Mutex _mutex;
    std::unordered_set<std::string> _keys;
    // TODO: add padding here so we can avoid false sharing
  };

  static constexpr size_t numKeyLockShards = 8;


  std::array<KeyLockShard, numKeyLockShards> _keyLockShards;

  // whether or not all documents are stored with a persistent LocalDocumentId
  std::atomic<bool> _hasAllPersistentLocalIds{true};
};
}

#endif
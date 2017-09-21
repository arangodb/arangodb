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
#include "Indexes/IndexLookupContext.h"
#include "MMFiles/MMFilesDatafileStatistics.h"
#include "MMFiles/MMFilesDatafileStatisticsContainer.h"
#include "MMFiles/MMFilesDitch.h"
#include "MMFiles/MMFilesDocumentPosition.h"
#include "MMFiles/MMFilesRevisionsCache.h"
#include "StorageEngine/PhysicalCollection.h"
#include "VocBase/KeyGenerator.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ManagedDocumentResult.h"

struct MMFilesDatafile;
struct MMFilesMarker;

namespace arangodb {
class LogicalCollection;
class ManagedDocumentResult;
struct MMFilesDocumentOperation;
class MMFilesPrimaryIndex;
class MMFilesWalMarker;
class Result;

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

  /// @brief state during opening of a collection
  struct OpenIteratorState {
    LogicalCollection* _collection;
    arangodb::MMFilesPrimaryIndex* _primaryIndex;
    TRI_voc_tid_t _tid;
    TRI_voc_fid_t _fid;
    std::unordered_map<TRI_voc_fid_t, MMFilesDatafileStatisticsContainer*>
        _stats;
    MMFilesDatafileStatisticsContainer* _dfi;
    transaction::Methods* _trx;
    ManagedDocumentResult _mmdr;
    IndexLookupContext _context;
    uint64_t _deletions;
    uint64_t _documents;
    uint64_t _operations;
    int64_t _initialCount;
    bool const _trackKeys;

    OpenIteratorState(LogicalCollection* collection, transaction::Methods* trx)
        : _collection(collection),
          _primaryIndex(
              static_cast<MMFilesCollection*>(collection->getPhysical())
                  ->primaryIndex()),
          _tid(0),
          _fid(0),
          _stats(),
          _dfi(nullptr),
          _trx(trx),
          _mmdr(),
          _context(trx, collection, &_mmdr, 1),
          _deletions(0),
          _documents(0),
          _operations(0),
          _initialCount(-1),
          _trackKeys(collection->keyGenerator()->trackKeys()) {
      TRI_ASSERT(collection != nullptr);
      TRI_ASSERT(trx != nullptr);
    }

    ~OpenIteratorState() {
      for (auto& it : _stats) {
        delete it.second;
      }
    }
  };

  struct DatafileDescription {
    MMFilesDatafile const* _data;
    TRI_voc_tick_t _dataMin;
    TRI_voc_tick_t _dataMax;
    TRI_voc_tick_t _tickMax;
    bool _isJournal;
  };

 public:
  explicit MMFilesCollection(LogicalCollection*, VPackSlice const& info);
  explicit MMFilesCollection(LogicalCollection*,
                             PhysicalCollection*);  // use in cluster only!!!!!

  ~MMFilesCollection();
  
  static constexpr uint32_t defaultIndexBuckets = 8;

  static constexpr double defaultLockTimeout = 10.0 * 60.0;

  std::string const& path() const override { return _path; };

  void setPath(std::string const& path) override { _path = path; };

  arangodb::Result updateProperties(VPackSlice const& slice,
                                    bool doSync) override;
  virtual arangodb::Result persistProperties() override;

  virtual PhysicalCollection* clone(LogicalCollection*) override;

  TRI_voc_rid_t revision(arangodb::transaction::Methods* trx) const override;
  TRI_voc_rid_t revision() const;

  void setRevision(TRI_voc_rid_t revision, bool force);

  void setRevisionError() { _revisionError = true; }

  size_t journalSize() const;
  bool isVolatile() const;

  TRI_voc_tick_t maxTick() const { return _maxTick; }
  void maxTick(TRI_voc_tick_t value) { _maxTick = value; }

  void getPropertiesVPack(velocypack::Builder&) const override;
  void getPropertiesVPackCoordinator(velocypack::Builder&) const override;

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

  int reserveJournalSpace(TRI_voc_tick_t tick, TRI_voc_size_t size,
                          char*& resultPosition,
                          MMFilesDatafile*& resultDatafile);

  /// @brief create compactor file
  MMFilesDatafile* createCompactor(TRI_voc_fid_t fid,
                                   TRI_voc_size_t maximalSize);

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
                   MMFilesDatafileStatisticsContainer const& values) {
    _datafileStatistics.update(fid, values);
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
  MMFilesPrimaryIndex* primaryIndex() const;

  inline bool useSecondaryIndexes() const { return _useSecondaryIndexes; }

  void useSecondaryIndexes(bool value) { _useSecondaryIndexes = value; }

  int fillAllIndexes(transaction::Methods*);

  void prepareIndexes(arangodb::velocypack::Slice indexesSlice) override;

  /// @brief Find index by definition
  std::shared_ptr<Index> lookupIndex(velocypack::Slice const&) const override;

  std::unique_ptr<IndexIterator> getAllIterator(transaction::Methods* trx,
                                                ManagedDocumentResult* mdr,
                                                bool reverse) const override;
  std::unique_ptr<IndexIterator> getAnyIterator(
      transaction::Methods* trx, ManagedDocumentResult* mdr) const override;
  void invokeOnAllElements(
      transaction::Methods* trx,
      std::function<bool(DocumentIdentifierToken const&)> callback) override;

  std::shared_ptr<Index> createIndex(transaction::Methods* trx,
                                     arangodb::velocypack::Slice const& info,
                                     bool& created) override;

  /// @brief Restores an index from VelocyPack.
  int restoreIndex(transaction::Methods*, velocypack::Slice const&,
                   std::shared_ptr<Index>&) override;

  /// @brief Drop an index with the given iid.
  bool dropIndex(TRI_idx_iid_t iid) override;

  ////////////////////////////////////
  // -- SECTION Locking --
  ///////////////////////////////////

  int lockRead(bool useDeadlockDetector, double timeout = 0.0);

  int lockWrite(bool useDeadlockDetector, double timeout = 0.0);

  int unlockRead(bool useDeadlockDetector);

  int unlockWrite(bool useDeadlockDetector);

  ////////////////////////////////////
  // -- SECTION DML Operations --
  ///////////////////////////////////

  void truncate(transaction::Methods* trx, OperationOptions& options) override;

  DocumentIdentifierToken lookupKey(transaction::Methods* trx,
                                    velocypack::Slice const& key) override;

  Result read(transaction::Methods*, arangodb::StringRef const& key,
              ManagedDocumentResult& result, bool) override;
  
  Result read(transaction::Methods*, arangodb::velocypack::Slice const& key,
              ManagedDocumentResult& result, bool) override;

  bool readDocument(transaction::Methods* trx,
                    DocumentIdentifierToken const& token,
                    ManagedDocumentResult& result) override;
  
  bool readDocumentWithCallback(transaction::Methods* trx,
                                DocumentIdentifierToken const& token,
                                IndexIterator::DocumentCallback const& cb) override;

  bool readDocumentConditional(transaction::Methods* trx,
                               DocumentIdentifierToken const& token,
                               TRI_voc_tick_t maxTick,
                               ManagedDocumentResult& result);

  Result insert(arangodb::transaction::Methods* trx,
                arangodb::velocypack::Slice const newSlice,
                arangodb::ManagedDocumentResult& result,
                OperationOptions& options,
                TRI_voc_tick_t& resultMarkerTick, bool lock) override;

  Result update(arangodb::transaction::Methods* trx,
                arangodb::velocypack::Slice const newSlice,
                arangodb::ManagedDocumentResult& result,
                OperationOptions& options,
                TRI_voc_tick_t& resultMarkerTick, bool lock,
                TRI_voc_rid_t& prevRev, ManagedDocumentResult& previous,
                TRI_voc_rid_t const& revisionId,
                arangodb::velocypack::Slice const key) override;

  Result replace(transaction::Methods* trx,
                 arangodb::velocypack::Slice const newSlice,
                 ManagedDocumentResult& result, OperationOptions& options,
                 TRI_voc_tick_t& resultMarkerTick, bool lock,
                 TRI_voc_rid_t& prevRev, ManagedDocumentResult& previous,
                 TRI_voc_rid_t const revisionId,
                 arangodb::velocypack::Slice const fromSlice,
                 arangodb::velocypack::Slice const toSlice) override;

  Result remove(arangodb::transaction::Methods* trx,
                arangodb::velocypack::Slice const slice,
                arangodb::ManagedDocumentResult& previous,
                OperationOptions& options, TRI_voc_tick_t& resultMarkerTick,
                bool lock, TRI_voc_rid_t const& revisionId,
                TRI_voc_rid_t& prevRev) override;

  /// @brief Defer a callback to be executed when the collection
  ///        can be dropped. The callback is supposed to drop
  ///        the collection and it is guaranteed that no one is using
  ///        it at that moment.
  void deferDropCollection(
      std::function<bool(LogicalCollection*)> callback) override;

  Result rollbackOperation(transaction::Methods*, TRI_voc_document_operation_e,
                           TRI_voc_rid_t oldRevisionId,
                           velocypack::Slice const& oldDoc,
                           TRI_voc_rid_t newRevisionId,
                           velocypack::Slice const& newDoc);

  MMFilesDocumentPosition insertRevision(TRI_voc_rid_t revisionId,
                                         uint8_t const* dataptr,
                                         TRI_voc_fid_t fid, bool isInWal,
                                         bool shouldLock);

  void insertRevision(MMFilesDocumentPosition const& position, bool shouldLock);

  void updateRevision(TRI_voc_rid_t revisionId, uint8_t const* dataptr,
                      TRI_voc_fid_t fid, bool isInWal);

  bool updateRevisionConditional(TRI_voc_rid_t revisionId,
                                 MMFilesMarker const* oldPosition,
                                 MMFilesMarker const* newPosition,
                                 TRI_voc_fid_t newFid, bool isInWal);

  void removeRevision(TRI_voc_rid_t revisionId, bool updateStats);

 private:
  void createInitialIndexes();
  void sizeHint(transaction::Methods* trx, int64_t hint);

  bool openIndex(VPackSlice const& description, transaction::Methods* trx);

  /// @brief initializes an index with all existing documents
  void fillIndex(std::shared_ptr<basics::LocalTaskQueue>, transaction::Methods*, Index*,
                 std::vector<std::pair<TRI_voc_rid_t, VPackSlice>> const&,
                 bool);

  /// @brief Fill indexes used in recovery
  int fillIndexes(transaction::Methods*,
                  std::vector<std::shared_ptr<Index>> const&,
                  bool skipPersistent = true);

  int openWorker(bool ignoreErrors);

  Result removeFastPath(arangodb::transaction::Methods* trx,
                        TRI_voc_rid_t oldRevisionId,
                        arangodb::velocypack::Slice const oldDoc,
                        OperationOptions& options,
                        TRI_voc_rid_t const& revisionId,
                        arangodb::velocypack::Slice const toRemove);

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
  MMFilesDatafile* createDatafile(TRI_voc_fid_t fid, TRI_voc_size_t journalSize,
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

  MMFilesDocumentPosition lookupRevision(TRI_voc_rid_t revisionId) const;

  Result insertDocument(arangodb::transaction::Methods* trx,
                        TRI_voc_rid_t revisionId,
                        arangodb::velocypack::Slice const& doc,
                        MMFilesDocumentOperation& operation,
                        MMFilesWalMarker const* marker, bool& waitForSync);

 private:
  uint8_t const* lookupRevisionVPack(TRI_voc_rid_t revisionId) const;
  uint8_t const* lookupRevisionVPackConditional(TRI_voc_rid_t revisionId,
                                                TRI_voc_tick_t maxTick,
                                                bool excludeWal) const;

  bool addIndex(std::shared_ptr<arangodb::Index> idx);
  void addIndexLocal(std::shared_ptr<arangodb::Index> idx);
  void addIndexCoordinator(std::shared_ptr<arangodb::Index> idx);

  bool removeIndex(TRI_idx_iid_t iid);

  /// @brief return engine-specific figures
  void figuresSpecific(
      std::shared_ptr<arangodb::velocypack::Builder>&) override;

  // SECTION: Index storage

  int saveIndex(transaction::Methods* trx,
                std::shared_ptr<arangodb::Index> idx);

  /// @brief Detect all indexes form file
  int detectIndexes(transaction::Methods* trx);

  Result insertIndexes(transaction::Methods* trx, TRI_voc_rid_t revisionId,
                       velocypack::Slice const& doc);

  Result insertPrimaryIndex(transaction::Methods*, TRI_voc_rid_t revisionId,
                            velocypack::Slice const&);

  Result deletePrimaryIndex(transaction::Methods*, TRI_voc_rid_t revisionId,
                            velocypack::Slice const&);

  Result insertSecondaryIndexes(transaction::Methods*, TRI_voc_rid_t revisionId,
                                velocypack::Slice const&, bool isRollback);

  Result deleteSecondaryIndexes(transaction::Methods*, TRI_voc_rid_t revisionId,
                                velocypack::Slice const&, bool isRollback);

  Result lookupDocument(transaction::Methods*, velocypack::Slice,
                        ManagedDocumentResult& result);

  Result updateDocument(transaction::Methods*, TRI_voc_rid_t oldRevisionId,
                        velocypack::Slice const& oldDoc,
                        TRI_voc_rid_t newRevisionId,
                        velocypack::Slice const& newDoc,
                        MMFilesDocumentOperation&,
                        MMFilesWalMarker const*, bool& waitForSync);

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

  bool _revisionError;

  MMFilesDatafileStatistics _datafileStatistics;

  TRI_voc_rid_t _lastRevision;

  MMFilesRevisionsCache _revisionsCache;

  std::atomic<int64_t> _uncollectedLogfileEntries;

  Mutex _compactionStatusLock;
  size_t _nextCompactionStartIndex;
  char const* _lastCompactionStatus;
  double _lastCompactionStamp;
  std::string _path;
  TRI_voc_size_t _journalSize;

  bool const _isVolatile;

  // SECTION: Indexes

  size_t _persistentIndexes;
  uint32_t _indexBuckets;

  // whether or not secondary indexes should be filled
  bool _useSecondaryIndexes;

  bool _doCompact;
  TRI_voc_tick_t _maxTick;
};
}

#endif

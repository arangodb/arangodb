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

#ifndef ARANGOD_STORAGE_ENGINE_MM_FILES_COLLECTION_H
#define ARANGOD_STORAGE_ENGINE_MM_FILES_COLLECTION_H 1

#include "Basics/Common.h"
#include "Basics/ReadWriteLock.h"
#include "Indexes/IndexLookupContext.h"
#include "StorageEngine/MMFilesDatafileStatistics.h"
#include "StorageEngine/MMFilesRevisionsCache.h"
#include "VocBase/Ditch.h"
#include "VocBase/KeyGenerator.h"
#include "VocBase/ManagedDocumentResult.h"
#include "VocBase/PhysicalCollection.h"

struct MMFilesDatafile;
struct TRI_df_marker_t;

namespace arangodb {
class LogicalCollection;
class PrimaryIndex;

class MMFilesCollection final : public PhysicalCollection {
 friend class MMFilesCompactorThread;
 friend class MMFilesEngine;

 public:
  /// @brief state during opening of a collection
  struct OpenIteratorState {
    LogicalCollection* _collection;
    arangodb::PrimaryIndex* _primaryIndex;
    TRI_voc_tid_t _tid;
    TRI_voc_fid_t _fid;
    std::unordered_map<TRI_voc_fid_t, DatafileStatisticsContainer*> _stats;
    DatafileStatisticsContainer* _dfi;
    arangodb::Transaction* _trx;
    ManagedDocumentResult _mmdr;
    IndexLookupContext _context;
    uint64_t _deletions;
    uint64_t _documents;
    uint64_t _operations;
    int64_t _initialCount;
    bool const _trackKeys;

    OpenIteratorState(LogicalCollection* collection, arangodb::Transaction* trx) 
        : _collection(collection),
          _primaryIndex(collection->primaryIndex()),
          _tid(0),
          _fid(0),
          _stats(),
          _dfi(nullptr),
          _trx(trx),
          _mmdr(trx),
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
  explicit MMFilesCollection(LogicalCollection*);
  ~MMFilesCollection();

  TRI_voc_rid_t revision() const override;

  void setRevision(TRI_voc_rid_t revision, bool force) override;

  int64_t initialCount() const override;
  void updateCount(int64_t) override;
  
  /// @brief return engine-specific figures
  void figures(std::shared_ptr<arangodb::velocypack::Builder>&) override;
  
  // datafile management
  bool applyForTickRange(TRI_voc_tick_t dataMin, TRI_voc_tick_t dataMax,
                         std::function<bool(TRI_voc_tick_t foundTick, TRI_df_marker_t const* marker)> const& callback) override;

  /// @brief closes an open collection
  int close() override;
  
  /// @brief rotate the active journal - will do nothing if there is no journal
  int rotateActiveJournal() override;

  /// @brief sync the active journal - will do nothing if there is no journal
  /// or if the journal is volatile
  int syncActiveJournal();

  int reserveJournalSpace(TRI_voc_tick_t tick, TRI_voc_size_t size,
                          char*& resultPosition, MMFilesDatafile*& resultDatafile);

  /// @brief create compactor file
  MMFilesDatafile* createCompactor(TRI_voc_fid_t fid, TRI_voc_size_t maximalSize);
  
  /// @brief close an existing compactor
  int closeCompactor(MMFilesDatafile* datafile);

  /// @brief replace a datafile with a compactor
  int replaceDatafileWithCompactor(MMFilesDatafile* datafile, MMFilesDatafile* compactor);

  bool removeCompactor(MMFilesDatafile*);
  bool removeDatafile(MMFilesDatafile*);
  
  /// @brief seal a datafile
  int sealDatafile(MMFilesDatafile* datafile, bool isCompactor);

  /// @brief increase dead stats for a datafile, if it exists
  void updateStats(TRI_voc_fid_t fid, DatafileStatisticsContainer const& values) override {
    _datafileStatistics.update(fid, values);
  }
   
  /// @brief report extra memory used by indexes etc.
  size_t memory() const override;

  void preventCompaction() override;
  bool tryPreventCompaction() override;
  void allowCompaction() override;
  void lockForCompaction() override;
  bool tryLockForCompaction() override;
  void finishCompaction() override;
  
  Ditches* ditches() const override { return &_ditches; }
  
  /// @brief iterate all markers of a collection on load
  int iterateMarkersOnLoad(arangodb::Transaction* trx) override;

 private:
  static int OpenIteratorHandleDocumentMarker(TRI_df_marker_t const* marker,
                                              MMFilesDatafile* datafile,
                                              OpenIteratorState* state);
  static int OpenIteratorHandleDeletionMarker(TRI_df_marker_t const* marker,
                                              MMFilesDatafile* datafile,
                                              OpenIteratorState* state);
  static bool OpenIterator(TRI_df_marker_t const* marker, OpenIteratorState* data, MMFilesDatafile* datafile);

  /// @brief create statistics for a datafile, using the stats provided
  void createStats(TRI_voc_fid_t fid, DatafileStatisticsContainer const& values) {
    _datafileStatistics.create(fid, values);
  }
  
  /// @brief iterates over a collection
  bool iterateDatafiles(std::function<bool(TRI_df_marker_t const*, MMFilesDatafile*)> const& cb);
  
  /// @brief creates a datafile
  MMFilesDatafile* createDatafile(TRI_voc_fid_t fid,
                                 TRI_voc_size_t journalSize, 
                                 bool isCompactor);

  /// @brief iterate over a vector of datafiles and pick those with a specific
  /// data range
  std::vector<DatafileDescription> datafilesInRange(TRI_voc_tick_t dataMin, TRI_voc_tick_t dataMax);
  
  /// @brief closes the datafiles passed in the vector
  bool closeDatafiles(std::vector<MMFilesDatafile*> const& files);

  bool iterateDatafilesVector(std::vector<MMFilesDatafile*> const& files,
                              std::function<bool(TRI_df_marker_t const*, MMFilesDatafile*)> const& cb);

  MMFilesDocumentPosition lookupRevision(TRI_voc_rid_t revisionId) const;

  uint8_t const* lookupRevisionVPack(TRI_voc_rid_t revisionId) const override;
  uint8_t const* lookupRevisionVPackConditional(TRI_voc_rid_t revisionId, TRI_voc_tick_t maxTick, bool excludeWal) const override;
  void insertRevision(TRI_voc_rid_t revisionId, uint8_t const* dataptr, TRI_voc_fid_t fid, bool isInWal, bool shouldLock) override;
  void updateRevision(TRI_voc_rid_t revisionId, uint8_t const* dataptr, TRI_voc_fid_t fid, bool isInWal) override;
  bool updateRevisionConditional(TRI_voc_rid_t revisionId, TRI_df_marker_t const* oldPosition, TRI_df_marker_t const* newPosition, TRI_voc_fid_t newFid, bool isInWal) override;
  void removeRevision(TRI_voc_rid_t revisionId, bool updateStats) override;
  
 private:
  mutable arangodb::Ditches _ditches;

  arangodb::basics::ReadWriteLock _filesLock;
  std::vector<MMFilesDatafile*> _datafiles;   // all datafiles
  std::vector<MMFilesDatafile*> _journals;    // all journals
  std::vector<MMFilesDatafile*> _compactors;  // all compactor files
  
  arangodb::basics::ReadWriteLock _compactionLock;

  int64_t _initialCount;
  
  MMFilesDatafileStatistics _datafileStatistics;

  TRI_voc_rid_t _lastRevision;
  
  MMFilesRevisionsCache _revisionsCache;
};

}

#endif

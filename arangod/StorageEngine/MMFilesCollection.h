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
#include "StorageEngine/MMFilesDatafileStatistics.h"
#include "VocBase/Ditch.h"
#include "VocBase/MasterPointers.h"
#include "VocBase/PhysicalCollection.h"

struct TRI_datafile_t;
struct TRI_df_marker_t;

namespace arangodb {
class LogicalCollection;

class MMFilesCollection final : public PhysicalCollection {
 friend class MMFilesCompactorThread;
 friend class MMFilesEngine;

  struct DatafileDescription {
    TRI_datafile_t const* _data;
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
  int applyForTickRange(TRI_voc_tick_t dataMin, TRI_voc_tick_t dataMax,
                        std::function<bool(TRI_voc_tick_t foundTick, TRI_df_marker_t const* marker)> const& callback) override;

  /// @brief closes an open collection
  int close() override;
  
  /// @brief rotate the active journal - will do nothing if there is no journal
  int rotateActiveJournal() override;

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
  
  /// @brief seal a datafile
  int sealDatafile(TRI_datafile_t* datafile, bool isCompactor);

  /// @brief increase dead stats for a datafile, if it exists
  void increaseDeadStats(TRI_voc_fid_t fid, int64_t number, int64_t size) override {
    _datafileStatistics.increaseDead(fid, number, size);
  }
  
  /// @brief increase dead stats for a datafile, if it exists
  void updateStats(TRI_voc_fid_t fid, DatafileStatisticsContainer const& values) override {
    _datafileStatistics.update(fid, values);
  }

  /// @brief order a new master pointer
  TRI_doc_mptr_t* requestMasterpointer() override;

  /// @brief release an existing master pointer
  void releaseMasterpointer(TRI_doc_mptr_t* mptr) override;
  
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
  /// @brief create statistics for a datafile, using the stats provided
  void createStats(TRI_voc_fid_t fid, DatafileStatisticsContainer const& values) {
    _datafileStatistics.create(fid, values);
  }
    
  /// @brief iterates over a collection
  bool iterateDatafiles(std::function<bool(TRI_df_marker_t const*, TRI_datafile_t*)> const& cb);
  
  /// @brief creates a datafile
  TRI_datafile_t* createDatafile(TRI_voc_fid_t fid,
                                 TRI_voc_size_t journalSize, 
                                 bool isCompactor);

  /// @brief iterate over a vector of datafiles and pick those with a specific
  /// data range
  std::vector<DatafileDescription> datafilesInRange(TRI_voc_tick_t dataMin, TRI_voc_tick_t dataMax);
  
  /// @brief closes the datafiles passed in the vector
  bool closeDatafiles(std::vector<TRI_datafile_t*> const& files);

  bool iterateDatafilesVector(std::vector<TRI_datafile_t*> const& files,
                              std::function<bool(TRI_df_marker_t const*, TRI_datafile_t*)> const& cb);

 private:
  arangodb::MasterPointers _masterPointers;

  mutable arangodb::Ditches _ditches;

  arangodb::basics::ReadWriteLock _filesLock;
  std::vector<TRI_datafile_t*> _datafiles;   // all datafiles
  std::vector<TRI_datafile_t*> _journals;    // all journals
  std::vector<TRI_datafile_t*> _compactors;  // all compactor files
  
  arangodb::basics::ReadWriteLock _compactionLock;

  int64_t _initialCount;
  
  MMFilesDatafileStatistics _datafileStatistics;

  TRI_voc_rid_t _revision;
};

}

#endif

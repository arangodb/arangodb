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

  /// @brief iterates over a collection
  bool iterateDatafiles(std::function<bool(TRI_df_marker_t const*, TRI_datafile_t*)> const& cb) override;

  void preventCompaction() override;
  bool tryPreventCompaction() override;
  void allowCompaction() override;
  void lockForCompaction() override;
  bool tryLockForCompaction() override;
  void finishCompaction() override;

 private:
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
  arangodb::basics::ReadWriteLock _filesLock;
  std::vector<TRI_datafile_t*> _datafiles;   // all datafiles
  std::vector<TRI_datafile_t*> _journals;    // all journals
  std::vector<TRI_datafile_t*> _compactors;  // all compactor files
  
  arangodb::basics::ReadWriteLock _compactionLock;
  int64_t _initialCount;
};

}

#endif

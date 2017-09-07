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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_MMFILES_MMFILES_DATAFILE_STATISTICS_H
#define ARANGOD_MMFILES_MMFILES_DATAFILE_STATISTICS_H 1

#include "Basics/Common.h"
#include "Basics/ReadWriteLock.h"
#include "MMFiles/MMFilesDatafileStatisticsContainer.h"
#include "VocBase/voc-types.h"

namespace arangodb {

/// @brief datafile statistics manager for a single collection
class MMFilesDatafileStatistics {
 private:
  int64_t _compactionCount;
  int64_t _compactionBytesRead;
  int64_t _compactionBytesWritten;
  int64_t _filesCombined;

 public:
  MMFilesDatafileStatistics(MMFilesDatafileStatistics const&) = delete;
  MMFilesDatafileStatistics& operator=(MMFilesDatafileStatistics const&) = delete;
  MMFilesDatafileStatistics();
  ~MMFilesDatafileStatistics();

 public:
  void compactionRun()                        { _compactionCount ++; }
  void compactionCombine(uint64_t noCombined) { _filesCombined += noCombined; }
  void compactionRead(uint64_t read)          { _compactionBytesRead += read; }
  void compactionWritten(uint64_t written)    { _compactionBytesWritten += written; }

  uint64_t getCompactionRuns()          const { return _compactionCount; }
  uint64_t getCompactionFilesCombined() const { return _filesCombined; }
  uint64_t getCompactionRead()          const { return _compactionBytesRead; }
  uint64_t getCompactionWritten()       const { return _compactionBytesWritten; }

  /// @brief create (empty) statistics for a datafile
  void create(TRI_voc_fid_t);

  /// @brief create statistics for a datafile, using the stats provided
  void create(TRI_voc_fid_t, MMFilesDatafileStatisticsContainer const&);

  /// @brief remove statistics for a datafile
  void remove(TRI_voc_fid_t);

  /// @brief merge statistics for a datafile
  void update(TRI_voc_fid_t, MMFilesDatafileStatisticsContainer const&);

  /// @brief merge statistics for a datafile, by copying the stats from another
  void update(TRI_voc_fid_t, TRI_voc_fid_t);

  /// @brief replace statistics for a datafile
  void replace(TRI_voc_fid_t, MMFilesDatafileStatisticsContainer const&);

  /// @brief increase dead stats for a datafile, if it exists
  void increaseDead(TRI_voc_fid_t, int64_t, int64_t);

  /// @brief increase number of uncollected entries
  void increaseUncollected(TRI_voc_fid_t, int64_t);

  /// @brief return a copy of the datafile statistics for a file
  MMFilesDatafileStatisticsContainer get(TRI_voc_fid_t);

  /// @brief return aggregated datafile statistics for all files
  MMFilesDatafileStatisticsContainer all();

 private:
  /// @brief lock to protect the statistics
  arangodb::basics::ReadWriteLock _lock;

  /// @brief per-file statistics
  std::unordered_map<TRI_voc_fid_t, MMFilesDatafileStatisticsContainer*> _stats;
};
}

#endif

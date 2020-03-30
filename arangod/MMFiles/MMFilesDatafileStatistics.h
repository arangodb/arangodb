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
#include "VocBase/Identifiers/FileId.h"
#include "VocBase/voc-types.h"

namespace arangodb {

/// @brief datafile statistics manager for a single collection
class MMFilesDatafileStatistics {
 public:
  struct CompactionStats {
    uint64_t _compactionCount;
    uint64_t _compactionBytesRead;
    uint64_t _compactionBytesWritten;
    uint64_t _filesCombined;
  };

 public:
  MMFilesDatafileStatistics(MMFilesDatafileStatistics const&) = delete;
  MMFilesDatafileStatistics& operator=(MMFilesDatafileStatistics const&) = delete;
  MMFilesDatafileStatistics();
  ~MMFilesDatafileStatistics();

 public:
  // @brief increase collection statistics
  void compactionRun(uint64_t noCombined, uint64_t read, uint64_t written);

  // @brief get current collection statistics
  MMFilesDatafileStatistics::CompactionStats getStats();

  /// @brief create (empty) statistics for a datafile
  void create(FileId);

  /// @brief create statistics for a datafile, using the stats provided
  void create(FileId, MMFilesDatafileStatisticsContainer const&);

  /// @brief remove statistics for a datafile
  void remove(FileId);

  /// @brief merge statistics for a datafile
  void update(FileId, MMFilesDatafileStatisticsContainer const&, bool warn);

  /// @brief merge statistics for a datafile, by copying the stats from another
  void update(FileId fid, FileId src, bool warn);

  /// @brief replace statistics for a datafile
  void replace(FileId, MMFilesDatafileStatisticsContainer const&, bool warn);

  /// @brief increase dead stats for a datafile, if it exists
  void increaseDead(FileId, int64_t, int64_t);

  /// @brief increase number of uncollected entries
  void increaseUncollected(FileId, int64_t);

  /// @brief return a copy of the datafile statistics for a file
  MMFilesDatafileStatisticsContainer get(FileId);

  /// @brief return aggregated datafile statistics for all files
  MMFilesDatafileStatisticsContainer all();

 private:
  /// @brief lock to protect the statistics
  arangodb::basics::ReadWriteLock _lock;

  /// @brief per-file statistics
  std::unordered_map<FileId, MMFilesDatafileStatisticsContainer*> _stats;

  // @brief per-collection runtime statistics
  arangodb::basics::ReadWriteLock _statisticsLock;
  MMFilesDatafileStatistics::CompactionStats _localStats;
};
}  // namespace arangodb

#endif

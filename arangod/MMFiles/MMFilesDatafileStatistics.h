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

#ifndef ARANGOD_STORAGE_ENGINE_MMFILES_DATAFILE_STATISTICS_H
#define ARANGOD_STORAGE_ENGINE_MMFILES_DATAFILE_STATISTICS_H 1

#include "Basics/Common.h"
#include "Basics/ReadWriteLock.h"
#include "VocBase/DatafileStatisticsContainer.h"
#include "VocBase/voc-types.h"

namespace arangodb {

/// @brief datafile statistics manager for a single collection
class MMFilesDatafileStatistics {
 public:
  MMFilesDatafileStatistics(MMFilesDatafileStatistics const&) = delete;
  MMFilesDatafileStatistics& operator=(MMFilesDatafileStatistics const&) = delete;
  MMFilesDatafileStatistics();
  ~MMFilesDatafileStatistics();

 public:
  /// @brief create (empty) statistics for a datafile
  void create(TRI_voc_fid_t);

  /// @brief create statistics for a datafile, using the stats provided
  void create(TRI_voc_fid_t, DatafileStatisticsContainer const&);

  /// @brief remove statistics for a datafile
  void remove(TRI_voc_fid_t);

  /// @brief merge statistics for a datafile
  void update(TRI_voc_fid_t, DatafileStatisticsContainer const&);

  /// @brief merge statistics for a datafile, by copying the stats from another
  void update(TRI_voc_fid_t, TRI_voc_fid_t);

  /// @brief replace statistics for a datafile
  void replace(TRI_voc_fid_t, DatafileStatisticsContainer const&);

  /// @brief increase dead stats for a datafile, if it exists
  void increaseDead(TRI_voc_fid_t, int64_t, int64_t);

  /// @brief increase number of uncollected entries
  void increaseUncollected(TRI_voc_fid_t, int64_t);

  /// @brief return a copy of the datafile statistics for a file
  DatafileStatisticsContainer get(TRI_voc_fid_t);

  /// @brief return aggregated datafile statistics for all files
  DatafileStatisticsContainer all();

 private:
  /// @brief lock to protect the statistics
  arangodb::basics::ReadWriteLock _lock;

  /// @brief per-file statistics
  std::unordered_map<TRI_voc_fid_t, DatafileStatisticsContainer*> _stats;
};
}

#endif

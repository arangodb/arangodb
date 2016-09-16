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

#include "MMFilesDatafileStatistics.h"
#include "Basics/Exceptions.h"
#include "Logger/Logger.h"
#include "Basics/ReadLocker.h"
#include "Basics/WriteLocker.h"
#include "VocBase/datafile.h"

using namespace arangodb;

/// @brief create statistics manager for a collection
MMFilesDatafileStatistics::MMFilesDatafileStatistics() : _lock(), _stats() {}

/// @brief destroy statistics manager
MMFilesDatafileStatistics::~MMFilesDatafileStatistics() {
  WRITE_LOCKER(writeLocker, _lock);

  for (auto& it : _stats) {
    delete it.second;
  }
}

/// @brief create an empty statistics container for a file
void MMFilesDatafileStatistics::create(TRI_voc_fid_t fid) {
  auto stats = std::make_unique<DatafileStatisticsContainer>();

  WRITE_LOCKER(writeLocker, _lock);

  auto it = _stats.find(fid);

  if (it != _stats.end()) {
    // already exists
    return;
  }

  LOG(TRACE) << "creating statistics for datafile " << fid;
  _stats.emplace(fid, stats.get());
  stats.release();
}

/// @brief create statistics for a datafile, using the stats provided
void MMFilesDatafileStatistics::create(TRI_voc_fid_t fid,
                                DatafileStatisticsContainer const& src) {
  auto stats = std::make_unique<DatafileStatisticsContainer>();
  *stats = src;

  WRITE_LOCKER(writeLocker, _lock);

  auto it = _stats.find(fid);

  if (it != _stats.end()) {
    // already exists
    return;
  }

  LOG(TRACE) << "creating statistics for datafile " << fid << " from initial data";

  _stats.emplace(fid, stats.get());
  stats.release();
}

/// @brief remove statistics for a file
void MMFilesDatafileStatistics::remove(TRI_voc_fid_t fid) {
  LOG(TRACE) << "removing statistics for datafile " << fid;

  DatafileStatisticsContainer* found = nullptr;
  {
    WRITE_LOCKER(writeLocker, _lock);

    auto it = _stats.find(fid);

    if (it != _stats.end()) {
      found = (*it).second;    
      _stats.erase(it);
    }
  }
 
  delete found;
}

/// @brief merge statistics for a file
void MMFilesDatafileStatistics::update(TRI_voc_fid_t fid,
                                DatafileStatisticsContainer const& src) {
  WRITE_LOCKER(writeLocker, _lock);

  auto it = _stats.find(fid);

  if (it == _stats.end()) {
    LOG(WARN) << "did not find required statistics for datafile " << fid;
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_DATAFILE_STATISTICS_NOT_FOUND,
                                   "required datafile statistics not found");
  }

  auto& dst = (*it).second;

  LOG(TRACE) << "updating statistics for datafile " << fid;
  dst->update(src);
}

/// @brief merge statistics for a file, by copying the stats from another
void MMFilesDatafileStatistics::update(TRI_voc_fid_t fid, TRI_voc_fid_t src) {
  WRITE_LOCKER(writeLocker, _lock);

  auto it = _stats.find(fid);

  if (it == _stats.end()) {
    LOG(WARN) << "did not find required statistics for datafile " << fid;
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_DATAFILE_STATISTICS_NOT_FOUND,
                                   "required datafile statistics not found");
  }

  auto& dst = (*it).second;

  it = _stats.find(src);

  if (it == _stats.end()) {
    LOG(WARN) << "did not find required statistics for source datafile " << src;
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_DATAFILE_STATISTICS_NOT_FOUND,
                                   "required datafile statistics not found");
  }

  LOG(TRACE) << "updating statistics for datafile " << fid;
  dst->update(*(*it).second);
}

/// @brief replace statistics for a file
void MMFilesDatafileStatistics::replace(TRI_voc_fid_t fid,
                                 DatafileStatisticsContainer const& src) {
  WRITE_LOCKER(writeLocker, _lock);

  auto it = _stats.find(fid);

  if (it == _stats.end()) {
    LOG(WARN) << "did not find required statistics for datafile " << fid;
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_DATAFILE_STATISTICS_NOT_FOUND,
                                   "required datafile statistics not found");
  }

  auto& dst = (*it).second;
  *dst = src;

  LOG(TRACE) << "replacing statistics for datafile " << fid;
}

/// @brief increase dead stats for a datafile, if it exists
void MMFilesDatafileStatistics::increaseDead(TRI_voc_fid_t fid, int64_t number,
                                      int64_t size) {
  WRITE_LOCKER(writeLocker, _lock);

  auto it = _stats.find(fid);

  if (it == _stats.end()) {
    // datafile not found. no problem
    return;
  }

  auto& dst = (*it).second;
  dst->numberDead += number;
  dst->sizeDead += size;
  dst->numberAlive -= number;
  dst->sizeAlive -= size;
}

/// @brief increase number of uncollected entries
void MMFilesDatafileStatistics::increaseUncollected(TRI_voc_fid_t fid,
                                             int64_t number) {
  WRITE_LOCKER(writeLocker, _lock);

  auto it = _stats.find(fid);

  if (it == _stats.end()) {
    // datafile not found. no problem
    return;
  }

  auto& dst = (*it).second;
  dst->numberUncollected += number;

  LOG(TRACE) << "increasing uncollected count for datafile " << fid;
}

/// @brief return a copy of the datafile statistics for a file
DatafileStatisticsContainer MMFilesDatafileStatistics::get(TRI_voc_fid_t fid) {
  DatafileStatisticsContainer result;
  {
    READ_LOCKER(readLocker, _lock);

    auto it = _stats.find(fid);

    if (it == _stats.end()) {
      LOG(WARN) << "did not find required statistics for datafile " << fid;
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_DATAFILE_STATISTICS_NOT_FOUND,
                                     "required datafile statistics not found");
    }

    result = *(*it).second;
  }

  return result;
}

/// @brief return a copy of the datafile statistics for a file
DatafileStatisticsContainer MMFilesDatafileStatistics::all() {
  DatafileStatisticsContainer result;
  {
    READ_LOCKER(readLocker, _lock);

    for (auto& it : _stats) {
      result.update(*(it.second));
    }
  }

  return result;
}

